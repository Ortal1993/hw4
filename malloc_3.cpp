#include <iostream>
#include <unistd.h>
#include <cstring>


class Metadata {
private:
    size_t size;
    bool is_free;
    Metadata* next;
    Metadata* prev;
    Metadata* histoNext;
    Metadata* histoPrev;


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

Metadata* bins[128] ={};

///find the right place in histogram (by size)
///


static void* smallocAux(size_t size){
    if(size == 0 || size > 100000000){
        return NULL;
    }
    void* p = sbrk(size);
    if(p == (void*)-1){
        return NULL;
    }
    return p;
}


void* smalloc(size_t size){
    Metadata* iterator = &metalistHead;
    while (iterator->getNext()){
        if (iterator->getSize() >= size){
            iterator->setIsFree(false);
            iterator->setSize(size);
            return (iterator + MetaDataSize);
        }
        iterator = iterator->getNext();
    }
    //check the last Metadata node
    if (iterator->getSize() >= size){
        iterator->setIsFree(false);
        iterator->setSize(size);
        return (iterator + MetaDataSize);
    }

    //didn't find a free block, allocates a new one
    if (!iterator->getNext()){
        Metadata* newMetaData = (Metadata*)smallocAux(MetaDataSize);
        *newMetaData = Metadata(size);
        if (!newMetaData){
            return NULL;
        }
        void* p = smallocAux(size);
        if (!p){
            newMetaData->setSize(0);
            return NULL;
            ///maybe need to free the newMetadata? but on the other hand, don't care about fragmentation/optimizations now
        }
        newMetaData->setPrev(iterator);
        iterator->setNext(newMetaData);
        return p;
    }
};


void* scalloc(size_t num, size_t size){
    if (size == 0){
        return NULL;
    }
    size_t desiredSize = num * size;

    Metadata* iterator = &metalistHead;
    while (iterator->getNext()){
        if (iterator->getSize() >= size){
            iterator->setIsFree(false);
            iterator->setSize(size);
            return (iterator + MetaDataSize);
        }
        iterator = iterator->getNext();
    }
    //check the last Metadata node
    if (iterator->getSize() >= size){
        iterator->setIsFree(false);
        iterator->setSize(size);
        return (iterator + MetaDataSize);
    }

    //didn't find a free block, allocates a new one
    if (!iterator->getNext()){
        Metadata* newMetaData = (Metadata*)smallocAux(MetaDataSize);
        *newMetaData = Metadata(size);
        if (!newMetaData){
            return NULL;
        }
        void* p = smallocAux(desiredSize);
        if (!p){
            newMetaData->setSize(0);
            return NULL;
            ///maybe need to free the newMetatData? but on the other hand, don't care about fragmentation/optimizations now

        }

        std::memset(p, 0, desiredSize);

        newMetaData->setPrev(iterator);
        iterator->setNext(newMetaData);

        return p;
    }

};


void sfree(void* p){///p points to the block after metadata?
    if(p == nullptr){
        return;
    }
    Metadata* iterator = &metalistHead;
    while (iterator->getNext()){
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

    //check the last Metadata node
    if(iterator + MetaDataSize == p){
        if(iterator->isFree()){
            return;
        }else{
            iterator->setIsFree(true);
            return;
        }
    }
};

void* srealloc(void* oldp, size_t size){
    if(size == 0 || size > 100000000){
        return nullptr;
    }

    if (oldp) {//check if current block is good enough
        Metadata *oldpMetaData = ((Metadata *) oldp - MetaDataSize);
        if (oldpMetaData->getSize() >= size) {
            return (oldpMetaData + MetaDataSize);
        }
    }

    /*Metadata* iterator = &metalistHead;
    while (iterator->getNext()) {
        if (iterator + MetaDataSize == oldp){
            if (iterator->getSize() >= size){
                return (iterator + MetaDataSize);
            }
        }
        iterator = iterator->getNext();
    }
    //check the last Metadata node
    if(iterator + MetaDataSize == oldp){
        if (iterator->getSize() >= size){
            return (iterator + MetaDataSize);
        }
    }*/

    //searching for a free block (big enough)
    Metadata* iterator = &metalistHead;
    while (iterator->getNext()) {
        if (iterator->isFree() == true){
            if (iterator->getSize() >= size){
                iterator->setIsFree(false);
                return (iterator + MetaDataSize);
            }
        }
        iterator = iterator->getNext();
    }
    //check the last Metadata node
    if(iterator->isFree() == true){
        if (iterator->getSize() >= size){
            iterator->setIsFree(false);
            return (iterator + MetaDataSize);
        }
    }

    //allocating new block
    Metadata* newMetaData = (Metadata*)smallocAux(MetaDataSize);
    *newMetaData = Metadata(size);
    if (!newMetaData){
        return NULL;
    }
    void* p = smallocAux(size);
    if (!p){
        newMetaData->setSize(0);
        return NULL;
        ///maybe need to free the newMetatData? but on the other hand, don't care about fragmentation/optimizations now
    }

    if (oldp != nullptr){
        Metadata *oldpMetaData = ((Metadata *) oldp - MetaDataSize);
        oldpMetaData->setIsFree(true);
        return std::memcpy(p, oldp, size);
    }

    return p;

};


static size_t _num_free_blocks(){
    Metadata* iterator = &metalistHead;
    int numOfBlocks = 0;
    while (iterator->getNext()){
        if (iterator->isFree() == true){
            numOfBlocks++;
        }
        iterator = iterator->getNext();
    }
    //check the last Metadata node
    if (iterator->isFree() == true){
        numOfBlocks++;
    }
    return numOfBlocks;
};

static size_t  _num_free_bytes(){
    Metadata* iterator = &metalistHead;
    int numOfBytes = 0;
    while (iterator->getNext()){
        if (iterator->isFree() == true){
            numOfBytes += iterator->getSize();
        }
        iterator = iterator->getNext();
    }
    //check the last Metadata node
    if (iterator->isFree() == true){
        numOfBytes += iterator->getSize();
    }
    return numOfBytes;
};

static size_t _num_allocated_blocks(){
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

static size_t  _num_allocated_bytes(){
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

static size_t  _num_meta_data_bytes(){
    size_t num = _num_allocated_blocks();
    return num * MetaDataSize;
};

static size_t _size_meta_data(){
    return MetaDataSize;
};

int main() {
    std::cout << "Hello, World!" << std::endl;
    std::cout << sbrk(0) << std::endl;
    int * arr = (int *)smalloc((sizeof(int))*3);
    if (!arr){
        return -2;
    }

    std::cout << sbrk(0) << std::endl;

    for (int i = 0; i < 3; i++){
        std::cout << arr[i] << ", "<< i << std::endl;
    }

    return 0;
}
