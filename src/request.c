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
        subLen = PAGE_SIZE_USBSSD >> SECTOR_SHIFT;
        subLen -= lsa % (PAGE_SIZE_USBSSD >> SECTOR_SHIFT);
        if(subLen > len){
            subLen = len;
        }
        bitMap = ~(SUB_PAGE_MASK << subLen);
        bitMap = bitMap << (lsa % (PAGE_SIZE_USBSSD >> SECTOR_SHIFT));
        lpn = lsa / (PAGE_SIZE_USBSSD >> SECTOR_SHIFT);

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

void boost_test_signal_thread(){
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    SubRequest_USBSSD *whead = NULL, *wtail = NULL;
    SubRequest_USBSSD *rhead = NULL, *rtail = NULL;
    SubRequest_USBSSD *iter = NULL;
    unsigned long long len = 0;
    unsigned long long lsa = 0;
    ret->req = NULL;
    ret->lsa = 2;
    ret->len = 16;
    ret->restSubreqCount = 0;
    ret->head = NULL;

    len = ret->len;
    lsa = ret->lsa;
    while(len > 0){
        unsigned long long lpn, bitMap;
        unsigned long long subLen = 0;
        SubRequest_USBSSD *now = NULL;
        subLen = PAGE_SIZE_USBSSD >> SECTOR_SHIFT;
        subLen -= lsa % (PAGE_SIZE_USBSSD >> SECTOR_SHIFT);
        if(subLen > len){
            subLen = len;
        }
        bitMap = ~(SUB_PAGE_MASK << subLen);
        bitMap = bitMap << (lsa % (PAGE_SIZE_USBSSD >> SECTOR_SHIFT));
        lpn = lsa / (PAGE_SIZE_USBSSD >> SECTOR_SHIFT);

        now = allocate_SubRequest_USBSSD(ret, lpn, bitMap, WRITE);

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
    
}

static atomic_t testV = ATOMIC_INIT(0);

int thread_fn_test(void *data){
    int i = 0;
    
    while(!kthread_should_stop()){
        set_current_state(TASK_INTERRUPTIBLE);
        if(i < 20){
            set_current_state(TASK_RUNNING);
            boost_test_signal_thread();
            i++;
            //printk("%d\n", i);
            if(i == 20){
                //printk("%d\n", get_kmalloc_count());
                atomic_inc(&testV);
            }
        }else{
            schedule();
        }
        udelay(20);
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
}
