/* Host stand-ins for the RT-Thread kernel services and the AT device that at_socket.c calls into. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <rthw.h>
#include <at_device.h>

void *rt_malloc(rt_size_t size) { return malloc(size); }
void *rt_calloc(rt_size_t count, rt_size_t size) { return calloc(count, size); }
void rt_free(void *ptr) { free(ptr); }
void *rt_memset(void *s, int c, rt_ubase_t n) { return memset(s, c, n); }
void *rt_memcpy(void *dst, const void *src, rt_ubase_t n) { return memcpy(dst, src, n); }
rt_int32_t rt_memcmp(const void *a, const void *b, rt_ubase_t n) { return memcmp(a, b, n); }

rt_int32_t rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

void rt_set_errno(rt_err_t error) { (void)error; }
rt_tick_t rt_tick_from_millisecond(rt_int32_t ms) { return (rt_tick_t)ms; }
rt_err_t rt_thread_delay(rt_tick_t tick) { (void)tick; return RT_EOK; }
rt_base_t rt_hw_interrupt_disable(void) { return 0; }
void rt_hw_interrupt_enable(rt_base_t level) { (void)level; }

/* Single-threaded harness: IPC objects are opaque non-NULL tokens. */
static int ipc_token;
rt_mutex_t rt_mutex_create(const char *name, rt_uint8_t flag) { (void)name; (void)flag; return (rt_mutex_t)&ipc_token; }
rt_err_t rt_mutex_delete(rt_mutex_t m) { (void)m; return RT_EOK; }
rt_err_t rt_mutex_take(rt_mutex_t m, rt_int32_t timeout) { (void)m; (void)timeout; return RT_EOK; }
rt_err_t rt_mutex_release(rt_mutex_t m) { (void)m; return RT_EOK; }
rt_sem_t rt_sem_create(const char *name, rt_uint32_t value, rt_uint8_t flag) { (void)name; (void)value; (void)flag; return (rt_sem_t)&ipc_token; }
rt_err_t rt_sem_delete(rt_sem_t s) { (void)s; return RT_EOK; }
rt_err_t rt_sem_take(rt_sem_t s, rt_int32_t timeout) { (void)s; (void)timeout; return RT_EOK; }
rt_err_t rt_sem_release(rt_sem_t s) { (void)s; return RT_EOK; }
rt_err_t rt_sem_control(rt_sem_t s, int cmd, void *arg) { (void)s; (void)cmd; (void)arg; return RT_EOK; }

/* One initialized AT device with its link up. Its modem resolves every host name to 10.0.0.1. */
static int fake_domain_resolve(const char *name, char ip[16])
{
    (void)name;
    memcpy(ip, "10.0.0.1", sizeof("10.0.0.1"));
    return 0;
}

static const struct at_socket_ops fake_socket_ops = { .at_domain_resolve = fake_domain_resolve };
static struct at_device_class fake_class = { .socket_ops = &fake_socket_ops };
static struct netdev fake_netdev = { .flags = NETDEV_FLAG_UP | NETDEV_FLAG_LINK_UP };
static struct at_device fake_device = { .netdev = &fake_netdev, .class = &fake_class };

struct netdev *netdev_default = &fake_netdev;
int netdev_family_get(struct netdev *netdev) { (void)netdev; return AF_AT; }
struct netdev *netdev_get_by_family(int family) { (void)family; return &fake_netdev; }
struct at_device *at_device_get_first_initialized(void) { return &fake_device; }
struct at_device *at_device_get_by_name(int type, const char *name) { (void)type; (void)name; return &fake_device; }
struct at_device *at_device_get_by_ipaddr(ip_addr_t *ip_addr) { (void)ip_addr; return &fake_device; }
