/*
 * Known-answer probe for the lwIP 2.2.1 copy shipped in RT-Thread (components/net/lwip/lwip-2.2.1).
 * Feeds fixed Ethernet frames into a NO_SYS lwIP netif (same options as test/fuzz) and prints every
 * frame the stack transmits in response, as hex. mayhem/test.sh diffs the output against
 * mayhem/kat/expected.txt, so any change in how lwIP answers ARP / ICMP / UDP / TCP / ND breaks it.
 */
#include <stdio.h>
#include <string.h>

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/ip_addr.h"
#include "lwip/ethip6.h"
#include "lwip/nd6.h"
#include "lwip/dns.h"
#include "netif/etharp.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/snmp.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/mdns.h"

u32_t sys_now_offset;

u32_t lwip_fuzz_rand(void)
{
  static u32_t state = 0xdeadbeef;
  state = (u32_t)((((unsigned long long)state * 1103515245ULL) + 12345ULL) & 0x7fffffffULL);
  return state;
}

static const u8_t host_mac[6] = {0x28, 0x00, 0x00, 0x22, 0x2b, 0x38};
static const u8_t our_mac[6]  = {0x00, 0x23, 0xC1, 0xDE, 0xD0, 0x0D};
static const u8_t host_ip[4]  = {172, 30, 115, 37};
static const u8_t our_ip[4]   = {172, 30, 115, 84};

static int tx_count;

static err_t tx_func(struct netif *netif, struct pbuf *p)
{
  u8_t buf[2048];
  u16_t i;
  (void)netif;
  pbuf_copy_partial(p, buf, sizeof(buf), 0);
  printf("TX %d len=%u ", ++tx_count, (unsigned)p->tot_len);
  for (i = 0; i < p->tot_len && i < sizeof(buf); i++) {
    printf("%02x", buf[i]);
  }
  printf("\n");
  return ERR_OK;
}

static err_t testif_init(struct netif *netif)
{
  netif->name[0] = 'f';
  netif->name[1] = 'z';
  netif->output = etharp_output;
  netif->linkoutput = tx_func;
  netif->mtu = 1500;
  netif->hwaddr_len = 6;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
  memcpy(netif->hwaddr, our_mac, 6);
  netif->output_ip6 = ethip6_output;
  netif_create_ip6_linklocal_address(netif, 1);
  netif->flags |= NETIF_FLAG_MLD6;
  return ERR_OK;
}

static u16_t csum(const u8_t *d, size_t n, u32_t sum)
{
  size_t i;
  for (i = 0; i + 1 < n; i += 2) {
    sum += (u32_t)((d[i] << 8) | d[i + 1]);
  }
  if (n & 1) {
    sum += (u32_t)(d[n - 1] << 8);
  }
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }
  return (u16_t)~sum;
}

static void feed(struct netif *netif, const u8_t *frame, size_t len)
{
  struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
  struct pbuf *q;
  if (p == NULL) {
    printf("FEED alloc failed\n");
    return;
  }
  for (q = p; q != NULL; q = q->next) {
    memcpy(q->payload, frame, q->len);
    frame += q->len;
  }
  if (netif->input(p, netif) != ERR_OK) {
    pbuf_free(p);
  }
  sys_check_timeouts();
}

static size_t eth_hdr(u8_t *f, const u8_t *dst, u16_t type)
{
  memcpy(f, dst, 6);
  memcpy(f + 6, host_mac, 6);
  f[12] = (u8_t)(type >> 8);
  f[13] = (u8_t)type;
  return 14;
}

/* Builds Ethernet+IPv4 header for a payload of paylen bytes; returns header length (34). */
static size_t ip4_hdr(u8_t *f, u8_t proto, size_t paylen)
{
  u8_t *ip = f + 14;
  size_t total = 20 + paylen;
  eth_hdr(f, our_mac, 0x0800);
  ip[0] = 0x45; ip[1] = 0;
  ip[2] = (u8_t)(total >> 8); ip[3] = (u8_t)total;
  ip[4] = 0x12; ip[5] = 0x34; ip[6] = 0x40; ip[7] = 0;
  ip[8] = 64; ip[9] = proto; ip[10] = ip[11] = 0;
  memcpy(ip + 12, host_ip, 4);
  memcpy(ip + 16, our_ip, 4);
  {
    u16_t c = csum(ip, 20, 0);
    ip[10] = (u8_t)(c >> 8); ip[11] = (u8_t)c;
  }
  return 34;
}

