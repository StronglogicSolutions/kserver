#ifndef __EXECXX_HPP__
#define __EXECXX_HPP__

#include <log/logger.h>
#include <sys/wait.h>
#include <unistd.h>

#include <codec/util.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <future>

namespace {
struct ProcessResult {
  std::string output;
  bool error = false;
};

auto KLOG = KLogger::GetInstance() -> get_logger();

constexpr int buf_size = 32768;

void readFd(int fd, std::string& s) {
  char buffer[32768];
  do {
    const ssize_t r = read(fd, buffer, buf_size);
    if (r > 0) {
      s.append(buffer, r);
    }
  } while (errno == EAGAIN || errno == EINTR);
}

ProcessResult qx(std::vector<std::string> args,
               const std::string& working_directory = "") {
  int stdout_fds[2];
  pipe(stdout_fds);

  int stderr_fds[2];
  pipe(stderr_fds);

  const pid_t pid = fork();
  std::cout << pid << std::endl;
  if (!pid) {
    if (!working_directory.empty()) {
      chdir(working_directory.c_str());
    }
    close(stdout_fds[0]);
    dup2(stdout_fds[1], 1);
    close(stdout_fds[1]);
    close(stderr_fds[0]);
    dup2(stderr_fds[1], 2);
    close(stderr_fds[1]);

    std::vector<char*> vc(args.size() + 1, 0);
    for (size_t i = 0; i < args.size(); ++i) {
      vc[i] = const_cast<char*>(args[i].c_str());
    }

    execvp(vc[0], &vc[0]);
    exit(0);
  }

  close(stdout_fds[1]);

  ProcessResult result{};
  std::future<void> stdout_future = std::async(&readFd, stdout_fds[0], std::ref(result.output));
  std::future<void> stderr_future = std::async(&readFd, stderr_fds[0], std::ref(result.output));

  bool finished_reading_output = false;

  while (!finished_reading_output) {
    if (stdout_future.valid()) {
      stdout_future.get();
      finished_reading_output = true;
    } else if (stderr_future.valid()) {
      stderr_future.get();
      result.error = true;
      finished_reading_output = true;
    }
  }

  close(stdout_fds[0]);
  close(stdout_fds[1]);

  close(stderr_fds[0]); // changed order
  close(stderr_fds[1]);

  int r, status;
  do {
    r = waitpid(pid, &status, 0);
  } while (r == -1 && errno == EINTR);

  return result;
}
}  // namespace
#endif // __EXECXX_HPP__
