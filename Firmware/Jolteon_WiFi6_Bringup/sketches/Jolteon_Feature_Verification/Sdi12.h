#pragma once
#include <stddef.h>
bool sdi12Exchange(const char *command,char *response,size_t capacity,const char *&error);
bool sdi12Power(bool enabled);
