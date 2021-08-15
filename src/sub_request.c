#include "sub_request.h"
#include <stddef.h>



static Allocator_USBSSD *allocator = NULL;
static SubRequest_USBSSD* HeadR = NULL;
static SubRequest_USBSSD* TailR = NULL;
static struct mutex subMutexR;

static SubRequest_USBSSD* HeadW = NULL;
static SubRequest_USBSSD* TailW = NULL;
static struct mutex subMutexW;

void add_To_List_SubRequest_USBSSD(SubRequest_USBSSD* h, SubRequest_USBSSD* t){
    SubRequest_USBSSD* now = h;
    while(now->next){
        now = now->next;
    }
    if(now != t){
        return;
    }

    if(h->operation == READ_USBSSD){
        mutex_lock(&subMutexR);
        if(HeadR == NULL){
            HeadR = h;
            TailR = t;
        }else{
            TailR->next = h;
            h->pre = TailR;
            TailR = t;
        }
        mutex_unlock(&subMutexR);
    }else{
        mutex_lock(&subMutexW);
        if(HeadW == NULL){
            HeadW = h;
            TailW = t;
        }else{
            TailW->next = h;
            h->pre = TailW;
            TailW = t;
        }
        mutex_unlock(&subMutexW);
    }   
}



void remove_From_List_SubRequest_USBSSD(SubRequest_USBSSD* target){
    SubRequest_USBSSD* now = NULL;

    if(target->operation == READ_USBSSD){
        mutex_lock(&subMutexR);
        now = HeadR;
        while(now && now != target){
            now = now->next;
        }
        if(!now){
            mutex_unlock(&subMutexR);
            return;
        }

        if(now == HeadR){
            HeadR = now->next;
            if(HeadR){
                HeadR->pre = NULL;
            }
            if(now == TailR){
                TailR = NULL;
            }
        }else{
            now->pre->next = now->next;
            if(now->next){
                now->next->pre = now->pre;
            }else{
                TailR = now->pre;
            }
        }
        mutex_unlock(&subMutexR);
    }else{
        mutex_lock(&subMutexW);
        now = HeadW;
        while(now && now != target){
            now = now->next;
        }
        if(!now){
            mutex_unlock(&subMutexW);
            return;
        }

        if(now == HeadW){
            HeadW = now->next;
            if(HeadW){
                HeadW->pre = NULL;
            }
            if(now == TailW){
                TailW = NULL;
            }
        }else{
            now->pre->next = now->next;
            if(now->next){
                now->next->pre = now->pre;
            }else{
                TailW = now->pre;
            }
        }
        mutex_unlock(&subMutexW);
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
    mutex_init(&subMutexR);
}

void destory_SubRequest_USBSSD(void){
    mutex_lock(&subMutexR);
    destory_allocator_USBSSD(allocator);
    mutex_destroy(&subMutexR);
}