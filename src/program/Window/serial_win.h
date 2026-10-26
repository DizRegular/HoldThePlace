#ifndef SERIAL_WIN_H
#define SERIAL_WIN_H

#include <stdbool.h>

bool SendSerialData(const char* portName, const char* data);

#endif