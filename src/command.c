#include "command.h"

static Allocator_USBSSD *allocator = NULL;
static unsigned long long all_capacity_USBSSD = 0;
static unsigned long long usable_capacity_USBSSD = 0;
static USBSSD_USBSSD *usbssd = NULL;
static struct mutex usbssdMutex;
static mapEntry_USBSSD *map;
static struct mutex mapMutex;

static unsigned long long mapSize = 0;

unsigned long long get_capacity_USBSSD(void){
    return usable_capacity_USBSSD;
}

void allocate_command_USBSSD(void){
    int chan = 0;
    if(!allocator || !usbssd){
        return;
    }

    mutex_lock(&usbssdMutex);
    for(chan = 0; chan < usbssd->channelCount; chan++){
        if(usbssd->channelInfos[chan].state == IDLE_HW_CHANNEL_STATE){
            int chip = 0;
            for(chip = 0; chip < usbssd->chipOfChannel; chip++){
                if(usbssd->channelInfos[chan].chipInfos[chip].state == IDLE_HW_CHIP_STATE){

                }
            }
        }
    }

}

int get_PPN_USBSSD(unsigned long long lpn, mapEntry_USBSSD *ret){
    if(lpn >= mapSize || lpn < 0 || !ret){
        return -1;
    }
    mutex_lock(&mapMutex);
    ret->subPage = map[lpn].subPage;
    memcpy(&(ret->ppn), &(map[lpn].ppn), sizeof(ret->ppn));
    mutex_unlock(&mapMutex);
    return 0;
}

void init_Command_USBSSD(void){
    Command_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(Command_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    mutex_init(&mapMutex);
    mutex_init(&usbssdMutex);
}

void destory_Command_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}

void setup_USBSSD(void){
    usbssd = NULL;
    all_capacity_USBSSD = 0;
    //set usbssd and map
}