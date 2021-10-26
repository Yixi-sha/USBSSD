#include "request.h"
#include "sub_request.h"
#include <stddef.h>

/*for kthread and test*/
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>

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