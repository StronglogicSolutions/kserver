#ifndef __UTIL_TEST_HPP__
#define __UTIL_TEST_HPP__

#include <gtest/gtest.h>
#include <codec/util.hpp>

auto util_cwd = get_executable_cwd();
auto cwd = std::string{util_cwd.begin(), util_cwd.end() - 4};

/****************************************************
 *************** UTIL TEST SUITE *************************
 ****************************************************/

/**
 * JSON Tools
 * createMessage
 */

TEST(KUtilities, createMessageTest) {
  const char* message = "This is a message";
  std::string arg_string{"Additional argument"};
  EXPECT_EQ("{\"type\":\"custom\",\"message\":\"This is a message\",\"args\":\"Additional argument\"}", createMessage(message, arg_string));
}

/**
 * FileUtils::readEnvFile
 */
TEST(KUtilities, readEnvFileTest) {
  auto env_file_path = cwd + "/mock_data/mock_v.env";
  EXPECT_EQ("#!/usr/bin/env bash\nKEY='value'", FileUtils::readEnvFile(env_file_path));
}

#endif // __UTIL_TEST_HPP__
