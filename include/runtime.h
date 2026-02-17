#pragma once

#include "app_config.h"
#include "event.h"
#include "mpsc_queue.h"
#include "tasks.h"
#include "lua_vm.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace backend {

class Runtime {
 public:
  explicit Runtime(const AppConfig& config);
  ~Runtime();

  void Start();
  void Stop();
  void Join();

 private:
  void StartTcpIoThreads();
  void StartUdpIoThreads();
  void StartWorkerThreads();
  void StartDiskThreads();
  void StartLogThreads();
  void StartTimerThreads();

  void RunTcpIoThread(int index);
  void RunUdpIoThread(int index);
  void RunWorkerThread(int index);
  void RunDiskThread(int index);
  void RunLogThread(int index);
  void RunTimerThread(int index);

  AppConfig config_;
  std::atomic<bool> running_;

  std::vector<std::unique_ptr<MpscQueue<Event>>> io_to_worker_;
  std::vector<std::unique_ptr<MpscQueue<GenericTask>>> worker_to_io_;
  std::vector<std::unique_ptr<MpscQueue<DiskTask>>> worker_to_disk_;
  std::unique_ptr<MpscQueue<LogTask>> worker_to_log_;

  std::vector<std::thread> tcp_io_threads_;
  std::vector<std::thread> udp_io_threads_;
  std::vector<std::thread> worker_threads_;
  std::vector<std::thread> disk_threads_;
  std::vector<std::thread> log_threads_;
  std::vector<std::thread> timer_threads_;
  std::vector<std::unique_ptr<LuaVm>> lua_vms_;
};

}  // namespace backend
