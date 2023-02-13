#include <log/logger.h>
#include <request/controller.hpp>
#include <server/kserver.hpp>

static const int32_t ERROR{0x01};

int main(int argc, char** argv)
{
  const auto config = kiq::ParseRuntimeArguments(argc, argv);
  int32_t    code   = 0x00;

  kiq::log::klogger::init(config.loglevel);
  try
  {
    kiq::KServer server(argc, argv);
    server.init();
    server.run();
  }
  catch (const std::runtime_error& e)
  {
    ELOG("Runtime exception was caught: {}", e.what());
    code = ERROR;
  }
  catch (const std::exception& e)
  {
    ELOG("Exception was caught: {}", e.what());
    code = ERROR;
  }
  catch (...)
  {
    ELOG("Caught unknown exception. Errno is currently {}", errno);
    code = ERROR;
  }

  return code;
}
