#include "sub_request.h"
#include <stddef.h>
#include "command.h"


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

    if(h->operation == READ){
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

static void remove_From_List_SubRequest_USBSSD_Unsafe(SubRequest_USBSSD* target){
    SubRequest_USBSSD* now = NULL;

    if(target->operation == READ){
        now = HeadR;
        while(now && now != target){
            now = now->next;
        }
        if(!now){
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
    }else{
        now = HeadW;
        while(now && now != target){
            now = now->next;
        }
        if(!now){
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
    }
}

SubRequest_USBSSD *get_SubRequests_Write_USBSSD(int chan, int chip){
    SubRequest_USBSSD *head = NULL;
    mutex_lock(&subMutexW);
    head = HeadW;
    while(head){
        if(head->relatedSub == NULL && head->location.channel == chan && head->location.chip == chip){
            break;
        }
        head = head->next;
    }
    if(head){
        remove_From_List_SubRequest_USBSSD_Unsafe(head);
    }
    mutex_unlock(&subMutexW);
    
    
    return head;
}

SubRequest_USBSSD *get_SubRequests_Read_USBSSD(int chan, int chip){
    SubRequest_USBSSD *head = NULL;
    mutex_lock(&subMutexR);
    head = HeadR;
    while(head){
        if(head->location.channel == chan && head->location.chip == chip){
            break;
        }
        head = head->next;
    }
    if(head){
        remove_From_List_SubRequest_USBSSD_Unsafe(head);
    }
    mutex_unlock(&subMutexR);
    
    return head;
}

void remove_From_List_SubRequest_USBSSD(SubRequest_USBSSD* target){
    SubRequest_USBSSD* now = NULL;

    if(target->operation == READ){
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

SubRequest_USBSSD *allocate_SubRequest_USBSSD(Request_USBSSD *req, unsigned long long lpn, unsigned long long bitMap, unsigned char operation){
    mapEntry_USBSSD nowMAp;
    SubRequest_USBSSD *ret = allocate_USBSSD(allocator);
    if(!ret){
        return NULL;
    }

    ret->req = req;
    ret->operation = operation;
    ret->lpn = lpn;
    ret->bitMap = bitMap;
    ret->jiffies = jiffies;
    ret->relatedSub = NULL;
    ret->pre = NULL;
    ret->next = NULL;
    ret->next_inter = NULL;

    if(get_PPN_USBSSD(ret->lpn, &nowMAp)){
        free_SubRequest_USBSSD(ret);
        return NULL;
    }
    
    if(ret->operation == READ){
        get_PPN_Detail_USBSSD(nowMAp.ppn, &ret->location);
    }else{
        allocate_location(&ret->location);
    }

    if(ret->operation == WRITE && ((ret->bitMap & nowMAp.subPage) != nowMAp.subPage)){
        SubRequest_USBSSD *relatedSub = allocate_SubRequest_USBSSD(NULL, lpn, nowMAp.subPage, READ);
        ret->relatedSub = relatedSub;
        relatedSub->relatedSub = ret;
    }else{
        ret->relatedSub = NULL;
    }

    return ret;
}

void subRequest_End(SubRequest_USBSSD *sub){
    if(sub->relatedSub){
        sub->relatedSub->relatedSub = NULL;
        free_SubRequest_USBSSD(sub);
    }else{
        request_May_End(sub->req);
    }
    
}

void free_SubRequest_USBSSD(SubRequest_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

int init_SubRequest_USBSSD(void){
    SubRequest_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(SubRequest_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    if(allocator == NULL){
        return -1;
    }
    mutex_init(&subMutexR);
    mutex_init(&subMutexW);
    return 0;
}

void destory_SubRequest_USBSSD(void){
    mutex_lock(&subMutexR);
    destory_allocator_USBSSD(allocator);
    mutex_destroy(&subMutexR);
}