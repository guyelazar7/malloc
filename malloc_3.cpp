#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <cstring>
#define MAX_ORDER 10
#define MAX_SIZE 131072
#define MIN_SIZE 128
//------------------------------- data structures for program-------------------------------
struct MallocMetadata {
    int cookies;
    size_t size;
    MallocMetadata* next;
    MallocMetadata* prev;
};
class OutOfMemExcept{};
bool allocatedFlag=false;
int countAllocatedBlocks = 0, sumAllocatedBytes = 0, cookies = std::rand();
MallocMetadata* blockListArr[11] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
//-------------------------------helpers for management--------------------------------------
void validate(MallocMetadata* block){
    if(block->cookies!=cookies) exit(0xdeadbeef);
}
MallocMetadata* append(MallocMetadata* list, MallocMetadata* element){

    if(list == NULL){
        validate(element);
        element->prev = NULL;
        element->next = NULL;
        return element;
    }
    if(element < list){
        validate(element);
        element->prev = NULL;
        list->prev = element;
        element->next = list;
        return element;
    }
    for(MallocMetadata* it = list; it != NULL; it = it->next){
        validate(it);
        if(it < element && it->next == NULL){
            it->next = element;
            element->next = NULL;
            element->prev = it;
            break;
        }
        if(it < element && element < it->next){
            MallocMetadata* temp = it->next;
            validate(temp);
            it->next = element;
            element->next = temp;
            temp->prev = element;
            element->prev = it;
            break;
        }
    }
    return list;
}

void initializer(){//need to adjust it a little bit more
    void* block = sbrk(0);
    unsigned long long align = 32*MAX_SIZE - (unsigned long long)(block) % (32*MAX_SIZE);
    block = sbrk(align);
    block = sbrk(32*MAX_SIZE); 
    if(block == (void*)-1) return;
    for(int i = 0; i < 32; i++){
        MallocMetadata* temp = (MallocMetadata*)((char*)block + i * MAX_SIZE);
        temp->cookies = cookies;
        temp->size = MAX_SIZE - sizeof(MallocMetadata);
        blockListArr[MAX_ORDER] = append(blockListArr[MAX_ORDER], temp);
    }
}

MallocMetadata* remove(MallocMetadata* list, MallocMetadata* element){
    if(list == element){
        validate(element);
        MallocMetadata* temp = list->next;
        if(temp) temp->prev = NULL;
        return temp;
    }
    for(MallocMetadata* it = list; it != NULL; it = it->next){
        validate(it);
        if(it == element){
            if(it->prev){
                validate(it->prev);
                it->prev->next=it->next;
            }
            if(it->next){
                validate(it->next);
                it->next->prev=it->prev;
            }
            break;
        }
    }
    return list;
}

int findArr(MallocMetadata* element){
    for(int i=0;i<11;i++){
        MallocMetadata* temp = blockListArr[i];
        while(temp!=NULL){
            if(temp==element)
                return i;
            temp=temp->next;
        }
    }
    return -1;
}
MallocMetadata* unionBlocks(MallocMetadata* block, int order, int orderLimit){
    if(order >= orderLimit)
        return block;
    MallocMetadata* buddy = (MallocMetadata*)((unsigned long long)(block) ^ (block->size + sizeof(MallocMetadata)));
    if(findArr(buddy)==-1)
        return block;
    blockListArr[order] = remove(blockListArr[order],block);
    blockListArr[order] = remove(blockListArr[order],buddy);
    block->size = block->size + buddy->size + sizeof(MallocMetadata);
    buddy->size = block->size;
    MallocMetadata* newBlock = (block < buddy) ? block : buddy;
    blockListArr[order+1] = append(blockListArr[order+1], newBlock);
    return unionBlocks(newBlock,order+1, orderLimit);
}
int getOrder(size_t size){
     for(unsigned i=0,n=128;i<10;i++,n*=2)
         if(size+sizeof(MallocMetadata)<=n)return i;
     return MAX_ORDER;
}
bool isMergeAvailable(MallocMetadata* block, int sizeNeeded){
    int size = (block->size + sizeof(MallocMetadata));
    for(int i = getOrder(block->size); i < getOrder(sizeNeeded); i++){
        MallocMetadata* buddy = (MallocMetadata*)((unsigned long long)(block) ^ size);
        if(findArr(buddy) == -1) return false;
        block = (block < buddy) ? block : buddy;
        size *= 2;
    }
    return true;
}
void fragment(int order){
    MallocMetadata* temp = blockListArr[order];
    blockListArr[order] = remove(blockListArr[order], temp);
    temp->size = ((temp->size + sizeof(MallocMetadata)) / 2) - sizeof(MallocMetadata);
    MallocMetadata* buddy = (MallocMetadata*)((char*)temp + temp->size + sizeof(MallocMetadata));
    buddy->cookies=cookies;
    buddy->size = temp->size;
    blockListArr[order-1] = append(blockListArr[order-1], temp);
    blockListArr[order-1] = append(blockListArr[order-1], buddy);
}

