#include "command.h"
#include "simulateNAND.h"

static Allocator_USBSSD *allocator = NULL;
static unsigned long long all_capacity_USBSSD = 0;
static unsigned long long usable_capacity_USBSSD = 0;
static USBSSD_USBSSD *usbssd = NULL;
static struct mutex usbssdMutex;
static struct mutex usbssdListMutex;
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



static void send_write_command(Command_USBSSD *command){
    int start = -1, len = 0;
    int i = 0;
    SubRequest_USBSSD *sub = command->subReqs;
    unsigned char *buf = sub->buf;
    allocate_location_in_plane(&(sub->location));
    for(i = 0; i < sizeof(sub->bitMap) * 8; i++){
        if(sub->bitMap & (1ULL << i)){
            if(start == -1){
                start = i * 512;
            }
            len += 512;
        }else if(start != -1){
            break;
        }
    }
    buf += start;

    send_cmd_2_NAND(CMD_PROGRAM_ADDR_DATE_TRANSFERRED, sub->location.channel, sub->location.chip, sub->location.die, \
    sub->location.plane, sub->location.block, sub->location.page, start, len, buf, command->ID);
}

static void send_erase_command(Command_USBSSD *command){
    PPN_USBSSD *location = command->subReqs;
    send_cmd_2_NAND(CMD_ERASE_TRANSFERRED, location->channel,location->chip, location->die, \
    location->plane, location->block, location->page, 0, 0, NULL, command->ID);
}

static void send_read_command(Command_USBSSD *command){
    SubRequest_USBSSD *sub = command->subReqs;
    
    send_cmd_2_NAND(CMD_READ_ADDR_TRANSFERRED, sub->location.channel, sub->location.chip, sub->location.die, \
    sub->location.plane, sub->location.block, sub->location.page, 0, 0, NULL, command->ID);
}

static void send_read_command_stage2(Command_USBSSD *command){
    int start = -1, len = 0;
    int i = 0;
    SubRequest_USBSSD *sub = command->subReqs;
    unsigned char *buf = sub->buf;
    
    for(i = 0; i < sizeof(sub->bitMap) * 8; i++){
        if(sub->bitMap & (1ULL << i)){
            if(start == -1){
                start = i * 512;
            }
            len += 512;
        }else if(start != -1){
            break;
        }
    }
    buf += start;
    send_cmd_2_NAND(CMD_READ_DATA_TRANSFERRED, sub->location.channel, sub->location.chip, sub->location.die, \
    sub->location.plane, sub->location.block, sub->location.page, start, len, buf, command->ID);
}

static void send_command(Command_USBSSD *command){
    PPN_USBSSD *location = NULL;
    
    switch (command->operation){
        case WRITE:
            location = &(((SubRequest_USBSSD*)command->subReqs)->location);
            usbssd->channelInfos[location->channel]->state = CMD_PROGRAM_ADDR_DATE_TRANSFERRED;
            usbssd->channelInfos[location->channel]->chipInfos[location->chip]->state = CMD_PROGRAM_ADDR_DATE_TRANSFERRED;
            send_write_command(command);
            break;
            
        case READ:
            location = &(((SubRequest_USBSSD*)command->subReqs)->location);
            
            if(command->stateEpage.state == 0){
                usbssd->channelInfos[location->channel]->state = CMD_READ_ADDR_TRANSFERRED;
                usbssd->channelInfos[location->channel]->chipInfos[location->chip]->state = CMD_READ_ADDR_TRANSFERRED;
                send_read_command(command);
            }
            else{
                usbssd->channelInfos[location->channel]->state = CMD_READ_DATA_TRANSFERRED;
                usbssd->channelInfos[location->channel]->chipInfos[location->chip]->state = CMD_READ_DATA_TRANSFERRED;
                send_read_command_stage2(command);
            }
            break;
        
        case ERASE:
            location = ((PPN_USBSSD*)command->subReqs);
            usbssd->channelInfos[location->channel]->state = CMD_ERASE_TRANSFERRED;
            usbssd->channelInfos[location->channel]->chipInfos[location->chip]->state = CMD_ERASE_TRANSFERRED;
            send_erase_command(command);
            break;
        default:
            break;
    }
}

static Command_USBSSD *get_end_CMD(int chan, int chip, int commandID, int CMD){
    Command_USBSSD *endCommand = NULL,**begin = NULL, **end = NULL;
    switch (CMD){
        case CMD_PROGRAM_ADDR_DATE_TRANSFERRED:
        case CMD_READ_ADDR_TRANSFERRED:
            begin = &(usbssd->channelInfos[chan]->chipInfos[chip]->commands);
            end = &(usbssd->channelInfos[chan]->chipInfos[chip]->commandsTail);
            break;
        
        case CMD_READ_DATA_TRANSFERRED:
            begin = &(usbssd->channelInfos[chan]->chipInfos[chip]->incompletedR);
            end = &(usbssd->channelInfos[chan]->chipInfos[chip]->incompletedRTail);

            break;
        
        case CMD_ERASE_TRANSFERRED:
            begin = &(usbssd->channelInfos[chan]->chipInfos[chip]->eraseCommands);
            end = &(usbssd->channelInfos[chan]->chipInfos[chip]->eraseCommandsTail);
            break;
    }
    if((*begin)->ID == commandID){
        endCommand = (*begin);
        (*begin) = endCommand->next;

        if(endCommand == (*end)){
            (*end) = NULL;
        }
    }else{
        Command_USBSSD *iter = (*begin);
        while(iter && iter->next->ID != commandID){
            iter = iter->next;
        }
        endCommand = iter->next;
        iter->next = endCommand->next;

        if(endCommand == (*end)){
            (*end) = iter;
        }
    }

    return endCommand;
}

