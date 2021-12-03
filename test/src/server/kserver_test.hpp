#pragma once

#include <gtest/gtest.h>

#include <server/kserver.hpp>

#include "client/client.hpp"

#include <thread>
#include <chrono>

using namespace kiq;

const constexpr char* SERVER_TEST_PORT_ARG  = "--port=9876";
const constexpr char* SERVER_TEST_IP_ARG    = "--ip=0.0.0.0";
const constexpr char* SERVER_TEST_ARG       = "argument_string";

const char* SERVER_TEST_ARGS[] = {
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
  kserver.set_handler(std::move(Request::Controller{}));
  std::cout << "KServer set handler" << std::endl;
  kserver.init(test_mode_enabled);
  std::cout << "KServer initialized" << std::endl;
  kserver.run();
}

/**
 * KServer instantiation test
 */
// TEST(KServerTest, InstantiateKServerTest) {
//   int argc    = 3;
//   char** argv = const_cast<char**>(SERVER_TEST_ARGS);
//   // try {
//   //   {
//   //     KServer kserver{argc, std::move(const_cast<char**>(SERVER_TEST_ARGS))};
//   //     EXPECT_NE(nullptr, &kserver);
//   //   }
//   // } catch (const std::exception& e) {
//   //   std::cout << e.what() << std::endl;
//   // }
//   KServer{argc, argv};

//   usleep(300000);
// }

// TEST(KServerTest, StartAndStopSession) {
//   bool started_session     = false;
//   try {
//     std::thread server_thread{runServer};
//     // std::this_thread::sleep_for(std::chrono::milliseconds(300));
//     std::thread client_thread{runClient};
//     std::string received_bytes{};
//     std::string op{};
//     // bool got_session_message = false;

//     while (g_client == nullptr || g_kserver == nullptr)
//       ;

//     // std::this_thread::sleep_for(std::chrono::milliseconds(300));

//     g_client->startSession();

//     uint8_t attempts{0};

//     while (!started_session) {
//       attempts++;
//       // std::this_thread::sleep_for(std::chrono::milliseconds(300));
//       received_bytes = g_client->getReceivedMessage();
//       started_session = IsNewSession(received_bytes.c_str());

//       if (attempts % 10 == 0) {
//         g_client->startSession();
//       }
//     }

//     attempts = 0;

//     std::this_thread::sleep_for(std::chrono::milliseconds(300));

//     g_client->stopSession();

//     std::this_thread::sleep_for(std::chrono::milliseconds(300));

//     g_client->close();

//     while (g_kserver->getNumConnections() != 0) {
//       ;
//     }

//     if (client_thread.joinable()) {
//       client_thread.join();
//     }

//     std::cout << "Client thread joined" << std::endl;

//     if (server_thread.joinable()) {
//       server_thread.join();
//     }


//   } catch (const std::exception& e) {
//     std::cout << "Exception was caught: " << e.what() << std::endl;
//   }

//   EXPECT_EQ(started_session, true);
// }

// TEST(KServerTest, MessageStressTest)
// {
//   std::thread server_thread{runServer};
//   std::this_thread::sleep_for(std::chrono::milliseconds(300));
//   std::thread client_thread{runClient};

//   bool started_session     = false;
//   bool got_session_message = false;

//   while (g_client == nullptr || g_kserver == nullptr)
//     ;

//   std::this_thread::sleep_for(std::chrono::milliseconds(300));

//   while (!started_session) {
//     g_client->startSession();
//     std::this_thread::sleep_for(std::chrono::milliseconds(300));
//     started_session = IsNewSession(g_client->getReceivedMessage().c_str());
//   }

//   for (uint8_t i = 0; i < 100; i++) {
//     g_client->sendCustomMessage("Ehyo!");
//     std::this_thread::sleep_for(std::chrono::milliseconds(10));
//   }

//   g_client->stopSession();

//   std::this_thread::sleep_for(std::chrono::milliseconds(300));

//   g_client->close();

//   if (client_thread.joinable()) {
//     client_thread.join();
//   }

//   if (server_thread.joinable()) {
//     server_thread.join();
//   }

//   EXPECT_EQ(started_session, true);
// }
