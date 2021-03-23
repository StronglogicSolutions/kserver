#include <iostream>
#include <log/logger.h>
#include <request/request_handler.hpp>
#include <server/kserver.hpp>

using namespace KYO;

int main(int argc, char** argv) {
  try {
    // Initialize logger
    LOG::KLogger::init();
    // Instantiate server
    KServer server(argc, argv);
    // Set request handler
    server.set_handler(std::move(Request::Controller{}));
    // Initialize task queue
    server.init();
    // Run service loop
    server.run();
  } catch (const std::exception& e) {
    std::cout << "Exception was caught: " << e.what() << std::endl;
  }

  return 0;
}
