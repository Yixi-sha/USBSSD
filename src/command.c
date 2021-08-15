#include "command.h"

static Allocator_USBSSD *allocator = NULL;
static unsigned long long all_capacity_USBSSD = 0;
static unsigned long long usable_capacity_USBSSD = 0;
static USBSSD_USBSSD *usbssd = NULL;
static mapEntry_USBSSD *map;
static struct mutex mapMutex;

static unsigned long long mapSize = 0;

unsigned long long get_capacity_USBSSD(void){
    return usable_capacity_USBSSD;
}

int get_PPN_USBSSD(unsigned long long lpn, mapEntry_USBSSD *ret){
    if(lpn >= mapSize || lpn < 0 || !ret){
        return -1;
    }
    mutex_lock(&mapMutex);
    ret->ppn = map[lpn].ppn;
    ret->subPage = map[lpn].subPage;
    mutex_unlock(&mapMutex);
    return 0;
}

void init_Command_USBSSD(void){
    Command_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(Command_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    mutex_init(&mapMutex);
}

void destory_Command_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}

void setup_USBSSD(void){
    usbssd = NULL;
    all_capacity_USBSSD = 0;
    //set usbssd and map
}