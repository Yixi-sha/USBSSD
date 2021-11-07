#include "command.h"

static Allocator_USBSSD *allocator = NULL;
static unsigned long long all_capacity_USBSSD = 0;
static unsigned long long usable_capacity_USBSSD = 0;
static USBSSD_USBSSD *usbssd = NULL;
static struct mutex usbssdMutex;
static mapEntry_USBSSD *map;
static unsigned long long mapSize = 0;
static struct mutex mapMutex;
static struct mutex commandIDMutex;

static unsigned long long commandID = 0;

unsigned long long get_commandID(void){
    unsigned long long  ret;
    mutex_lock(&commandIDMutex);
    ret = commandID++;
    mutex_unlock(&commandIDMutex);
    return ret;
}

unsigned long long get_capacity_USBSSD(void){
    return usable_capacity_USBSSD;
}

static void send_command(Command_USBSSD *command){
    if(command->operation == ERASE){
        //send write + read
        //update
        //send erase
        //update meta
    }else if(command->operation == WRITE){
        // send write
        //update meta
    }else{
        // send read
        //update meta
    }
}

void command_end(int commandID){
    Command_USBSSD *command = NULL, *pre = NULL;
    int chann, chip;
    int i, j;
    mutex_lock(&usbssdMutex);
    for(i = 0; i < usbssd->channelCount; i++){
        for(j = 0; j < usbssd->chipOfChannel; j++){
            command = usbssd->channelInfos[i].chipInfos[j].eraseCommands;
            pre = NULL;
            while(command){
                if(command->ID == commandID){
                    chann = i;
                    chip = j;
                    break;
                }
                pre = command;
                command = command->next;
            }
            command = usbssd->channelInfos[i].chipInfos[j].commands;
            while(command){
                if(command->ID == commandID){
                    chann = i;
                    chip = j;
                    break;
                }
                pre = command;
                command = command->next;
            }
        }
        if(command){
            break;
        }
    }

    if(command->operation == ERASE){

    }else{
        SubRequest_USBSSD *sub = command->subReqs;
        if(pre == NULL){
            usbssd->channelInfos[chann].chipInfos[chip].commands = command->next;
        }else{
            pre->next = command->next;
        }
        if(usbssd->channelInfos[chann].chipInfos[chip].commandsTail == command){
            usbssd->channelInfos[chann].chipInfos[chip].commandsTail = pre;
        }
        free_USBSSD(allocator, command);
        subRequest_End(sub);
    }

    mutex_unlock(&usbssdMutex);

    allocate_command_USBSSD();
}

