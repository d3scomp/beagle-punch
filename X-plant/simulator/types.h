
#ifndef TYPES_H_
#define TYPES_H_

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/atomic.h>

#else

typedef int atomic_t;
static inline void atomic_add(int val, atomic_t * ptr) { *ptr += val; }
static inline void atomic_sub(int val, atomic_t * ptr) { *ptr -= val; }
static inline int atomic_read(atomic_t * ptr) { return *ptr; }

#include <stdint.h>
typedef int8_t s8;
typedef uint8_t u8;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

#endif

#endif
