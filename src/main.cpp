#include <log/logger.h>
#include <request/controller.hpp>
#include <server/kserver.hpp>

using namespace KYO;

int main(int argc, char** argv)
{
  try
  {
    LOG::KLogger::init();

    KServer server(argc, argv);

    server.set_handler(std::move(Request::Controller{}));
    server.init();
    server.run();
  }
  catch (const std::exception& e)
  {
    std::cout << "Exception was caught: " << e.what() << std::endl;
  }

  return 0;
}
