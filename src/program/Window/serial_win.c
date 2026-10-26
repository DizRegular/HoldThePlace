#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool SendSerialData(const char* portName, const char* data) {
    char fullPortName[32];
    snprintf(fullPortName, sizeof(fullPortName), "\\\\.\\%s", portName);

    HANDLE hSerial = CreateFileA(fullPortName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) return false;

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(dcb);
    if (GetCommState(hSerial, &dcb)) {
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        SetCommState(hSerial, &dcb);
    }

    Sleep(2000);

    DWORD bytesWritten = 0;
    DWORD len = (DWORD)strlen(data);
    bool ok = WriteFile(hSerial, data, len, &bytesWritten, NULL) && (bytesWritten == len);
    FlushFileBuffers(hSerial);
    CloseHandle(hSerial);
    return ok;
}