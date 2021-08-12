#include "request.h"
#include <stddef.h>



static Allocator_USBSSD *allocator = NULL;

Request_USBSSD *allocate_Request_USBSSD(struct request *req){
    Request_USBSSD *ret = allocate_USBSSD(allocator);
    ret->req = req;
    ret->restDataCount = 0;
    return ret;
}

void free_Request_USBSSD(Request_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

void init_Request_USBSSD(void){
    Request_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(Request_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
}

void destory_Request_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}