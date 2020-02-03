#ifndef __REQUEST_HANDLER_HPP__
#define __REQUEST_HANDLER_HPP__

#include <codec/kmessage_generated.h>
#include <database/DatabaseConnection.h>
#include <log/logger.h>
#include <stdlib.h>

#include <chrono>
#include <codec/util.hpp>
#include <config/config_parser.hpp>
#include <database/kdb.hpp>
#include <executor/executor.hpp>
#include <executor/scheduler.hpp>
#include <iostream>
#include <map>
#include <server/types.hpp>
#include <string>
#include <system/cron.hpp>
#include <utility>
#include <vector>

namespace Request {

enum DevTest { Schedule = 1, ExecuteTask = 2 };

using namespace KData;

flatbuffers::FlatBufferBuilder builder(1024);

auto KLOG = KLogger::GetInstance() -> get_logger();

typedef std::pair<std::string, std::string> FileInfo;

int findIndexAfter(std::string s, int pos, char c) {
  for (int i = pos; i < s.size(); i++) {
    if (s.at(i) == c) {
      return i;
    }
  }
  return -1;
}

std::vector<FileInfo> parseFileInfo(std::string file_info) {
  KLOG->info("Request::parseFileInfo() - Parsing: {}", file_info);
  std::vector<FileInfo> info_v{};
  info_v.reserve(file_info.size() /
                 25);  // Estimating number of files being represented
  size_t pipe_pos = 0;
  size_t index = 0;
  size_t delim_pos = 0;
  std::string parsing{file_info, file_info.size()};
  do {
    auto timestamp = file_info.substr(index, 10);
    pipe_pos = findIndexAfter(file_info, index, '|');
    auto file_name = file_info.substr(index + 10, (pipe_pos - index - 10));
    delim_pos = findIndexAfter(file_info, index, '::');
    auto type =
        file_info.substr(index + 10 + file_name.size() + 1,
                         (delim_pos - index - 10 - file_name.size() - 1));
    KLOG->info("Parsed file {} of type {} with timestamp {}", file_name, type,
               timestamp);

    info_v.push_back(FileInfo{file_name, timestamp});
    index += timestamp.size() + file_name.size() + type.size() +
             3;  // 3 strings + 3 delim chars
  } while (index < file_info.size());
  return info_v;
}

class RequestHandler {
 public:
  RequestHandler() : m_executor(nullptr) {
    if (!ConfigParser::initConfig()) {
      KLOG->info("RequestHandler::RequestHandler() - Unable to load config");
      return;
    }

    m_credentials = {.user = ConfigParser::getDBUser(),
                     .password = ConfigParser::getDBPass(),
                     .name = ConfigParser::getDBName()};

    DatabaseConfiguration configuration{
        .credentials = m_credentials, .address = "127.0.0.1", .port = "5432"};

    m_connection = DatabaseConnection{};
    m_connection.setConfig(configuration);
    KLOG->info("RequestHandler::RequestHandler - set database credentials");
  }

  RequestHandler(RequestHandler &&r)
      : m_executor(r.m_executor),
        m_connection(r.m_connection),
        m_credentials(r.m_credentials) {
    r.m_executor = nullptr;
  }

  RequestHandler(const RequestHandler &r)
      : m_executor(nullptr),  // We do not copy the Executor
        m_connection(r.m_connection),
        m_credentials(r.m_credentials) {}

  RequestHandler &operator=(const RequestHandler &handler) {
    this->m_executor = nullptr;
    this->m_connection = handler.m_connection;
    this->m_credentials = handler.m_credentials;
    return *this;
  }

  RequestHandler &operator=(RequestHandler &&handler) {
    if (&handler != this) {
      delete m_executor;
      m_executor = handler.m_executor;
      handler.m_executor = nullptr;
    }
    return *this;
  }

