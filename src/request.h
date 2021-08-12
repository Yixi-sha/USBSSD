#ifndef __REQUEST_USBSSD_H__
#define __REQUEST_USBSSD_H__

#include <linux/blkdev.h>
#include "allocator.h"

typedef struct Request_USBSSD_{
    struct request *req;
    unsigned int restDataCount;

    Inter_Allocator_USBSSD allocator;
}Request_USBSSD;

Request_USBSSD *allocate_Request_USBSSD(struct request *req);
void free_Request_USBSSD(Request_USBSSD*);

void init_Request_USBSSD(void);
void destory_Request_USBSSD(void);

#endif // !__REQUEST_USBSSD_H__
