#ifndef ETHERNETIP_MASTER__WATCHDOG_HPP_
#define ETHERNETIP_MASTER__WATCHDOG_HPP_

#include <chrono>
#include <functional>
#include <string>

namespace ethernetip_master
{

/// Per‑device watchdog that fires when cyclic data has not been
/// received for longer than the configured timeout.
class Watchdog
{
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = std::chrono::milliseconds;

  /// Callback invoked when the watchdog expires.
  using TimeoutCallback = std::function<void(const std::string & device_name)>;

  Watchdog() = default;

  /// Configure the watchdog.
  /// @param device_name  Identifier used in the timeout callback.
  /// @param timeout_ms   Timeout duration in milliseconds.
  /// @param callback     Function called on timeout.
  void configure(
    const std::string & device_name,
    double timeout_ms,
    TimeoutCallback callback);

  /// Call this every time a valid packet is received.
  void feed();

  /// Check whether the watchdog has expired.  Returns true once per
  /// expiration event (latched until reset).
  bool check();

  /// Reset the watchdog (clears the expired flag).
  void reset();

  /// Return whether the device is currently timed‑out.
  bool isExpired() const { return expired_; }

private:
  std::string device_name_;
  Duration timeout_{100};
  TimeoutCallback callback_;
  TimePoint last_feed_;
  bool expired_{false};
  bool configured_{false};
};

}  // namespace ethernetip_master

#endif  // ETHERNETIP_MASTER__WATCHDOG_HPP_
