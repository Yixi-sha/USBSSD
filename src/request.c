#include "request.h"
#include "sub_request.h"
#include <stddef.h>

/*for bio*/

static Allocator_USBSSD *allocator = NULL;

Request_USBSSD *allocate_Request_USBSSD(struct bio *bio){
    struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    SubRequest_USBSSD *head = NULL, *tail = NULL;
    if(!ret){
        return NULL;
    }
    ret->bio = bio;
    ret->restDataCount = 0;
    sector = bio->bi_iter.bi_sector;

    bio_for_each_segment(bvec, bio, iter){
        unsigned int len = bvec.bv_len; 
        SubRequest_USBSSD *now = allocate_SubRequest_USBSSD(ret, &bvec, sector, bio_data_dir(bio));
        if(!now){
            goto allocate_Request_USBSSD_err;
        }
        now->pre = NULL;
        now->next = NULL;
        if(!head){
            head = now;
            tail = now;
        }else{
            now->pre = tail;
            tail->next = now;
            tail = now;
        }
        sector += len >> SECTOR_SHIFT;
    }
    add_To_List_SubRequest_USBSSD(head, tail);
    return ret;

allocate_Request_USBSSD_err:
    while(head){
        SubRequest_USBSSD *wfree = head;
        head = head->next;
        free_SubRequest_USBSSD(wfree);
    }
    free_Request_USBSSD(ret);
    return NULL;
}

void free_Request_USBSSD(Request_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

void init_Request_USBSSD(void){
    Request_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(Request_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
}

void destory_Request_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}