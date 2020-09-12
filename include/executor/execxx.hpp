#ifndef __EXECXX_HPP__
#define __EXECXX_HPP__

#include <log/logger.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <spawn.h>

#include <codec/util.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <future>

namespace {
extern "C" char ** environ;
char ** getenviron(void)
{
    return environ;
}
struct ProcessResult {
  std::string output;
  bool error = false;
};

constexpr int buf_size = 32768;

std::string readFd(int fd) {
  char buffer[32768];
  std::string s{};
  do {
    const ssize_t r = read(fd, buffer, buf_size);
    if (r > 0) {
      s.append(buffer, r);
    }
  } while (errno == EAGAIN || errno == EINTR);
  return s;
}

ProcessResult qx(std::vector<std::string> args,
               const std::string& working_directory = "") {
  pid_t pid = 0;
  std::vector<char*> process_arguments{};
  process_arguments.reserve(args.size());

  for (size_t i = 0; i < args.size(); ++i) {
    process_arguments[i] = const_cast<char*>(args[i].c_str());
  }

  posix_spawn_file_actions_t action;
  posix_spawn_file_actions_init(&action);
  posix_spawn_file_actions_addopen(&action, STDOUT_FILENO, "/data/c/kserver/posix_spawn.log", O_RDWR, 0);
  posix_spawn_file_actions_addopen(&action, STDERR_FILENO, "/data/c/kserver/posix_spawn_err.log", O_RDWR, 0);

  // posix_spawn_file_actions_addclose(&action, stdout_fd[0]);
  // posix_spawn_file_actions_addclose(&action, stderr_fd[0]);
  // posix_spawn_file_actions_adddup2(&action, stdout_fd[1], STDOUT_FILENO);
  // posix_spawn_file_actions_adddup2(&action, stderr_fd[1], STDERR_FILENO);
  // posix_spawn_file_actions_addclose(&action, stdout_fd[1]);
  // posix_spawn_file_actions_addclose(&action, stdout_fd[1]);

  ProcessResult result{};                         // To gather result

  int spawn_result = posix_spawn(&pid, process_arguments[0], &action, nullptr, &process_arguments[0], getenviron());

  if (spawn_result != 0) {
    result.error = true;
    result.output = "Failed to spawn child process";
    return result;
  }

  pid_t ret;
  int status;

  for(;;) {
    ret = waitpid(pid, &status, (WNOHANG | WUNTRACED | WCONTINUED));

    if (ret != -1)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status) || WIFSTOPPED(status))
        {
          break;
        }
    } else {
      std::cout << "waitpid failed" << std::endl;
    }
  }

  KLOG("Child process exited with code: {}", status);

  if (status != 0) {
    result.output = FileUtils::readFile("/data/c/kserver/posix_spawn_err.log");
    result.error = true;
  } else {
    result.output = FileUtils::readFile("/data/c/kserver/posix_spawn.log");
  }
  if (result.output.empty()) {
    result.output = "Child process did not return output";
  }

  return result;
}
}  // namespace
#endif // __EXECXX_HPP__
