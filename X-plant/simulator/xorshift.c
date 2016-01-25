
#include "xorshift.h"

#ifdef __KERNEL__
#include <linux/random.h>
#else
#include <stdlib.h>
#include <time.h>
#endif

static u32 x, y, z, w;

static u32 get_seed(void)
{
#ifdef __KERNEL__
	u32 num;
	get_random_bytes(&num, sizeof(num));
	return num;
#else
	return time(NULL);
#endif
}

void xorshift_srand(void)
{
	x = get_seed();
	y = get_seed();
	w = get_seed();
	z = get_seed();
}

static u32 xorshift128(void)
{
	u32 t = x ^ (x << 11);
	x = y;
	y = z;
	z = w;
	return w = w ^ (w >> 19) ^ t ^ (t >> 8);
}

u32 xorshift_rand(void)
{
	return xorshift128();
}
