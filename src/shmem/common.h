#pragma once

// Alignment for all allocations (64-bit)
#define ALIGNMENT sizeof(void *)
// Round up to nearest multiple of ALIGNMENT
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// No block offset (used for empty lists)
#define NULL_OFFSET ((size_t)-1)
