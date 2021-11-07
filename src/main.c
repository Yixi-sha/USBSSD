#include <linux/major.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/mutex.h>


#define SIMP_BLKDEV_BYTES            (64 * 1024 * 1024)
static char simp_blkdev_data[SIMP_BLKDEV_BYTES];

static struct blk_mq_tag_set tag_set;
static int BLOCK_MAJORY = 0;
static struct gendisk *USBSSD_gendisk;

static DEFINE_SPINLOCK(USBSSD_lock);

static blk_status_t USBSSD_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd){
    struct request *req = bd->rq;
	unsigned long start;
	unsigned long len;
    void *buffer = NULL;
    blk_status_t err;
    blk_mq_start_request(req);
    
    spin_lock_irq(&USBSSD_lock);
    printk("req s %lld req len %d\n", blk_rq_pos(req), blk_rq_bytes(req));
    do{
        buffer = bio_data(req->bio);
        len = blk_rq_cur_bytes(req);
        start = blk_rq_pos(req) << SECTOR_SHIFT;
        printk("the start is %ld len is %ld sec %ld\n", start, len, start >> SECTOR_SHIFT);
        if (rq_data_dir(req) == READ){
            memcpy(buffer, simp_blkdev_data + start, len);
        }else{
            memcpy(simp_blkdev_data + start, buffer, len);
        }
        err = BLK_STS_OK;
    }while(blk_update_request(req, err, blk_rq_cur_bytes(req)));
    printk("\n");

    spin_unlock_irq(&USBSSD_lock);
    blk_mq_end_request(req, err);
    return BLK_STS_OK;
}

static const struct blk_mq_ops USBSSD_mq_ops = {
	.queue_rq = USBSSD_queue_rq,
};

static const struct block_device_operations USBSSD_fops = {
	.owner = THIS_MODULE,
};

static int USBSSD_register_disk(void){
    struct request_queue *q;
	struct gendisk *disk;
    disk = alloc_disk(1);
	if (!disk){
        return -ENOMEM;
    }
    q = blk_mq_init_queue(&tag_set);
    if (IS_ERR(q)) {
		put_disk(disk);
        USBSSD_gendisk = NULL;
		return PTR_ERR(q);
	}
    disk->major = BLOCK_MAJORY;
	disk->first_minor = 0;
	disk->fops = &USBSSD_fops;
    
	sprintf(disk->disk_name, "USBSSD");
    disk->queue = q;
    USBSSD_gendisk = disk;

    set_capacity(USBSSD_gendisk, SIMP_BLKDEV_BYTES >> 9); // number of sector
    add_disk(disk);

    return 0;
}

static int __init USBSSD_init(void){
    int ret = 0;
    ret = -EBUSY;

    BLOCK_MAJORY = register_blkdev(0, "USBSSD"); 
    if(BLOCK_MAJORY < 0){
        goto err;
    }
    tag_set.ops = &USBSSD_mq_ops;
    tag_set.nr_hw_queues = 1;
	tag_set.queue_depth = 16;
    tag_set.numa_node = NUMA_NO_NODE;
    tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    ret = blk_mq_alloc_tag_set(&tag_set);
    if (ret)
		goto err1;

    ret = USBSSD_register_disk();
    if (ret)
		goto err2;

    return 0;
err2:
    blk_mq_free_tag_set(&tag_set);   
err1:
    unregister_blkdev(BLOCK_MAJORY, "USBSSD");
err:
    return ret;
}


static void __exit USBSSD_exit(void){
    unregister_blkdev(BLOCK_MAJORY, "USBSSD");
    del_gendisk(USBSSD_gendisk);
    blk_cleanup_queue(USBSSD_gendisk->queue);
    put_disk(USBSSD_gendisk);
    blk_mq_free_tag_set(&tag_set);
}

module_init(USBSSD_init);
module_exit(USBSSD_exit);
MODULE_LICENSE("GPL");