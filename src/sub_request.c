#include "sub_request.h"
#include <stddef.h>
#include "command.h"


static Allocator_USBSSD *allocator = NULL;


void add_To_List_SubRequest_USBSSD(SubRequest_USBSSD* h, SubRequest_USBSSD* t){
    SubRequest_USBSSD* now = h;
    while(now->next){
        now = now->next;
    }
    if(now != t){
        return;
    }
    add_subReqs_to_chip(h);
    
    allocate_command_USBSSD();  
}

SubRequest_USBSSD *allocate_SubRequest_for_Erase(){
    SubRequest_USBSSD *ret = allocate_USBSSD(allocator);
    return ret;
}

void free_SubRequest_for_Erase(void* addr){
    if(addr){
        free_SubRequest_USBSSD(addr);
    }
}

SubRequest_USBSSD *allocate_SubRequest_USBSSD(Request_USBSSD *req, unsigned long long lpn, unsigned long long bitMap, unsigned char operation){
    mapEntry_USBSSD nowMAp;
    SubRequest_USBSSD *ret = allocate_USBSSD(allocator);
    if(!ret){
        return NULL;
    }

    ret->req = req;
    ret->operation = operation;
    ret->lpn = lpn;
    ret->bitMap = bitMap;
    ret->jiffies = jiffies;
    ret->relatedSub = NULL;
    ret->pre = NULL;
    ret->next = NULL;
    ret->next_inter = NULL;

    if(get_PPN_USBSSD(ret->lpn, &nowMAp)){
        free_SubRequest_USBSSD(ret);
        return NULL;
    }
    
    if(ret->operation == READ){
        get_PPN_Detail_USBSSD(nowMAp.ppn, &ret->location);
    }else{
        allocate_location(&ret->location);
    }

    if(ret->operation == WRITE && ((ret->bitMap & nowMAp.subPage) != nowMAp.subPage)){
        SubRequest_USBSSD *relatedSub = allocate_SubRequest_USBSSD(NULL, lpn, nowMAp.subPage, READ);
        ret->relatedSub = relatedSub;
        relatedSub->relatedSub = ret;
    }else{
        ret->relatedSub = NULL;
    }

    return ret;
}

void subRequest_End(SubRequest_USBSSD *sub){
    if(sub->relatedSub){
        int i = 0;
        for(; i < sizeof(sub->bitMap) * 8;++i){
            if(sub->bitMap & (1ULL << i) && ((sub->relatedSub->bitMap & (1ULL << i)) == 0)){
                memcpy(sub->relatedSub->buf + (i << SECTOR_SHIFT), sub->buf + (i << SECTOR_SHIFT) , SECTOR_SIZE);
                sub->relatedSub->bitMap |= (1ULL << i);
            }
        }
        sub->relatedSub->relatedSub = NULL;
        free_SubRequest_USBSSD(sub);
    }else{
        request_May_End(sub->req);
    }
    
}

void free_SubRequest_USBSSD(SubRequest_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

int init_SubRequest_USBSSD(void){
    SubRequest_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(SubRequest_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
    if(allocator == NULL){
        return -1;
    }
    return 0;
}

void destory_SubRequest_USBSSD(void){

    destory_allocator_USBSSD(allocator);
   
}

void boost_gc_test_sub(PPN_USBSSD* loc){
    boost_test_gc(loc);
}