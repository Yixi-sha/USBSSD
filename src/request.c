#include <stddef.h>
#include "sub_request.h"
#include "request.h"

/*for kthread and test*/
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>


static Allocator_USBSSD *allocator = NULL;

Request_USBSSD *allocate_Request_USBSSD(struct request *req){
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    SubRequest_USBSSD *whead = NULL, *wtail = NULL;
    SubRequest_USBSSD *rhead = NULL, *rtail = NULL;
    SubRequest_USBSSD *iter = NULL;
    unsigned long long len = 0;
    unsigned long long lsa = 0;
    if(!ret){
        return NULL;
    }
    ret->req = req;
    ret->lsa = blk_rq_pos(req);
    ret->len = blk_rq_sectors(req);
    ret->restSubreqCount = 0;
    ret->head = NULL;

    len = ret->len;
    lsa = ret->lsa;
    while(len > 0){
        unsigned long long lpn, bitMap;
        unsigned long long subLen = 0;
        SubRequest_USBSSD *now = NULL;
        subLen = SUB_PAGE_SIZE_USBSSD >> SECTOR_SHIFT;
        subLen -= lsa % (SUB_PAGE_SIZE_USBSSD >> SECTOR_SHIFT);
        if(subLen > len){
            subLen = len;
        }
        bitMap = ~(SUB_PAGE_MASK << subLen);
        bitMap = bitMap << (lsa % (SUB_PAGE_SIZE_USBSSD >> SECTOR_SHIFT));
        lpn = lsa / (SUB_PAGE_SIZE_USBSSD >> SECTOR_SHIFT);

        now = allocate_SubRequest_USBSSD(ret, lpn, bitMap, rq_data_dir(req));
        if(!now){
            goto allocate_Request_USBSSD_err;
        }

        ret->restSubreqCount++;

        if(!iter){
            ret->head = now;
        }else{
            iter->next = now;
        }
        iter = now;
        iter->next_inter = NULL;

        now->pre = NULL;
        now->next = NULL;
        if(now->operation == WRITE){
            if(!whead){
                whead = now;
                wtail = now;
            }else{
                now->pre = wtail;
                wtail->next = now;
                wtail = now;
            }
            now = now->relatedSub;
        }
    
        if(now && now->operation == READ){
            if(!rhead){
                rhead = now;
                rtail = now;
            }else{
                now->pre = rtail;
                rtail->next = now;
                rtail = now;
            }
        }

        len -= subLen;
        lsa += subLen;
    }

    if(whead){
        add_To_List_SubRequest_USBSSD(whead, wtail);
    }
    if(rhead){
        add_To_List_SubRequest_USBSSD(rhead, rtail);
    }
    mutex_init(&ret->subMutex);
    return ret;
allocate_Request_USBSSD_err:
    while(whead){
        SubRequest_USBSSD *wfree = whead;
        whead = whead->next;
        free_SubRequest_USBSSD(wfree);
    }
    while(rhead){
        SubRequest_USBSSD *rfree = rhead;
        rhead = rhead->next;
        free_SubRequest_USBSSD(rfree);
    }
    free_Request_USBSSD(ret);
    return NULL;

}

void request_May_End(Request_USBSSD *req){
    mutex_lock(&req->subMutex);
    if(--req->restSubreqCount == 0){
        //end req
        free_Request_USBSSD(req);
    }else{
        mutex_unlock(&req->subMutex);
    }


    
}   

void free_Request_USBSSD(Request_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

int init_Request_USBSSD(void){
    Request_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(Request_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    return allocator == NULL ? -1 : 0;
}

void destory_Request_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}

/*void boost_test_signal_thread(){
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    struct bio_vec bvec;
    sector_t sector = 100;
    SubRequest_USBSSD *head = NULL, *tail = NULL;
    int i = 0;

    bvec.bv_page = NULL;
    bvec.bv_offset = 0;
    bvec.bv_len = SUB_PAGE_SIZE_USBSSD;
    for(i = 0; i < 100; i++){
        SubRequest_USBSSD *now = allocate_SubRequest_USBSSD(ret, &bvec, sector, 1);
        unsigned int len = bvec.bv_len; 
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

    for(i = 0; i < 100; i++){
        SubRequest_USBSSD *now = allocate_SubRequest_USBSSD(ret, &bvec, sector, 0);
        unsigned int len = bvec.bv_len; 
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

    

}

static atomic_t testV = ATOMIC_INIT(0);

int thread_fn_test(void *data){
    int i = 0;
    int f = 0;
    int dec = 0;
    int inc = 0;
    void *ptr[100];
    
    while(!kthread_should_stop()){
        set_current_state(TASK_INTERRUPTIBLE);
        if(f && dec == 0){
            schedule();
        }else if (dec && inc == 0){
            set_current_state(TASK_RUNNING);
            if(i > 0){
                free_USBSSD(allocator, ptr[i - 1]);
                --i;
            }
            if(i == 0 && inc == 0){
                printk("inc == 0 %d\n", get_kmalloc_count());
                atomic_inc(&testV);
                inc = 1;
            }
        }else if(inc == 0){
            set_current_state(TASK_RUNNING);
            if( i < 20){
                ptr[i] = allocate_USBSSD(allocator);
                i++;
            }else{
                printk("%d\n", get_kmalloc_count());
                dec = 1;
            }
        }else{
            schedule();
        }
        udelay(200);
    }
    return 0;
}

void boost_test_requsts(){
    int i = 0;
    static struct task_struct *test_task[5];
    for(; i < 5; i++){
        test_task[i] = kthread_create(thread_fn_test, NULL, "thread_fn_test %d", i);
    }

    for(i = 0; i < 5; i++){
        wake_up_process(test_task[i]);
    }

    while(atomic_read(&testV) < 5){
        udelay(200);
    }
    for(i = 0; i < 5; i++){
        kthread_stop(test_task[i]);
    }
}*/
