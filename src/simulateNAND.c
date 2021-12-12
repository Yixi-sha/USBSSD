#include "simulateNAND.h"
#include "command.h"
#include "memCheck.h"

static SSD_HW_USBSSD *ssd;

static const int CHANNEL_NO_PER_SSD = 1;
static const int CHIP_NO_PER_CHANNEL = 1;
static const int DIE_NO_PER_CHIP = 1;
static const int PLANE_NO_PER_DIE = 1;
static const int BLOCK_NO_PER_PLANE = 512;
static const int PAGE_NO_PER_BLOCK = 64;


static const int CMD_AND_DATA_TIME = 1;
static const int PROGRAM_TIME = 10;
static const int READ_TIME = 5;
static const int ERASE_TIME = 30;

#define BLOCK_SIZE_USBSSD (PAGE_SIZE_USBSSD * PAGE_NO_PER_BLOCK)
#define PLANE_SIZE_USBSSD (BLOCK_SIZE_USBSSD * BLOCK_NO_PER_PLANE)
#define DIE_SIZE_USBSSD (PLANE_SIZE_USBSSD * PLANE_NO_PER_DIE)


static void timer_callback_chip(struct timer_list *unused){
    Chip_HW_USBSSD* chipInfo = container_of(unused, Chip_HW_USBSSD, timer);
    mutex_lock(&chipInfo->Mutex);
    switch (chipInfo->state){
        
        case CMD_READ_ADDR_TRANSFERRED:
            //send_read_ready()
            break;
        
        case CMD_PROGRAM_ADDR_DATE_TRANSFERRED: 
        case CMD_ERASE_TRANSFERRED:
        case CMD_READ_DATA_TRANSFERRED:
            chipInfo->state = IDEL_HW;
            //send_channel_idle()
            break;
        default:
            break;
    }
    mutex_unlock(&chipInfo->Mutex);
}

static void timer_callback_channel(struct timer_list *unused){
    Channel_HW_USBSSD* chanInfo = container_of(unused, Channel_HW_USBSSD, timer);
    mutex_lock(&chanInfo->Mutex);
    switch (chanInfo->state){
        case CMD_PROGRAM_ADDR_DATE_TRANSFERRED:
        case CMD_READ_ADDR_TRANSFERRED:
        case CMD_ERASE_TRANSFERRED:
            chanInfo->state = IDEL_HW;
            //send_channel_idle()
            break;
        default:
            break;
    }
    
    mutex_unlock(&chanInfo->Mutex);
}

static int write_data(Channel_HW_USBSSD *chanInfo, Chip_HW_USBSSD* chipInfo, int die, int plane, int block, int page, int start, int len,unsigned char *buf){
    int offset = 0;
    mutex_lock(&chanInfo->Mutex);
    mutex_lock(&chipInfo->Mutex);

    if(chanInfo->state != IDEL_HW || chipInfo->state != IDEL_HW){
        mutex_unlock(&chanInfo->Mutex);
        mutex_unlock(&chipInfo->Mutex);
        return -1;
    }
    
    chanInfo->state = CMD_PROGRAM_ADDR_DATE_TRANSFERRED;
    chipInfo->state = CMD_PROGRAM_ADDR_DATE_TRANSFERRED;

    offset = DIE_SIZE_USBSSD * die + plane * PLANE_SIZE_USBSSD + block * BLOCK_SIZE_USBSSD + page * PAGE_SIZE_USBSSD + start;

    memcpy(chipInfo->data + offset, buf, len);

    chipInfo->preCMDPlane = plane;
    chipInfo->preCMDDie = die;
    chipInfo->preCMDPage = page;
    chipInfo->preCMDBlock = block;   

    timer_setup(&(chipInfo->timer), timer_callback_chip, 0);
    chipInfo->timer.expires = jiffies + CMD_AND_DATA_TIME + PROGRAM_TIME;
    add_timer(&(chipInfo->timer));

    timer_setup(&(chanInfo->timer), timer_callback_channel, 0);
    chanInfo->timer.expires = jiffies + CMD_AND_DATA_TIME;
    add_timer(&(chanInfo->timer));

    mutex_unlock(&chipInfo->Mutex);
    mutex_unlock(&chanInfo->Mutex);

    return 0;
}

