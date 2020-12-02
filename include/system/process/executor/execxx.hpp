#ifndef __EXECXX_HPP__
#define __EXECXX_HPP__

#include <log/logger.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <future>

namespace {
/**
 * ProcessResult
 */
struct ProcessResult {
  std::string output;
  bool error = false;
};

/**
 * Child process output buffer
 * 32k
 */
const uint32_t buf_size{32768};

/**
 * readFd
 *
 * Helper function to read output from a file descriptor
 *
 * @param   [in]
 * @returns [out]
 *
 */
std::string readFd(int fd) {
  char buffer[buf_size];
  std::string s{};
  ssize_t r{};
  do {
    r = read(fd, buffer, buf_size);
    if (r > 0) {
      s.append(buffer, r);
    }
  } while (r > 0);
  return s;
}

/**
 * qx
 *
 * Function to fork a child process, execute an application and return the stdout or stderr
 *
 * @param   [in]
 * @param   [in]
 * @returns [out]
 */
ProcessResult qx(    std::vector<std::string> args,
               const std::string&             working_directory = "") {
  int stdout_fds[2];
  int stderr_fds[2];

  pipe(stdout_fds);
  pipe(stderr_fds);

  const pid_t pid = fork();

  if (!pid) {                                     // Child process

    if (!working_directory.empty()) {
      chdir(working_directory.c_str());
    }

    close(stdout_fds[0]);
    dup2 (stdout_fds[1], 1);
    close(stdout_fds[1]);
    close(stderr_fds[0]);
    dup2 (stderr_fds[1], 2);
    close(stderr_fds[1]);

    std::vector<char*> vc(args.size() + 1, 0);

    for (size_t i = 0; i < args.size(); ++i) {
      vc[i] = const_cast<char*>(args[i].c_str());
    }

    execvp(vc[0], &vc[0]); // Execute application
    exit(0);               // Exit with no error
  }
                                                  // Parent process
  close(stdout_fds[1]);
  close(stderr_fds[1]);

  ProcessResult result{};

  std::clock_t start_time = std::clock();
  int          ret{};
  int          status{};

  for (;;) {

    ret = waitpid(pid, &status, (WNOHANG | WUNTRACED | WCONTINUED));

    KLOG("waitpid returned {}", ret);
    if (ret == 0) {
      break;
    }

    if ((std::clock() - start_time) > 30) {

      kill(pid, SIGKILL);
      result.error  = true;
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
    // TODO: Do something with result or remove
    int poll_result = poll(poll_fds, 2, 30000);

    if        (poll_fds[1].revents & POLLIN) {

      result.output = readFd(poll_fds[1].fd);;
      result.error  = true;
      break;

    } else if (poll_fds[0].revents & POLLIN) {

      result.output = readFd(poll_fds[0].fd);
      if (!result.output.empty()) {
        break;
      }
      result.error = true;

    } else if (poll_fds[0].revents & POLLHUP) {

      close(stdout_fds[0]);
      close(stderr_fds[0]);

      result.error  = true;
      result.output = "Lost connection to forked process";

    } else {

      kill(pid, SIGKILL);  // Make sure the process is dead
      result.error  = true;
      result.output = "Child process timed out";

    }

    if (result.error) {
      break;
    }
  }

  close(stdout_fds[0]);
  close(stderr_fds[0]);
  close(stderr_fds[1]);

  if (result.output.empty()) {
    result.output = "Process returned no output";
  }

  return result;
}
}  // namespace

#endif // __EXECXX_HPP__
