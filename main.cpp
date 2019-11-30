#include <iostream>

#include "include/kserver.hpp"

int main(int argc, char** argv) {
  KServer server(argc, argv);
  if (server.init()) {
    server.run();
  }

  return 0;
}
