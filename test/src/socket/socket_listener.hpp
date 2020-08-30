#ifndef __SOCKET_LISTENER_TEST_HPP__
#define __SOCKET_LISTENER_TEST_HPP__

#include <gtest/gtest.h>

#include <interface/socket_listener.hpp>
#include "../server/client/client.hpp"

#include <thread>
#include <chrono>

const constexpr char* SERVER_TEST_PORT_ARG  = "--port=9876";
const constexpr char* SERVER_TEST_IP_ARG    = "--ip=127.0.0.1";
const constexpr char* SERVER_TEST_ARG       = "argument_string";
const constexpr char* SERVER_TEST_ARGS[]    = {
      SERVER_TEST_IP_ARG,
      SERVER_TEST_PORT_ARG,
      SERVER_TEST_ARG
};
const           int   SERVER_TEST_ARGC      = 3;

SocketListener*    g_listener;
MockClient*        g_client;

void runClientLoop() {
  try {
    auto raw_mode = true;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    MockClient client{raw_mode};
    g_client = &client;
    std::cout << "Client instantiated" << std::endl;
    client.start();
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }
  std::cout << "runClient will exit" << std::endl;
}

void runListener() {
  auto test_mode = true;
  SocketListener socket_listener{SERVER_TEST_ARGC, const_cast<char**>(SERVER_TEST_ARGS)};
  g_listener = &socket_listener;
  socket_listener.init(test_mode);
  socket_listener.run();
}

TEST(KServerTest, StartAndStopSession) {

  std::thread server_thread{runListener};
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  std::thread client_thread{runClientLoop};

  while (g_client == nullptr || g_listener == nullptr)
    ;

  for (uint8_t i = 0; i < 100; i++) { // Stress test
    g_client->sendMessage();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  g_client->close();

  if (client_thread.joinable()) {
    client_thread.join();
  }

  if (server_thread.joinable()) {
    server_thread.join();
  }

  EXPECT_EQ(true, true);
}


#endif  // __SOCKET_LISTENER_TEST_HPP__
