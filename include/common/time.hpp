#pragma once
#include <chrono>

namespace kiq {
struct Timer {
using  TimePoint = std::chrono::time_point<std::chrono::system_clock>;
using  Duration  = std::chrono::seconds;
static const uint64_t ONE_MINUTE     = 60UL;
static const uint64_t TWO_MINUTES    = 120UL;
static const uint64_t TEN_MINUTES    = 600UL;
static const uint64_t TWENTY_MINUTES = 1200UL;
Timer(const uint64_t duration_ = TWENTY_MINUTES);
bool active()  const;
bool expired() const;
void start();
void stop();

private:
TimePoint time_point;
bool      timer_active;
uint64_t   duration;
};

} // ns kiq
