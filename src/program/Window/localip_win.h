#ifndef LOCALIP_WIN_H
#define LOCALIP_WIN_H

#include <stdbool.h>
#include <stddef.h>

// Writes this PC's IPv4 address (e.g. "192.168.1.23") into out.
// Call after WSAStartup (net_init already does this).
bool GetLocalIP(char *out, size_t outLen);

#endif
