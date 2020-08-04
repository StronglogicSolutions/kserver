#include <iostream>
#include <log/logger.h>
#include <request/request_handler.hpp>
#include <server/kserver.hpp>

using namespace KYO;

int main(int argc, char** argv) {
  LOG::KLogger::init();
  KServer server(argc, argv);
  server.set_handler(std::move(Request::RequestHandler{}));
  if (server.init()) {
    server.run();
  }

  return 0;
}
