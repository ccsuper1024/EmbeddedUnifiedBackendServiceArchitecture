#include "runtime.h"

#include "conn.h"
#include "logger.h"
#include "lua_vm.h"

#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>

namespace backend {

namespace {

std::uint64_t NowMs() {
  auto now = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
  return static_cast<std::uint64_t>(ms.count());
}

bool IsRtpPacket(const char* data, std::size_t len, std::uint32_t& out_ssrc) {
  if (len < 12) {
    return false;
  }
  unsigned char b0 = static_cast<unsigned char>(data[0]);
  unsigned int version = b0 >> 6;
  if (version != 2) {
    return false;
  }
  unsigned char pt = static_cast<unsigned char>(data[1]) & 0x7F;
  if (pt > 127) {
    return false;
  }
  out_ssrc =
      (static_cast<std::uint32_t>(static_cast<unsigned char>(data[8])) << 24) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(data[9])) << 16) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(data[10])) << 8) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(data[11])));
  return true;
}

void LoadStateFiles(LuaVm& vm) {
  DIR* dir = ::opendir("state");
  if (!dir) {
    return;
  }
  dirent* entry = nullptr;
  while ((entry = ::readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.') {
      continue;
    }
    std::string filename(entry->d_name);
    std::string path = std::string("state/") + filename;
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
      continue;
    }
    std::string data((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    std::size_t dot = filename.find('.');
    std::string name = dot == std::string::npos ? filename : filename.substr(0, dot);
    vm.RestoreState(name, data);
  }
  ::closedir(dir);
}

int SetNonBlocking(int fd) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return -1;
  }
  if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    return -1;
  }
  return 0;
}

int CreateTcpListenSocket(std::uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  int on = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return -1;
  }
  if (::listen(fd, 128) < 0) {
    ::close(fd);
    return -1;
  }
  if (SetNonBlocking(fd) != 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

int CreateUdpSocket(std::uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return -1;
  }
  int on = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return -1;
  }
  if (SetNonBlocking(fd) != 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

std::string IpFromSockaddr(const sockaddr_in& addr) {
  char buffer[64];
  const char* result = ::inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer));
  if (!result) {
    return {};
  }
  return std::string(result);
}

}  // namespace

Runtime::Runtime(const AppConfig& config)
    : config_(config),
      running_(false) {
  for (int i = 0; i < config_.worker_threads; ++i) {
    io_to_worker_.push_back(
        std::make_unique<MpscQueue<Event>>(config_.queue_size_io_to_worker));
    worker_to_io_.push_back(
        std::make_unique<MpscQueue<GenericTask>>(config_.queue_size_worker_to_io));
    worker_to_disk_.push_back(
        std::make_unique<MpscQueue<DiskTask>>(config_.queue_size_worker_to_disk));
    auto vm = std::make_unique<LuaVm>(config_.lua_main_script,
                                      worker_to_io_.back().get(),
                                      worker_to_disk_.back().get(),
                                      worker_to_log_.get(),
                                      i);
    if (!vm->Init()) {
      throw std::runtime_error("failed to initialize lua vm");
    }
    lua_vms_.push_back(std::move(vm));
  }
  worker_to_log_ =
      std::make_unique<MpscQueue<LogTask>>(config_.queue_size_worker_to_log);
  if (!lua_vms_.empty()) {
    LoadStateFiles(*lua_vms_[0]);
  }
}

Runtime::~Runtime() {
  Stop();
  Join();
}

void Runtime::Start() {
  if (running_.exchange(true)) {
    return;
  }
  StartTcpIoThreads();
  StartUdpIoThreads();
  StartWorkerThreads();
  StartDiskThreads();
  StartLogThreads();
  StartTimerThreads();
}

void Runtime::Stop() {
  running_.store(false);
}

