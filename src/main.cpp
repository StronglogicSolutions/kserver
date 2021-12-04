#include <log/logger.h>
#include <request/controller.hpp>
#include <server/kserver.hpp>

static const int32_t ERROR{0x01};

int main(int argc, char** argv)
{
  kiq::RuntimeConfig config = kiq::ParseRuntimeArguments(argc, argv);
  int32_t            code{};

  kiq::LOG::KLogger::Init(config.loglevel);

  try
  {
    kiq::KServer server(argc, argv);
    server.init();
    server.run();
  }
  catch (const std::exception& e)
  {
    std::cout << "Exception was caught: " << e.what() << std::endl;
    code = ERROR;
  }
  catch (...)
  {
    std::cout << "Caught unknown exception" << std::endl;
    code = ERROR;
  }

  return code;
}
