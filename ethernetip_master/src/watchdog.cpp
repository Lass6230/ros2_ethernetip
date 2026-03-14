#include "ethernetip_master/watchdog.hpp"

namespace ethernetip_master
{

void Watchdog::configure(
  const std::string & device_name,
  double timeout_ms,
  TimeoutCallback callback)
{
  device_name_ = device_name;
  timeout_ = Duration(static_cast<int64_t>(timeout_ms));
  callback_ = std::move(callback);
  last_feed_ = Clock::now();
  expired_ = false;
  configured_ = true;
}

void Watchdog::feed()
{
  last_feed_ = Clock::now();
  expired_ = false;
}

bool Watchdog::check()
{
  if (!configured_) {
    return false;
  }
  if (expired_) {
    return true;  // already fired
  }

  auto elapsed = std::chrono::duration_cast<Duration>(
    Clock::now() - last_feed_);

  if (elapsed > timeout_) {
    expired_ = true;
    if (callback_) {
      callback_(device_name_);
    }
    return true;
  }
  return false;
}

void Watchdog::reset()
{
  expired_ = false;
  last_feed_ = Clock::now();
}

}  // namespace ethernetip_master