static void erase_block(PPN_USBSSD *location){
    Plane_USBSSD *plane;
    int i;
    usbssd->channelInfos[location->channel]->invalidPageCount -= usbssd->pageOfBlock;
    usbssd->channelInfos[location->channel]->freePageCount += usbssd->pageOfBlock;

    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->invalidPageCount -= usbssd->pageOfBlock;
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->freePageCount += usbssd->pageOfBlock;

    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->invalidPageCount -= usbssd->pageOfBlock;
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->freePageCount += usbssd->pageOfBlock;

    plane = (usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]);

    plane->invalidPageCount -= usbssd->pageOfBlock;
    plane->freePageCount += usbssd->pageOfBlock;

    plane->blockInfos[location->block]->invalidPageCount = 0;
    plane->blockInfos[location->block]->freePageCount = usbssd->pageOfBlock;
    plane->blockInfos[location->block]->validPageCount = 0;
    plane->blockInfos[location->block]->startWrite = 0;

    for(i = 0; i < usbssd->pageOfBlock; i++){
        plane->blockInfos[location->block]->pageInfos[i]->state = 0;
    }
}

static void invilate_page(PPN_USBSSD *location){
    Plane_USBSSD *plane;

    usbssd->channelInfos[location->channel]->validPageCount--;
    usbssd->channelInfos[location->channel]->invalidPageCount++;

    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->validPageCount--;
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->invalidPageCount++;

    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->validPageCount--;
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->invalidPageCount++;


    plane = (usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]);
    plane->invalidPageCount++;
    plane->validPageCount--;

    plane->blockInfos[location->block]->invalidPageCount++;
    plane->blockInfos[location->block]->validPageCount--;

    plane->blockInfos[location->block]->pageInfos[location->page]->state = 0;

}

static void update_Map_and_invilate_page(Command_USBSSD *command){
    SubRequest_USBSSD *sub = (SubRequest_USBSSD*)command->subReqs;
    PPN_USBSSD prelocation;
    Page_USBSSD *page;
    unsigned long long prePPN;
    unsigned long long preMap;
    mutex_lock(&mapMutex);
    prePPN = map[sub->lpn].ppn;
    preMap = map[sub->lpn].subPage;
    map[sub->lpn].subPage = sub->bitMap;
    map[sub->lpn].ppn = get_PPN_From_Detail_USBSSD(&sub->location);
    mutex_unlock(&mapMutex);
    page = usbssd->channelInfos[sub->location.channel]->chipInfos[sub->location.chip]->dieInfos[sub->location.die]->planeInfos[sub->location.plane]->blockInfos[sub->location.block]->pageInfos[sub->location.page];
    page->state = sub->bitMap;
    page->lpn = sub->lpn;
    
    if(preMap){
        get_PPN_Detail_USBSSD(prePPN, &prelocation);
        invilate_page(&prelocation);
    }
    
}

static void check(void){
    int chan, chip, die, plane, block;
    mutex_lock(&usbssdMutex);
    mutex_lock(&usbssdListMutex);
    for(chan = 0; chan < usbssd->channelCount; ++chan){
        int chanV = 0, chanI = 0, chanF = 0;
        for(chip = 0; chip < usbssd->chipOfChannel; ++chip){
            int chipV = 0, chipI = 0, chipF = 0;
            for(die = 0; die < usbssd->dieOfChip; ++die){
                int dieV = 0, dieI = 0, dieF = 0;
                for(plane = 0; plane < usbssd->planeOfDie; ++plane){
                    int planeV = 0, planeI = 0, planeF = 0;
                    for(block = 0; block < usbssd->blockOfPlane; ++block){
                        planeF += usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->freePageCount;
                        planeI += usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->invalidPageCount;
                        planeV += usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->validPageCount;
                    }
                    if(planeV != usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->validPageCount){
                        printk("planeV\n");
                    }
                    if(planeF != usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->freePageCount){
                        printk("planeF\n");
                    }
                    if(planeI != usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->invalidPageCount){
                        printk("planeI\n");
                    }
                    dieF += planeF;
                    dieI += planeI;
                    dieV += planeV;
                }
                if(dieV != usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->validPageCount){
                    printk("dieV\n");
                }
                if(dieF != usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->freePageCount){
                    printk("dieF\n");
                }
                if(dieI != usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->invalidPageCount){
                    printk("dieI\n");
                }
                chipF += dieF;
                chipI += dieI;
                chipV += dieV;
            }
            if(chipV != usbssd->channelInfos[chan]->chipInfos[chip]->validPageCount){
                printk("dieV\n");
            }
            if(chipF != usbssd->channelInfos[chan]->chipInfos[chip]->freePageCount){
                printk("dieF\n");
            }
            if(chipI != usbssd->channelInfos[chan]->chipInfos[chip]->invalidPageCount){
                printk("dieI\n");
            }
            chanF += chipF;
            chanI += chipI;
            chanV += chipV;
        }
        if(chanV != usbssd->channelInfos[chan]->validPageCount){
            printk("dieV\n");
        }
        if(chanF != usbssd->channelInfos[chan]->freePageCount){
            printk("dieF\n");
        }
        if(chanI != usbssd->channelInfos[chan]->invalidPageCount){
            printk("dieI\n");
        }
    }
    mutex_unlock(&usbssdListMutex);
    mutex_unlock(&usbssdMutex);
}


