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
  int stdout_fds[2], stderr_fds[2];
  pipe(stdout_fds);
  pipe(stderr_fds);

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
    execvp(process_arguments[0], &process_arguments[0]);
    exit(0); // Exit with no error
  }
  close(stdout_fds[1]);

  ProcessResult result{};                         // To gather result

  fd_set read_file_descriptors{};                 // Mask for file descriptors

  timeval time_value{                             // 30 second timeout
    .tv_sec = 30,
    .tv_usec = 0
  };

  int retval;
  int max_fd_range = stdout_fds[0] > stderr_fds[0] ?
    (stdout_fds[0]) + 1 :
    (stderr_fds[0]) + 1;

  FD_ZERO(&read_file_descriptors);                 // clear
  FD_SET(stdout_fds[0], &read_file_descriptors);   // add stdout to mask
  FD_SET(stderr_fds[0], &read_file_descriptors);   // add stderr to mask

  for (;;) {
    // select() call determines if there are file descriptors to read from
    int num_of_fd_outputs = select(
      max_fd_range,                                // max range of file descriptor to listen for
      &read_file_descriptors,                      // read fd mask
      NULL,                                        // write fd mask
      NULL,                                        // error fd mask
      &time_value
    );

    if (num_of_fd_outputs > 0) {                   // output can be read
      if (FD_ISSET(stdout_fds[0], &read_file_descriptors)) {
        result.output = readFd(stdout_fds[0]);
        if (result.output.empty()) {               // stdout had empty output
          result.output = "Unknown error. Nothing returned from process.";
          result.error = true;
        }
      }
      if (FD_ISSET(stderr_fds[0], &read_file_descriptors)) { // stderr has output
        printf("ProcessExecutor's forked process returned an error");
        result.output = readFd(stderr_fds[0]);
        result.error = true;
      }
      break;
    } else {
      printf("ProcessExecutor's forked process timed out");
      result.output = "Timeout";
      result.error = true;
      break;
    }
  }

  close(stdout_fds[0]);
  close(stderr_fds[0]);
  close(stderr_fds[1]);

  return result;
}
}  // namespace
#endif // __EXECXX_HPP__
