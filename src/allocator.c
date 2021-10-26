#include "allocator.h"

void *allocate_USBSSD(Allocator_USBSSD* a){
    Inter_Allocator_USBSSD *ret = NULL;
    if(!a){
        return NULL;
    }
    mutex_lock(&(a->freeMutex));
    if(a->freeHead){
        ret = a->freeHead;
        a->freeHead = ret->next;
        if(a->freeHead){
            a->freeHead->pre = NULL;
        }
    }
    mutex_unlock(&(a->freeMutex));

    if(!ret){
        ret = kmalloc_wrap(a->allSize, GFP_KERNEL);
        ret = ((void*)ret) + a->offset;
    }
    if(!ret){
        return ret;
    }

    mutex_lock(&(a->usedMutex));
    ret->pre = NULL;
    ret->next = a->usedHead;
    if(a->usedHead)
        a->usedHead->pre = ret;
    a->usedHead = ret;
    mutex_unlock(&(a->usedMutex));

    return ((void*)ret) - a->offset;
}

void free_USBSSD(Allocator_USBSSD* a, void* addr){
    Inter_Allocator_USBSSD *ptr = addr + a->offset;
    Inter_Allocator_USBSSD *iter = a->usedHead;
    if(!ptr || !a){
        return;
    }

    mutex_lock(&(a->usedMutex));
    while(iter != ptr && iter->next){
        iter = iter->next;
    }
    if(!iter){
        mutex_unlock(&(a->usedMutex)); 
        return;
    }

    if(ptr == a->usedHead){
        a->usedHead = ptr->next;
        if(a->usedHead)
            a->usedHead->pre = NULL;
    }else{
        ptr->pre->next = ptr->next;
        if(ptr->next)
            ptr->next->pre = ptr->pre;
    }
    mutex_unlock(&(a->usedMutex));

    ptr->pre = NULL;

    mutex_lock(&(a->freeMutex));
    ptr->next = a->freeHead;
    if(a->freeHead)
        a->freeHead->pre = ptr;
    a->freeHead = ptr;
    mutex_unlock(&(a->freeMutex));
}

Allocator_USBSSD* init_allocator_USBSSD(int allsize, int offset){
    Allocator_USBSSD *ret = kmalloc_wrap(sizeof(Allocator_USBSSD), GFP_KERNEL);
    if(!ret){
        return ret;
    }

    ret->freeHead = NULL;
    ret->usedHead = NULL;
    ret->allSize = allsize;
    ret->offset = offset;
    mutex_init(&(ret->freeMutex));
    mutex_init(&(ret->usedMutex));

    return ret;
}

void destory_allocator_USBSSD(Allocator_USBSSD* ptr){
    if(!ptr){
        return;
    }

    mutex_lock(&(ptr->freeMutex));
    mutex_lock(&(ptr->usedMutex));

    mutex_destroy(&(ptr->freeMutex));
    mutex_destroy(&(ptr->usedMutex));

    while(ptr->freeHead){
        Inter_Allocator_USBSSD *wfree = ptr->freeHead;
        ptr->freeHead = ptr->freeHead->next;

        kfree_wrap(((void*)wfree) - ptr->offset);
    }

    while(ptr->usedHead){
        Inter_Allocator_USBSSD *wfree = ptr->usedHead;
        ptr->usedHead = ptr->usedHead->next;

        kfree_wrap(((void*)wfree) - ptr->offset);
    }

    kfree_wrap(ptr);
}