void recv_signal_back(int chan, int chip, unsigned char isChan, unsigned long long commandID){
    Command_USBSSD *endCommand = NULL;
    PPN_USBSSD *location = NULL;
    mutex_lock(&usbssdMutex);
    if(isChan){
        usbssd->channelInfos[chan]->state = IDEL_HW;
    }else{
        // printk("commandID is %lld\n", commandID);
        switch (usbssd->channelInfos[chan]->chipInfos[chip]->state){
            case CMD_PROGRAM_ADDR_DATE_TRANSFERRED:
                usbssd->channelInfos[chan]->chipInfos[chip]->state = IDEL_HW;
                endCommand = get_end_CMD(chan, chip, commandID, CMD_PROGRAM_ADDR_DATE_TRANSFERRED);
                update_Map_and_invilate_page(endCommand);
                endCommand->completed = 1;
                break;

            case CMD_READ_ADDR_TRANSFERRED :
                usbssd->channelInfos[chan]->chipInfos[chip]->state = IDEL_HW;
                endCommand = get_end_CMD(chan, chip, commandID, CMD_READ_ADDR_TRANSFERRED);
                endCommand->next = NULL;
                if(usbssd->channelInfos[chan]->chipInfos[chip]->incompletedR == NULL){
                    usbssd->channelInfos[chan]->chipInfos[chip]->incompletedR = endCommand;
                }else{
                    usbssd->channelInfos[chan]->chipInfos[chip]->incompletedRTail->next = endCommand;
                }
                usbssd->channelInfos[chan]->chipInfos[chip]->incompletedRTail = endCommand;
                endCommand->stateEpage.state = 1;
                endCommand = NULL;
                break;
            
            case CMD_READ_DATA_TRANSFERRED:
                usbssd->channelInfos[chan]->chipInfos[chip]->state = IDEL_HW;
                endCommand = get_end_CMD(chan, chip, commandID, CMD_READ_DATA_TRANSFERRED);
                endCommand->completed = 1;
                break;
            
            case CMD_ERASE_TRANSFERRED:
                usbssd->channelInfos[chan]->chipInfos[chip]->state = IDEL_HW;
                endCommand = get_end_CMD(chan, chip, commandID, CMD_ERASE_TRANSFERRED);
                location = (PPN_USBSSD *)endCommand->subReqs;
                erase_block(location);
                usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[location->die]->planeInfos[location->plane]->eraseUsed = 0;
                endCommand->completed = 1;
                break;
            
            default:
                break;
        }
    }
    mutex_unlock(&usbssdMutex);
    // check();
    if(endCommand && endCommand->related){
        // allocate_command_USBSSD();
        // printk("end sub\n");
        // return;
    }else if(endCommand && endCommand->operation == ERASE){
        // printk("erase end\n");
    }else if(endCommand && endCommand->related == NULL){
        SubRequest_USBSSD *sub = endCommand->subReqs;
        if(sub->operation == WRITE){
            mutex_lock(&usbssdMutex);
            check_and_create_gc(&(sub->location));
            mutex_unlock(&usbssdMutex);
        }
        subRequest_End(sub); 
        free_USBSSD(allocator, endCommand);
    }
    allocate_command_USBSSD();
}



void recv_signal(int chan, int chip, unsigned char isChan, unsigned long long commandID){
    if(!isChan){
        usbssd->channelInfos[chan]->chipInfos[chip]->commandID = commandID;
        usbssd->channelInfos[chan]->chipInfos[chip]->isChan = isChan;
        schedule_work(&usbssd->channelInfos[chan]->chipInfos[chip]->work);
    }else{
        schedule_work(&usbssd->channelInfos[chan]->work);
    }
}

static void work_func_recv_signal_chan(struct work_struct *work){
    Channle_USBSSD *chanA = container_of(work, Channle_USBSSD, work);
    recv_signal_back(chanA->channelNUM, 0, 1, 0);
}

static void work_func_recv_signal(struct work_struct *work){
    Chip_USBSSD *chipA = container_of(work, Chip_USBSSD, work);
    recv_signal_back(chipA->channelNUM, chipA->chipNUM, chipA->isChan, chipA->commandID);
}

