#ifndef __SUB_REQUEST_USBSSD_H__
#define __SUB_REQUEST_USBSSD_H__
/*for bevc*/

#include "request.h"
#include "allocator.h"
#include <linux/bio.h>

#define READ_USBSSD 0
#define WRITE_USBSSD 0

#define SUB_PAGE_SIZE_USBSSD PAGE_SIZE // actual map granularity PAGE_SIZE
#define NUM_DEST 2 // SUB_PAGE_SIZE_USBSSD = PAGE_SIZE, so NUM_DEST max is 2

typedef struct SubRequest_USBSSD_{
    Request_USBSSD *req;

    unsigned char operation;
    unsigned long long lpn;

    int start; // for page of ssd
    int len;   // for page of ssd

    /*for bvec callvack*/
    unsigned long offset; // for  offset of page in operatin system
    void *page;

    struct SubRequest_USBSSD_ *pre;
    struct SubRequest_USBSSD_ *next;
    /*for allocter*/
    Inter_Allocator_USBSSD allocator;
}SubRequest_USBSSD;

SubRequest_USBSSD *allocate_SubRequest_USBSSD(Request_USBSSD *req, struct bio_vec *bvec, sector_t sector, unsigned char operation);
void free_SubRequest_USBSSD(SubRequest_USBSSD*);

void add_To_List(SubRequest_USBSSD*, SubRequest_USBSSD*);

void init_SubRequest_USBSSD(void);
void destory_SubRequest_USBSSD(void);

#endif