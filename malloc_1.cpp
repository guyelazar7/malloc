#include <unistd.h>


void* smalloc(size_t size){
    if(size <= 0 || size > 100000000) return NULL;
    void* res = sbrk(0);
    int* allocating =static_cast<int*> (sbrk(size));
    if(*allocating == -1) return NULL;
    return res;
}