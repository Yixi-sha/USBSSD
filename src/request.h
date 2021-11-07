#ifndef __REQUEST_USBSSD_H__
#define __REQUEST_USBSSD_H__

#include <linux/blkdev.h>
#include <linux/bio.h>
#include "allocator.h"


typedef struct Request_USBSSD_{
    struct request *req;
    unsigned long long lsa;
    unsigned long long len;

    struct mutex subMutex;
    unsigned long long restSubreqCount;

    struct SubRequest_USBSSD_ *head;

    Inter_Allocator_USBSSD allocator;
}Request_USBSSD;

Request_USBSSD *allocate_Request_USBSSD(struct request *req);
void request_May_End(Request_USBSSD *req);
void free_Request_USBSSD(Request_USBSSD*);

int init_Request_USBSSD(void);
void destory_Request_USBSSD(void);

void boost_test_requsts(void);
void boost_test_signal_thread(void);


#endif // !__REQUEST_USBSSD_H__
