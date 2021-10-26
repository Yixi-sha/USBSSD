#include <linux/major.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/bio.h>

#include "request.h"
#include "allocator.h"
#include "memCheck.h"

#define SIMP_BLKDEV_BYTES            (64 * 1024 * 1024)
char simp_blkdev_data[SIMP_BLKDEV_BYTES];

int BLOCK_MAJORY = 0;
struct request_queue *q = NULL;
struct gendisk *USBSSD_DISK = NULL;

static blk_qc_t USBSSD_submit_bio(struct bio *bio){
    struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;

    sector = bio->bi_iter.bi_sector;

    if (bio_end_sector(bio) > get_capacity(bio->bi_disk))
		goto io_error;

    bio_for_each_segment(bvec, bio, iter){
        unsigned int len = bvec.bv_len;
        unsigned long flags;
        void *buffer = bvec_kmap_irq(&bvec, &flags);
        unsigned long start = (sector) << SECTOR_SHIFT;
        
        if (!bio_data_dir(bio)){
            memcpy(buffer, simp_blkdev_data + start, len);
        }else{
            memcpy(simp_blkdev_data + start, buffer, len);
        }   
        
        flush_kernel_dcache_page(bvec.bv_page);
        bvec_kunmap_irq(buffer, &flags);
        sector += len >> SECTOR_SHIFT;
    }

    bio_endio(bio);
    return BLK_QC_T_NONE;
io_error:
	bio_io_error(bio);
	return BLK_QC_T_NONE;
}

const struct block_device_operations USBSSD_fops = {
	.owner		= THIS_MODULE,
    .submit_bio = USBSSD_submit_bio,
};

static int __init USBSSD_init(void){
    int ret = 0;
    
    ret = -EBUSY;

    BLOCK_MAJORY = register_blkdev(0, "USBSSD");  

    if(BLOCK_MAJORY < 0){
        goto err;
    }

    ret = -ENOMEM;
    USBSSD_DISK = alloc_disk(1);
    if(USBSSD_DISK == NULL){
        goto err1;
    }

    q = blk_alloc_queue(NUMA_NO_NODE);;
    if(IS_ERR(q)){
        goto err2;
    }

    sprintf(USBSSD_DISK->disk_name, "usbssd");
    USBSSD_DISK->queue = q;
    USBSSD_DISK->major = BLOCK_MAJORY;
    USBSSD_DISK->first_minor = 0;
    USBSSD_DISK->fops = &USBSSD_fops;
    set_capacity(USBSSD_DISK, SIMP_BLKDEV_BYTES >> 9); // number of sector
    
    add_disk(USBSSD_DISK);
    printk("PAG_SIZE %ld\n", PAGE_SIZE);

    if(init_Request_USBSSD()){
        goto err3;
    }
    boost_test_requsts();
    return 0;
err3:
    del_gendisk(USBSSD_DISK);
    blk_cleanup_queue(q);
err2:
    put_disk(USBSSD_DISK); 
err1:
    unregister_blkdev(BLOCK_MAJORY,"USBSSD");
err:
    return ret;
}

static void __exit USBSSD__exit(void){
    del_gendisk(USBSSD_DISK);
    blk_cleanup_queue(q);
    put_disk(USBSSD_DISK);
	unregister_blkdev(BLOCK_MAJORY,"USBSSD");

    destory_Request_USBSSD();
    printk("get_kmalloc_count %d\n", get_kmalloc_count());
}


module_init(USBSSD_init);
module_exit(USBSSD__exit);
MODULE_LICENSE("GPL");