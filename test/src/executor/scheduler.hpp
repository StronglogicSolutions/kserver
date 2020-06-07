#ifndef __SCHEDULER_TEST_HPP__
#define __SCHEDULER_TEST_HPP__

#include <executor/scheduler.hpp>
#include <executor/task_handlers/generic.hpp>
#include <gtest/gtest.h>

using namespace Executor;

/**
 * GenericTask test
 */

TEST(SchedulerTest, ScheduleInvalidTaskReturnsEmptyString) {
  GenericTaskHandler generic_task_hander{};
  auto uuid = "38920jd93274098327489d033";
  std::vector<std::string> argv{"-2", "1590776872testfile.jpg|image:", "1590776872", "Test description", "1", "Test header", "test_user"};

  Task generic_task = generic_task_hander.prepareTask(argv, uuid);
  Scheduler::Scheduler scheduler{};
  auto id = scheduler.schedule(generic_task);

  EXPECT_EQ(id, "");

}

#endif // __SCHEDULER_TEST_HPP__
