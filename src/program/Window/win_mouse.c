#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include "win_mouse.h"

bool net_init(NetSocket *sock, int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        WSACleanup();
        return false;
    }

    u_long non_blocking = 1;
    ioctlsocket(s, FIONBIO, &non_blocking);

    *sock = (NetSocket)s;
    return true;
}

int net_receive(NetSocket sock, char *buffer, int max_len) {
    struct sockaddr_in client;
    int client_len = sizeof(client);
    int received = recvfrom((SOCKET)sock, buffer, max_len - 1, 0, (struct sockaddr *)&client, &client_len);
    if (received > 0) {
        buffer[received] = '\0';
        return received;
    }
    return -1;
}

void net_close(NetSocket sock) {
    closesocket((SOCKET)sock);
    WSACleanup();
}

// Runs "netsh advfirewall ... add rule" elevated. shell32 is loaded at runtime
// so no extra linker flag (-lshell32) is needed.
bool open_firewall_udp(int port) {
    typedef HINSTANCE (WINAPI *ShellExecuteAFn)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
    HMODULE shell = LoadLibraryA("shell32.dll");
    if (!shell) return false;
    ShellExecuteAFn pShellExecuteA = (ShellExecuteAFn)GetProcAddress(shell, "ShellExecuteA");
    if (!pShellExecuteA) { FreeLibrary(shell); return false; }

    char args[256];
    snprintf(args, sizeof(args),
             "advfirewall firewall add rule name=\"AirMouse UDP %d\" dir=in action=allow "
             "protocol=UDP localport=%d profile=any",
             port, port);

    HINSTANCE r = pShellExecuteA(NULL, "runas", "netsh.exe", args, NULL, SW_HIDE);
    FreeLibrary(shell);
    return (INT_PTR)r > 32;
}

void mouse_move_relative(int dx, int dy) {
    POINT pt;
    if (GetCursorPos(&pt)) {
        SetCursorPos(pt.x + dx, pt.y + dy);
    }
}

void mouse_left_click(){
    INPUT inputs[2] = {0};

    // --- Mouse Down Event ---
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    // --- Mouse Up Event ---
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    // 3. Send the input events to the operating system
    // (Size of array, pointer to the array, size of the struct element)
    // Send down and up separately with a short gap; some apps ignore 0 ms clicks
    SendInput(1, &inputs[0], sizeof(INPUT));
    Sleep(15);
    SendInput(1, &inputs[1], sizeof(INPUT));
}