  ~RequestHandler() {
    if (m_executor != nullptr) {
      delete m_executor;
    }
    if (m_maintenance_worker.valid()) {
      KLOG->info(
          "RequestHandler::~RequestHandler() - Waiting for maintenance worker "
          "to complete");
      m_maintenance_worker.get();
    }
  }

  void initialize(
      std::function<void(std::string, int, std::string, int)> event_callback_fn,
      std::function<void(int, int, std::vector<std::string>)>
          system_callback_fn,
      std::function<void(int, std::vector<Executor::Task>)> task_callback_fn) {
    m_executor = new ProcessExecutor();
    m_executor->setEventCallback([this](std::string result, int mask,
                                        std::string request_id,
                                        int client_socket_fd) {
      onProcessComplete(result, mask, request_id, client_socket_fd);
    });
    m_system_callback_fn = system_callback_fn;
    m_event_callback_fn = event_callback_fn;
    m_task_callback_fn = task_callback_fn;

    // Begin maintenance loop to process scheduled tasks as they become ready
    m_maintenance_worker =
        std::async(std::launch::async, &RequestHandler::maintenanceLoop, this);
    KLOG->info("RequestHandler::initialize() - Initialization complete");
  }

  Executor::Scheduler getScheduler() {
    return Executor::Scheduler{
        [this](std::string result, int mask, int id, int client_socket_fd) {
          onProcessComplete(result, mask, std::to_string(id), client_socket_fd);
        }};
  }

  void maintenanceLoop() {
    KLOG->info(
        "RequestHandler::maintenanceLoop() - Beginning maintenance loop");
    for (;;) {
      int client_socket_fd = -1;
      Executor::Scheduler scheduler = getScheduler();
      std::vector<Executor::Task> tasks = scheduler.fetchTasks();
      if (!tasks.empty()) {
        KLOG->info("There are tasks to be reviewed");
        for (const auto &task : tasks) {
          KLOG->info(
              "Task info: {} - Mask: {}\n Args: {}\n {}\n. Excluded: Execution "
              "Flags",
              task.datetime, std::to_string(task.execution_mask),
              task.file ? "hasFile(s)" : "", task.envfile);
        }
        std::string tasks_message = std::to_string(tasks.size());
        tasks_message += " tasks scheduled to run in the next 24 hours";
        m_system_callback_fn(client_socket_fd,
                             SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
                             {tasks_message});

        // for (const auto& task : tasks) {
        // scheduler.executeTask(client_socket_fd, tasks.at(0));
        // m_task_callback_fn(client_socket_fd, tasks);
        auto it = m_tasks_map.find(client_socket_fd);
        if (it == m_tasks_map.end()) {
          m_tasks_map.insert(std::pair<int, std::vector<Executor::Task>>(
              client_socket_fd, tasks));
        } else {
          it->second.insert(it->second.end(), tasks.begin(), tasks.end());
        }
        KLOG->info(
            "RequestHandler::maintenanceLoop() - KServer has {} {} pending "
            "execution",
            m_tasks_map.at(client_socket_fd).size(),
            m_tasks_map.at(client_socket_fd).size() == 1 ? "task" : "task");
      } else {
        KLOG->info(
            "RequestHandler::maintenanceLoop() - There are currently no tasks "
            "ready for execution");
        m_system_callback_fn(
            client_socket_fd, SYSTEM_EVENTS__SCHEDULED_TASKS_NONE,
            {"There are currently no tasks ready for execution"});
      }
      handlePendingTasks();
      KLOG->info("RequestHandler::maintenanceLoop() - Running scheduled tasks");

      System::Cron<System::SingleJob> cron{};
      std::string jobs = cron.listJobs();

      KLOG->info(
          "RequestHandler::maintenanceLoop() - System Cron returned the "
          "following jobs from the operating system: "
          "\n{}",
          jobs);
      KLOG->info(
          "RequestHandler::maintenanceLoop() - Testing adding job to system "
          "cron");

      System::SingleJob job{.path = "ls -la ~",
                            .month = System::Month::DECEMBER,
                            .day_of_month = 25,
                            .hour = 23,
                            .minute = 59};
      cron.addJob(job);

      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      KLOG->info(
          "RequestHandler::maintenanceLoop() - Testing deleting job to system "
          "cron");

      cron.deleteJob(job);
      std::this_thread::sleep_for(std::chrono::minutes(5));
    }
  }

