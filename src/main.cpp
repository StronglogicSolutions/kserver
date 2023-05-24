#include <logger.hpp>
#include <request/controller.hpp>
#include <server/kserver.hpp>

static const int32_t ERROR{0x01};

using namespace kiq::log;

int main(int argc, char** argv)
{
  const auto config = kiq::ParseRuntimeArguments(argc, argv);
  int32_t    code   = 0x00;

  klogger::init("kserver", config.loglevel);
  try
  {
    kiq::KServer server(argc, argv);
    server.init();
    server.run();
  }
  catch (const std::runtime_error& e)
  {
    klog().e("Runtime exception was caught: {}", e.what());
    code = ERROR;
  }
  catch (const std::exception& e)
  {
    klog().e("Exception was caught: {}", e.what());
    code = ERROR;
  }
  catch (...)
  {
    klog().e("Caught unknown exception. Errno is currently {}", errno);
    code = ERROR;
  }

  return code;
}
