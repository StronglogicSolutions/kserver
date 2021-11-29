#ifndef __UTIL_TEST_HPP__
#define __UTIL_TEST_HPP__

#include <gtest/gtest.h>
#include <common/util.hpp>

auto util_cwd = kiq::GetExecutableCWD();
auto cwd = std::string{util_cwd.begin(), util_cwd.end() - 4};

/****************************************************
 *************** UTIL TEST SUITE *************************
 ****************************************************/

/**
 * JSON Tools
 * CreateMessage
 */

TEST(KUtilities, createMessageTest) {
  const char* message = "This is a message";
  std::string arg_string{"Additional argument"};
  EXPECT_EQ("{\"type\":\"custom\",\"message\":\"This is a message\",\"args\":\"Additional argument\"}", kiq::CreateMessage(message, arg_string));
}

/**
 * FileUtils::ReadEnvFile
 */
TEST(KUtilities, DISABLED_readEnvFileTest) {
  auto env_file_path = cwd + "/data/mock_v.env";
  EXPECT_EQ("#!/usr/bin/env bash\nKEY='value'", kiq::FileUtils::ReadEnvFile(env_file_path));
}

/**
 * Extract tokens
 */
TEST(KUtilities, extractTokens) {
  std::string flag_s{"--description=$DESCRIPTION --hashtags=$HASHTAGS --requested_by=$REQUESTED_BY --media=$FILE_TYPE --requested_by_phrase=$REQUESTED_BY_PHRASE --promote_share=$PROMOTE_SHARE --link_bio=$LINK_BIO --header=$HEADER --user=$USER"};
  auto env_file_path = cwd + "/data/mock_v_2.env";

  auto token_values = kiq::FileUtils::ReadFlagTokens(env_file_path, flag_s);

  for (const auto& value : token_values)
    std::cout << value << std::endl;

  EXPECT_FALSE(token_values.empty());
}

TEST(KUtilities, DISABLED_writeTokens) {
  auto env_file_path = cwd + "/data/mock_v_2.env";
  std::string flag_s{"--description=$DESCRIPTION --hashtags=$HASHTAGS --requested_by=$REQUESTED_BY --media=$FILE_TYPE --requested_by_phrase=$REQUESTED_BY_PHRASE --promote_share=$PROMOTE_SHARE --link_bio=$LINK_BIO --header=$HEADER --user=$USER"};

  auto old_env = kiq::FileUtils::ReadEnvFile(env_file_path);

  std::cout << "Size was " << old_env.size() << std::endl;

  auto old_values = kiq::FileUtils::ReadFlagTokens(env_file_path, flag_s);

  auto changed = kiq::FileUtils::WriteEnvToken(env_file_path, "DESCRIPTION", "thisthat");

  auto new_values = kiq::FileUtils::ReadFlagTokens(env_file_path, flag_s);

  for (const auto& value : new_values) {
    std::cout << value << std::endl;
  }

  auto new_env = kiq::FileUtils::ReadEnvFile(env_file_path);

  std::cout << "Size now " << new_env.size() << std::endl;

  EXPECT_TRUE(changed);
}

#endif // __UTIL_TEST_HPP__
