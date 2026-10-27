#ifndef SERIAL_WIN_H
#define SERIAL_WIN_H

#include <stdbool.h>

// Sends "data" to the board on a background thread (UI never freezes).
// Returns false if a send is already running or the thread could not start.
// The board is written to exactly ONCE (the firmware saves + reconnects on every line).
bool SerialSendStart(const char *portName, const char *data);

// true while the background send/listen is still running.
bool SerialBusy(void);

// After SerialBusy() is false: true if the data was written to the port.
bool SerialLastOk(void);

// Blocking version (kept for compatibility). Prefer SerialSendStart.
bool SendSerialData(const char *portName, const char *data);

// Windows error code of the last failed send (0 if none).
unsigned long SerialLastError(void);

// Status lines the board printed after the data was sent, joined by spaces
// (e.g. "[EEPROM] Credentials saved successfully. Connecting to X..... Connected! Device IP: 192.168.1.9").
// Gyro number lines are filtered out.
const char *SerialLastReply(void);

// Windows error code if reading the board's reply failed (0 = read worked).
unsigned long SerialLastReadError(void);

// Lists all non-Bluetooth COM ports, space separated (e.g. "COM3 COM7").
void ListComPorts(char *out, int outLen);

// Finds a COM port (prefers USB-serial devices, skips Bluetooth). Returns false if none.
bool FindComPort(char *out, int outLen);

#endif