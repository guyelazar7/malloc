#include <unistd.h>
#include <cstring>
#include <iostream>
struct MallocMetadata {
    size_t size;
    bool is_free; 
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* blockList;
int countAllocatedBlocks=0,sumAllocatedBytes=0,countFreeBlocks=0,sumFreeBytes=0,listSize=0;
MallocMetadata* append(MallocMetadata* element){
    listSize++;
    if(blockList == NULL){
        element->prev = NULL;
        element->next = NULL;
        return element;
    }
    if(element < blockList){
        element->prev = NULL;
        blockList->prev = element;
        element->next = blockList;
        return element;
    }
    for(MallocMetadata* it = blockList; it != NULL; it = it->next){
        if(it < element && it->next == NULL){
            it->next = element;
            element->next = NULL;
            element->prev = it;
            return blockList;
        }
        if(it < element && element < it->next){
            MallocMetadata* temp = it->next;
            it->next = element;
            element->next = temp;
            temp->prev = element;
            element->prev = it;
            return blockList;
        }
    }
    return NULL;
}

void* smalloc(size_t size){
    if(size == 0 || size > 100000000)// illegal input
        return NULL;
    for(MallocMetadata* it = blockList; it != NULL; it = it->next){// search in free list
        if(it->is_free && it->size >= size){
            it->is_free = false;
            countAllocatedBlocks++;
            countFreeBlocks--;
            sumAllocatedBytes+=it->size;
            sumFreeBytes-=it->size;
            return (void*)(it + 1);
        }
    }
    // if list is null or not found:
    void* res = sbrk(0);
    int* allocating =static_cast<int*>(sbrk(size + sizeof(MallocMetadata)));
    if(*allocating == -1) return NULL;
    MallocMetadata* temp = (MallocMetadata*)res;
    temp->size=size;
    temp->is_free=false;
    blockList = append(temp);
    countAllocatedBlocks++;
    sumAllocatedBytes+= temp->size;
    return (void*)((MallocMetadata*)(res) + 1);
}

void* scalloc(size_t num, size_t size){
    void* temp = smalloc(num * size);
    if(temp == NULL) return NULL;
    return std::memset(temp, 0, num * size);
}

void sfree(void* p){
    if(p == NULL) return;
    MallocMetadata* temp = (MallocMetadata*)p-1;
    std::cout<<"allocated bytes "<<sumAllocatedBytes<<std::endl;
    std::cout<<"free byts"<<sumFreeBytes<<std::endl;
    temp->is_free = true;
    sumFreeBytes+=temp->size;
    countFreeBlocks++;
    countAllocatedBlocks--;
    sumAllocatedBytes-=temp->size;
    std::cout<<"allocated bytes after "<<sumAllocatedBytes<<std::endl;
    std::cout<<"free byts after"<<sumFreeBytes<<std::endl;
}

void* srealloc(void* oldp, size_t size){
    if(size == 0 || size > 100000000)// illegal input
        return NULL;
    if(oldp == NULL) return smalloc(size);
    MallocMetadata* temp = (MallocMetadata*)oldp -1;
    if(size <= temp->size){
        //temp->size = size;
        return oldp;
    }
    void* res = smalloc(size);
    if(res == NULL)
        return NULL;
    std::memmove(res, oldp, size);
    sfree(oldp);
    return res;
}

size_t _num_free_blocks(){
    return countFreeBlocks;
}
size_t _num_free_bytes(){
    return sumFreeBytes;
}

size_t _num_allocated_blocks(){
    return countAllocatedBlocks+countFreeBlocks;
}

size_t _num_allocated_bytes(){
    return sumAllocatedBytes+sumFreeBytes;
}

size_t _num_meta_data_bytes(){
    return sizeof(MallocMetadata) * listSize;
}

size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}

