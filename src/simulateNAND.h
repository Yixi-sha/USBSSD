#ifndef __SIMULATE_NAMD_USBSSD_H__
#define __SIMULATE_NAMD_USBSSD_H__

#include <linux/mutex.h>
#include <linux/timer.h>

#define PAGE_SIZE_USBSSD 2048

typedef struct Page_IN_Cache_{
    unsigned int order;
    struct page *page;
    unsigned char *data;
    unsigned long long len;
}Page_IN_Cache;

typedef struct Chip_HW_USBSSD_{
    unsigned int state;
    struct timer_list timer;
    struct mutex Mutex;

    unsigned long long ID;
    int dataLen;
    Page_IN_Cache *pages;
    
    int chanelN;
    int chipN;

    int preCMDDie;
    int preCMDPlane;
    int preCMDBlock;
    int preCMDPage;
}Chip_HW_USBSSD;

typedef struct Channel_HW_USBSSD_{
    unsigned int state;
    struct timer_list timer;
    struct mutex Mutex;

    int chanelN;

    int chipNUM;
    Chip_HW_USBSSD **chips;
}Channel_HW_USBSSD;

typedef struct SSD_HW_USBSSD_{

    int channelNUM;
    Channel_HW_USBSSD **channels;
}SSD_HW_USBSSD;

int init_Simulate_USBSSD(void);
void destory_Simulate_USBSSD(void);
int send_cmd_2_NAND(int CMD, int chan, int chip, int die, int plane, int block, int page, int start, int len,unsigned char *buf, unsigned long long ID);

#endif