void Runtime::Join() {
  for (auto& t : tcp_io_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  for (auto& t : udp_io_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  for (auto& t : worker_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  for (auto& t : disk_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  for (auto& t : log_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  for (auto& t : timer_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void Runtime::StartTcpIoThreads() {
  for (int i = 0; i < config_.tcp_io_threads; ++i) {
    tcp_io_threads_.push_back(std::thread([this, i]() { RunTcpIoThread(i); }));
  }
}

void Runtime::StartUdpIoThreads() {
  for (int i = 0; i < config_.udp_io_threads; ++i) {
    udp_io_threads_.push_back(std::thread([this, i]() { RunUdpIoThread(i); }));
  }
}

void Runtime::StartWorkerThreads() {
  for (int i = 0; i < config_.worker_threads; ++i) {
    worker_threads_.push_back(std::thread([this, i]() { RunWorkerThread(i); }));
  }
}

void Runtime::StartDiskThreads() {
  for (int i = 0; i < config_.disk_threads; ++i) {
    disk_threads_.push_back(std::thread([this, i]() { RunDiskThread(i); }));
  }
}

void Runtime::StartLogThreads() {
  for (int i = 0; i < config_.log_threads; ++i) {
    log_threads_.push_back(std::thread([this, i]() { RunLogThread(i); }));
  }
}

void Runtime::StartTimerThreads() {
  for (int i = 0; i < config_.timer_threads; ++i) {
    timer_threads_.push_back(std::thread([this, i]() { RunTimerThread(i); }));
  }
}

void Runtime::RunTcpIoThread(int index) {
  auto logger = GetLogger();
  logger->info("tcp io thread {} started", index);
  int listen_fd = CreateTcpListenSocket(config_.tcp_port);
  if (listen_fd < 0) {
    logger->error("tcp io thread {} failed to create listen socket", index);
    return;
  }
  int epoll_fd = ::epoll_create1(0);
  if (epoll_fd < 0) {
    ::close(listen_fd);
    logger->error("tcp io thread {} failed to create epoll", index);
    return;
  }
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd;
  if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
    ::close(epoll_fd);
    ::close(listen_fd);
    logger->error("tcp io thread {} failed to add listen fd to epoll", index);
    return;
  }
  TcpConnTable conn_table;
  const int max_events = 64;
  std::vector<epoll_event> events(max_events);
  while (running_.load()) {
    int n = ::epoll_wait(epoll_fd, events.data(), max_events, 1000);
    if (n < 0) {
      continue;
    }
    for (int i_event = 0; i_event < n; ++i_event) {
      int fd = events[i_event].data.fd;
      if (fd == listen_fd) {
        while (true) {
          sockaddr_in addr;
          socklen_t addr_len = sizeof(addr);
          int client_fd =
              ::accept(listen_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
          if (client_fd < 0) {
            break;
          }
          if (SetNonBlocking(client_fd) != 0) {
            ::close(client_fd);
            continue;
          }
          epoll_event client_ev;
          client_ev.events = EPOLLIN | EPOLLRDHUP;
          client_ev.data.fd = client_fd;
          if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
            ::close(client_fd);
            continue;
          }
          Conn conn;
          conn.fd = client_fd;
          conn.state = ConnState::Established;
          conn.worker_index = client_fd % config_.worker_threads;
          conn.protocol = ProtocolType::Tcp;
          conn.remote_ip = IpFromSockaddr(addr);
          conn.remote_port = ntohs(addr.sin_port);
          conn.last_active_ms = NowMs();
          conn_table.Add(conn);
          logger->info("tcp connection accepted fd={} worker={}", client_fd,
                       conn.worker_index);
        }
      } else {
        Conn* conn = conn_table.Find(fd);
        if (!conn) {
          continue;
        }
        bool closed = false;
        char buffer[4096];
        while (true) {
          ssize_t received = ::recv(fd, buffer, sizeof(buffer), 0);
          if (received > 0) {
            conn->last_active_ms = NowMs();
            conn->recv_buffer.append(buffer, static_cast<std::size_t>(received));
          } else if (received == 0) {
            closed = true;
            break;
          } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            closed = true;
            break;
          }
        }
        if (!conn->recv_buffer.empty()) {
          Event event;
          event.protocol = ProtocolType::Tcp;
          event.session_id = static_cast<std::uint64_t>(fd);
          event.context.timestamp_ms = NowMs();
          event.context.remote_ip = conn->remote_ip;
          event.context.remote_port = conn->remote_port;
          event.payload = conn->recv_buffer;
          conn->recv_buffer.clear();
          int worker_index = conn->worker_index % config_.worker_threads;
          io_to_worker_[worker_index]->Push(std::move(event));
        }
        if (closed) {
          ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
          ::close(fd);
          conn_table.Remove(fd);
          logger->info("tcp connection closed fd={}", fd);
        }
      }
    }
    GenericTask outbound;
    bool has_task = false;
    for (auto& queue : worker_to_io_) {
      if (queue->Pop(outbound)) {
        has_task = true;
        break;
      }
    }
    if (has_task) {
      if (outbound.type == TaskType::Tcp) {
        int fd = static_cast<int>(outbound.session_id);
        Conn* target = conn_table.Find(fd);
        if (target) {
          const char* data = outbound.payload.data();
          std::size_t remaining = outbound.payload.size();
          while (remaining > 0) {
            ssize_t sent = ::send(fd, data, remaining, 0);
            if (sent > 0) {
              data += sent;
              remaining -= static_cast<std::size_t>(sent);
            } else {
              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
              }
              break;
            }
          }
        }
      }
    }
  }
  ::close(epoll_fd);
  ::close(listen_fd);
  logger->info("tcp io thread {} stopped", index);
}

void Runtime::RunUdpIoThread(int index) {
  auto logger = GetLogger();
  logger->info("udp io thread {} started", index);
  int udp_fd = CreateUdpSocket(config_.tcp_port);
  if (udp_fd < 0) {
    logger->error("udp io thread {} failed to create udp socket", index);
    return;
  }
  int epoll_fd = ::epoll_create1(0);
  if (epoll_fd < 0) {
    ::close(udp_fd);
    logger->error("udp io thread {} failed to create epoll", index);
    return;
  }
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = udp_fd;
  if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_fd, &ev) < 0) {
    ::close(epoll_fd);
    ::close(udp_fd);
    logger->error("udp io thread {} failed to add udp fd to epoll", index);
    return;
  }
  UdpSessionTable session_table;
  RtpSessionTable rtp_table;
  const int max_events = 64;
  std::vector<epoll_event> events(max_events);
  while (running_.load()) {
    int n = ::epoll_wait(epoll_fd, events.data(), max_events, 1000);
    if (n < 0) {
      continue;
    }
    for (int i_event = 0; i_event < n; ++i_event) {
      int fd = events[i_event].data.fd;
      if (fd != udp_fd) {
        continue;
      }
      while (true) {
        sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        char buffer[4096];
        ssize_t received =
            ::recvfrom(fd, buffer, sizeof(buffer), 0,
                       reinterpret_cast<sockaddr*>(&addr), &addr_len);
        if (received > 0) {
          std::string ip = IpFromSockaddr(addr);
          std::uint16_t port = ntohs(addr.sin_port);
          std::uint64_t now = NowMs();
          std::uint32_t ssrc = 0;
          bool is_rtp =
              IsRtpPacket(buffer, static_cast<std::size_t>(received), ssrc);
          if (is_rtp) {
            RtpSession* rtp_session = rtp_table.FindOrCreate(ssrc, now);
            Event event;
            event.protocol = ProtocolType::Rtp;
            event.session_id = rtp_session->id;
            event.context.timestamp_ms = now;
            event.context.remote_ip = ip;
            event.context.remote_port = port;
            event.payload.assign(buffer, static_cast<std::size_t>(received));
            int worker_index =
                static_cast<int>(rtp_session->id %
                                 static_cast<std::uint64_t>(config_.worker_threads));
            io_to_worker_[worker_index]->Push(std::move(event));
            if (worker_index >= 0 &&
                worker_index < static_cast<int>(worker_to_disk_.size())) {
              DiskTask record;
              record.op = DiskOp::Append;
              record.path =
                  "rtp/session_" + std::to_string(rtp_session->id) + ".bin";
              record.data.assign(buffer, static_cast<std::size_t>(received));
              worker_to_disk_[worker_index]->Push(std::move(record));
            }
          } else {
            UdpSession* session =
                session_table.FindOrCreate(ip, port, ProtocolType::Udp, now);
            Event event;
            event.protocol = ProtocolType::Udp;
            event.session_id = session->id;
            event.context.timestamp_ms = now;
            event.context.remote_ip = ip;
            event.context.remote_port = port;
            event.payload.assign(buffer, static_cast<std::size_t>(received));
            int worker_index =
                static_cast<int>(session->id %
                                 static_cast<std::uint64_t>(config_.worker_threads));
            io_to_worker_[worker_index]->Push(std::move(event));
            if (worker_index >= 0 &&
                worker_index < static_cast<int>(worker_to_disk_.size())) {
              DiskTask record;
              record.op = DiskOp::Append;
              record.path = "recordings/udp_session_" +
                            std::to_string(session->id) + ".bin";
              record.data.assign(buffer, static_cast<std::size_t>(received));
              worker_to_disk_[worker_index]->Push(std::move(record));
            }
          }
        } else if (received < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
          }
          break;
        } else {
          break;
        }
      }
    }
    GenericTask outbound;
    bool has_task = false;
    for (auto& queue : worker_to_io_) {
      if (queue->Pop(outbound)) {
        has_task = true;
        break;
      }
    }
    if (has_task) {
      if (outbound.type == TaskType::Udp) {
        UdpSession* s = session_table.FindById(outbound.session_id);
        if (s) {
          sockaddr_in addr;
          addr.sin_family = AF_INET;
          addr.sin_port = htons(s->remote_port);
          ::inet_pton(AF_INET, s->remote_ip.c_str(), &addr.sin_addr);
          const char* data = outbound.payload.data();
          std::size_t len = outbound.payload.size();
          ::sendto(udp_fd, data, len, 0,
                   reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        }
      }
    }
  }
  ::close(epoll_fd);
  ::close(udp_fd);
  logger->info("udp io thread {} stopped", index);
}

void Runtime::RunWorkerThread(int index) {
  auto logger = GetLogger();
  logger->info("worker thread {} started", index);
  auto& from_io = io_to_worker_[index];
  Event inbound;
  while (running_.load()) {
    if (!from_io->Pop(inbound)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    if (index >= 0 && index < static_cast<int>(lua_vms_.size())) {
      lua_vms_[index]->HandleEvent(inbound);
    }
  }
  logger->info("worker thread {} stopped", index);
}

void Runtime::RunDiskThread(int index) {
  auto logger = GetLogger();
  logger->info("disk thread {} started", index);
  DiskTask inbound;
  while (running_.load()) {
    bool has_task = false;
    for (auto& queue : worker_to_disk_) {
      if (queue->Pop(inbound)) {
        has_task = true;
        if (inbound.op == DiskOp::Read) {
        } else if (inbound.op == DiskOp::Write || inbound.op == DiskOp::Append) {
          try {
            std::size_t slash = inbound.path.find_last_of('/');
            if (slash != std::string::npos) {
              std::string dir = inbound.path.substr(0, slash);
              ::mkdir(dir.c_str(), 0755);
            }
            const char* mode =
                inbound.op == DiskOp::Append ? "ab" : "wb";
            FILE* f = ::fopen(inbound.path.c_str(), mode);
            if (f) {
              (void)::fwrite(inbound.data.data(), 1, inbound.data.size(), f);
              ::fclose(f);
            } else {
              logger->warn("disk thread {} failed to open {}", index,
                           inbound.path);
            }
          } catch (...) {
            logger->error("disk thread {} unexpected error during disk task",
                          index);
          }
        }
        break;
      }
    }
    if (!has_task) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  logger->info("disk thread {} stopped", index);
}

void Runtime::RunLogThread(int index) {
  auto logger = GetLogger();
  logger->info("log thread {} started", index);
  LogTask inbound;
  while (running_.load()) {
    if (!worker_to_log_->Pop(inbound)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    switch (inbound.level) {
      case LogTask::Level::Trace: logger->trace("{}", inbound.message); break;
      case LogTask::Level::Debug: logger->debug("{}", inbound.message); break;
      case LogTask::Level::Info: logger->info("{}", inbound.message); break;
      case LogTask::Level::Warn: logger->warn("{}", inbound.message); break;
      case LogTask::Level::Error: logger->error("{}", inbound.message); break;
      case LogTask::Level::Critical: logger->critical("{}", inbound.message); break;
    }
  }
  logger->info("log thread {} stopped", index);
}

void Runtime::RunTimerThread(int index) {
  auto logger = GetLogger();
  logger->info("timer thread {} started", index);
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::uint64_t now = NowMs();
    for (std::size_t i = 0; i < io_to_worker_.size(); ++i) {
      Event event;
      event.protocol = ProtocolType::Unknown;
      event.session_id = 0;
      event.context.timestamp_ms = now;
      event.context.remote_ip.clear();
      event.context.remote_port = 0;
      event.payload.clear();
      io_to_worker_[i]->Push(std::move(event));
    }
  }
  logger->info("timer thread {} stopped", index);
}

}  // namespace backend
