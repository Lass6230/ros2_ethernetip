// Copyright 2026, ethernetip_master contributors
// Licensed under the Apache License, Version 2.0

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "ethernetip_master/watchdog.hpp"

using ethernetip_master::Watchdog;

// ---------------------------------------------------------------------------
// Unconfigured watchdog never fires
// ---------------------------------------------------------------------------
TEST(WatchdogTest, UnconfiguredDoesNotFire)
{
  Watchdog wd;
  EXPECT_FALSE(wd.isExpired());
  EXPECT_FALSE(wd.check());
}

// ---------------------------------------------------------------------------
// Configured + fed → not expired
// ---------------------------------------------------------------------------
TEST(WatchdogTest, FedDoesNotExpire)
{
  Watchdog wd;
  bool callback_fired = false;
  wd.configure("test_dev", 100.0, [&](const std::string &) {
    callback_fired = true;
  });

  wd.feed();
  EXPECT_FALSE(wd.check());
  EXPECT_FALSE(wd.isExpired());
  EXPECT_FALSE(callback_fired);
}

// ---------------------------------------------------------------------------
// Timeout triggers callback exactly once
// ---------------------------------------------------------------------------
TEST(WatchdogTest, TimeoutFiresCallback)
{
  Watchdog wd;
  std::atomic<int> fire_count{0};
  std::string reported_name;

  wd.configure("my_device", 10.0, [&](const std::string & name) {
    fire_count++;
    reported_name = name;
  });

  // Wait for the watchdog to expire
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  EXPECT_TRUE(wd.check());
  EXPECT_TRUE(wd.isExpired());
  EXPECT_EQ(fire_count.load(), 1);
  EXPECT_EQ(reported_name, "my_device");

  // Second check should still report expired but NOT fire callback again
  EXPECT_TRUE(wd.check());
  EXPECT_EQ(fire_count.load(), 1);
}

// ---------------------------------------------------------------------------
// Feed resets the timer
// ---------------------------------------------------------------------------
TEST(WatchdogTest, FeedResetsTimer)
{
  Watchdog wd;
  int fire_count = 0;

  wd.configure("dev", 50.0, [&](const std::string &) { fire_count++; });

  // Feed before timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  wd.feed();
  EXPECT_FALSE(wd.check());

  // Feed again before timeout
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  wd.feed();
  EXPECT_FALSE(wd.check());

  EXPECT_EQ(fire_count, 0);
}

// ---------------------------------------------------------------------------
// Reset clears expired state
// ---------------------------------------------------------------------------
TEST(WatchdogTest, ResetClearsExpired)
{
  Watchdog wd;
  wd.configure("dev", 10.0, [](const std::string &) {});

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  EXPECT_TRUE(wd.check());
  EXPECT_TRUE(wd.isExpired());

  wd.reset();
  EXPECT_FALSE(wd.isExpired());
  EXPECT_FALSE(wd.check());  // timer was also reset
}

// ---------------------------------------------------------------------------
// Null callback does not crash
// ---------------------------------------------------------------------------
TEST(WatchdogTest, NullCallbackDoesNotCrash)
{
  Watchdog wd;
  wd.configure("dev", 10.0, nullptr);

  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  // Should not crash even with nullptr callback
  EXPECT_TRUE(wd.check());
  EXPECT_TRUE(wd.isExpired());
}
