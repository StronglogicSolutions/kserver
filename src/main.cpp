#include <log/logger.h>
#include <request/controller.hpp>
#include <server/kserver.hpp>

static const int32_t ERROR{0x01};

int main(int argc, char** argv)
{
  int32_t code{};
  try
  {
    kiq::LOG::KLogger::init();

    kiq::KServer server(argc, argv);

    server.set_handler(std::move(kiq::Request::Controller{}));
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
