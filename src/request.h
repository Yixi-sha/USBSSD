#ifndef __REQUEST_USBSSD_H__
#define __REQUEST_USBSSD_H__

#include <linux/blkdev.h>
#include <linux/bio.h>
#include "allocator.h"

typedef struct Request_USBSSD_{
    struct bio *bio;
    unsigned int restDataCount;

    Inter_Allocator_USBSSD allocator;
}Request_USBSSD;

Request_USBSSD *allocate_Request_USBSSD(struct bio *bio);
void free_Request_USBSSD(Request_USBSSD*);

int init_Request_USBSSD(void);
void destory_Request_USBSSD(void);

void boost_test_requsts(void);


#endif // !__REQUEST_USBSSD_H__
