#include <gtest/gtest.h>
#
#include <sys/stat.h>
#include <cstdio>
#include <string>
#include <vector>
#
// 简易录制文件校验：写入 -> 读取 -> 比较
TEST(RtpRecording, WriteAndReadBack) {
  std::string dir = "rtp";
  ::mkdir(dir.c_str(), 0755);
  std::string path = dir + "/session_999.bin";
  std::string payload = std::string("RTPDATA") + std::string(1024, '\x01');
  {
    FILE* f = ::fopen(path.c_str(), "wb");
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(::fwrite(payload.data(), 1, payload.size(), f), payload.size());
    ::fclose(f);
  }
  std::vector<char> buf;
  {
    FILE* f = ::fopen(path.c_str(), "rb");
    ASSERT_NE(f, nullptr);
    ::fseek(f, 0, SEEK_END);
    long size = ::ftell(f);
    ::fseek(f, 0, SEEK_SET);
    ASSERT_GT(size, 0);
    buf.resize(static_cast<std::size_t>(size));
    ASSERT_EQ(::fread(buf.data(), 1, buf.size(), f), buf.size());
    ::fclose(f);
  }
  std::string readback(buf.begin(), buf.end());
  ASSERT_EQ(readback, payload);
}
