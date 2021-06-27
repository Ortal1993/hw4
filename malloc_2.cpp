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
        Metadata* newMetatData = (Metadata*)smallocAux(MetaDataSize);
        *newMetatData = Metadata(size);
        if (!newMetatData){
            return NULL;
        }
        void* p = smallocAux(size);
        if (!p){
            return NULL;
            ///maybe need to free the newMetatData? but on the other hand, don't care about fragmentation/optimizations now
        }
        newMetatData->setPrev(iterator);
        iterator->setNext(newMetatData);
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
        Metadata* newMetatData = (Metadata*)smallocAux(MetaDataSize);
        *newMetatData = Metadata(size);
        if (!newMetatData){
            return NULL;
        }
        void* p = smallocAux(desiredSize);
        if (!p){
            return NULL;
            ///maybe need to free the newMetatData? but on the other hand, don't care about fragmentation/optimizations now
        }

        std::memset(p, 0, desiredSize);

        newMetatData->setPrev(iterator);
        iterator->setNext(newMetatData);

        return p;
    }

};


void sfree(void* p){};///Ortal
void* srealloc(void* oldp, size_t size){};///Ortal


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

static size_t  _num_meta_data_bytes(){};///Ortal

static size_t _size_meta_data(){};///Ortal

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