  void handlePendingTasks() {
    Executor::Scheduler scheduler = getScheduler();
    if (!m_tasks_map.empty()) {
      for (const auto &client_tasks : m_tasks_map) {
        if (!client_tasks.second.empty()) {
          for (const auto &task : client_tasks.second) {
            scheduler.executeTask(client_tasks.first, task);
            KLOG->info(
                "RequestHandler::handlePendingTasks() - Would be handling task "
                "with ID {} scheduled for {}",
                task.id, task.datetime);
          }
        }
      }
    }
  }

  std::string operator()(KOperation op, std::vector<std::string> argv,
                         int client_socket_fd, std::string uuid) {
    if (op == "Schedule") {
      KLOG->info("RequestHandler:: Handling schedule request");
      if (argv.empty()) {
        KLOG->info(
            "RequestHandler::Scheduler - Can't handle a task with no "
            "arguments");
        return "";
      }
      auto mask = argv.at(argv.size() - 1);
      auto kdb = Database::KDB();

      QueryValues result =
          kdb.select("apps", {"name", "path"}, {{"mask", mask}});
      std::string name{};
      std::string path{};
      for (const auto &value : result) {
        if (value.first == "name") {
          name += value.second;
          continue;
        }
        if (value.first == "path") {
          path += value.second;
          continue;
        }
      }
      if (!path.empty() && !name.empty()) {
        if (name == "Instagram") {
          KLOG->info("RequestHandler:: Instagram task");
          auto file_info = argv.at(0);
          std::vector<FileInfo> files_to_update = parseFileInfo(file_info);
          std::vector<std::string> filenames{};

          auto file_index = 0;
          for (const auto &file_info : files_to_update) {
            std::vector<std::string> callback_args{file_info.first,
                                                   file_info.second, uuid};
            if (file_index == files_to_update.size() - 1) {
              callback_args.push_back("final file");
            }
            m_system_callback_fn(client_socket_fd, SYSTEM_EVENTS__FILE_UPDATE,
                                 callback_args);
            std::string media_filename = get_cwd();
            media_filename += "/data/" + uuid + "/" + file_info.first;
            filenames.push_back(media_filename);
          }
          // notify KServer of filename received
          auto datetime = argv.at(1);
          auto description = argv.at(2);
          auto hashtags = argv.at(3);
          auto requested_by = argv.at(4);
          auto requested_by_phrase = argv.at(5);
          auto promote_share = argv.at(6);
          auto link_bio = argv.at(7);
          auto is_video = argv.at(8) == "1";

          std::string env_file_string{"#!/usr/bin/env bash\n"};
          env_file_string += "DESCRIPTION='" + description + "'\n";
          env_file_string += "HASHTAGS='" + hashtags + "'\n";
          env_file_string += "REQUESTED_BY='" + requested_by + "'\n";
          env_file_string +=
              "REQUESTED_BY_PHRASE='" + requested_by_phrase + "'\n";
          env_file_string += "PROMOTE_SHARE='" + promote_share + "'\n";
          env_file_string += "LINK_BIO='" + link_bio + "'\n";
          env_file_string += "FILE_TYPE='";
          env_file_string += is_video ? "video'\n" : "image'\n";
          std::string env_filename = {"data/"};
          env_filename += uuid;
          env_filename += "/v.env";

          FileUtils::saveEnvFile(env_file_string, env_filename);

          auto validate = true;  // TODO: Replace this with actual validation
          if (validate) {
            KLOG->info("Sending task request to Scheduler");
            Executor::Scheduler scheduler{};
            Executor::Task task{
                .execution_mask = std::stoi(mask),
                .datetime = datetime,
                .file = (!filenames.empty()),
                .file_names = filenames,
                .envfile = env_filename,
                .execution_flags =
                    "--description=$DESCRIPTION --hashtags=$HASHTAGS "
                    "--requested_by=$REQUESTED_BY --media=$FILE_TYPE "
                    "--requested_by_phrase=$REQUESTED_BY_PHRASE "
                    "--promote_share=$PROMOTE_SHARE --link_bio=$LINK_BIO"};
            scheduler.schedule(task);
          }
        }
      }
    }
    return std::string{"Operation failed"};
  }

