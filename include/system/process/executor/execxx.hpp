#pragma once

#include <log/logger.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <future>

namespace kiq
{
struct ProcessResult {
  std::string output;
  bool error = false;
};

static const uint32_t buf_size{32768};
//----------------------------------------------------------------------
inline std::string read_fd(int fd)
{
  std::string s;
  ssize_t     r;
  char        buffer[buf_size];

  do
  {
    r = read(fd, buffer, buf_size);
    if (r > 0)
      s.append(buffer, r);
  }
  while (r > 0);

  return s;
}
//----------------------------------------------------------------------
[[ maybe_unused ]]
inline ProcessResult qx(      std::vector<std::string> args,
                        const std::string&             working_directory = "")
{
#ifndef NDEBUG
  std::string execution_command;
  for (const auto& arg : args) execution_command += arg + ' ';
  VLOG("ProcessExecutor running the following command:\n {}", execution_command);
#endif

  int stdout_fds[2];
  int stderr_fds[2];
  pipe(stdout_fds);
  pipe(stderr_fds);

  const pid_t pid = fork();

  if (!pid)                                       // Child process
  {
    if (!working_directory.empty())
      chdir(working_directory.c_str());

    close(stdout_fds[0]);
    dup2 (stdout_fds[1], 1);
    close(stdout_fds[1]);
    close(stderr_fds[0]);
    dup2 (stderr_fds[1], 2);
    close(stderr_fds[1]);

    std::vector<char*> vc(args.size() + 1, 0);

    for (size_t i = 0; i < args.size(); ++i)
      vc[i] = const_cast<char*>(args[i].c_str());

    if (execvp(vc[0], &vc[0]) == -1)
      exit(errno);
    exit(0);
  }
                                                  // Parent process
  close(stdout_fds[1]);
  close(stderr_fds[1]);

  ProcessResult result;

  std::clock_t start_time = std::clock();
  int          ret{};
  int          status{};

  for (;;)
  {
    ret = waitpid(pid, &status, (WNOHANG | WUNTRACED | WCONTINUED));
    KLOG("waitpid returned {}", ret);

    if (!ret)
      break;

    if ((std::clock() - start_time) > 30)
    {
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
      .revents  =   short{0}},
    pollfd{
      .fd       =   stderr_fds[0] & 0xFF,
      .events   =   POLLHUP | POLLERR | POLLIN,
      .revents  =   short{0}}};

  for (;;)
  {

    if (!poll(poll_fds, 2, 30000))
      ELOG("Failed to poll file descriptor");

    if (poll_fds[1].revents & POLLIN)
    {
      result.output = read_fd(poll_fds[1].fd);;
      result.error  = true;
      break;

    }
    else
    if (poll_fds[0].revents & POLLIN)
    {
      result.output = read_fd(poll_fds[0].fd);

      if (!result.output.empty())
        break;

      result.error = true;

    }
    else
    if (poll_fds[0].revents & POLLHUP)
    {
      close(stdout_fds[0]);
      close(stderr_fds[0]);
      result.error  = true;
      result.output = "Lost connection to forked process";
    }
    else
    {
      kill(pid, SIGKILL);  // Make sure the process is dead
      result.error  = true;
      result.output = "Child process timed out";
    }

    if (result.error)
      break;
  }

  close(stdout_fds[0]);
  close(stderr_fds[0]);
  close(stderr_fds[1]);

  if (result.output.empty())
    result.output = "Process returned no output";

  return result;
}
} // ns kiq