static int send_Read_addr(Channel_HW_USBSSD *chanInfo, Chip_HW_USBSSD* chipInfo, int die, int plane, int block, int page){
    mutex_lock(&chanInfo->Mutex);
    mutex_lock(&chipInfo->Mutex);

    if(chanInfo->state != IDEL_HW || chipInfo->state != IDEL_HW){
        mutex_unlock(&chanInfo->Mutex);
        mutex_unlock(&chipInfo->Mutex);
        return -1;
    }

    chanInfo->state = CMD_READ_ADDR_TRANSFERRED;
    chipInfo->state = CMD_READ_ADDR_TRANSFERRED;

    chipInfo->preCMDPlane = plane;
    chipInfo->preCMDDie = die;
    chipInfo->preCMDBlock = block; 
    chipInfo->preCMDPage = page;

    timer_setup(&(chipInfo->timer), timer_callback_chip, 0);
    chipInfo->timer.expires = jiffies + CMD_AND_DATA_TIME + READ_TIME;
    add_timer(&(chipInfo->timer));

    timer_setup(&(chanInfo->timer), timer_callback_channel, 0);
    chanInfo->timer.expires = jiffies + CMD_AND_DATA_TIME;
    add_timer(&(chanInfo->timer));

    mutex_unlock(&chipInfo->Mutex);
    mutex_unlock(&chanInfo->Mutex);
    return 0;
}

static int send_Read_data(Channel_HW_USBSSD *chanInfo, Chip_HW_USBSSD* chipInfo, int die, int plane, int block, int page, int start, int len,unsigned char *buf){
    int offset = 0;
    mutex_lock(&chanInfo->Mutex);
    mutex_lock(&chipInfo->Mutex);

    if(chanInfo->state != IDEL_HW || chipInfo->state != CMD_READ_ADDR_TRANSFERRED){
        mutex_unlock(&chanInfo->Mutex);
        mutex_unlock(&chipInfo->Mutex);
        return -1;
    }

    if(chipInfo->preCMDDie != die || chipInfo->preCMDPlane != plane || chipInfo->preCMDBlock != block || chipInfo->preCMDPage != page){
        mutex_unlock(&chanInfo->Mutex);
        mutex_unlock(&chipInfo->Mutex);
        return -1;
    }    

    offset = DIE_SIZE_USBSSD * die + plane * PLANE_SIZE_USBSSD + block * BLOCK_SIZE_USBSSD + page * PAGE_SIZE_USBSSD + start;
    memcpy(buf, chipInfo->data + offset, len);
    
    chanInfo->state = CMD_READ_DATA_TRANSFERRED;
    chipInfo->state = CMD_READ_DATA_TRANSFERRED;

    timer_setup(&(chipInfo->timer), timer_callback_chip, 0);
    chipInfo->timer.expires = jiffies + CMD_AND_DATA_TIME;
    add_timer(&(chipInfo->timer));

    // timer_setup(&(chanInfo->timer), timer_callback_channel, 0);
    // chanInfo->timer.expires = jiffies + CMD_AND_DATA_TIME;
    // add_timer(&(chanInfo->timer));

    mutex_unlock(&chanInfo->Mutex);
    mutex_unlock(&chipInfo->Mutex);

    return 0;
}

static int send_erase(Channel_HW_USBSSD *chanInfo, Chip_HW_USBSSD* chipInfo, int die, int plane, int block){
    mutex_lock(&chanInfo->Mutex);
    mutex_lock(&chipInfo->Mutex);

    if(chanInfo->state != IDEL_HW || chipInfo->state != IDEL_HW){
        mutex_unlock(&chanInfo->Mutex);
        mutex_unlock(&chipInfo->Mutex);
        return -1;
    }

    chanInfo->state = CMD_ERASE_TRANSFERRED;
    chipInfo->state = CMD_ERASE_TRANSFERRED;

    chipInfo->preCMDPlane = plane;
    chipInfo->preCMDDie = die;
    chipInfo->preCMDBlock = block; 

    timer_setup(&(chipInfo->timer), timer_callback_chip, 0);
    chipInfo->timer.expires = jiffies + CMD_AND_DATA_TIME + ERASE_TIME;
    add_timer(&(chipInfo->timer));

    timer_setup(&(chanInfo->timer), timer_callback_channel, 0);
    chanInfo->timer.expires = jiffies + CMD_AND_DATA_TIME;
    add_timer(&(chanInfo->timer));

    mutex_unlock(&chanInfo->Mutex);
    mutex_unlock(&chipInfo->Mutex);

}

//start is offset of page in

int send_cmd_2_NAND(int CMD, int chan, int chip, int die, int plane, int block, int page, int start, int len,unsigned char *buf){
    switch (CMD){
        case CMD_PROGRAM_ADDR_DATE_TRANSFERRED:
            if(write_data(ssd->channels[chan], ssd->channels[chan]->chips[chip], die, plane, block, page, start, len, buf) == 0){
                //send_success
            }else{
                //send failure
            }
            break;
        
        case CMD_READ_ADDR_TRANSFERRED:
            if(send_Read_addr(ssd->channels[chan], ssd->channels[chan]->chips[chip], die, plane, block, page) == 0){
                //send_success
            }else{
                //send failure
            }
            break;
        
        case CMD_READ_DATA_TRANSFERRED:
            if(send_Read_data(ssd->channels[chan], ssd->channels[chan]->chips[chip], die, plane, block, page, start, len, buf) == 0){
                //send_success
            }else{
                //send failure
            }

        case CMD_ERASE_TRANSFERRED:
            break;

        default:
            break;
    }
    return 0;
}


