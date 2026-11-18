#pragma once
#include <stddef.h>
#include <stdint.h>

bool responseLine(const char *text, const char *line);
bool responsePrefix(const char *text, const char *prefix);
bool jsonFieldEquals(const char *text, const char *key, const char *value);
bool bridgeAck(const char *line, unsigned payloadLength, const char *tag);
bool rtcCalendarValid(const uint8_t clock[7]);
bool parseUnsigned(const char *text, uint32_t &value, uint32_t maximum);
bool validUdpTarget(const char *ip,const char *port,bool allowUnset);
bool networkRegistered(const char *reply);
uint32_t crc32Byte(uint32_t crc,uint8_t byte);
