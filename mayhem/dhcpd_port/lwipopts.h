#ifndef MAYHEM_DHCPD_LWIPOPTS_H
#define MAYHEM_DHCPD_LWIPOPTS_H

/* RT-Thread's lwIP runs with the socket API on (NO_SYS=0); dhcp_server.c needs lwip/sockets.h. */
#define NO_SYS                  0
#define LWIP_SOCKET             1
#define LWIP_NETCONN            1
#define SYS_LIGHTWEIGHT_PROT    0
#define LWIP_DHCP               1
#define LWIP_IPV6               0
/* The host's libc already defines timeval/fd_set. */
#define LWIP_TIMEVAL_PRIVATE    0
#define LWIP_SOCKET_SELECT      0
#define LWIP_SOCKET_POLL        0

#endif
