#ifndef __SUB_REQUEST_USBSSD_H__
#define __SUB_REQUEST_USBSSD_H__
/*for bevc*/

#include "request.h"
#include "allocator.h"
#include <linux/bio.h>
#include <linux/timer.h>


#define SUB_PAGE_SIZE_USBSSD 2048// actual map granularity PAGE_SIZE
#define SUB_PAGE_MASK 0xffffffffffffffffULL

typedef struct PPN_USBSSD_{
    int channel;
    int chip;
    int die;
    int plane;
    int block;
    int page;
}PPN_USBSSD;

typedef struct SubRequest_USBSSD_{
    Request_USBSSD *req;

    unsigned char operation;
    unsigned long long lpn; // the address

    unsigned long long bitMap;
    unsigned char buf[SUB_PAGE_SIZE_USBSSD];

    unsigned long jiffies;
    
    struct SubRequest_USBSSD_ *relatedSub;
    PPN_USBSSD location;

    struct SubRequest_USBSSD_ *pre;
    struct SubRequest_USBSSD_ *next;

    struct SubRequest_USBSSD_ *next_inter;
    /*for allocter*/
    Inter_Allocator_USBSSD allocator;
}SubRequest_USBSSD;

SubRequest_USBSSD *allocate_SubRequest_USBSSD(Request_USBSSD *req, unsigned long long lpn, unsigned long long bitMap, unsigned char operation);
void subRequest_End(SubRequest_USBSSD *sub);
void free_SubRequest_USBSSD(SubRequest_USBSSD*);

void add_To_List_SubRequest_USBSSD(SubRequest_USBSSD*, SubRequest_USBSSD*);

int init_SubRequest_USBSSD(void);
void destory_SubRequest_USBSSD(void);

#endif