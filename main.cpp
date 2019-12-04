#include <iostream>

#include "include/kserver.hpp"
#include "include/request_handler.hpp"

int main(int argc, char** argv) {
  KServer server(argc, argv);
  server.set_handler(RequestHandler{});
  if (server.init()) {
    server.run();
  }

  return 0;
}
