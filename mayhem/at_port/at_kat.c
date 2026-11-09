/* Known-answer probe for the AT socket layer's host name lookup (normal flags, not sanitized). */
#include <stdio.h>
#include <string.h>

#include <rtthread.h>
#include <at_socket.h>

static void lookup(const char *name)
{
    struct hostent *h = at_gethostbyname(name);
    if (h == NULL)
    {
        printf("gethostbyname \"%s\" -> not found\n", name);
        return;
    }
    const unsigned char *a = (const unsigned char *)h->h_addr_list[0];
    printf("gethostbyname \"%s\" -> %s %u.%u.%u.%u\n", name, h->h_name, a[0], a[1], a[2], a[3]);
}

static void lookup_r(const char *name)
{
    char buf[512];
    struct hostent ret, *result = NULL;
    int err = 0;
    int rc = at_gethostbyname_r(name, &ret, buf, sizeof(buf), &result, &err);
    if (rc != 0 || result == NULL)
    {
        printf("gethostbyname_r \"%s\" -> failed\n", name);
        return;
    }
    const unsigned char *a = (const unsigned char *)result->h_addr_list[0];
    printf("gethostbyname_r \"%s\" -> %s %u.%u.%u.%u\n", name, result->h_name, a[0], a[1], a[2], a[3]);
}

static void info(const char *name)
{
    struct addrinfo *res = NULL;
    int rc = at_getaddrinfo(name, "80", NULL, &res);
    if (rc != 0)
    {
        printf("getaddrinfo \"%s\" -> error\n", name);
        return;
    }
    const unsigned char *a = (const unsigned char *)&((struct sockaddr_in *)res->ai_addr)->sin_addr;
    printf("getaddrinfo \"%s\" -> %s %u.%u.%u.%u\n", name, res->ai_canonname, a[0], a[1], a[2], a[3]);
    at_freeaddrinfo(res);
}

int main(void)
{
    static const char *const names[] = {
        "192.168.1.10", "255.255.255.255", "10.0.0.1", "0.0.0.0", "example.com", "a", "1.2.3", "", "256.1.1.1",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        lookup(names[i]);
        lookup_r(names[i]);
        info(names[i]);
    }
    return 0;
}
