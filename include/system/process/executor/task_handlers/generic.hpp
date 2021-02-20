#ifndef __GENERIC_HPP__
#define __GENERIC_HPP__

#include <iostream>
#include <vector>

#include "system/process/scheduler.hpp"


/**
 * GenericTaskIndex
 *
 * These indices describe the order of arguments expected for processing of an IGTask
 */
namespace GenericTaskIndex {
  static const uint8_t MASK        = TaskIndexes::MASK;
  static const uint8_t FILEINFO    = 1;
  static const uint8_t DATETIME    = 2;
  static const uint8_t DESCRIPTION = 3;
  static const uint8_t IS_VIDEO    = 4;
  static const uint8_t HEADER      = 5;
  static const uint8_t USER        = 6;
  static const uint8_t RECURRING   = 7;
  static const uint8_t NOTIFY      = 8;
  static const uint8_t RUNTIME     = 9;
}

const std::string GENERIC_TASK_EXECUTION_FLAGS{"--description=$DESCRIPTION "\
                                                "--media=$FILE_TYPE "\
                                                "--header=$HEADER --user=$USER"};

class GenericTaskHandler : public TaskHandler {
 public:
  virtual Task prepareTask(std::vector<std::string> argv,
                                     std::string uuid, Task* task_ptr = nullptr) override;
};

#endif  // __GENERIC_HPP__
