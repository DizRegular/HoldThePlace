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
    SendInput(2, inputs, sizeof(INPUT));
}