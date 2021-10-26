#include "sub_request.h"
#include <stddef.h>
#include "command.h"

#define TIME_INTERVAL 100

static Allocator_USBSSD *allocator = NULL;
static Allocator_USBSSD *allocatorBuf = NULL;
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

static void remove_From_List_SubRequest_Unsafe_USBSSD(SubRequest_USBSSD* target){
    SubRequest_USBSSD* now = NULL;

    if(target->operation == READ_USBSSD){
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
       
    }else{
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
    }
}
SubRequest_USBSSD *get_Update_USBSSD(SubRequest_USBSSD* write){
    SubPageBuf_USBSSD *buf = allocate_USBSSD(allocatorBuf);
    SubRequest_USBSSD *ret = NULL;
    if(!buf){
        return NULL;
    }
    ret = allocate_USBSSD(allocator);
    if(!ret){
        free_USBSSD(allocatorBuf, buf);
        return NULL;
    }

    ret->operation = READ_USBSSD;

    ret->lpn = write->lpn;
    ret->start = 0;
    ret->offset = 0;

    ret->page = NULL;
    ret->jiffies = jiffies;

    ret->update = NULL;
    ret->updateLen = 0;

    ret->updateSub = write;

    ret->pre = NULL;
    ret->next = NULL;

    ret->page = buf->buf;

    return ret;
}

SubRequest_USBSSD *get_SubRequests_Read_Start_USBSSD(void){
    mutex_lock(&subMutexR);
    return HeadR;
}

SubRequest_USBSSD *get_SubRequests_Read_Iter_USBSSD(SubRequest_USBSSD *iter){
    if(iter->operation != READ_USBSSD){
        return NULL;
    }
    return iter->next;
}

SubRequest_USBSSD *get_SubRequests_Read_Get_USBSSD(SubRequest_USBSSD *iter){
    SubRequest_USBSSD* now = NULL, *head = NULL;
    mapEntry_USBSSD target, miter;

    if(!iter || iter->operation != READ_USBSSD || get_PPN_USBSSD(iter->lpn, &target)){
        return NULL;
    }
    head = iter;
    iter = iter->next;
    now = head;
    remove_From_List_SubRequest_Unsafe_USBSSD(head);
    now->pre = NULL;
    now->next = NULL;

    while(iter){
        if(get_PPN_USBSSD(iter->lpn, &miter)){
            return head;
        }
        if(miter.subPage >= 0 && target.ppn.channel == miter.ppn.channel && target.ppn.chip == miter.ppn.chip){
            now->next = iter;
            iter = iter->next;
            remove_From_List_SubRequest_Unsafe_USBSSD(now->next);
            now->next->pre = now;
            now->next->next = NULL;
            now = now->next;
        }else{
            iter = iter->next;
        }
    }
    return head;
}

void get_SubRequests_Read_End_USBSSD(void){
    mutex_unlock(&subMutexR);
}

SubRequest_USBSSD *get_SubRequests_Write_USBSSD(void){
    SubRequest_USBSSD* now = NULL, *head = NULL, *iter = NULL;
    mapEntry_USBSSD target, miter;

    
    int targetCount = PAGE_SIZE_HW_USBSSD / SUB_PAGE_SIZE_USBSSD;
    int nowCount = 0;
    unsigned char preRead = 0;

    mutex_lock(&subMutexW);
    iter = HeadW;
    if(!iter){
        return NULL;
    }
    while(nowCount < targetCount && iter){
        if(iter->len != SUB_PAGE_SIZE_USBSSD && iter->update == NULL){
            if(get_PPN_USBSSD(iter->lpn, &target)){
                mutex_unlock(&subMutexW);
                return NULL;
            }
            if(miter.subPage >= 0){
                preRead++;
                break;
            }
        }
        iter = iter->next;
        nowCount++;
    }
    if(preRead){
        head = get_Update_USBSSD(iter);
        if(!head){
            mutex_unlock(&subMutexW);
            return NULL;
        }
        now = head;
        iter = iter->next;
        while(iter){
            if(iter->len != SUB_PAGE_SIZE_USBSSD && iter->update == NULL){
                if(get_PPN_USBSSD(iter->lpn, &miter)){
                    mutex_unlock(&subMutexW);
                    break;
                    //return head;
                }
                if(miter.subPage >= 0 && miter.ppn.channel == target.ppn.channel && \
                miter.ppn.chip == target.ppn.chip && miter.ppn.die == target.ppn.die && \
                miter.ppn.plane == target.ppn.plane && miter.ppn.block == target.ppn.block &&
                miter.ppn.page == target.ppn.page){
                    now->next = get_Update_USBSSD(iter);
                    if(!now->next){
                        mutex_unlock(&subMutexW);
                        //return head;
                        break;
                    }
                    now->next->pre = now;
                    now = now->next;
                }
            }
            iter = iter->next;
        }
        add_To_List_SubRequest_USBSSD(head, now);
        return NULL;
    }else if(nowCount != targetCount && (HeadW->jiffies + TIME_INTERVAL) < jiffies){
        //set up timer
    }else{
        iter = HeadW;
        nowCount = 0;

        while(nowCount < targetCount && iter){
            if(!head){
                head = iter;
                iter = iter->next;
                remove_From_List_SubRequest_Unsafe_USBSSD(head);
                head->pre = NULL;
                head->next = NULL;
                now = head;
            }else{
                now->next = iter;
                iter = iter->next;
                remove_From_List_SubRequest_Unsafe_USBSSD(now->next);
                now->next->next = NULL;
                now->next->pre = now;
                now = now->next;
            }
            nowCount++;
        }

    }
    mutex_unlock(&subMutexW);
    
    return head;
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

    ret->jiffies = jiffies;

    ret->update = NULL;
    ret->updateLen = 0;
    ret->updateSub = NULL;

    ret->pre = NULL;
    ret->next = NULL;

    return ret;
}

void free_SubRequest_USBSSD(SubRequest_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

void init_SubRequest_USBSSD(void){
    SubRequest_USBSSD *tmp = 0;
    SubPageBuf_USBSSD *buf = 0;
    allocator = init_allocator_USBSSD(sizeof(SubRequest_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    allocatorBuf = init_allocator_USBSSD(sizeof(SubPageBuf_USBSSD), ((void*)(&(buf->allocator))) - ((void*)buf));
    mutex_init(&subMutexR);
    mutex_init(&subMutexW);
}

void destory_SubRequest_USBSSD(void){
    mutex_lock(&subMutexR);
    destory_allocator_USBSSD(allocator);
    mutex_destroy(&subMutexR);
}