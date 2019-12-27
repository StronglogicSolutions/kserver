#include <iostream>
#include <request/request_handler.hpp>
#include <server/kserver.hpp>

using namespace KYO;

int main(int argc, char** argv) {
  KServer server(argc, argv);
  server.set_handler(Request::RequestHandler{});
  if (server.init()) {
    server.run();
  }

  return 0;
}
