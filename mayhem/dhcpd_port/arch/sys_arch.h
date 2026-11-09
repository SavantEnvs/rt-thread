#ifndef MAYHEM_DHCPD_ARCH_SYS_ARCH_H
#define MAYHEM_DHCPD_ARCH_SYS_ARCH_H

/* The harness never touches lwIP's OS layer: these types only let lwip/sockets.h compile (NO_SYS=0). */
typedef int sys_sem_t;
typedef int sys_mutex_t;
typedef int sys_mbox_t;
typedef int sys_thread_t;

#endif
