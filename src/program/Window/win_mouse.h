#ifndef WIN_MOUSE_H
#define WIN_MOUSE_H

#include <stdbool.h>

// Forward-declare the socket descriptor so mainWin.c doesn't need winsock2.h
typedef unsigned long long NetSocket; 

bool net_init(NetSocket *sock, int port);
int  net_receive(NetSocket sock, char *buffer, int max_len);
void net_close(NetSocket sock);

void mouse_move_relative(int dx, int dy);
void mouse_left_click();

// Asks Windows (UAC prompt) to add an inbound firewall rule for UDP `port`.
// Returns true if the prompt was launched (not a guarantee the user accepted).
bool open_firewall_udp(int port);


#endif