void allocate_command_USBSSD(void){
    int i;
    
    if(!allocator || !usbssd){
        return;
    }

    mutex_lock(&usbssdMutex);

    for(i = 0; i < usbssd->channelCount; i++){
        int j;
        for(j = 0; j < usbssd->chipOfChannel && usbssd->channelInfos[i].state == IDLE_HW_CHANNEL_STATE; j++){
            if(usbssd->channelInfos[i].chipInfos[j].state == IDLE_HW_CHIP_STATE){
                SubRequest_USBSSD *sub = NULL;
                if(usbssd->channelInfos[i].chipInfos[j].eraseCommands){
                    send_command(usbssd->channelInfos[i].chipInfos[j].eraseCommands);
                    usbssd->channelInfos[i].state = BUSY_HW_CHANNEL_STATE;
                    usbssd->channelInfos[i].chipInfos[j].state = BUSY_HW_CHIP_STATE;

                    continue;
                }
                if((sub = get_SubRequests_Read_USBSSD(i, j)) != NULL || (sub = get_SubRequests_Write_USBSSD(i, j)) != NULL){
                    Command_USBSSD *command = allocate_USBSSD(allocator);
                    command->operation = sub->operation;
                    command->subReqs = sub;
                    command->ID = get_commandID();
                    command->next = NULL;

                    if(usbssd->channelInfos[i].chipInfos[j].commandsTail){
                        usbssd->channelInfos[i].chipInfos[j].commandsTail->next = command;
                    }else{
                        usbssd->channelInfos[i].chipInfos[j].commands = command;
                    }
                    usbssd->channelInfos[i].chipInfos[j].commandsTail = command;
                    send_command(command);
                    usbssd->channelInfos[i].state = BUSY_HW_CHANNEL_STATE;
                    usbssd->channelInfos[i].chipInfos[j].state = BUSY_HW_CHIP_STATE;
                }
            }
        }
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

void allocate_location(PPN_USBSSD *location){
    Plane_USBSSD *plane;

    mutex_lock(&usbssdMutex);

    location->channel = usbssd->channelToken;
    if(++usbssd->channelToken == usbssd->channelCount){
        usbssd->channelToken = 0;
    }

    location->chip = usbssd->channelInfos[location->channel].chipToken;
    if(++usbssd->channelInfos[location->channel].chipToken == usbssd->chipOfChannel){
        usbssd->channelInfos[location->channel].chipToken = 0;
    }
    
    location->die = usbssd->channelInfos[location->channel].chipInfos[location->chip].dieToken;
    if(++usbssd->channelInfos[location->channel].chipInfos[location->chip].dieToken == usbssd->dieOfChip){
        usbssd->channelInfos[location->channel].chipInfos[location->chip].dieToken = 0;
    }

    location->plane = usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].planeToken;
    if(++usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].planeToken == usbssd->planeOfDie){
        usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].planeToken = 0;
    }

    usbssd->channelInfos[location->channel].freePageCount--;
    usbssd->channelInfos[location->channel].validPageCount++;
    
    usbssd->channelInfos[location->channel].chipInfos[location->chip].freePageCount--;
    usbssd->channelInfos[location->channel].chipInfos[location->chip].validPageCount++;

    usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].freePageCount--;
    usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].validPageCount++;

    usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].planeInfos[location->plane].freePageCount--;
    usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].planeInfos[location->plane].validPageCount++;
    
    plane = &(usbssd->channelInfos[location->channel].chipInfos[location->chip].dieInfos[location->die].planeInfos[location->plane]);

    location->block = plane->activeBlock;
    location->page = plane->blockInfos[location->block].startWrite;

    plane->blockInfos[location->block].freePageCount--;
    plane->blockInfos[location->block].validPageCount++;

    if(plane->blockInfos[location->block].freePageCount == 0){
        int activeBlock = plane->activeBlock;
        while(plane->blockInfos[activeBlock].freePageCount != usbssd->pageOfBlock){
            if(++activeBlock == usbssd->blockOfPlane){
                activeBlock = 0;
            }
        }
        plane->activeBlock = activeBlock;
    }

    location->page = plane->blockInfos[location->block].startWrite;
    plane->blockInfos[location->block].startWrite++;

    if(plane->freePageCount <=  (usbssd->pageOfBlock * usbssd->blockOfPlane / 10)  && plane->eraseUsed == 0){
        int targetBlcok = -1;
        int i = 0;

        
        plane->eraseLocation.channel = location->channel;
        plane->eraseLocation.chip = location->chip;
        plane->eraseLocation.die = location->die;
        plane->eraseLocation.plane = location->plane;

        for(i = 0; i < usbssd->blockOfPlane; ++i){
            if(plane->blockInfos[i].freePageCount == 0 && \
            (plane->blockInfos[i].invalidPageCount != 0 && (targetBlcok == -1 || plane->blockInfos[i].invalidPageCount > plane->blockInfos[targetBlcok].invalidPageCount))){
                targetBlcok = i;
                if(plane->blockInfos[targetBlcok].invalidPageCount == usbssd->pageOfBlock){
                    break;
                }
            }
        }

        if(targetBlcok != -1){
            Command_USBSSD *command = allocate_USBSSD(allocator);
            plane->eraseLocation.block = targetBlcok;
            plane->eraseUsed = 1;

            command->operation = ERASE;
            command->subReqs = &(plane->eraseLocation);
            command->ID = get_commandID();
            command->next = NULL;

            if(usbssd->channelInfos[location->channel].chipInfos[location->chip].eraseCommandsTail){
                usbssd->channelInfos[location->channel].chipInfos[location->chip].eraseCommandsTail->next = command;
            }else{
                usbssd->channelInfos[location->channel].chipInfos[location->chip].eraseCommands = command;
            }
            usbssd->channelInfos[location->channel].chipInfos[location->chip].eraseCommandsTail = command;
        }
    }

    mutex_unlock(&usbssdMutex);
}

unsigned long long page_no_per_channel, page_no_per_chip, page_no_per_die, page_no_per_plane, pages_no_per_block, pages_no_per_block;


int get_PPN_Detail_USBSSD(unsigned long long ppn, PPN_USBSSD *ppn_USBSSD){
    if(ppn >= mapSize || ppn < 0 || !ppn_USBSSD){
        return -1;
    }

    ppn_USBSSD->channel = (ppn / page_no_per_channel);
    ppn_USBSSD->chip = ((ppn % page_no_per_channel) / page_no_per_chip);
    ppn_USBSSD->die = (((ppn % page_no_per_channel) % page_no_per_chip) / page_no_per_die);
    ppn_USBSSD->plane = ((((ppn % page_no_per_channel) % page_no_per_chip) % page_no_per_die) / page_no_per_plane);
    ppn_USBSSD->block = (((((ppn % page_no_per_channel) % page_no_per_chip) % page_no_per_die) % page_no_per_plane) / pages_no_per_block);
    ppn_USBSSD->page = ((((((ppn % page_no_per_channel) % page_no_per_chip) % page_no_per_die) % page_no_per_plane) % pages_no_per_block) % pages_no_per_block);
    return 0;
}

unsigned long long get_PPN_From_Detail_USBSSD(PPN_USBSSD *ppn_USBSSD){
    return page_no_per_channel * ppn_USBSSD->channel + page_no_per_chip * ppn_USBSSD->chip + \
           page_no_per_die * ppn_USBSSD->die + page_no_per_plane * ppn_USBSSD->plane + \
           pages_no_per_block * ppn_USBSSD->block + ppn_USBSSD->page;
}

void init_Command_USBSSD(void){
    Command_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(Command_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    mutex_init(&mapMutex);
    mutex_init(&usbssdMutex);
    mutex_init(&commandIDMutex);
}

void destory_Command_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}

void setup_USBSSD(void){
    usbssd = NULL;
    all_capacity_USBSSD = 0;
    //set usbssd and map
}