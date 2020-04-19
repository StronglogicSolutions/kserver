#include <gtest/gtest.h>
#include <iostream>
#include <codec/util.hpp>
#include <server/kserver.hpp>

using namespace FileUtils;
using namespace KYO;

/**
 * KServerTestEnvironment
 *
 * Testing helper to provide values and repeatably behaviour
 */
class KServerTestEnvironment {
  public:
  KServerTestEnvironment() {
    setUp();
  }

  char** getArgv() { return argv; };
  int getArgc() { return argc; };

  private:
  void setUp() {
    int arg_num = 3;
    char* char_args[3] = {"127.0.0.1", "9876", "argument_string"};
    argv = char_args;
    argc = arg_num;
  }

  char** argv;
  int argc;
};

/**
 * TESTING GLOBALS
 */
auto util_cwd = get_executable_cwd();
auto cwd = std::string{util_cwd.begin(), util_cwd.end() - 5};
KServerTestEnvironment* ktest_env;

/**
 * TEST SUITE
 */
TEST(KUtilities, readEnvFileTest) {
  auto env_file_path = cwd + "/mock_data/mock_v.env";
  std::cout << env_file_path << std::endl;
  EXPECT_NE("", readEnvFile(env_file_path));
}

TEST(KServer, instantiateKServerTest) {
  KServer* kserver = new KServer{ktest_env->getArgc(),
                                 ktest_env->getArgv()};
  EXPECT_NE(nullptr, kserver);
}

int main(int argc, char** argv) {
  ktest_env = new KServerTestEnvironment{};
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
