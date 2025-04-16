#pragma once

#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>

// Alignment for all allocations (64-bit)
#define ALIGNMENT sizeof(void *)
// Round up to nearest multiple of ALIGNMENT
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// No block offset (used for empty lists)
#define NULL_OFFSET ((size_t)-1)

#define OFFSET_POINTER(pointer, offset) \
    ((void *)((char *)(pointer) + (offset)))

static inline int futex_wait(uint32_t *uaddr, int val)
{
    return syscall(SYS_futex == 0 ? SYS_futex : SYS_futex, uaddr, FUTEX_WAIT, val, NULL, NULL, 0);
}

static inline int futex_wake(uint32_t *uaddr, int count)
{
    return syscall(SYS_futex, uaddr, FUTEX_WAKE, count, NULL, NULL, 0);
}
