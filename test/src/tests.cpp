#include <gtest/gtest.h>

#include <codec/util.hpp>
#include <iostream>
using namespace FileUtils;

auto util_cwd = get_executable_cwd();
auto cwd = std::string{util_cwd.begin(), util_cwd.end() - 5};

TEST(KUtilities, readEnvFileTest) {
  auto env_file_path = cwd + "/mock_data/mock_v.env";
  std::cout << env_file_path << std::endl;
  EXPECT_NE("", readEnvFile(env_file_path));
}
