#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdbool.h>
#include "localip_win.h"

bool GetLocalIP(char *out, size_t outLen) {
    // UDP "connect" sends no packet; it just lets Windows pick the adapter
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s != INVALID_SOCKET) {
        struct sockaddr_in remote;
        memset(&remote, 0, sizeof(remote));
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);
        inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);

        if (connect(s, (struct sockaddr *)&remote, sizeof(remote)) == 0) {
            struct sockaddr_in local;
            int len = sizeof(local);
            if (getsockname(s, (struct sockaddr *)&local, &len) == 0 &&
                local.sin_addr.s_addr != 0 &&
                inet_ntop(AF_INET, &local.sin_addr, out, (socklen_t)outLen)) {
                closesocket(s);
                return true;
            }
        }
        closesocket(s);
    }

    // Fallback: resolve own hostname
    char host[256];
    if (gethostname(host, sizeof(host)) == 0) {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, NULL, &hints, &res) == 0 && res) {
            struct sockaddr_in *a = (struct sockaddr_in *)res->ai_addr;
            bool ok = inet_ntop(AF_INET, &a->sin_addr, out, (socklen_t)outLen) != NULL;
            freeaddrinfo(res);
            return ok;
        }
    }
    return false;
}