SubRequest_USBSSD *get_SubRequests_Read_USBSSD(int chan, int chip){
    SubRequest_USBSSD *ret;
    mutex_lock(&usbssdListMutex);
    ret = usbssd->channelInfos[chan]->chipInfos[chip]->rHead;
    
    if(ret == NULL){
        mutex_unlock(&usbssdListMutex);
        return NULL;
    }
    usbssd->channelInfos[chan]->chipInfos[chip]->rHead = ret->next;

    if(ret == usbssd->channelInfos[chan]->chipInfos[chip]->rTail){
        usbssd->channelInfos[chan]->chipInfos[chip]->rTail = NULL;
    }
    mutex_unlock(&usbssdListMutex);

    ret->pre = NULL;
    ret->next = NULL;
    
    return ret;
}

SubRequest_USBSSD *get_SubRequests_Write_USBSSD(int chan, int chip){
    SubRequest_USBSSD *iter = NULL, *ret = NULL;
    mutex_lock(&usbssdListMutex);
    iter = usbssd->channelInfos[chan]->chipInfos[chip]->wHead;
    ret = NULL;
    if(iter == NULL){
        mutex_unlock(&usbssdListMutex);
        return NULL;
    }
    if(iter->relatedSub == NULL){
        ret = iter;
        usbssd->channelInfos[chan]->chipInfos[chip]->wHead = ret->next;
    }else{
        while(iter->next){
            if(iter->next->relatedSub == NULL){
                break;
            }
            iter = iter->next;
        }

        if(iter->next){
            ret = iter->next;
            iter->next = ret->next;
        }else{
            mutex_unlock(&usbssdListMutex);
            return NULL;
        }
    }

    if(ret == usbssd->channelInfos[chan]->chipInfos[chip]->wTail){
        usbssd->channelInfos[chan]->chipInfos[chip]->wTail = NULL;
    }
    mutex_unlock(&usbssdListMutex);
    ret->pre = NULL;
    ret->next = NULL;
    return ret;
}

void boost_test_gc(PPN_USBSSD *location){
    Plane_USBSSD *plane;
    int targetBlcok = -1;
    int i = 0;
    mutex_lock(&usbssdMutex);
    plane = (usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]);
    
    plane->eraseLocation.channel = location->channel;
    plane->eraseLocation.chip = location->chip;
    plane->eraseLocation.die = location->die;
    plane->eraseLocation.plane = location->plane;

    for(i = 0; i < usbssd->blockOfPlane; ++i){
        if(plane->blockInfos[i]->freePageCount == 0 && \
        (plane->blockInfos[i]->invalidPageCount != 0 && (targetBlcok == -1 || plane->blockInfos[i]->invalidPageCount > plane->blockInfos[targetBlcok]->invalidPageCount))){
            targetBlcok = i;
            if(plane->blockInfos[targetBlcok]->invalidPageCount == usbssd->pageOfBlock){
                break;
            }
        }
    }

    if(targetBlcok != -1){
        Command_USBSSD *command = allocate_USBSSD(allocator);
        printk("T %d ac %lld %lld \n", targetBlcok, usbssd->pageOfBlock - plane->blockInfos[targetBlcok]->invalidPageCount, plane->blockInfos[targetBlcok]->invalidPageCount + plane->blockInfos[targetBlcok]->freePageCount + plane->blockInfos[targetBlcok]->validPageCount);
        printk("T %lld %lld %lld \n",plane->blockInfos[1]->validPageCount, plane->blockInfos[1]->freePageCount, plane->blockInfos[1]->invalidPageCount);
        printk("T %lld %lld %lld \n",plane->blockInfos[2]->validPageCount, plane->blockInfos[2]->freePageCount, plane->blockInfos[2]->invalidPageCount);
        plane->eraseLocation.block = targetBlcok;
        plane->eraseUsed = 1;

        command->operation = ERASE;
        command->subReqs = &(plane->eraseLocation);
        command->ID = get_commandID();
        command->completed = 0;
        command->related = NULL;
        command->next = NULL;
        command->stateEpage.EPage = 0;

        if(usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommandsTail){
            usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommandsTail->next = command;
        }else{
            usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommands = command;
        }
        usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommandsTail = command;

    }
    
    mutex_unlock(&usbssdMutex);

    allocate_command_USBSSD();
}

