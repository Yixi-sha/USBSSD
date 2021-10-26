#include "memCheck.h"

static atomic_t v = ATOMIC_INIT(0);

void *kmalloc_wrap(size_t size, gfp_t f){
    atomic_inc(&v);
    return kmalloc(size, f);
}

void kfree_wrap(void* p){
    atomic_dec(&v);
    kfree(p);
}

int get_kmalloc_count(){
    return atomic_read(&v);
}