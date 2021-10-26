#ifndef __MEMCHECK_USBSSD_H__
#define __MEMCHECK_USBSSD_H__

#include <linux/slab.h>

void *kmalloc_wrap(size_t size, gfp_t f);
void kfree_wrap(void* p);
int get_kmalloc_count(void);

#endif
