#ifndef __SUB_REQUEST_USBSSD_H__
#define __SUB_REQUEST_USBSSD_H__

#include "request.h"
#include "allocator.h"

#define PAGE_SIZE_USBSSD 4096

typedef struct Dest_Sub_USBSSD_{
    char *dest;
    int start;
    int len;
}Dest_Sub_USBSSD;

typedef struct SubRequest_USBSSD_{
    struct Request_USBSSD *req;
    unsigned int restDataCount;

    Dest_Sub_USBSSD dest[(PAGE_SIZE_USBSSD/PAGE_SIZE) + 1];
    /*for allocter*/
    Inter_Allocator_USBSSD allocator;
}SubRequest_USBSSD;

SubRequest_USBSSD *allocate_SubRequest_USBSSD(void);
void free_SubRequest_USBSSD(SubRequest_USBSSD*);

void init_SubRequest_USBSSD(void);
void destory_SubRequest_USBSSD(void);

#endif