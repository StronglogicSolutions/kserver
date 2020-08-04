#include <log/logger.h>

#include <fstream>
#include <sstream>
#include <string>

namespace System {

/*

  * * * * *
  | | | | |
  | | | | + --- day of week (0 - 6 / Sun - Sat)
  | | | +------ month (1 - 12)
  | | +-------- day of month (1 - 31)
  | +---------- hour (0 - 23)
  +------------ minute (0 - 59)


 */

namespace Month {
static constexpr int JANUARY = 0;
static constexpr int FEBRUARY = 1;
static constexpr int MARCH = 2;
static constexpr int APRIL = 3;
static constexpr int MAY = 4;
static constexpr int JUNE = 5;
static constexpr int JULY = 6;
static constexpr int AUGUST = 7;
static constexpr int SEPTEMBER = 8;
static constexpr int OCTOBER = 9;
static constexpr int NOVEMBER = 10;
static constexpr int DECEMBER = 11;
}  // namespace Month

enum JobType { Single = 0, Daily = 1, Weekly = 2, Monthly = 3, Yearly = 4 };
struct WeeklyJob {
  std::string path;
  int day;
  int hour;
  int minute;
  JobType type = JobType::Weekly;
};

struct SingleJob {
  std::string path;
  int month;
  int day_of_month;
  int hour;
  int minute;
  JobType type = JobType::Single;
};

struct DailyJob {
  std::string path;
  int hour;
  int minute;
  JobType type = JobType::Daily;
};

template <typename Job>
class CronInterface {
 public:
  virtual std::string listJobs() = 0;
  virtual void addJob(Job job) = 0;
  virtual void deleteJob(Job job) = 0;
};

template <typename Job>
class Cron : public CronInterface<Job> {
 public:
  virtual std::string listJobs() override {
    std::system("crontab -l > cron.txt");
    return std::string{
        static_cast<std::stringstream const&>(
            std::stringstream() << std::ifstream("cron.txt").rdbuf())
            .str()};
  }

  std::string createJobCommand(Job job) {
    if (job.type == JobType::Single) {
      return std::string{"(crontab -l ; echo \"" + std::to_string(job.minute) +
                         " " + std::to_string(job.hour) + " " +
                         std::to_string(job.day_of_month) + " " +
                         std::to_string(job.month) + " * " + job.path +
                         "\") | sort - | uniq - | crontab -"};
    }
    return "";
  }

  virtual void addJob(Job job) {
    std::system(createJobCommand(job).c_str());
    if (findJob(job.path)) {
      KLOG("Successfully added job to cron: {}", job.path);
    } else {
      KLOG("Failed to add job to cron: {}", job.path);
    }
  }

  virtual void deleteJob(Job job) {
    std::system(std::string{"crontab -l | grep -v \"" + job.path + "\" |crontab -"}.c_str());
     if (findJob(job.path)) { // This match only works if the execution string is identical
       KLOG("Failed to remove job from cron: {}", job.path);
     } else {
       KLOG("Successfully removed job from cron: {}", job.path);
     }
  }

  bool findJob(std::string job_command) {
    return (listJobs().find(job_command) != std::string::npos);
  }
};
}  // namespace System
