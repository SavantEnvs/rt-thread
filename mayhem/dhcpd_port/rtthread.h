#ifndef MAYHEM_DHCPD_RTTHREAD_H
#define MAYHEM_DHCPD_RTTHREAD_H

/* The few RT-Thread kernel names components/net/lwip-dhcpd/dhcp_server.c uses, mapped to libc. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef long rt_err_t;
typedef unsigned long rt_size_t;
typedef uint32_t rt_uint32_t;
typedef struct rt_thread *rt_thread_t;

#define RT_NULL                 ((void *)0)
#define RT_EOK                  0
#define RT_ENOMEM               12
#define RT_THREAD_PRIORITY_MAX  32
#define RT_ASSERT(x)            do { if (!(x)) abort(); } while (0)

#define rt_memset   memset
#define rt_memcmp   memcmp
#define rt_free     free
#define rt_kprintf  printf

/* Zeroed, so the bytes past the received packet are the same on every run. */
void *rt_malloc(rt_size_t size);

rt_thread_t rt_thread_create(const char *name, void (*entry)(void *parameter), void *parameter,
                             rt_uint32_t stack_size, uint8_t priority, uint32_t tick);
rt_err_t rt_thread_startup(rt_thread_t thread);

#endif
