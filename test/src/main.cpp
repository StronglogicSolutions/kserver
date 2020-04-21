#include <gtest/gtest.h>
#include "codec/util_test.hpp"
#include "kserver/kserver_test.hpp"
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
    const char* char_args[3] = {"127.0.0.1", "9876", "argument_string"};
    argv = const_cast<char**>(char_args);
    argc = arg_num;
  }

  char** argv;
  int argc;
};

/**
 * TESTING GLOBALS
 */



int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
