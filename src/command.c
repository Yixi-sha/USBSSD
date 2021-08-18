#include "command.h"

static Allocator_USBSSD *allocator = NULL;
static unsigned long long all_capacity_USBSSD = 0;
static unsigned long long usable_capacity_USBSSD = 0;
static USBSSD_USBSSD *usbssd = NULL;
static struct mutex usbssdMutex;
static mapEntry_USBSSD *map;
static unsigned long long mapSize = 0;
static struct mutex mapMutex;



unsigned long long get_capacity_USBSSD(void){
    return usable_capacity_USBSSD;
}

static void generate_Read_Command(int chan){
    SubRequest_USBSSD *iterRead = NULL, *subReads = NULL;
    
    for(iterRead = get_SubRequests_Read_Start_USBSSD(); iterRead; iterRead = get_SubRequests_Read_Iter_USBSSD(iterRead)){
        if(map[iterRead->lpn].ppn.channel == chan){
            int chip = map[iterRead->lpn].ppn.chip;
            if(usbssd->channelInfos[chan].chipInfos[chip].state == IDLE_HW_CHIP_STATE){
                subReads = get_SubRequests_Read_Get_USBSSD(iterRead);
                break;
            }
        }
    }
    get_SubRequests_Read_End_USBSSD();
    if(subReads){

    }
}

void allocate_command_USBSSD(void){
    int chan = -1;
    int i;
    
    if(!allocator || !usbssd){
        return;
    }

    mutex_lock(&usbssdMutex);

    for(i = 0; i < usbssd->channelCount; i++){
        int j;
        for(j = 0; j < usbssd->channelCount; j++){
            if(usbssd->channelInfos[j].state == IDLE_HW_CHANNEL_STATE){
                int x;
                for(x = 0; x < usbssd->chipOfChannel; x++){
                    if(usbssd->channelInfos[j].chipInfos[x].state == IDLE_HW_CHIP_STATE){
                        break;
                    }
                }
                if(x != usbssd->chipOfChannel && \
                (chan == -1 || usbssd->channelInfos[chan].validPageCount > usbssd->channelInfos[j].validPageCount)){
                    chan = j;
                }
            }
        }
        if(chan == -1){
            break;
        }
        generate_Read_Command(chan);
        
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