#include <iostream>

#include "util.h"

int main(int argc, char** argv) {
  std::vector<std::string> arg_vector{"arg1", "arg2", "arg3"};
  std::string message_string = createMessage("This is a custom message");

  std::string op_string = createOperation("start", arg_vector);

  std::cout << "Message: " << message_string << std::endl;
  std::cout << "OperatioN: " << op_string << std::endl;

  return 0;
}