void check_and_create_gc(PPN_USBSSD *location){
    Plane_USBSSD *plane = (usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]);
    if(plane->eraseUsed == 0 && plane->freePageCount <=  (usbssd->pageOfBlock * usbssd->blockOfPlane / GC_RATE) ){
        int targetBlcok = -1;
        int i = 0;

        
        plane->eraseLocation.channel = location->channel;
        plane->eraseLocation.chip = location->chip;
        plane->eraseLocation.die = location->die;
        plane->eraseLocation.plane = location->plane;

        for(i = 0; i < usbssd->blockOfPlane; ++i){
            if(plane->blockInfos[i]->freePageCount == 0 && \
            (plane->blockInfos[i]->invalidPageCount != 0 && (targetBlcok == -1 || plane->blockInfos[i]->invalidPageCount > plane->blockInfos[targetBlcok]->invalidPageCount))){
                targetBlcok = i;
                if(plane->blockInfos[targetBlcok]->invalidPageCount == usbssd->pageOfBlock){
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
            command->completed = 0;
            command->related = NULL;
            command->next = NULL;
            command->stateEpage.EPage = 0;

            if(usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommandsTail){
                usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommandsTail->next = command;
            }else{
                usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommands = command;
            }
            usbssd->channelInfos[location->channel]->chipInfos[location->chip]->eraseCommandsTail = command;
        }
    }
}

void allocate_location_in_plane(PPN_USBSSD *location){
    Plane_USBSSD *plane;

    usbssd->channelInfos[location->channel]->freePageCount--;
    usbssd->channelInfos[location->channel]->validPageCount++;
    
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->freePageCount--;
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->validPageCount++;

    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->freePageCount--;
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->validPageCount++;

    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]->freePageCount--;
    usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]->validPageCount++;
    
    plane = (usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]);

    location->block = plane->activeBlock;
    location->page = plane->blockInfos[location->block]->startWrite;

    plane->blockInfos[location->block]->freePageCount--;
    plane->blockInfos[location->block]->validPageCount++;

    if(plane->blockInfos[location->block]->freePageCount == 0){
        int activeBlock = plane->activeBlock;
        while(plane->blockInfos[activeBlock]->freePageCount != usbssd->pageOfBlock){
            if(++activeBlock == usbssd->blockOfPlane){
                activeBlock = 0;
            }
        }
        plane->activeBlock = activeBlock;
    }

    location->page = plane->blockInfos[location->block]->startWrite;
    plane->blockInfos[location->block]->startWrite++;

}

static void add_Read_Write_CMD(int channel, int chip, Command_USBSSD *cmd){
    if(usbssd->channelInfos[channel]->chipInfos[chip]->commandsTail){
        usbssd->channelInfos[channel]->chipInfos[chip]->commandsTail->next = cmd;
    }else{
        usbssd->channelInfos[channel]->chipInfos[chip]->commands = cmd;
    }
    usbssd->channelInfos[channel]->chipInfos[chip]->commandsTail = cmd;
}

static Command_USBSSD *for_erase(Command_USBSSD *cmd){
    static int i = 0;
    PPN_USBSSD *location = (PPN_USBSSD *)cmd->subReqs;
    Block_USBSSD *targetB = usbssd->channelInfos[location->channel]->chipInfos[location->chip]->dieInfos[location->die]->planeInfos[location->plane]->blockInfos[location->block];
    Command_USBSSD *related = NULL;
    SubRequest_USBSSD *sub = NULL;
    if(cmd->related && cmd->related->completed != 1){
        return NULL;
    }
    ++i;
    if(cmd->related){
        if(cmd->related->completed == 1 && cmd->related->operation == READ){
            related = cmd->related;
            sub = related->subReqs;
            
            related->completed = 0;
            related->ID = get_commandID();
            related->next = NULL;
            related->operation = WRITE;
            related->stateEpage.state = 0;

            sub->jiffies = jiffies;
            sub->operation = WRITE;
            // allocate_location_in_plane(&sub->location, 1);
            add_Read_Write_CMD(location->channel, location->chip, related);
            return NULL;
        }else if(cmd->related->operation == READ){
            return NULL;
        }
    }else if(targetB->invalidPageCount == usbssd->pageOfBlock){
        // printk("end %d\n", i);
        // return NULL;
        return cmd;
    }
    

    while(cmd->stateEpage.EPage < usbssd->pageOfBlock){
        if(targetB->pageInfos[cmd->stateEpage.EPage]->state){
            break;
        }
        ++cmd->stateEpage.EPage;
    }
    // printk("create read\n");
    // return NULL;
    if(cmd->stateEpage.EPage == usbssd->pageOfBlock){
        // printk("end1 %lld\n", targetB->invalidPageCount);
        // return NULL;
        if(cmd->related){
            //delete cmd and subreq
            free_SubRequest_for_Erase(cmd->related->subReqs);
            cmd->related->subReqs = NULL;
            free_USBSSD(allocator, cmd->related);
            cmd->related = NULL;
        }
        return cmd;
    }
    
    //create read subreq and cmd
    if(cmd->related == NULL){
        cmd->related = allocate_USBSSD(allocator);
        cmd->related->related = cmd;
    }
    if(cmd->related == NULL){
        return NULL;
    }
    if(cmd->related->subReqs == NULL){
        cmd->related->subReqs = allocate_SubRequest_for_Erase();
    }
    if(cmd->related->subReqs == NULL){
        return NULL;
    }

    related = cmd->related;

    related->completed = 0;
    related->ID = get_commandID();
    related->next = NULL;
    related->operation = READ;
    related->stateEpage.state = 0;

    sub = related->subReqs;

    sub->bitMap = targetB->pageInfos[cmd->stateEpage.EPage]->state;
    sub->jiffies = jiffies;

    sub->location.channel = location->channel;
    sub->location.chip = location->chip;
    sub->location.die = location->die;
    sub->location.plane = location->plane;
    sub->location.block = location->block;
    sub->location.page = cmd->stateEpage.EPage;

    sub->lpn = targetB->pageInfos[cmd->stateEpage.EPage]->lpn;
    sub->next = NULL;
    sub->next_inter = NULL;
    sub->operation = READ;
    sub->pre = NULL;
    sub->relatedSub = NULL;
    sub->req = NULL;


    add_Read_Write_CMD(location->channel, location->chip, related);

    return NULL;
}

