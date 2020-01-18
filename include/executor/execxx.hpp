#include <sys/wait.h>
#include <unistd.h>
#include <codec/util.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <log/logger.h>

namespace {

auto KLOG = KLogger::GetInstance() -> get_logger();

std::string qx( std::vector<std::string> args,
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

  std::string out;
  const int buf_size = 32768;
  char buffer[buf_size];
  do {
    const ssize_t r = read(stdout_fds[0], buffer, buf_size);
    if (r > 0) {
      out.append(buffer, r);
    }
  } while (errno == EAGAIN || errno == EINTR);

  close(stdout_fds[0]);
  close(stdout_fds[1]);

  close(stderr_fds[1]);
  close(stderr_fds[0]);

  int r, status;
  do {
    r = waitpid(pid, &status, 0);
  } while (r == -1 && errno == EINTR);

  return out;
}

std::string qx_debug(const char* path, std::vector<std::string> args,
               const std::string& working_directory = "") {
  int stdout_fds[2];
  pipe(stdout_fds);

  int stderr_fds[2];
  pipe(stderr_fds);

  // char* raw_args[args.size() + 2];
  // raw_args[0] = const_cast<char*>(path);

  // for (size_t i = 1; i < args.size() + 1; i++) {
  //   raw_args[i] = const_cast<char*>(args[i - 1].c_str());
  // }

  char* raw_args[2];
  raw_args[0] = const_cast<char*>(path);
  // raw_args[1] = const_cast<char*>("test");
  raw_args[1] = NULL;
  // raw_args[args.size() + 1] = NULL;
  std::string execution_string{path};
  execution_string += " ";
  execution_string += args.size() > 0 ? raw_args[1] : "";
  KLOG->info("Executing {}", execution_string);

  const pid_t pid = fork();
  std::cout << pid << std::endl;
  if (!pid) {
    if (!working_directory.empty()) {
      chdir(working_directory.c_str());
    }
    close(stdout_fds[0]);
    dup2(stdout_fds[1], 1);
    // close(stdout_fds[1]);
    close(stderr_fds[0]);
    dup2(stderr_fds[1], 2);
    // close(stderr_fds[1]);
    auto result = execvp(raw_args[0], &raw_args[0]);
    KLOG->info("execvp result: {}", result);
    exit(0);
  }

  // close(stdout_fds[1]);

  std::string out;
  const int buf_size = 32768;
  char buffer[buf_size];

  do {
    const ssize_t r = read(stdout_fds[0], buffer, buf_size);
    if (r > 0) {
      out.append(buffer, r);
    }
  } while (errno == EAGAIN || errno == EINTR);
  // close(stdout_fds[0]);
  // close(stdout_fds[1]);

  // close(stderr_fds[1]);
  // close(stderr_fds[0]);

  int r, status;
  do {
    r = waitpid(pid, &status, 0);
  } while (r == -1 && errno == EINTR);

  return out;
}
}