  /**
   *
   * generic functor overload for running operations during development
   *
   * Calls the scheduler to fetch tasks which need to be executed soon and
   * returns them to the KServer. These tasks can be performed iteratively on a
   * message loop, to ensure each execution completes before another one begins.
   *
   * @overload
   *
   * @param[in] {int} client_socket_fd The client socket file descriptor
   * @param[in] {KOperation} op The operation requested
   * @param[in] {DevTest} test An enum value representing the type of operation
   * to perform
   *
   */
  void operator()(int client_socket_fd, KOperation op, DevTest test) {
    if (strcmp(op.c_str(), "Test") == 0 && test == DevTest::Schedule) {
      Executor::Scheduler scheduler{[this](std::string result, int mask, int id,
                                           int client_socket_fd) {
        onProcessComplete(result, mask, std::to_string(id), client_socket_fd);
      }};
      std::vector<Executor::Task> tasks = scheduler.fetchTasks();
      if (!tasks.empty()) {
        KLOG->info("There are tasks to be reviewed");
        for (const auto &task : tasks) {
          KLOG->info(
              "RequestHandler:: OPERATION HANDLER - Task info: {} - Mask: {}\n "
              "Args: {}\n {}\n. Excluded: Execution "
              "Flags",
              task.datetime, std::to_string(task.execution_mask),
              task.file ? "hasFile(s)" : "", task.envfile);
        }
        std::string tasks_message = std::to_string(tasks.size());
        tasks_message += " tasks scheduled to run in the next 24 hours";
        m_system_callback_fn(client_socket_fd,
                             SYSTEM_EVENTS__SCHEDULED_TASKS_READY,
                             {tasks_message});

        // for (const auto& task : tasks) {
        // We need to inform the client of available tasks, and let them decide
        // if a task should be executed.
        scheduler.executeTask(client_socket_fd, tasks.at(0));
        // m_task_callback_fn(client_socket_fd, tasks);
        auto it = m_tasks_map.find(client_socket_fd);
        if (it == m_tasks_map.end()) {
          m_tasks_map.insert(std::pair<int, std::vector<Executor::Task>>(
              client_socket_fd, tasks));
        } else {
          it->second.insert(it->second.end(), tasks.begin(), tasks.end());
        }
        KLOG->info(
            "RequestHandler:: OPERATION HANDLER - {} currently has {} tasks "
            "pending execution",
            client_socket_fd, m_tasks_map.at(client_socket_fd).size());
      } else {
        KLOG->info("There are currently no tasks ready for execution");
        m_system_callback_fn(
            client_socket_fd, SYSTEM_EVENTS__SCHEDULED_TASKS_NONE,
            {"There are currently no tasks ready for execution"});
      }
    }
  }

  /**
   * fetchAvailableApplications
   *
   * Fetches the available applications that can be requested for execution by a
   * client
   */
  std::map<int, std::string> operator()(KOperation op) {
    // TODO: We need to fix the DB query so it is orderable and groupable
    DatabaseQuery select_query{.table = "apps",
                               .fields = {"name", "path", "data", "mask"},
                               .type = QueryType::SELECT,
                               .values = {}};
    QueryResult result = m_connection.query(select_query);
    std::map<int, std::string> command_map{};
    std::vector<std::string> names{};
    std::vector<int> masks{};
    // int mask = -1;
    std::string name{""};
    for (const auto &row : result.values) {
      if (row.first == "name") {
        names.push_back(row.second);
      } else if (row.first == "mask") {
        masks.push_back(stoi(row.second));
      }
    }
    if (masks.size() == names.size()) {
      for (int i = 0; i < masks.size(); i++) {
        command_map.emplace(masks.at(i), names.at(i));
      }
    }
    return command_map;
  }

