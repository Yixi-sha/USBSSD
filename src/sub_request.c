#include "sub_request.h"
#include <stddef.h>



static Allocator_USBSSD *allocator = NULL;
SubRequest_USBSSD* Head = NULL;
SubRequest_USBSSD* Tail = NULL;

void add_To_List(SubRequest_USBSSD* h, SubRequest_USBSSD* t){
    SubRequest_USBSSD* now = h;
    while(now->next){
        now = now->next;
    }
    if(now != t){
        return;
    }
    if(Head == NULL){
        Head = h;
        Tail = t;
    }else{
        Tail->next = h;
        h->pre = Tail;
        Tail = t;
    }
}

SubRequest_USBSSD *allocate_SubRequest_USBSSD(Request_USBSSD *req, struct bio_vec *bvec, sector_t sector, unsigned char operation){
    SubRequest_USBSSD *ret = allocate_USBSSD(allocator);
    unsigned long start = (sector) << SECTOR_SHIFT;
    unsigned int len = bvec->bv_len;
    if(!ret){
        return NULL;
    }

    ret->req = req;
    ret->operation = operation ? WRITE_USBSSD : READ_USBSSD;
    ret->lpn = start / SUB_PAGE_SIZE_USBSSD;

    ret->start = start % SUB_PAGE_SIZE_USBSSD;
    ret->len = len;

    ret->page = bvec->bv_page;
    ret->offset = bvec->bv_offset;

    ret->pre = NULL;
    ret->next = NULL;

    return ret;
}

void free_SubRequest_USBSSD(SubRequest_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

void init_SubRequest_USBSSD(void){
    SubRequest_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(SubRequest_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
}

void destory_SubRequest_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}