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

int generate_Command(SubRequest_USBSSD *subs, int chan, int chip){
    Command_USBSSD *command;
    if(!subs){
        return -1;
    }
    command = allocate_USBSSD(allocator);
    if(!command){
        return -1;
    }
    command->operation = subs->operation;
    command->subReqs = subs;
    if(subs->operation == READ_USBSSD){
        command->ppnInfo.channel = chan;
        command->ppnInfo.chip = chip;
        command->ppnInfo.die = map[subs->lpn].ppn.die;
        command->ppnInfo.plane = map[subs->lpn].ppn.plane;
        command->ppnInfo.block = map[subs->lpn].ppn.block;
        command->ppnInfo.page = map[subs->lpn].ppn.page;
    }else{
        int i, t = -1;
        Plane_USBSSD *plane;
        for(i = 0; i < usbssd->dieOfChip; i++){
            if(t == -1 || usbssd->channelInfos[chan].chipInfos[chip].dieInfos[i].validPageCount < usbssd->channelInfos[chan].chipInfos[chip].dieInfos[t].validPageCount){
                t = i;
            }
        }
        command->ppnInfo.die = t;
        t = -1;
        for(i = 0; i < usbssd->planeOfDie; i++){
            if(t == -1 || \
            usbssd->channelInfos[chan].chipInfos[chip].dieInfos[command->ppnInfo.die].planeInfos[i].validPageCount <  usbssd->channelInfos[chan].chipInfos[chip].dieInfos[command->ppnInfo.die].planeInfos[t].validPageCount){
                t = i;
            }
        }
        command->ppnInfo.plane = t;
        plane = &(usbssd->channelInfos[chan].chipInfos[chip].dieInfos[command->ppnInfo.die].planeInfos[t]);
        while(plane->blockInfos[plane->activeBlock].freePageCount == 0){
            if(++plane->activeBlock == usbssd->blockOfPlane){
                plane->activeBlock = 0;
            }
        }
        command->ppnInfo.block = plane->activeBlock;
        i = plane->blockInfos[plane->activeBlock].startWrite;
        while(plane->blockInfos[plane->activeBlock].pageInfos[i].state != FREE_HW_PAGE_STATE){
            if(++i == usbssd->pageOfBlock){
                i = 0;
            }
        }
        command->ppnInfo.page = i;
    }

    //send comand
    
    return 0;
}

static void generate_Read_Command(int chan){
    SubRequest_USBSSD *iterRead = NULL, *subReads = NULL;
    int chip;
    for(iterRead = get_SubRequests_Read_Start_USBSSD(); iterRead; iterRead = get_SubRequests_Read_Iter_USBSSD(iterRead)){
        if(map[iterRead->lpn].ppn.channel == chan){
            chip = map[iterRead->lpn].ppn.chip;
            if(usbssd->channelInfos[chan].chipInfos[chip].state == IDLE_HW_CHIP_STATE){
                subReads = get_SubRequests_Read_Get_USBSSD(iterRead);
                break;
            }
        }
    }
    get_SubRequests_Read_End_USBSSD();
    if(subReads){
        generate_Command(subReads, chan, chip);
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
        if(usbssd->channelInfos[chan].state == IDLE_HW_CHANNEL_STATE){
            int j, chip = -1;
            for(j = 0; j < usbssd->chipOfChannel; j++){
                if(usbssd->channelInfos[chan].chipInfos[j].state == IDLE_HW_CHIP_STATE){
                    if(chip == -1 || usbssd->channelInfos[chan].chipInfos[j].freePageCount < usbssd->channelInfos[chan].chipInfos[chip].freePageCount){
                        chip = j;
                    }
                }
            }
            if(chip != -1){
                SubRequest_USBSSD *subWrites = get_SubRequests_Write_USBSSD();
                if(subWrites){
                    generate_Command(subWrites, chan, chip);
                }
            }
        }
    }
    
    if(get_SubRequests_Read_Start_USBSSD()){
        get_SubRequests_Read_End_USBSSD();
        for(i = 0; i < usbssd->channelCount; i++){
            if(usbssd->channelInfos[i].state == IDLE_HW_CHANNEL_STATE){
                int j;
                for(j = 0; j < usbssd->chipOfChannel; j++){
                    if(usbssd->channelInfos[i].chipInfos[j].state == IDLE_HW_CHIP_STATE){
                        break;
                    }
                }
                if(j != usbssd->chipOfChannel){
                   generate_Read_Command(j); 
                }
            }
        }
    }else{
        get_SubRequests_Read_End_USBSSD();
    }
    mutex_unlock(&usbssdMutex);
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