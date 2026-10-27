#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include "serial_win.h"

#define SERIAL_BAUD  CBR_9600   // matches Serial.begin(9600); UNO R4 USB ignores it anyway
#define CHUNK_BYTES  16         // small pieces so tiny RX buffers don't overflow
#define REPLY_MAX    512
#define LISTEN_MS    12000      // max time to wait for "Connected!" / "failed"

static DWORD g_lastError = 0;
static DWORD g_readErr = 0;
static char  g_reply[REPLY_MAX] = "";
static bool  g_ok = false;
static volatile LONG g_busy = 0;
static char  g_portArg[32];
static char  g_dataArg[256];

unsigned long SerialLastError(void) { return g_lastError; }
const char *SerialLastReply(void) { return g_reply; }
unsigned long SerialLastReadError(void) { return g_readErr; }
bool SerialLastOk(void) { return g_ok; }
bool SerialBusy(void) { return g_busy != 0; }

// Keeps only lines that contain a letter (status text); pure number lines are gyro data.
static void commit_line(const char *line, size_t *total, DWORD *doneAt) {
    bool hasAlpha = false;
    for (const char *p = line; *p; p++) {
        if (isalpha((unsigned char)*p)) { hasAlpha = true; break; }
    }
    if (!hasAlpha) return;

    size_t n = strlen(line);
    if (*total + n + 2 < REPLY_MAX) {
        if (*total) g_reply[(*total)++] = ' ';
        memcpy(g_reply + *total, line, n);
        *total += n;
        g_reply[*total] = '\0';
    }

    if (*doneAt == 0) {
        if (strstr(line, "Connected!"))
            *doneAt = GetTickCount() + 700;          // let "Device IP: ..." arrive
        else if (strstr(line, "failed") || strstr(line, "ERROR"))
            *doneAt = GetTickCount() + 100;
    }
}

static bool send_blocking(const char *portName, const char *data) {
    g_lastError = 0;
    g_readErr = 0;
    g_reply[0] = '\0';

    char fullPortName[64];
    snprintf(fullPortName, sizeof(fullPortName), "\\\\.\\%s", portName);

    HANDLE h = CreateFileA(fullPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        g_lastError = GetLastError();
        return false;
    }

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        g_lastError = GetLastError();
        CloseHandle(h);
        return false;
    }
    dcb.BaudRate = SERIAL_BAUD;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;   // UNO R4 (native USB) only talks while DTR is on
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(h, &dcb)) {
        g_lastError = GetLastError();
        CloseHandle(h);
        return false;
    }

    COMMTIMEOUTS to = { 0 };
    to.ReadIntervalTimeout = MAXDWORD;       // ReadFile returns immediately with whatever is there
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = 0;
    to.WriteTotalTimeoutConstant = 3000;
    to.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &to);

    Sleep(300);                              // let the USB port settle
    PurgeComm(h, PURGE_TXCLEAR | PURGE_RXCLEAR);

    // ---- write once ----
    DWORD len = (DWORD)strlen(data);
    for (DWORD off = 0; off < len; off += CHUNK_BYTES) {
        DWORD n = (len - off < CHUNK_BYTES) ? (len - off) : CHUNK_BYTES;
        DWORD written = 0;
        if (!WriteFile(h, data + off, n, &written, NULL) || written != n) {
            g_lastError = GetLastError();
            CloseHandle(h);
            return false;
        }
        Sleep(15);
    }
    FlushFileBuffers(h);

    // ---- listen for the board's status lines ----
    char line[200];
    size_t ll = 0, total = 0;
    DWORD doneAt = 0;
    DWORD start = GetTickCount();
    int readErrors = 0;

    while (GetTickCount() - start < LISTEN_MS) {
        if (doneAt && GetTickCount() >= doneAt) break;

        char tmp[128];
        DWORD got = 0;
        if (!ReadFile(h, tmp, sizeof(tmp), &got, NULL)) {
            g_readErr = GetLastError();
            DWORD e; COMSTAT st;
            ClearCommError(h, &e, &st);
            if (++readErrors > 20) break;
            Sleep(20);
            continue;
        }
        if (got == 0) { Sleep(10); continue; }

        for (DWORD i = 0; i < got; i++) {
            unsigned char c = (unsigned char)tmp[i];
            if (c == '\n' || c == '\r') {
                if (ll > 0) {
                    line[ll] = '\0';
                    commit_line(line, &total, &doneAt);
                    ll = 0;
                }
            } else if (ll < sizeof(line) - 1) {
                line[ll++] = (c < 32 || c > 126) ? '?' : (char)c;
            }
        }
    }

    CloseHandle(h);
    return true;
}