static Command_USBSSD *get_Command_USBSSD(int chan, int chip){
    SubRequest_USBSSD *sub = NULL;
    
    if(usbssd->channelInfos[chan]->chipInfos[chip]->incompletedR){
        return usbssd->channelInfos[chan]->chipInfos[chip]->incompletedR;
    }

    if(usbssd->channelInfos[chan]->chipInfos[chip]->eraseCommands){
        Command_USBSSD *ret = for_erase(usbssd->channelInfos[chan]->chipInfos[chip]->eraseCommands);
        if(ret){
            return ret;
        }
    }

    if(usbssd->channelInfos[chan]->chipInfos[chip]->commands){
        return usbssd->channelInfos[chan]->chipInfos[chip]->commands;
    }
    
    if((sub = get_SubRequests_Read_USBSSD(chan, chip)) != NULL || (sub = get_SubRequests_Write_USBSSD(chan, chip)) != NULL){
        Command_USBSSD *command = allocate_USBSSD(allocator);
        command->operation = sub->operation;
        command->subReqs = sub;
        command->ID = get_commandID();
        command->completed = 0;
        command->next = NULL;
        command->related = NULL;
        command->stateEpage.state = 0;

        
        if(usbssd->channelInfos[chan]->chipInfos[chip]->commandsTail){
            usbssd->channelInfos[chan]->chipInfos[chip]->commandsTail->next = command;
        }else{
            usbssd->channelInfos[chan]->chipInfos[chip]->commands = command;
        }
        usbssd->channelInfos[chan]->chipInfos[chip]->commandsTail = command;
        
        return command;
    }

    return NULL;
}

