/*
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
*/

#include "qczmq.h"

///
//  Copy-construct to return the proper wrapped c types
QZproc::QZproc (zproc_t *self, QObject *qObjParent) : QObject (qObjParent)
{
    this->self = self;
}


///
//  Create a new zproc.
//  NOTE: On Windows and with libzmq3 and libzmq2 this function
//  returns NULL. Code needs to be ported there.
QZproc::QZproc (QObject *qObjParent) : QObject (qObjParent)
{
    this->self = zproc_new ();
}

///
//  Destroy zproc, wait until process ends.
QZproc::~QZproc ()
{
    zproc_destroy (&self);
}

///
//  Return command line arguments (the first item is the executable) or
//  NULL if not set.
QZlist * QZproc::args ()
{
    QZlist *rv = new QZlist (zproc_args (self));
    return rv;
}

///
//  Setup the command line arguments, the first item must be an (absolute) filename
//  to run.
void QZproc::setArgs (QZlist *arguments)
{
    zproc_set_args (self, &arguments->self);

}

///
//  Setup the environment variables for the process.
void QZproc::setEnv (QZhash *arguments)
{
    zproc_set_env (self, &arguments->self);

}

///
//  Connects process stdin with a readable ('>', connect) zeromq socket. If
//  socket argument is NULL, zproc creates own managed pair of inproc
//  sockets.  The writable one is then accessbile via zproc_stdin method.
void QZproc::setStdin (void *socket)
{
    zproc_set_stdin (self, socket);

}

///
//  Connects process stdout with a writable ('@', bind) zeromq socket. If
//  socket argument is NULL, zproc creates own managed pair of inproc
//  sockets.  The readable one is then accessbile via zproc_stdout method.
void QZproc::setStdout (void *socket)
{
    zproc_set_stdout (self, socket);

}

///
//  Connects process stderr with a writable ('@', bind) zeromq socket. If
//  socket argument is NULL, zproc creates own managed pair of inproc
//  sockets.  The readable one is then accessbile via zproc_stderr method.
void QZproc::setStderr (void *socket)
{
    zproc_set_stderr (self, socket);

}

///
//  Return subprocess stdin writable socket. NULL for
//  not initialized or external sockets.
void * QZproc::stdin ()
{
    void * rv = zproc_stdin (self);
    return rv;
}

///
//  Return subprocess stdout readable socket. NULL for
//  not initialized or external sockets.
void * QZproc::stdout ()
{
    void * rv = zproc_stdout (self);
    return rv;
}

///
//  Return subprocess stderr readable socket. NULL for
//  not initialized or external sockets.
void * QZproc::stderr ()
{
    void * rv = zproc_stderr (self);
    return rv;
}

///
//  Starts the process, return just before execve/CreateProcess.
int QZproc::run ()
{
    int rv = zproc_run (self);
    return rv;
}

///
//  process exit code
int QZproc::returncode ()
{
    int rv = zproc_returncode (self);
    return rv;
}

///
//  PID of the process
int QZproc::pid ()
{
    int rv = zproc_pid (self);
    return rv;
}

///
//  return true if process is running, false if not yet started or finished
bool QZproc::running ()
{
    bool rv = zproc_running (self);
    return rv;
}

///
//  The timeout should be zero or greater, or -1 to wait indefinitely.
//  wait or poll process status, return return code
int QZproc::wait (int timeout)
{
    int rv = zproc_wait (self, timeout);
    return rv;
}

///
//  send SIGTERM signal to the subprocess, wait for grace period and
//  eventually send SIGKILL
void QZproc::shutdown (int timeout)
{
    zproc_shutdown (self, timeout);

}

///
//  return internal actor, useful for the polling if process died
void * QZproc::actor ()
{
    void * rv = zproc_actor (self);
    return rv;
}

///
//  send a signal to the subprocess
void QZproc::kill (int signal)
{
    zproc_kill (self, signal);

}

///
//  set verbose mode
void QZproc::setVerbose (bool verbose)
{
    zproc_set_verbose (self, verbose);

}

///
//  Self test of this class.
void QZproc::test (bool verbose)
{
    zproc_test (verbose);

}
/*
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
*/