bool SendSerialData(const char *portName, const char *data) {
    return send_blocking(portName, data);
}

static DWORD WINAPI send_thread(LPVOID arg) {
    (void)arg;
    g_ok = send_blocking(g_portArg, g_dataArg);
    InterlockedExchange(&g_busy, 0);
    return 0;
}

bool SerialSendStart(const char *portName, const char *data) {
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return false;

    snprintf(g_portArg, sizeof(g_portArg), "%s", portName);
    snprintf(g_dataArg, sizeof(g_dataArg), "%s", data);
    g_ok = false;
    g_lastError = 0;
    g_readErr = 0;
    g_reply[0] = '\0';

    HANDLE t = CreateThread(NULL, 0, send_thread, NULL, 0, NULL);
    if (!t) {
        InterlockedExchange(&g_busy, 0);
        return false;
    }
    CloseHandle(t);
    return true;
}

// Writes every non-Bluetooth COM port found, e.g. "COM3 COM7 COM12", into out.
void ListComPorts(char *out, int outLen) {
    out[0] = '\0';
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return;
    for (DWORD i = 0; ; i++) {
        char name[256], val[64];
        DWORD nameLen = sizeof(name), valLen = sizeof(val) - 1, type = 0;
        LONG r = RegEnumValueA(key, i, name, &nameLen, NULL, &type, (LPBYTE)val, &valLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || type != REG_SZ) continue;
        val[valLen] = '\0';
        if (strstr(name, "Bth") || strstr(name, "BTH")) continue;
        if (strncmp(val, "COM", 3) != 0) continue;
        size_t used = strlen(out);
        if (used + strlen(val) + 2 >= (size_t)outLen) break;
        if (used) strcat(out, " ");
        strcat(out, val);
    }
    RegCloseKey(key);
}

// Picks a USB-serial port first (Arduino / CH340 / CP210x / FTDI), else the highest COM number.
bool FindComPort(char *out, int outLen) {
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;

    char found[32] = "";
    int bestScore = -1, bestNum = -1;
    for (DWORD i = 0; ; i++) {
        char name[256], val[64];
        DWORD nameLen = sizeof(name), valLen = sizeof(val) - 1, type = 0;
        LONG r = RegEnumValueA(key, i, name, &nameLen, NULL, &type, (LPBYTE)val, &valLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || type != REG_SZ) continue;
        val[valLen] = '\0';
        if (strstr(name, "Bth") || strstr(name, "BTH")) continue;   // skip Bluetooth
        if (strncmp(val, "COM", 3) != 0) continue;

        int num = atoi(val + 3);                                     // "COM12" -> 12
        int score = (strstr(name, "USBSER") || strstr(name, "Silabser") ||
                     strstr(name, "VCP") || strstr(name, "usbser")) ? 1 : 0;
        if (score > bestScore || (score == bestScore && num > bestNum)) {
            bestScore = score;
            bestNum = num;
            snprintf(found, sizeof(found), "%s", val);
        }
    }
    RegCloseKey(key);

    if (bestNum < 0) return false;
    snprintf(out, outLen, "%s", found);
    return true;
}