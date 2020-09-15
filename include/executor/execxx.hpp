#ifndef __EXECXX_HPP__
#define __EXECXX_HPP__

#include <log/logger.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <unistd.h>
#include <ctime>

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
               const std::string& working_directory = "") {
  int stdout_fds[2];
  pipe(stdout_fds);

  int stderr_fds[2];
  pipe(stderr_fds);

  const pid_t pid = fork();
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
  close(stderr_fds[1]);

  ProcessResult result{};

  std::clock_t start_time = std::clock();
  int ret, status;

  for (;;) {
    ret = waitpid(pid, &status, (WNOHANG | WUNTRACED | WCONTINUED));
    KLOG("waitpid returned {}", ret);
    if (ret == 0) {
      break;
    }
    if ((std::clock() - start_time) > 30) {
      kill(pid, SIGKILL);
      result.error = true;
      result.output = "Child process timed out";

      return result;
    }
  }

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
    std::cout << "result was " << poll_result << std::endl;
    // stdout
    if        (poll_fds[0].revents & POLLIN) {
      result.output = readFd(poll_fds[0].fd);
      if (!result.output.empty()) {
        break;
      }
      result.error = true;
      poll_fds[0].revents = 0;
    } else if (poll_fds[0].revents & POLLHUP) {
      close(stdout_fds[0]);
      close(stderr_fds[0]);
      printf("POLLHUP\n");
      result.output = "Lost connection to forked process";
      result.error = true;
      break;
    } else if (poll_fds[1].revents & POLL_IN) {
      std::string stderr_output = readFd(poll_fds[0].fd);
      result.output = stderr_output;
      result.error = true;
      break;
    } else {
      kill(pid, SIGKILL);
      result.error = true;
      result.output = "Child process timed out";
    }
  }
  // std::string stdout_string = readFd(stdout_fds[0]);

  // if (stdout_string.size() <= 1) { // an empty stdout might be a single whitespace
  //   // TODO: Find out what's missing from the output (thrown exception messages aren't included)
  //   std::string stderr_string = readFd(stderr_fds[0]);
  //   result.output = stderr_string;
  //   result.error = true;
  // } else {
  //   result.output = stdout_string;
  // }

  close(stdout_fds[0]);
  close(stderr_fds[0]);
  close(stderr_fds[1]);

  return result;
}
}  // namespace
#endif // __EXECXX_HPP__
