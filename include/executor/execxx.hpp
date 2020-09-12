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

const char* CHILD_STDOUT{"/data/c/kserver/posix_spawn.log"};
const char* CHILD_STDERR{"/data/c/kserver/posix_spawn_err.log"};

namespace {
extern "C" char ** environ;
char ** getEnvironment() {
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
    std::cout << "Process arg: " << args[i];
    process_arguments[i] = const_cast<char*>(args[i].c_str());
  }

  posix_spawn_file_actions_t action{};
  posix_spawn_file_actions_init(&action);
  posix_spawn_file_actions_addopen(&action, STDOUT_FILENO, CHILD_STDOUT, O_RDWR, 0);
  posix_spawn_file_actions_addopen(&action, STDERR_FILENO, CHILD_STDERR, O_RDWR, 0);

  ProcessResult result{};                         // To gather result

  int spawn_result = posix_spawn(&pid, process_arguments[0], &action, nullptr, process_arguments.data(), getEnvironment());

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


  result.output = FileUtils::readFile(CHILD_STDERR);
  if (result.output.empty()) {
    result.output = FileUtils::readFile(CHILD_STDOUT);
  } else {
    result.error = true;
  }

  if (result.output.empty()) {
    result.output = "Child process did not return output";
  }

  FileUtils::clearFile(CHILD_STDOUT);
  FileUtils::clearFile(CHILD_STDERR);

  return result;
}
}  // namespace
#endif // __EXECXX_HPP__
