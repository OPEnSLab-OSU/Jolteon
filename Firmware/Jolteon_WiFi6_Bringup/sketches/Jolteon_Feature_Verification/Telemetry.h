#pragma once
#include "BoardIO.h"

class ReportConsole : public Print {
public:
  using Print::write;
  size_t write(uint8_t c) override;
};
extern ReportConsole report;
void telemetryService();
void telemetryPause(bool pause);
void telemetryMode(const char *mode);
bool telemetryTarget(const char *ip, const char *port);
void telemetryStatus();
bool telemetryCommand(char *out, size_t size);
enum RemoteSource { REMOTE_NONE, REMOTE_BLE, REMOTE_WIFI, REMOTE_BRIDGE };
bool telemetryCommand(char *out, size_t size, RemoteSource &source);
void telemetryFeed(uint8_t c); // Diagnostics feed bytes too; commands/acks survive UART ownership.
