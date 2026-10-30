#include "lwip/sys.h"

#ifdef LWIP_FUZZ_SYS_NOW
extern u32_t sys_now_offset;
#else
static const u32_t sys_now_offset = 0;
#endif

/* NO_SYS=1 time source: deterministic, driven only by the fuzzed delays. */
u32_t sys_now(void)
{
  return sys_now_offset;
}
