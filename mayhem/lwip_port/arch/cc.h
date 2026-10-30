#ifndef MAYHEM_LWIP_ARCH_CC_H
#define MAYHEM_LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pools/heap hold pointers: align like the lwIP unix port does on LP64. */
#define MEM_ALIGNMENT 8

/* glibc rand() is unseeded here, so it is reproducible run to run. */
#define LWIP_RAND() ((u32_t)rand())

#endif
