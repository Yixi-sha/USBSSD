#ifndef __COMMAND_USBSSD_H__
#define __COMMAND_USBSSD_H__

#include "allocator.h"
#include "sub_request.h"

#define PAGE_SIZE_HW_USBSSD 4096
#define RESERVE_RATE 0.1

typedef struct PPN_Info_USBSSD_{
    unsigned long long channel;
    unsigned long long chip;
    unsigned long long die;
    unsigned long long plane;
    unsigned long long block;
    unsigned long long page;
    unsigned long long subPage;
}PPN_Info_USBSSD;

typedef struct Command_USBSSD_{
    unsigned long long lpn;
    PPN_Info_USBSSD ppnInfo;
    unsigned char operation;

    SubRequest_USBSSD *subReqs;
    Inter_Allocator_USBSSD allocator;
}Command_USBSSD;

#define FREE_HW_PAGE_STATE 0
#define VALID_HW_PAGE_STATE 1
#define INVALID_HW_PAGE_STATE 2

typedef struct Subpage_USBSSD_{
    unsigned char state;
    unsigned long long lpn;
}Subpage_USBSSD;

typedef struct Page_USBSSD_{
    unsigned char state; // if a subpage is valid, it is valid it is valid. 
                        //if all subpage is free, it is valid it is free.
                        //if all subpage is invalid, it is valid it is invalid.
    Subpage_USBSSD* subpageInfos;
}Page_USBSSD;

typedef struct Block_USBSSD_{
    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    Page_USBSSD* pageInfos;
}Block_USBSSD;

typedef struct Plane_USBSSD_{
    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    Block_USBSSD* blockInfos;
}Plane_USBSSD;

typedef struct Die_USBSSD_{
    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    Plane_USBSSD* planeInfos;
}Die_USBSSD;


#define IDLE_HW_CHIP_STATE 0
#define BUSY_HW_CHIP_STATE 1

typedef struct Chip_USBSSD_{
    unsigned char state;

    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    Command_USBSSD *chipCommand;

    Die_USBSSD* dieInfos;
}Chip_USBSSD;

#define IDLE_HW_CHANNEL_STATE 0
#define BUSY_HW_CHANNEL_STATE 1

typedef struct Channle_USBSSD_{
    unsigned char state;

    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    Chip_USBSSD* chipInfos;
}Channle_USBSSD;

typedef struct USBSSD_USBSSD_{
    int channelCount;
    int chipOfChannel;
    int dieOfChip;
    int planeOfDie;
    int blockOfPlane;
    int pageOfBlock;
    int subpageOfPage;

    Channle_USBSSD* channelInfos;
}USBSSD_USBSSD;

typedef struct mapEntry_USBSSD_{
    int subPage; // if < 0, it is invalid.
    PPN_Info_USBSSD ppn; // for HW page
}mapEntry_USBSSD; 

void init_Command_USBSSD(void);
void destory_Command_USBSSD(void);

unsigned long long get_capacity_USBSSD(void);
void setup_USBSSD(void);
int get_PPN_USBSSD(unsigned long long lpn, mapEntry_USBSSD *ret);

void allocate_command_USBSSD(void);

#endif