  /**
   * runApplication
   *
   * Calls on the ProcessExecutor and requests that it execute an appication
   * whose mask value matches those contained within the value passed as a
   * parameter
   *
   * @overload
   * @param[in] {uint32_t} mask The requested mask
   * @param[in] {int} client_socket_fd The file descriptor for the client making
   * the request
   */
  void operator()(uint32_t mask, std::string request_id, int client_socket_fd) {
    QueryResult result = m_connection.query(
        DatabaseQuery{.table = "apps",
                      .fields = {"path"},
                      .type = QueryType::SELECT,
                      .values = {},
                      .filter = QueryFilter{std::make_pair(
                          std::string("mask"), std::to_string(mask))}});

    for (const auto &row : result.values) {
      KLOG->info("Field: {}, Value: {}", row.first, row.second);
      m_executor->request(row.second, mask, client_socket_fd, request_id, {});
    }
    std::string info_string{
        "RequestHandler:: PROCESS RUNNER - Process execution requested for "
        "applications matching the mask "};
    info_string += std::to_string(mask);
    m_system_callback_fn(client_socket_fd,
                         SYSTEM_EVENTS__PROCESS_EXECUTION_REQUESTED,
                         {info_string, request_id});
  }

  std::pair<uint8_t *, int> operator()(std::vector<uint32_t> bits) {
    QueryFilter filter{};

    for (const auto &bit : bits) {
      filter.push_back(
          std::make_pair(std::string("mask"), std::to_string(bit)));
    }
    DatabaseQuery select_query{.table = "apps",
                               .fields = {"name", "path", "data"},
                               .type = QueryType::SELECT,
                               .values = {},
                               .filter = filter};
    QueryResult result = m_connection.query(select_query);
    std::string paths{};
    if (!result.values.empty()) {
      for (const auto &row : result.values) {
        if (row.first == "path") paths += row.second + "\n";
      }
      unsigned char message_bytes[] = {33, 34, 45, 52, 52, 122, 101, 35};
      auto byte_vector = builder.CreateVector(message_bytes, 8);
      auto message = CreateMessage(builder, 0, byte_vector);
      builder.Finish(message);

      uint8_t *message_buffer = builder.GetBufferPointer();
      int size = builder.GetSize();

      std::pair<uint8_t *, int> ret_tuple =
          std::make_pair(message_buffer, std::ref(size));

      return ret_tuple;
    }
    /* return std::string{"Application not found"}; */
    uint8_t byte_array[6]{1, 2, 3, 4, 5, 6};
    return std::make_pair(&byte_array[0], 6);
  }

 private:
  // Callback
  void onProcessComplete(std::string value, int mask, std::string request_id,
                         int client_socket_fd) {
    KLOG->info(
        "RequestHandler::onProcessComplete() - Process complete notification "
        "for client {}'s request {}",
        client_socket_fd, request_id);
    m_event_callback_fn(value, mask, request_id, client_socket_fd);
  }

  std::function<void(std::string, int, std::string, int)> m_event_callback_fn;
  std::function<void(int, int, std::vector<std::string>)> m_system_callback_fn;
  std::function<void(int, std::vector<Executor::Task>)> m_task_callback_fn;

  std::map<int, std::vector<Executor::Task>> m_tasks_map;

  ProcessExecutor *m_executor;
  DatabaseConnection m_connection;
  DatabaseCredentials m_credentials;
  std::future<void> m_maintenance_worker;
};
}  // namespace Request
#endif  // __REQUEST_HANDLER_HPP__
