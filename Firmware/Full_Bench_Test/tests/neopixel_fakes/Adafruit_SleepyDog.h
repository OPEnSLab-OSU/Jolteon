#pragma once
struct FakeWatchdog { void reset() {} void disable() {} };
extern FakeWatchdog Watchdog;
