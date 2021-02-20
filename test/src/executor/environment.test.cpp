#include "environment.test.hpp"

TEST(EnvironmentTest, ParseFlagsFromString) {
  std::string flag_string{
    "--description=$DESCRIPTION --media=$FILE_TYPE --header=$HEADER --user=$USER"
  };

  // std::string env_file{
  //   "#!/usr/bin/env bash"
  //   "HEADER='Test Task'"
  //   "USER='Test User'"
  //   "DESCRIPTION='Test Description'"
  //   "FILE_TYPE='IMAGE'"
  // };

  std::string env_file{"data/e47919f8-0c2a-41f6-908c-8112f4923781/v.env"};

  Task test_task{
    .execution_mask = 16,
    .datetime = "1590776872",
    .file = true,
    .files = {FileInfo{std::pair<std::string, std::string>{"testfile.txt", "1590776872"}}},
    .envfile = env_file,
    .execution_flags = flag_string,
    .id = 0, // default initialized value in Task struct
    .completed = 0,
    .recurring = Constants::Recurring::YEARLY,
    .notify = true,
    .runtime = "runtime_arg",
    .filenames = {"thisfile.jpg", "thatfile.mpg"}
  };

  Environment runtime_environment{};

  runtime_environment.setTask(test_task);

  // Environment runtime_environment{};

  std::vector<std::string> flags = exec_flags_to_vector(flag_string);

  bool prepared_successfully = runtime_environment.prepareRuntime();
  ExecutionState exec_state  = runtime_environment.get();

  EXPECT_FALSE(flags.empty());
  EXPECT_FALSE(exec_state.argv.empty());
  EXPECT_TRUE(prepared_successfully);
  EXPECT_EQ(flags.at(0), "DESCRIPTION");
}
