#include "time.hpp"

namespace kiq
{
  Timer::Timer(const uint64_t duration_)
      : duration(duration_)
  {
  }

  bool Timer::active() const
  {
    return timer_active;
  }

  bool Timer::expired() const
  {
    const TimePoint now     = std::chrono::system_clock::now();
    const int64_t   elapsed = std::chrono::duration_cast<Duration>(now - time_point).count();
    return timer_active && (elapsed > duration);
  }

  void Timer::reset()
  {
    time_point   = std::chrono::system_clock::now();
    timer_active = true;
  }

  void Timer::stop()
  {
    timer_active = false;
  }

} // ns kiq
