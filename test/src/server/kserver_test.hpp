#ifndef __KSERVER_TEST_HPP__
#define __KSERVER_TEST_HPP__

#include <gtest/gtest.h>

#include <server/kserver.hpp>

#include "client/client.hpp"

#include <thread>
#include <chrono>

using namespace KYO;

const constexpr char* SERVER_TEST_PORT_ARG  = "--port=9876";
const constexpr char* SERVER_TEST_IP_ARG    = "--ip=0.0.0.0";
const constexpr char* SERVER_TEST_ARG       = "argument_string";

const constexpr char* SERVER_TEST_ARGS[] = {
      SERVER_TEST_IP_ARG,
      SERVER_TEST_PORT_ARG,
      SERVER_TEST_ARG
};

KServer*    g_kserver;
MockClient* g_client;

void runClient() {
  try {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    MockClient client{};
    g_client = &client;
    std::cout << "Client instantiated" << std::endl;
    client.start();
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }
  std::cout << "runClient will exit" << std::endl;
}

void runServer() {
  int argc = 3;
  bool test_mode_enabled = true;
  KServer kserver{argc, std::move(const_cast<char**>(SERVER_TEST_ARGS))};
  g_kserver = &kserver;
  kserver.set_handler(std::move(Request::RequestHandler{}));
  std::cout << "KServer set handler" << std::endl;
  kserver.init(test_mode_enabled);
  std::cout << "KServer initialized" << std::endl;
  kserver.run();
}

/**
 * KServer instantiation test
 */
TEST(KServerTest, InstantiateKServerTest) {
  int argc = 3;
  KServer kserver{argc, std::move(const_cast<char**>(SERVER_TEST_ARGS))};
  EXPECT_NE(nullptr, &kserver);
}

TEST(KServerTest, StartAndStopSession) {

  std::thread server_thread{runServer};
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  std::thread client_thread{runClient};
  std::string received_bytes{};
  std::string op{};
  bool started_session     = false;
  bool got_session_message = false;

  while (g_client == nullptr || g_kserver == nullptr)
    ;

  while (!started_session) {
    received_bytes = g_client->getReceivedMessage();
    started_session = isNewSession(received_bytes.c_str());

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    g_client->startSession();
  }

  while (!got_session_message) {
    got_session_message = isSessionMessageEvent(getEvent(g_client->getReceivedMessage()));
  }

  g_client->stopSession();

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  g_client->close();

  if (client_thread.joinable()) {
    client_thread.join();
  }



  if (server_thread.joinable()) {
    server_thread.join();
  }

  EXPECT_EQ(started_session, true);
  EXPECT_EQ(got_session_message, true);
}


#endif  // __KSERVER_TEST_HPP__