void allocate_command_USBSSD(void){
    int i;
    Command_USBSSD *command = NULL;
    if(!allocator || !usbssd){
        return;
    }
    mutex_lock(&usbssdMutex);
    for(i = 0; i < usbssd->channelCount; i++){
        int j;
        for(j = 0; j < usbssd->chipOfChannel && usbssd->channelInfos[i]->state == IDEL_HW; j++){
            // printk("allocate_command_USBSSD\n");
            if(usbssd->channelInfos[i]->chipInfos[j]->state == IDEL_HW){
                if((command = get_Command_USBSSD(i, j)) != NULL){
                    send_command(command);
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

void allocate_location_for_plane(PPN_USBSSD *location, unsigned long long lpn){
    
    location->channel = lpn % usbssd->channelCount;
    location->chip = (lpn / usbssd->channelCount) % usbssd->chipOfChannel;
    location->die =  (lpn / (usbssd->channelCount * usbssd->chipOfChannel)) % usbssd->dieOfChip;
    location->plane = (lpn / (usbssd->channelCount * usbssd->chipOfChannel * usbssd->dieOfChip)) % usbssd->planeOfDie;
    
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

void add_subReqs_to_chip(SubRequest_USBSSD *head){
    mutex_lock(&usbssdListMutex);

    while(head){
        SubRequest_USBSSD *add = head;
        head = head->next;

        add->next = NULL;
        if(add->operation == READ){
            add->pre = usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->rTail;
            if(usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->rTail)
                usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->rTail->next = add;
            usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->rTail = add;

            if(usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->rHead == NULL){
                usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->rHead = add;
            }
        }else{
            add->pre = usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->wTail;
            if(usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->wTail)
                usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->wTail->next = add;
            usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->wTail = add;

            if(usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->wHead == NULL){
                usbssd->channelInfos[add->location.channel]->chipInfos[add->location.chip]->wHead = add;
            }
        }
    }

    mutex_unlock(&usbssdListMutex);
}

int init_Command_USBSSD(void){
    Command_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(Command_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    if(allocator == NULL){
        return -1;
    }
    mutex_init(&mapMutex);
    mutex_init(&usbssdMutex);
    mutex_init(&usbssdListMutex);
    mutex_init(&commandIDMutex);

    if(setup_USBSSD()){
        destory_allocator_USBSSD(allocator);
        return -1;
    }
    
    return 0;
}

void destory_Command_USBSSD(void){
    destory_USBSSD();
    destory_allocator_USBSSD(allocator);
}

int setup_USBSSD(void){
    int chan = 0, chip = 0, die = 0, plane = 0, block = 0, page = 0;
    long long i = 0;
    unsigned char fail = 0;
    mutex_lock(&usbssdMutex);
    mutex_lock(&usbssdListMutex);
    mutex_lock(&mapMutex);
    mutex_lock(&commandIDMutex);

    usbssd = kmalloc_wrap(sizeof(*usbssd), GFP_KERNEL);
    if(usbssd == NULL){
        return -1;
    }
    
    //set usbssd and map
    usbssd->channelCount = 1;
    usbssd->chipOfChannel = 1;
    usbssd->dieOfChip = 1;
    usbssd->planeOfDie = 1;
    usbssd->blockOfPlane = 256;
    usbssd->pageOfBlock = 64;
    usbssd->subpageOfPage = (2048 >> SECTOR_SHIFT);

    pages_no_per_block = usbssd->pageOfBlock;
    page_no_per_plane = pages_no_per_block * usbssd->blockOfPlane;
    page_no_per_die = page_no_per_plane * usbssd->planeOfDie;
    page_no_per_chip = page_no_per_die * usbssd->dieOfChip;
    page_no_per_channel = page_no_per_chip * usbssd->chipOfChannel;

    all_capacity_USBSSD = usbssd->channelCount * usbssd->chipOfChannel * usbssd->dieOfChip *
                          usbssd->planeOfDie * usbssd->blockOfPlane * usbssd->pageOfBlock * usbssd->subpageOfPage * 512;
    usable_capacity_USBSSD = all_capacity_USBSSD - all_capacity_USBSSD / RESERVE_RATE;
    mapSize = (usable_capacity_USBSSD + PAGE_SIZE_USBSSD - 1) / PAGE_SIZE_USBSSD;

    usbssd->channelToken = 0;
    usbssd->channelInfos = kmalloc_wrap(sizeof(Channle_USBSSD*) * usbssd->channelCount, GFP_KERNEL);
    if(usbssd->channelInfos == NULL){
        fail = 1;
    }
    

    for(chan = 0; chan < usbssd->channelCount && usbssd->channelInfos; ++chan){
        usbssd->channelInfos[chan] = kmalloc_wrap(sizeof(Channle_USBSSD), GFP_KERNEL);
        if(usbssd->channelInfos[chan] == NULL){
            fail = 1;
            continue;
        }
        usbssd->channelInfos[chan]->chipInfos = kmalloc_wrap(sizeof(Chip_USBSSD*) * usbssd->chipOfChannel, GFP_KERNEL);
        if(usbssd->channelInfos[chan]->chipInfos == NULL){
            fail = 1;
            continue;
        }

        usbssd->channelInfos[chan]->state = IDEL_HW;
        usbssd->channelInfos[chan]->chipToken = 0;
        usbssd->channelInfos[chan]->validPageCount = 0;
        usbssd->channelInfos[chan]->invalidPageCount = 0;
        usbssd->channelInfos[chan]->freePageCount = usbssd->chipOfChannel * usbssd->dieOfChip * usbssd->planeOfDie * \
                                                   usbssd->blockOfPlane * usbssd->pageOfBlock;
        
        usbssd->channelInfos[chan]->channelNUM = chan;
        INIT_WORK(&usbssd->channelInfos[chan]->work, work_func_recv_signal_chan);
        for(chip = 0; chip < usbssd->chipOfChannel; ++chip){
            usbssd->channelInfos[chan]->chipInfos[chip] = kmalloc_wrap(sizeof(Chip_USBSSD), GFP_KERNEL);
            if(usbssd->channelInfos[chan]->chipInfos[chip] == NULL){
                fail = 1;
                continue;
            }
            usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos = kmalloc_wrap(sizeof(Die_USBSSD*) * usbssd->dieOfChip, GFP_KERNEL);
            if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos == NULL){
                fail = 1;
                continue;
            }

            usbssd->channelInfos[chan]->chipInfos[chip]->channelNUM = chan;
            usbssd->channelInfos[chan]->chipInfos[chip]->chipNUM = chip;
            INIT_WORK(&usbssd->channelInfos[chan]->chipInfos[chip]->work, work_func_recv_signal);

            usbssd->channelInfos[chan]->chipInfos[chip]->rHead = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->rTail = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->wHead = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->wTail = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->incompletedR = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->incompletedRTail = NULL;

            usbssd->channelInfos[chan]->chipInfos[chip]->commands = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->commandsTail = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->dieToken = 0;
            usbssd->channelInfos[chan]->chipInfos[chip]->eraseCommands = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->eraseCommandsTail = NULL;
            usbssd->channelInfos[chan]->chipInfos[chip]->invalidPageCount = 0;
            usbssd->channelInfos[chan]->chipInfos[chip]->state = IDEL_HW;
            usbssd->channelInfos[chan]->chipInfos[chip]->validPageCount = 0;

            usbssd->channelInfos[chan]->chipInfos[chip]->freePageCount =  usbssd->dieOfChip * usbssd->planeOfDie * \
                                                                        usbssd->blockOfPlane * usbssd->pageOfBlock;

            for(die = 0; die < usbssd->dieOfChip; ++die){
                usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die] = kmalloc_wrap(sizeof(Die_USBSSD), GFP_KERNEL);
                if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die] == NULL){
                    fail = 1;
                    continue;
                }
                usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos = kmalloc_wrap(sizeof(Plane_USBSSD*) * usbssd->planeOfDie, GFP_KERNEL);
                if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos == NULL){
                    fail = 1;
                    continue;
                }

                usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->invalidPageCount = 0;
                usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeToken = 0;
                usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->validPageCount = 0;

                usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->freePageCount = usbssd->planeOfDie * \
                                                                                         usbssd->blockOfPlane * usbssd->pageOfBlock;
            
                for(plane = 0; plane < usbssd->planeOfDie; ++plane){
                    usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane] = kmalloc_wrap(sizeof(Plane_USBSSD), GFP_KERNEL);
                    if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane] == NULL){
                        fail = 1;
                        continue;
                    }
                    usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos = kmalloc_wrap(sizeof(Block_USBSSD*) * usbssd->blockOfPlane, GFP_KERNEL);
                    if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos == NULL){
                        fail = 1;
                        continue;
                    }

                    usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->activeBlock = 0;
                    usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->eraseUsed = 0;
                    usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->invalidPageCount = 0;
                    usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->validPageCount = 0;

                    usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->freePageCount = usbssd->blockOfPlane * usbssd->pageOfBlock;
                
                    for(block = 0; block < usbssd->blockOfPlane; block++){
                        usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block] = kmalloc_wrap(sizeof(Block_USBSSD), GFP_KERNEL);
                        if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block] == NULL){
                            fail = 1;
                            continue;
                        }
                        usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos = kmalloc_wrap(sizeof(Block_USBSSD*) * usbssd->pageOfBlock, GFP_KERNEL);
                        if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos == NULL){
                            fail = 1;
                            continue;
                        }

                        usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->startWrite = 0;
                        usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->validPageCount = 0;
                        usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->invalidPageCount = 0;
                        usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->freePageCount = usbssd->pageOfBlock;

                        for(page = 0; page < usbssd->pageOfBlock; ++page){
                            usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos[page] = kmalloc_wrap(sizeof(Page_USBSSD), GFP_KERNEL);
                            if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos[page] == NULL){
                                fail = 1;
                                continue;
                            }

                            usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos[page]->state = 0;
                        }
                    }
                }
            }

        }
    }
    map = kmalloc_wrap(sizeof(mapEntry_USBSSD) * mapSize, GFP_KERNEL);
    if(map == NULL){
        fail = 1;
    }else{
        for(i = 0; i < mapSize; i++){
            map[i].subPage = 0;
        }
    }
    mutex_unlock(&usbssdListMutex);
    mutex_unlock(&usbssdMutex);
    mutex_unlock(&mapMutex);
    mutex_unlock(&commandIDMutex);

    if(fail){
        destory_USBSSD();
        return -1;
    }

    return 0;
}

