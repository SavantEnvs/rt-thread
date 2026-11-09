/*
 * dhcpd_fuzz: feed one UDP datagram to RT-Thread's DHCP server (components/net/lwip-dhcpd/dhcp_server.c).
 *
 * The server is entered through its public API, dhcpd_start(). Its worker thread reads the datagram with
 * recvfrom() on a UDP socket bound to port 67; this file stands in for the RTOS and the socket layer:
 * rt_thread_startup() runs the worker inline, and recvfrom() hands it the input once, then reports the
 * socket closed so the worker returns. Everything above the socket (the DHCP parsing and the reply
 * built into the same receive buffer) is the unmodified upstream code. Reply frames go to a netif whose
 * linkoutput is a sink; with -DDHCPD_KAT they are printed in hex, which is what mayhem/test.sh checks.
 *
 * Usage: dhcpd_fuzz <file>...   (one datagram per file; the fuzz target is given exactly one)
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <lwip/opt.h>
#include <lwip/sockets.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <netif/etharp.h>

#define MAX_DATAGRAM 2048

static uint8_t g_datagram[MAX_DATAGRAM];
static size_t g_datagram_len;
static int g_delivered;

static void (*g_entry)(void *);
static void *g_entry_arg;

struct netif *netif_list;
const struct eth_addr ethbroadcast = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

void *rt_malloc(rt_size_t size)
{
    return calloc(1, size);
}

rt_thread_t rt_thread_create(const char *name, void (*entry)(void *parameter), void *parameter,
                             rt_uint32_t stack_size, uint8_t priority, uint32_t tick)
{
    (void)name; (void)stack_size; (void)priority; (void)tick;
    g_entry = entry;
    g_entry_arg = parameter;
    return (rt_thread_t)&g_entry;
}

rt_err_t rt_thread_startup(rt_thread_t thread)
{
    (void)thread;
    g_entry(g_entry_arg);
    return RT_EOK;
}

void set_if(const char *netif_name, const char *ip_addr, const char *gw_addr, const char *nm_addr)
{
    (void)netif_name; (void)ip_addr; (void)gw_addr; (void)nm_addr;
}

void netif_set_up(struct netif *netif)
{
    (void)netif;
}

void dhcp_stop(struct netif *netif)
{
    (void)netif;
}

struct pbuf *pbuf_alloc(pbuf_layer layer, u16_t length, pbuf_type type)
{
    struct pbuf *p = calloc(1, sizeof(*p));
    (void)layer; (void)type;
    p->payload = calloc(1, length ? length : 1);
    p->len = p->tot_len = length;
    return p;
}

u8_t pbuf_free(struct pbuf *p)
{
    free(p->payload);
    free(p);
    return 1;
}

u16_t inet_chksum(const void *dataptr, u16_t len)
{
    const uint8_t *d = dataptr;
    uint32_t sum = 0;
    u16_t i;
    for (i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)(d[i] << 8 | d[i + 1]);
    if (len & 1)
        sum += (uint32_t)d[len - 1] << 8;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return lwip_htons((u16_t)~sum);
}

int lwip_socket(int domain, int type, int protocol)
{
    (void)domain; (void)type; (void)protocol;
    return 3;
}

int lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    (void)s; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    (void)s; (void)name; (void)namelen;
    return 0;
}

int lwip_close(int s)
{
    (void)s;
    return 0;
}

ssize_t lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    size_t n;
    (void)s; (void)flags; (void)from; (void)fromlen;
    if (g_delivered)
        return 0;
    g_delivered = 1;
    n = g_datagram_len < len ? g_datagram_len : len;
    memcpy(mem, g_datagram, n);
    return (ssize_t)n;
}

static err_t sink_linkoutput(struct netif *netif, struct pbuf *p)
{
    (void)netif;
#ifdef DHCPD_KAT
    {
        const uint8_t *b = p->payload;
        u16_t i;
        printf("TX %u:", (unsigned)p->len);
        for (i = 0; i < p->len; i++)
            printf("%02x", b[i]);
        printf("\n");
    }
#else
    (void)p;
#endif
    return ERR_OK;
}

extern void dhcpd_start(const char *netif_name);

int main(int argc, char **argv)
{
    static struct netif nif;
    int i;

    nif.name[0] = 'e';
    nif.name[1] = '0';
    nif.hwaddr_len = 6;
    memcpy(nif.hwaddr, "\x02\x00\x00\x00\x00\x01", 6);
    nif.linkoutput = sink_linkoutput;
    netif_list = &nif;

    for (i = 1; i < argc; i++)
    {
        FILE *f = fopen(argv[i], "rb");
        if (f == NULL)
        {
            perror(argv[i]);
            return 1;
        }
        g_datagram_len = fread(g_datagram, 1, sizeof(g_datagram), f);
        fclose(f);
        g_delivered = 0;
#ifdef DHCPD_KAT
        printf("PACKET %s\n", argv[i]);
#endif
        dhcpd_start("e0");
    }
    return 0;
}
