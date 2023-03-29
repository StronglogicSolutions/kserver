#include "environment.test.hpp"

TEST(EnvironmentTest, DISABLED_ParseFlagsFromString) {
  std::string flag_string{
    "--description=$DESCRIPTION --media=$FILE_TYPE --header=$HEADER --user=$USER"
  };

  std::string env_file{"data/mock_v_2.env"};

  kiq::Task test_task{
    .mask = 16,
    .datetime = "1590776872",
    .file = true,
    .files = {kiq::FileInfo{std::pair<std::string, std::string>{"testfile.txt", "1590776872"}}},
    .env = env_file,
    .flags = flag_string,
    .task_id   = 0, // default initialized value in Task struct
    .completed = 0,
    .recurring = kiq::Constants::Recurring::YEARLY,
    .notify = true,
    .runtime = "runtime_arg",
    .filenames = {"thisfile.jpg", "thatfile.mpg"}
  };

  kiq::Environment runtime_environment{};

  runtime_environment.setTask(test_task);

  // Environment runtime_environment{};

  std::vector<std::string> flags = kiq::exec_flags_to_vector(flag_string);

  bool prepared_successfully = runtime_environment.prepareRuntime();
  kiq::ExecutionState exec_state  = runtime_environment.get();

  EXPECT_FALSE(flags.empty());
  EXPECT_FALSE(exec_state.argv.empty());
  EXPECT_TRUE(prepared_successfully);
  EXPECT_EQ(flags.at(0), "DESCRIPTION");
}