void destory_USBSSD(){
    int chan = 0, chip = 0, die = 0, plane = 0, block = 0, page = 0;
    SubRequest_USBSSD *sub;
    mutex_lock(&usbssdListMutex);
    mutex_lock(&usbssdMutex);
    mutex_lock(&mapMutex);
    mutex_lock(&commandIDMutex);

    if(map){
        kfree_wrap(map);
    }

    if(usbssd == NULL){
        mutex_unlock(&usbssdMutex);
        mutex_unlock(&mapMutex);
        mutex_unlock(&commandIDMutex);

        return;
    }

    for(chan = 0; chan < usbssd->channelCount && usbssd->channelInfos; ++chan){
        for(chip = 0; chip < usbssd->chipOfChannel; ++chip){
            int count = 0;
            printk("chan %p ",usbssd->channelInfos[chan]->chipInfos[chip]->incompletedR);
            sub = usbssd->channelInfos[chan]->chipInfos[chip]->rHead;
            printk("chan %d chip %d\n", chan, chip);
            while(sub){
                printk("\t %lld %lld %d\n", sub->lpn, sub->bitMap, count++);
                sub = sub->next;
            }
        }
    }

    for(chan = 0; chan < usbssd->channelCount && usbssd->channelInfos; ++chan){
        if(usbssd->channelInfos[chan] == NULL){
            continue;
        }
        if(usbssd->channelInfos[chan]->chipInfos == NULL){
            kfree_wrap(usbssd->channelInfos[chan]);
            continue;
        }
        for(chip = 0; chip < usbssd->chipOfChannel; ++chip){
            if(usbssd->channelInfos[chan]->chipInfos[chip] == NULL){
                continue;
            }
            if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos == NULL){
                kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos);
                continue;
            }
            for(die = 0; die < usbssd->dieOfChip; ++die){
                if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die] == NULL){
                    continue;
                }
                if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos == NULL){
                    kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos);
                    continue;
                }
                for(plane = 0; plane < usbssd->planeOfDie; ++plane){
                    if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane] == NULL){
                        continue;
                    }
                    if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos == NULL){
                        kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos);
                        continue;
                    }
                    for(block = 0; block < usbssd->blockOfPlane; block++){
                        if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block] == NULL){
                            continue;
                        }
                        if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos == NULL){
                            kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos);
                            continue;
                        }
                        for(page = 0; page < usbssd->pageOfBlock; ++page){
                            if(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos[page] == NULL){
                                continue;
                            }
                            kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos[page]);
                        }
                        kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]->pageInfos);
                        kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos[block]);
                    }
                    kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]->blockInfos);
                    kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos[plane]);
                }
                kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]->planeInfos);
                kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos[die]);
            }
            kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]->dieInfos);
            kfree_wrap(usbssd->channelInfos[chan]->chipInfos[chip]);
        }
        kfree_wrap(usbssd->channelInfos[chan]->chipInfos);
        kfree_wrap(usbssd->channelInfos[chan]);
    }
    if(usbssd->channelInfos){
        kfree_wrap(usbssd->channelInfos);
    }
    kfree_wrap(usbssd);

    mutex_unlock(&usbssdMutex);
    mutex_unlock(&usbssdListMutex);
    mutex_unlock(&mapMutex);
    mutex_unlock(&commandIDMutex);
}