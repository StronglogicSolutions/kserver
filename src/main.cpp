#include <log/logger.h>
#include <request/controller.hpp>
#include <server/kserver.hpp>

int main(int argc, char** argv)
{
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
    return 1;
  }

  return 0;
}
