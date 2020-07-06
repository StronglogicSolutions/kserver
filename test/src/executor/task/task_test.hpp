#ifndef __TASK_TEST_HPP__
#define __TASK_TEST_HPP__

#include <executor/task_handlers/task.hpp>
#include <executor/task_handlers/instagram.hpp>
#include <executor/task_handlers/generic.hpp>
#include <gtest/gtest.h>

using namespace Executor;

/**
 * GenericTask test
 */

TEST(Task, PrepareTaskTest) {
  GenericTaskHandler generic_task_hander{};
  auto uuid = "38920jd93274098327489d032";
  auto id = 5;
  auto mask = 64;
  auto expected_task = Task{
    .execution_mask = mask,
    .datetime = "1590776872",
    .file = true,
    .files = {FileInfo{std::pair<std::string, std::string>{"testfile.txt", "1590776872"}}},
    .envfile = "",
    .execution_flags = "--description=$DESCRIPTION --media=$FILE_TYPE --header=$HEADER --user=$USER",
    .id = 0, // default initialized value in Task struct
    .completed = 0,
    .recurring = Executor::Constants::Recurring::YEARLY,
    .notify = true
  };
  std::vector<std::string> argv{std::to_string(mask), "1590776872testfile.txt|image:", "1590776872", "Test description", "1", "Test header", "test_user", "5", "1"};

  Task generic_task = generic_task_hander.prepareTask(argv, uuid);

  EXPECT_EQ(generic_task.execution_mask, expected_task.execution_mask);
  EXPECT_EQ(generic_task.datetime, expected_task.datetime);
  EXPECT_EQ(generic_task.file, expected_task.file);
  EXPECT_EQ(generic_task.execution_flags, expected_task.execution_flags);
  EXPECT_EQ(generic_task.id, expected_task.id);

}

#endif // __TASK_TEST_HPP__
