#pragma once
#include <chrono>

namespace kiq {
struct Timer {
using  TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using  Duration  = std::chrono::seconds;
static const uint32_t ONE_MINUTE     = 60;
static const uint32_t TEN_MINUTES    = 600;
static const uint32_t TWENTY_MINUTES = 1200;
Timer(const int64_t duration_ = TWENTY_MINUTES);
bool active()  const;
bool expired() const;
void start();
void stop();

private:
TimePoint time_point;
bool      timer_active;
int64_t   duration;
};

} // ns kiq
