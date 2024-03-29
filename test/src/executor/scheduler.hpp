#ifndef __SCHEDULER_TEST_HPP__
#define __SCHEDULER_TEST_HPP__

#include <system/process/scheduler.hpp>
#include <system/process/executor/task_handlers/generic.hpp>
#include <gtest/gtest.h>

/**
 * GenericTask test
 */

TEST(SchedulerTest, ScheduleInvalidTaskReturnsEmptyString) {
  kiq::GenericTaskHandler generic_task_hander{};
  auto uuid = "38920jd93274098327489d033";
  std::vector<std::string> argv{"0", "1590776872testfile.txt|image:", "1590776872", "Test description", "1", "Test header", "test_user", "5", "1", "runtime_arg"};
  Database::KDB kdb{
    DatabaseConfiguration{
      DatabaseCredentials{
        .user="ktestadmin", .password="ktestadmin", .name="ktesting"
      },
      "127.0.0.1",
      "5432"
    }
  };
  kiq::Scheduler scheduler{std::move(kdb)};

  kiq::Task generic_task = generic_task_hander.prepareTask(argv, uuid);
  generic_task.mask = 0; // invalidate task
  generic_task.flags.clear(); // invalidate task
  try {
    auto id = scheduler.schedule(generic_task);
    EXPECT_EQ(id, "");
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }
}

#endif // __SCHEDULER_TEST_HPP__
