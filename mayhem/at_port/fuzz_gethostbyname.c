/* Resolve a fuzzer-chosen host name through the AT socket layer's three public lookup entry points. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <at_socket.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *name = malloc(size + 1);
    char buf[512];
    struct hostent ret, *result = NULL;
    struct addrinfo *res = NULL;
    int h_errnop = 0;

    if (name == NULL)
        return 0;
    memcpy(name, data, size);
    name[size] = '\0';

    at_gethostbyname(name);
    at_gethostbyname_r(name, &ret, buf, sizeof(buf), &result, &h_errnop);
    if (at_getaddrinfo(name, NULL, NULL, &res) == 0)
        at_freeaddrinfo(res);

    free(name);
    return 0;
}
