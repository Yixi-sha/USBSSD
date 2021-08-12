#include "sub_request.h"
#include <stddef.h>



static Allocator_USBSSD *allocator = NULL;

SubRequest_USBSSD *allocate_SubRequest_USBSSD(void){
    return allocate_USBSSD(allocator);
}

void free_SubRequest_USBSSD(SubRequest_USBSSD* addr){
    free_USBSSD(allocator, addr);
}

void init_SubRequest_USBSSD(void){
    SubRequest_USBSSD *tmp = 0;
    allocator = init_allocator_USBSSD(sizeof(SubRequest_USBSSD), ((void*)(&(tmp->allocator))) - ((void*)tmp));
}

void destory_SubRequest_USBSSD(void){
    destory_allocator_USBSSD(allocator);
}