int init_Simulate_USBSSD(void){
    int chan = 0;
    int chip = 0;
    unsigned char fail = 0;
    ssd = kmalloc_wrap(sizeof(SSD_HW_USBSSD), GFP_KERNEL);
    if(!ssd){
        return -1;
    }

    ssd->channelNUM = CHANNEL_NO_PER_SSD;
    ssd->channels = kmalloc_wrap(sizeof(Channel_HW_USBSSD*) * ssd->channelNUM, GFP_KERNEL);
    
    for(chan = 0; chan < ssd->channelNUM && ssd->channels; ++chan){
        ssd->channels[chan] = kmalloc_wrap(sizeof(Channel_HW_USBSSD), GFP_KERNEL);
        if(ssd->channels[chan] == NULL){
            fail = 1;
            continue;
        }
        ssd->channels[chan]->chipNUM = CHIP_NO_PER_CHANNEL;
        ssd->channels[chan]->chips = kmalloc_wrap(sizeof(Chip_HW_USBSSD*) * ssd->channels[chan]->chipNUM, GFP_KERNEL);
        if(ssd->channels[chan]->chips == NULL){
            fail = 1;
            continue;
        }
        ssd->channels[chan]->state = IDEL_HW;
        ssd->channels[chan]->chanelN = chan;
        for(chip = 0; chip < ssd->channels[chan]->chipNUM; chip++){
            unsigned int order;
            ssd->channels[chan]->chips[chip] = kmalloc_wrap(sizeof(Chip_HW_USBSSD), GFP_KERNEL);
            if(ssd->channels[chan]->chips[chip] == NULL){
                fail = 1;
                continue;
            }
            ssd->channels[chan]->chips[chip]->dataLen = DIE_NO_PER_CHIP * PLANE_NO_PER_DIE * BLOCK_NO_PER_PLANE * PAGE_NO_PER_BLOCK * PAGE_SIZE_USBSSD;
            order = get_order(ssd->channels[chan]->chips[chip]->dataLen);
        
            ssd->channels[chan]->chips[chip]->page = alloc_pages(GFP_KERNEL, order);
            if(ssd->channels[chan]->chips[chip]->page == NULL){
                fail = 1;
                ssd->channels[chan]->chips[chip]->data = NULL;
                continue;
            }
            ssd->channels[chan]->chips[chip]->data = page_address(ssd->channels[chan]->chips[chip]->page);
            if(ssd->channels[chan]->chips[chip]->data == NULL){
                fail = 1;
                continue;
            }
            ssd->channels[chan]->chips[chip]->chanelN = chan;
            ssd->channels[chan]->chips[chip]->chipN = chip;
            
            ssd->channels[chan]->chips[chip]->preCMDPlane = -1;
            ssd->channels[chan]->chips[chip]->preCMDDie = -1;
            ssd->channels[chan]->chips[chip]->preCMDPage = -1;
            ssd->channels[chan]->chips[chip]->preCMDBlock = -1;
            
            ssd->channels[chan]->chips[chip]->state = IDEL_HW;
        }
    }
    if(fail){
        destory_Simulate_USBSSD();
        return -1;
    }

    return 0;
}

void destory_Simulate_USBSSD(void){
    int chan = 0;
    int chip = 0;
    if(!ssd){
        return;
    }
    for(chan = 0; chan < ssd->channelNUM && ssd->channels; ++chan){
        if(ssd->channels[chan] == NULL){
            continue;
        }
        if(ssd->channels[chan]->chips == NULL){
            kfree_wrap(ssd->channels[chan]);
            continue;
        }
        for(chip = 0; chip < ssd->channels[chan]->chipNUM; chip++){
            unsigned int order;
            if(ssd->channels[chan]->chips[chip] == NULL){
                continue;
            }
            if(ssd->channels[chan]->chips[chip]->data == NULL){
                kfree_wrap(ssd->channels[chan]->chips[chip]);
                continue;
            }
            order = get_order(ssd->channels[chan]->chips[chip]->dataLen);
            free_pages((unsigned long)ssd->channels[chan]->chips[chip]->page, order);
            kfree_wrap(ssd->channels[chan]->chips[chip]);
        }
        kfree_wrap(ssd->channels[chan]->chips);
        kfree_wrap(ssd->channels[chan]);
    }
    if(ssd->channels){
        kfree_wrap(ssd->channels);
    }
    kfree_wrap(ssd);
}