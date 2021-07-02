#include <iostream>
#include <unistd.h>
#include <cstring>

class Metadata {
private:
    size_t size;
    bool is_free;
    Metadata* next;
    Metadata* prev;
public:
    Metadata(size_t size, bool is_free = false, Metadata* next = NULL, Metadata* prev = NULL): size(size), is_free(is_free), next(next), prev(prev){};
    ~Metadata() = default;///?
    Metadata* getNext(){return this->next;};
    Metadata* getPrev(){return this->prev;};
    size_t getSize(){return this->size;};
    bool isFree(){return this->is_free;};
    void setNext(Metadata* next){this->next = next;};
    void setPrev(Metadata* prev){this->prev = prev;};
    void setSize(size_t size){this->size = size;};
    void setIsFree(bool arg){this->is_free = arg;};
};

Metadata metalistHead(0);
size_t MetaDataSize = sizeof(Metadata);

void* smallocAux(size_t size){
    if(size == 0 || size > 100000000){
        return NULL;
    }
    void* p = sbrk(size);
    if(p == (void*)-1){
        return NULL;
    }
    return p;
}

size_t _num_free_blocks(){
    Metadata* iterator = &metalistHead;
    int numOfBlocks = 0;
    while (iterator){
        if (iterator->isFree()){
            numOfBlocks++;
        }
        iterator = iterator->getNext();
    }
    return numOfBlocks;
};

size_t  _num_free_bytes(){
    Metadata* iterator = &metalistHead;
    int numOfBytes = 0;
    while (iterator){
        if (iterator->isFree()){
            numOfBytes += iterator->getSize();
        }
        iterator = iterator->getNext();
    }
    return numOfBytes;
};

size_t _num_allocated_blocks(){
    Metadata* iterator = &metalistHead;
    int numOfBlocks = 0;
    while (iterator){
        if (iterator){
            numOfBlocks++;
        }
        iterator = iterator->getNext();
    }
    return numOfBlocks - 1; //we counted the head too, so need -1
};

size_t  _num_allocated_bytes(){
    Metadata* iterator = &metalistHead;
    int numOfBytes = 0;
    while (iterator){
        if (iterator){
            numOfBytes += iterator->getSize();
        }
        iterator = iterator->getNext();
    }
    return numOfBytes;
};

size_t  _num_meta_data_bytes(){
    size_t num = _num_allocated_blocks();
    return num * MetaDataSize;
};

size_t _size_meta_data(){
    return MetaDataSize;
};

void* smalloc(size_t size){

    Metadata *iterator = &metalistHead;
    while (iterator) {
        if (iterator->isFree()) {
            if (iterator->getSize() >= size) {
                iterator->setIsFree(false);
                return (iterator + MetaDataSize);
            }
        }
        iterator = iterator->getNext();
    }

    //didn't find a free block, allocates a new one
    void * newBlock = smallocAux((MetaDataSize + size));
    if (!newBlock){
        return NULL;
    }
    Metadata * newMetaData = (Metadata*)newBlock;
    *newMetaData = Metadata(size);
    newMetaData->setPrev(iterator);
    iterator->setNext(newMetaData);
    void* ptrBlock = (void*)((size_t)newMetaData + MetaDataSize);
    return ptrBlock;

};


void* scalloc(size_t num, size_t size){
    if (size == 0){
        return NULL;
    }
    size_t desiredSize = num * size;
    if(desiredSize > 100000000){
        return NULL;
    }

    Metadata* iterator = &metalistHead;
    while (iterator){
        if(iterator->isFree()) {
            if (iterator->getSize() >= desiredSize) {
                iterator->setIsFree(false);
                void* ptrBlock = (void*)((size_t)iterator + MetaDataSize);
                std::memset(ptrBlock, 0, desiredSize);
                return ptrBlock;
            }
        }
        iterator = iterator->getNext();
    }

    //didn't find a free block, allocates a new one
    void * newBlock = smallocAux((MetaDataSize + desiredSize));
    if (!newBlock){
        return NULL;
    }
    Metadata * newMetaData = (Metadata*)newBlock;
    *newMetaData = Metadata(desiredSize);
    newMetaData->setPrev(iterator);
    iterator->setNext(newMetaData);
    void* ptrBlock = (void*)((size_t)newMetaData + MetaDataSize);

    std::memset(ptrBlock, 0, desiredSize);

    return (ptrBlock);
};


void sfree(void* p){///p points to the block after metadata?
    if(p == nullptr){
        return;
    }
    Metadata* iterator = &metalistHead;
    while (iterator){
        if(iterator + MetaDataSize == p){
            if(iterator->isFree()){
                return;
            }else{
                iterator->setIsFree(true);
                return;
            }
        }
        iterator = iterator->getNext();
    }
};

void* srealloc(void* oldp, size_t size){
    if(size == 0 || size > 100000000){
        return nullptr;
    }

    if (oldp) {//check if current block is good enough
        void* ptrMetadata = (void*)((size_t)oldp - MetaDataSize);
        Metadata* oldpMetaData = (Metadata*)ptrMetadata;
        if (oldpMetaData->getSize() >= size) {
            void* ptrBlock = (void*)((size_t)oldpMetaData + MetaDataSize);
            return (ptrBlock);
        }
    }

    //searching for a free block (big enough)
    Metadata* iterator = &metalistHead;
    while (iterator) {
        if (iterator->isFree()){
            if (iterator->getSize() >= size){
                iterator->setIsFree(false);//changing the new block
                //need to mark the oldp block as free now, since we moved it to a new block
                if (oldp != nullptr){
                    void* ptrMetadata = (void*)((size_t)oldp - MetaDataSize);
                    Metadata* oldpMetaData = (Metadata*)ptrMetadata;
                    oldpMetaData->setIsFree(true);
                    void* ptrBlock = (void*)((size_t)iterator + MetaDataSize);
                    size_t min = size > oldpMetaData->getSize() ? oldpMetaData->getSize() : size;
                    return std::memmove(ptrBlock, oldp, min);
                }
            }
        }
        iterator = iterator->getNext();
    }

    //didn't find a free block, allocates a new one
    void * newBlock = smallocAux((MetaDataSize + size));
    if (!newBlock){
        return oldp;
    }
    Metadata * newMetaData = (Metadata*)newBlock;
    *newMetaData = Metadata(size);
    newMetaData->setPrev(iterator);
    iterator->setNext(newMetaData);

    //need to mark the oldp block as free now, since we moved it to a new block
    if (oldp != nullptr){
        void* ptrMetadata = (void*)((size_t)oldp - MetaDataSize);
        Metadata* oldpMetaData = (Metadata*)ptrMetadata;
        oldpMetaData->setIsFree(true);
        void* ptrBlock = (void*)((size_t)newMetaData + MetaDataSize);
        size_t min = size > oldpMetaData->getSize() ? oldpMetaData->getSize() : size;
        return std::memmove(ptrBlock, oldp, min);
    }
};