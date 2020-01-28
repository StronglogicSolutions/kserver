#include <string>
#include <log/logger.h>
#include <fstream>
#include <sstream>

namespace System {

struct Job {
  std::string path;
  bool one_time;
  int hour;
  int minute;
  int second;
};

auto KLOG = KLogger::GetInstance()->get_logger();

class CronInterface {
  public:
    virtual std::string listJobs() = 0;
    virtual void addJob() = 0;
    virtual void deleteJob() = 0;
};

class Cron : public CronInterface {
  public:
    virtual std::string listJobs() override {

      auto jobs = std::system("crontab -l > cron.txt");
      return std::string {static_cast<std::stringstream const&>(std::stringstream() << std::ifstream("cron.txt").rdbuf()).str()};
    }

    virtual void addJob(Job job) {
      KLOG->info("Add Job to be implemented");
      std::system(
        std::string(
          "(crontab -l ; echo \"0 * * * * " +
          job.path +
          ") | sort - | uniq - | crontab -"
        ).c_str()
      );
      if (findJob(job.path)) {
        KLOG->info("Successfully added job to cron: {}", job.path);
      } else {
          KLOG->info("Failed to add job to cron: {}", job.path);
      }
    }

    virtual void deleteJob() {
      KLOG->info("Delete Job to be implemented");
    }

    bool findJob(std::string job) {
      auto jobs = std::string {static_cast<std::stringstream const&>(std::stringstream() << std::ifstream("cron.txt").rdbuf()).str()};
      return std::string::npos != jobs.find(job);
  }
};
}