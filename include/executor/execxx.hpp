#ifndef __EXECXX_HPP__
#define __EXECXX_HPP__

#include <log/logger.h>
#include <sys/wait.h>
#include <sys/poll.h>
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
                 const std::string&       working_directory = "") {
  int stdout_fds[2], stderr_fds[2];
  pipe(stdout_fds);
  pipe(stderr_fds);

  sigset_t sig_set{};

  const pid_t pid = fork();

  if (!pid) { // Child process

    if (!working_directory.empty()) {
      chdir(working_directory.c_str());
    }

    close(stdout_fds[0]);
    dup2 (stdout_fds[1], 1);
    close(stdout_fds[1]);
    close(stderr_fds[0]);
    dup2 (stderr_fds[1], 2);
    close(stderr_fds[1]);

    std::vector<char*> process_arguments{};
    process_arguments.reserve(args.size() + 1);

    for (size_t i = 0; i < args.size(); ++i) {
      process_arguments[i] = const_cast<char*>(args[i].c_str());
    }
    // Execute
    execvp(*process_arguments.data(), process_arguments.data());
    exit(0); // Exit with no error
  }
  close(stdout_fds[1]);
  close(stderr_fds[1]);

  ProcessResult result{};                         // To gather result

  pollfd poll_fds[2]{
    pollfd{
      .fd       =   stdout_fds[0] & 0xFF,
      .events   =   POLL_OUT | POLL_ERR | POLL_IN,
      .revents  =   short{0}
    },
    pollfd{
      .fd       =   stderr_fds[0] & 0xFF,
      .events   =   POLLHUP | POLLERR | POLLIN,
      .revents  =   short{0}
    }
  };

  for (;;) {

    int poll_result = poll(poll_fds, 2, 30000);
    // stdout
    if (poll_fds[0].revents & POLLIN) {
      result.output = readFd(poll_fds[0].fd);
      if (!result.output.empty()) {
        break;
      }
      result.error = true;
      poll_fds[0].revents = 0;
    }

    if (poll_fds[0].revents & POLLHUP) {
      close(stdout_fds[0]);
      close(stderr_fds[0]);
      printf("POLLHUP\n");
      result.output = "Lost connection to forked process";
      result.error = true;
      break;
    }
    // stderr
    if (poll_fds[1].revents & POLL_IN) {
      std::string stderr_output = readFd(poll_fds[0].fd);
      result.output = stderr_output;
      result.error = true;
      break;
    }
  }

  close(stdout_fds[0]);
  close(stderr_fds[0]);

  std::cout << "Result output: " << result.output << std::endl;

  return result;
}
}  // namespace
#endif // __EXECXX_HPP__
