#ifndef __COMMAND_USBSSD_H__
#define __COMMAND_USBSSD_H__

#include "allocator.h"
#include "sub_request.h"

#define RESERVE_RATE 10
#define GC_RATE 10

#define ERASE 2


#define CMD_PROGRAM_ADDR_DATE_TRANSFERRED 100

#define CMD_READ_ADDR_TRANSFERRED 200
#define CMD_READ_DATA_TRANSFERRED 201

#define CMD_ERASE_TRANSFERRED 300

#define IDEL_HW 0

typedef struct Command_USBSSD_{
    unsigned char operation;
    void *subReqs;
    struct Command_USBSSD_ *next;
    unsigned long long ID;
    struct Command_USBSSD_ *related;

    Inter_Allocator_USBSSD allocator;
}Command_USBSSD;

#define FREE_HW_PAGE_STATE 0
#define VALID_HW_PAGE_STATE 1
#define INVALID_HW_PAGE_STATE 2


typedef struct Page_USBSSD_{
    unsigned char state; // if a subpage is valid, it is valid it is valid. 
                        //if all subpage is free, it is valid it is free.
                        //if all subpage is invalid, it is valid it is invalid.
    unsigned long long lpn;
}Page_USBSSD;

typedef struct Block_USBSSD_{
    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    unsigned long long startWrite;
    Page_USBSSD **pageInfos;
}Block_USBSSD;

typedef struct Plane_USBSSD_{
    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    unsigned long long activeBlock;
    PPN_USBSSD eraseLocation;
    unsigned char eraseUsed;

    Block_USBSSD **blockInfos;
}Plane_USBSSD;

typedef struct Die_USBSSD_{
    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    int planeToken;

    Plane_USBSSD **planeInfos;
}Die_USBSSD;

typedef struct Chip_USBSSD_{
    unsigned int state;

    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    Command_USBSSD *incompletedR;

    Command_USBSSD *commands;
    Command_USBSSD *commandsTail;

    Command_USBSSD *eraseCommands;
    Command_USBSSD *eraseCommandsTail;

    SubRequest_USBSSD *rHead;
    SubRequest_USBSSD *rTail;

    SubRequest_USBSSD *wHead;
    SubRequest_USBSSD *wTail;
    int dieToken;
    Die_USBSSD **dieInfos;
}Chip_USBSSD;

typedef struct Channle_USBSSD_{
    unsigned int state;

    unsigned long long freePageCount;
    unsigned long long validPageCount;
    unsigned long long invalidPageCount;

    int chipToken;

    Chip_USBSSD **chipInfos;
}Channle_USBSSD;

typedef struct USBSSD_USBSSD_{
    int channelCount;
    int chipOfChannel;
    int dieOfChip;
    int planeOfDie;
    int blockOfPlane;
    int pageOfBlock;
    int subpageOfPage;

    int channelToken;

    Channle_USBSSD **channelInfos;
}USBSSD_USBSSD;

typedef struct mapEntry_USBSSD_{
    unsigned long long subPage; // if == 0, it is invalid.
    unsigned long long ppn; // for HW page
}mapEntry_USBSSD; 

int init_Command_USBSSD(void);
void destory_Command_USBSSD(void);

unsigned long long get_capacity_USBSSD(void);
int setup_USBSSD(void);
void destory_USBSSD(void);


void add_subReqs_to_chip(SubRequest_USBSSD *head);

int get_PPN_USBSSD(unsigned long long lpn, mapEntry_USBSSD *ret);
int get_PPN_Detail_USBSSD(unsigned long long ppn, PPN_USBSSD *ppn_USBSSD);
void allocate_location(PPN_USBSSD *location);
unsigned long long get_PPN_From_Detail_USBSSD(PPN_USBSSD *ppn_USBSSD);

void allocate_command_USBSSD(void);


typedef struct Operation_CMD_{
    int CMD;
    int chan;
    int chip;
    int die;
    int plane;
    int block;
    int page;
    int start;
    int len;
    unsigned char *buf;
}Operation_CMD;


#endif