#ifndef __ALLOCATOR_USBSSD_H__
#define __ALLOCATOR_USBSSD_H__

#include <linux/mutex.h>

typedef struct Inter_Allocator_USBSSD_{
    struct Inter_Allocator_USBSSD_ *pre;
    struct Inter_Allocator_USBSSD_ *next;
}Inter_Allocator_USBSSD;

typedef struct Allocator_USBSSD_{
    unsigned int allSize;
    unsigned int offset;

    struct mutex freeMutex;
    struct mutex usedMutex;

    Inter_Allocator_USBSSD *freeHead;
    Inter_Allocator_USBSSD *usedHead;
}Allocator_USBSSD;


void *allocate_USBSSD(Allocator_USBSSD*);
void free_USBSSD(Allocator_USBSSD*, void*);

Allocator_USBSSD* init_allocator_USBSSD(int allsize, int offset);
void destory_allocator_USBSSD(Allocator_USBSSD*);

#endif