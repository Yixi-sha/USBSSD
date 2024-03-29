#include <linux/major.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/mutex.h>

#include "request.h"
#include "sub_request.h"
#include "command.h"
#include "simulateNAND.h"




#define SIMP_BLKDEV_BYTES            (64 * 1024 * 1024)
// static char simp_blkdev_data[SIMP_BLKDEV_BYTES];

static struct blk_mq_tag_set tag_set;
static int BLOCK_MAJORY = 0;
static struct gendisk *USBSSD_gendisk;

// static DEFINE_SPINLOCK(USBSSD_lock);

static blk_status_t USBSSD_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd){
    struct request *req = bd->rq;
	// unsigned long start;
	// unsigned long len;
    // void *buffer = NULL;
    // blk_status_t err;
    blk_mq_start_request(req);
    
    // spin_lock_irq(&USBSSD_lock);
    
    // do{
    //     buffer = bio_data(req->bio);
    //     len = blk_rq_cur_bytes(req);
    //     start = blk_rq_pos(req) << SECTOR_SHIFT;
        
    //     if (rq_data_dir(req) == READ){
    //         memcpy(buffer, simp_blkdev_data + start, len);
    //     }else{
    //         memcpy(simp_blkdev_data + start, buffer, len);
    //     }
    //     err = BLK_STS_OK;
    // }while(blk_update_request(req, err, blk_rq_cur_bytes(req)));
    

    // spin_unlock_irq(&USBSSD_lock);
    // blk_mq_end_request(req, err);
    allocate_Request_USBSSD(req);

    return BLK_STS_OK;
}

void req_end(struct request *req){
    blk_status_t err = BLK_STS_OK;
    // spin_unlock_irq(&USBSSD_lock);
    blk_mq_end_request(req, err);
}

static const struct blk_mq_ops USBSSD_mq_ops = {
	.queue_rq = USBSSD_queue_rq,
};

static const struct block_device_operations USBSSD_fops = {
	.owner = THIS_MODULE,
};

static int USBSSD_register_disk(void){
    // struct request_queue *q;
	struct gendisk *disk;
    // disk = alloc_disk(1);
    int err;
    disk = blk_mq_alloc_disk(&tag_set, NULL);
	if (IS_ERR(disk)){
        return PTR_ERR(disk);
    }
    printk(KERN_EMERG "blk_mq_alloc_disk %p\n", disk);
    // q = blk_mq_init_queue(&tag_set);
    // if (IS_ERR(q)) {
	// 	put_disk(disk);
    //     USBSSD_gendisk = NULL;
	// 	return PTR_ERR(q);
	// }
    disk->major = BLOCK_MAJORY;
	disk->first_minor = 0;
    disk->minors = 1;
	disk->fops = &USBSSD_fops;
    
	sprintf(disk->disk_name, "USBSSD");
    // disk->queue = q;
    USBSSD_gendisk = disk;

    set_capacity(USBSSD_gendisk, get_capacity_USBSSD() >> 9); // number of sector
    err = add_disk(disk);
    printk(KERN_EMERG "blk_mq_alloc_disk %d\n", err);
	if (err){
        USBSSD_gendisk = NULL;
		blk_cleanup_disk(disk);
    }

    return err;
}

static int __init USBSSD_init(void){
    int ret = 0;
    ret = -EBUSY;

    
    if(init_Request_USBSSD()){
        goto err3;
    }

    if(init_SubRequest_USBSSD()){
        goto err4;
    }

    if(init_Command_USBSSD()){
        goto err5;
    }

    if(init_Simulate_USBSSD()){
        goto err6;
    }

    
    printk(KERN_EMERG "init start\n");
    BLOCK_MAJORY = register_blkdev(0, "USBSSD"); 
    if(BLOCK_MAJORY < 0){
        goto err;
    }
    tag_set.ops = &USBSSD_mq_ops;
    tag_set.nr_hw_queues = 1;
	tag_set.queue_depth = 1;
    tag_set.numa_node = NUMA_NO_NODE;
    tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    ret = blk_mq_alloc_tag_set(&tag_set);
    printk(KERN_EMERG "blk_mq_alloc_tag_set\n");
    if (ret)
		goto err1;

    ret = USBSSD_register_disk();
    printk(KERN_EMERG "init end %d\n", ret);
    if (ret)
		goto err2;
    
    // printk("start memcheck %d\n", get_kmalloc_count());
    // boost_test_signal_thread();
    // boost_test_requsts();
    // printk("memcheck %d\n", get_kmalloc_count());

    return 0;

err2:
    blk_mq_free_tag_set(&tag_set);   
err1:
    unregister_blkdev(BLOCK_MAJORY, "USBSSD");
err6:
    destory_Command_USBSSD();
err5:
    destory_SubRequest_USBSSD();
err4:
    destory_Request_USBSSD();
err3:
    del_gendisk(USBSSD_gendisk);
    put_disk(USBSSD_gendisk);
err:
    return ret;
}


static void __exit USBSSD_exit(void){
    unregister_blkdev(BLOCK_MAJORY, "USBSSD");
    del_gendisk(USBSSD_gendisk);
    // blk_cleanup_queue(USBSSD_gendisk->queue);
    put_disk(USBSSD_gendisk);
    blk_mq_free_tag_set(&tag_set);

    destory_Simulate_USBSSD();
    destory_Command_USBSSD();
    destory_SubRequest_USBSSD();
    destory_Request_USBSSD();

    printk("end memcheck %d\n", get_kmalloc_count());
}

module_init(USBSSD_init);
module_exit(USBSSD_exit);
MODULE_LICENSE("GPL");