int main(void)
{
  struct netif nif;
  ip4_addr_t addr, mask, gw;
  u8_t f[256];
  size_t n;
  static const u8_t bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  lwip_init();
  IP4_ADDR(&addr, 172, 30, 115, 84);
  IP4_ADDR(&mask, 255, 255, 255, 0);
  IP4_ADDR(&gw, 172, 30, 115, 1);
  netif_add(&nif, &addr, &mask, &gw, &nif, testif_init, ethernet_input);
  netif_set_up(&nif);
  netif_set_link_up(&nif);
  nd6_tmr();
  dns_setserver(0, &nif.gw);
  httpd_init();
  lwiperf_start_tcp_server_default(NULL, NULL);
  mdns_resp_init();
  mdns_resp_add_netif(&nif, "hostname");
  snmp_init();
  tx_count = 0;
  printf("SETUP done\n");

  /* A: ARP who-has 172.30.115.84 tell 172.30.115.37 -> ARP reply to the requester */
  printf("STEP arp-request\n");
  n = eth_hdr(f, bcast, 0x0806);
  {
    static const u8_t hdr[8] = {0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01};
    memcpy(f + n, hdr, 8); n += 8;
    memcpy(f + n, host_mac, 6); n += 6;
    memcpy(f + n, host_ip, 4); n += 4;
    memset(f + n, 0, 6); n += 6;
    memcpy(f + n, our_ip, 4); n += 4;
  }
  feed(&nif, f, n);

  /* B: ICMP echo request -> echo reply (ARP entry for the host was learned in step A) */
  printf("STEP icmp-echo\n");
  {
    static const u8_t payload[12] = {0x08, 0x00, 0x00, 0x00, 0x12, 0x34, 0x00, 0x01, 'm', 'a', 'y', 'h'};
    u16_t c;
    n = ip4_hdr(f, 1, sizeof(payload));
    memcpy(f + n, payload, sizeof(payload));
    c = csum(f + n, sizeof(payload), 0);
    f[n + 2] = (u8_t)(c >> 8); f[n + 3] = (u8_t)c;
    n += sizeof(payload);
    feed(&nif, f, n);
  }

  /* C: UDP datagram to a closed port -> ICMP port unreachable */
  printf("STEP udp-closed-port\n");
  {
    static const u8_t udp[12] = {0x30, 0x39, 0x00, 0x09, 0x00, 0x0c, 0x00, 0x00, 'k', 'a', 't', '!'};
    n = ip4_hdr(f, 17, sizeof(udp));
    memcpy(f + n, udp, sizeof(udp));
    n += sizeof(udp);
    feed(&nif, f, n);
  }

  /* D: TCP SYN to port 80 (httpd) -> SYN+ACK */
  printf("STEP tcp-syn\n");
  {
    static const u8_t tcp[24] = {0x9c, 0x40, 0x00, 0x50, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x60, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x05, 0xb4};
    n = ip4_hdr(f, 6, sizeof(tcp));
    memcpy(f + n, tcp, sizeof(tcp));
    n += sizeof(tcp);
    feed(&nif, f, n);
  }

  /* E: IPv6 neighbor solicitation for our link-local address -> neighbor advertisement */
  printf("STEP ipv6-ns\n");
  {
    static const u8_t ns[] = {
      0x60, 0x00, 0x00, 0x00, 0x00, 0x20, 0x3a, 0xff,
      0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x2a, 0x00, 0x00, 0xff, 0xfe, 0x22, 0x2b, 0x38,
      0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x23, 0xc1, 0xff, 0xfe, 0xde, 0xd0, 0x0d,
      0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x23, 0xc1, 0xff, 0xfe, 0xde, 0xd0, 0x0d,
      0x01, 0x01, 0x28, 0x00, 0x00, 0x22, 0x2b, 0x38};
    static const u8_t our6_mac[6] = {0x00, 0x23, 0xC1, 0xDE, 0xD0, 0x0D};
    n = eth_hdr(f, our6_mac, 0x86dd);
    memcpy(f + n, ns, sizeof(ns));
    n += sizeof(ns);
    feed(&nif, f, n);
  }

  printf("TOTAL tx=%d\n", tx_count);
  return 0;
}
