#ifndef __INSTAGRAM_HPP__
#define __INSTAGRAM_HPP__

#include <iostream>
#include <vector>

#include "system/process/scheduler.hpp"
#include "task.hpp"

/**
 * IGTaskIndex
 *
 * These indices describe the order of arguments expected for processing of an IGTask
 */
namespace IGTaskIndex {
  static constexpr uint8_t MASK = TaskIndexes::MASK;
  static constexpr uint8_t FILEINFO = 1;
  static constexpr uint8_t DATETIME = 2;
  static constexpr uint8_t DESCRIPTION = 3;
  static constexpr uint8_t HASHTAGS = 4;
  static constexpr uint8_t REQUESTED_BY = 5;
  static constexpr uint8_t REQUESTED_BY_PHRASE = 6;
  static constexpr uint8_t PROMOTE_SHARE = 7;
  static constexpr uint8_t LINK_BIO = 8;
  static constexpr uint8_t IS_VIDEO = 9;
  static constexpr uint8_t HEADER = 10;
  static constexpr uint8_t USER = 11;
} //

namespace Name {
  static constexpr const char* INSTAGRAM = "Instagram";
} //

class IGTaskHandler : public TaskHandler {
 public:
  virtual Task prepareTask(const std::vector<std::string>& argv,
                           const std::string&              uuid,
                           Task*                           task_ptr = nullptr) override;
};

#endif  // __INSTAGRAM_HPP__
