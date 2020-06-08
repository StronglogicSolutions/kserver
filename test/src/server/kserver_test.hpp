#ifndef __KSERVER_TEST_HPP__
#define __KSERVER_TEST_HPP__

#include <gtest/gtest.h>

#include <server/kserver.hpp>

using namespace KYO;

/**
 * KServer instantiation test
 */
TEST(KServer, InstantiateKServerTest) {
  const char* argv[3] = {"127.0.0.1", "9876", "argument_string"};
  int argc = 3;
  KServer kserver{argc, std::move(const_cast<char**>(argv))};
  EXPECT_NE(nullptr, &kserver);
}

#endif  // __KSERVER_TEST_HPP__
