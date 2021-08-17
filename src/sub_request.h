#ifndef __SUB_REQUEST_USBSSD_H__
#define __SUB_REQUEST_USBSSD_H__
/*for bevc*/

#include "request.h"
#include "allocator.h"
#include <linux/bio.h>
#include <linux/timer.h>

#define READ_USBSSD 0
#define WRITE_USBSSD 1

#define SUB_PAGE_SIZE_USBSSD PAGE_SIZE // actual map granularity PAGE_SIZE

typedef struct SubPageBuf_USBSSD_{
    unsigned char buf[SUB_PAGE_SIZE_USBSSD];
    Inter_Allocator_USBSSD allocator;
}SubPageBuf_USBSSD;

typedef struct SubRequest_USBSSD_{
    Request_USBSSD *req;

    unsigned char operation;
    unsigned long long lpn;

    int start; // for page of ssd
    int len;   // for page of ssd

    unsigned long offset; // for  offset of page in operatin system
    void *page;

    unsigned long jiffies;
    
    void *update; // for write
    int updateLen;
    struct SubRequest_USBSSD_ *updateSub; // for read

    struct SubRequest_USBSSD_ *pre;
    struct SubRequest_USBSSD_ *next;
    /*for allocter*/
    Inter_Allocator_USBSSD allocator;
}SubRequest_USBSSD;

SubRequest_USBSSD *allocate_SubRequest_USBSSD(Request_USBSSD *req, struct bio_vec *bvec, sector_t sector, unsigned char operation);
void free_SubRequest_USBSSD(SubRequest_USBSSD*);

void add_To_List_SubRequest_USBSSD(SubRequest_USBSSD*, SubRequest_USBSSD*);
void remove_From_List_SubRequest_USBSSD(SubRequest_USBSSD*);

SubRequest_USBSSD *get_SubRequests_USBSSD(int chann, int chip, int operation);

void init_SubRequest_USBSSD(void);
void destory_SubRequest_USBSSD(void);

#endif