int findFree(int order){
    if(order>MAX_ORDER)
        return -1;
    MallocMetadata* temp=blockListArr[order];
    if(temp!=NULL)
        return order;
    return findFree(order+1);
}
//-----------------------------------large allocations helpers--------------------------------------------
void* allocateOverMax(size_t size){
    MallocMetadata* address = (MallocMetadata*)mmap(NULL,size + sizeof(MallocMetadata),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(address==MAP_FAILED)
        return NULL;
    sumAllocatedBytes+=size;
    countAllocatedBlocks++;
    address->size = size;
    return (void*)(address + 1);
}
void freeOverMax(void* p){
    MallocMetadata* temp = (MallocMetadata*)p - 1;
    countAllocatedBlocks--;
    sumAllocatedBytes -= temp->size;
    munmap(temp, temp->size + sizeof(MallocMetadata));
}
///--------------------------------library functions----------------------------------------------------
void* smalloc(size_t size){
    if(!allocatedFlag){
        allocatedFlag=true;
        initializer();
    }
    if(size == 0 || size > 100000000)
        return NULL;
    if(size + sizeof(MallocMetadata) > MAX_SIZE) return allocateOverMax(size);
    int order = getOrder(size);
    int freeOrder = findFree(order);
    if(freeOrder==-1)
        return NULL;
    while(freeOrder>order){
        fragment(freeOrder);
        freeOrder--;
    }
    MallocMetadata* temp = blockListArr[order];
    blockListArr[order]=remove(blockListArr[order],temp);
    countAllocatedBlocks++;
    sumAllocatedBytes += temp->size;
    return (void*)(temp+1);
}
void sfree(void* p){
    if(p == NULL)
        return;
    MallocMetadata* temp=(MallocMetadata*)p - 1;
    if(findArr(temp)!=-1) return;
    if(temp->size + sizeof(MallocMetadata)> MAX_SIZE){
        freeOverMax(p);
        return;
    }
    int order=getOrder(temp->size);
    blockListArr[order] = append(blockListArr[order], temp);
    sumAllocatedBytes-=temp->size;
    unionBlocks(temp, order, MAX_ORDER);
    countAllocatedBlocks--;
}
void* scalloc(size_t num, size_t size){
    void* temp = smalloc(num * size);
    if(temp == NULL) return NULL;
    return std::memset(temp, 0, num * size);
}
void* srealloc(void* oldp, size_t size){
    if(size == 0)// illegal input
        return NULL;
    if(oldp == NULL) return smalloc(size);
    MallocMetadata* temp = (MallocMetadata*)oldp - 1;
    if(temp->size + sizeof(MallocMetadata) > MAX_SIZE){//over max size
        void* res = smalloc(size);
        if(res > 0){
            std::memmove(res, oldp, temp->size);
            sfree(oldp);
            return res;
        }
        return oldp;
    }
    if(size <= temp->size){//can fit
        return oldp;
    }
    if(isMergeAvailable(temp, size)){ //merging case
        int oldSize = temp->size;
        MallocMetadata* res = unionBlocks(temp, getOrder(temp->size), getOrder(size));
        std::memmove((void*)(res + 1), oldp, oldSize);
        int order = getOrder(res->size);
        blockListArr[order] = remove(blockListArr[order], res);
        sumAllocatedBytes += (res->size - oldSize);
        return (void*)(res + 1);
    }
    //normal case
    void* res = smalloc(size);
    if(res == NULL)
        return NULL;
    std::memmove(res, oldp, temp->size);
    sfree(oldp);
    return res;
}
size_t _num_free_blocks(){
    size_t count = 0;
    for(int i = 0; i <= MAX_ORDER; i++){
        for(MallocMetadata* it = blockListArr[i]; it!= NULL; it = it->next){
            count++;
        }
    }
    return count;
}
size_t _num_free_bytes(){
    size_t count = 0;
    for(int i = 0; i <= MAX_ORDER; i++){
        for(MallocMetadata* it = blockListArr[i]; it!= NULL; it = it->next){
            count += it->size;
        }
    }
    return count;
}
size_t _num_allocated_blocks(){
    return countAllocatedBlocks + _num_free_blocks();
}
size_t _num_allocated_bytes(){
    return sumAllocatedBytes + _num_free_bytes();
}
size_t _num_meta_data_bytes(){
    return sizeof(MallocMetadata) * _num_allocated_blocks();
}
size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}