// #include <stddef.h>
#include "sub_request.h"
#include "request.h"

/*for kthread and test*/
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/blk-mq.h>

static Allocator_USBSSD *allocator = NULL;

Request_USBSSD *allocate_Request_USBSSD(struct request *req){
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    unsigned char *buf;
    unsigned int bufLen = 0;
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
    // printk("lsd %lld %lld %d", ret->lsa, ret->len,rq_data_dir(req) == READ);
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
            iter->next_inter = now;
        }
        iter = now;
        iter->next_inter = NULL;

        now->pre = NULL;
        now->next = NULL;
        if(now->operation == WRITE){
            unsigned int copyLen = subLen << SECTOR_SHIFT;
            unsigned int copyStart = (lsa % (PAGE_SIZE_USBSSD >> SECTOR_SHIFT));
            copyStart = copyStart << SECTOR_SHIFT;
            while(copyLen > 0){
                unsigned int nowCopyLen = 0;
                if(bufLen == 0){
                    buf = bio_data(req->bio);
                    bufLen = blk_rq_cur_bytes(req);
                }
                nowCopyLen = bufLen;
                if(nowCopyLen > copyLen){
                    nowCopyLen = copyLen;
                }
                memcpy(now->buf + copyStart, buf, nowCopyLen);
                copyLen -= nowCopyLen;
                copyStart += nowCopyLen;

                bufLen -= nowCopyLen;
                buf += nowCopyLen;
                if(bufLen == 0){
                    blk_status_t err;
                    err = BLK_STS_OK;
                    blk_update_request(req, err, blk_rq_cur_bytes(req));
                }
            }
        
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

static int get_read_copy_start_and_len(SubRequest_USBSSD *sub, int *start){
    int i = 0;
    int ret = 0;
    (*start) = -1;

    for(; i < sizeof(sub->bitMap) * 8;++i){
        if(sub->bitMap & (1ULL << i)){
            ret += SECTOR_SIZE;
            if((*start) == -1){
                (*start) = i * SECTOR_SIZE;
            }
        }
    }
    return ret;
}

void boost_test_signal_thread_READ(void);

void req_end(struct request *req);

void request_May_End(Request_USBSSD *req){
    // static int count = 0;
    mutex_lock(&req->subMutex); 
    // printk("%lld \n",req->restSubreqCount);
    if(--req->restSubreqCount == 0){
        SubRequest_USBSSD *sub = NULL;
        if(req->req && rq_data_dir(req->req) == READ){
            unsigned char *buf;
            unsigned int bufLen = 0;
            sub = req->head;
            
            while(sub){
                int copyStart = 0;
                int copyLen = 0;
                copyLen = get_read_copy_start_and_len(sub, &copyStart);

                while(copyLen > 0){
                    int nowCopyLen = 0;
                    if(bufLen == 0){
                        buf = bio_data(req->req->bio);
                        bufLen = blk_rq_cur_bytes(req->req);
                    }
                    nowCopyLen = copyLen;
                    if(nowCopyLen > bufLen){
                        nowCopyLen = bufLen;
                    }

                    memcpy(buf, sub->buf + copyStart, nowCopyLen);
                    copyLen -= nowCopyLen;
                    copyStart += nowCopyLen;

                    bufLen -= nowCopyLen;
                    buf += nowCopyLen;
                    if(bufLen == 0){
                        blk_status_t err;
                        err = BLK_STS_OK;
                        blk_update_request(req->req, err, blk_rq_cur_bytes(req->req));
                    }
                }

                sub = sub->next_inter;
            }
        }else{
            // printk("write end\n");
        }


        sub = req->head;
        while(sub){
            SubRequest_USBSSD *now = sub;
            if(sub->operation == WRITE){
                printk("%c %d %c\n", now->buf[0], now->buf[100] ,now->buf[PAGE_SIZE_USBSSD - 1]);
                printk("%d %d %d %d %d %d\n", now->location.channel, now->location.chip, now->location.die, now->location.plane, now->location.block, now->location.page);
            }
            sub = sub->next_inter;
            free_SubRequest_USBSSD(now);
        }
        if(req->req){
            req_end(req->req);
        }
        free_Request_USBSSD(req);
        // if(count <= 0){
        //     ++count;
        //     // boost_test_signal_thread();
        //     // boost_test_signal_thread_READ();
        // }else if(count == 1){
        //     PPN_USBSSD location;
        //     location.channel = 0;
        //     location.chip = 0;
        //     location.die = 0;
        //     location.plane = 0;
        //     ++count;
        //     // boost_test_signal_thread_READ();
        //     // boost_gc_test_sub(&location);
        // }
    }else{
        mutex_unlock(&req->subMutex);
    }

}   

void boost_test_signal_thread_READ(){
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    SubRequest_USBSSD *whead = NULL, *wtail = NULL;
    SubRequest_USBSSD *rhead = NULL, *rtail = NULL;
    SubRequest_USBSSD *iter = NULL;
    unsigned long long len = 0;
    unsigned long long lsa = 0;

    printk("bost read\n");
    ret->req = NULL;
    ret->lsa = 2;
    ret->len = 160;
    ret->restSubreqCount = 0;
    ret->head = NULL;

    len = ret->len;
    lsa = ret->lsa;
    mutex_init(&ret->subMutex);
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
        now = allocate_SubRequest_USBSSD(ret, lpn, bitMap, READ);

        ret->restSubreqCount++;

        if(!iter){
            ret->head = now;
        }else{
            iter->next_inter = now;
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
}


void free_Request_USBSSD(Request_USBSSD* addr){
    mutex_destroy(&addr->subMutex);
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
    static int count = 0;
    int i = 0;
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    SubRequest_USBSSD *whead = NULL, *wtail = NULL;
    SubRequest_USBSSD *rhead = NULL, *rtail = NULL;
    SubRequest_USBSSD *iter = NULL;
    unsigned long long len = 0;
    unsigned long long lsa = 0;
    ret->req = NULL;
    ret->lsa = 2 + count;
    ret->len = 160;
    ret->restSubreqCount = 0;
    ret->head = NULL;

    len = ret->len;
    lsa = ret->lsa;
    mutex_init(&ret->subMutex);
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

        for(i = 0; i < PAGE_SIZE_USBSSD; i++){
            now->buf[i] = lpn + count * 100;
        }

        ret->restSubreqCount++;

        if(!iter){
            ret->head = now;
        }else{
            iter->next_inter = now;
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
    ++count;
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
            
            if(i == 20){
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
