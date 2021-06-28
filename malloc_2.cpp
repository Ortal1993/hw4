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


void* smalloc(size_t size){

        Metadata *iterator = &metalistHead;
        while (iterator->getNext()) {
            if (iterator->isFree() == true) {
                if (iterator->getSize() >= size) {
                    iterator->setIsFree(false);
                    return (iterator + MetaDataSize);
                }
            }
            iterator = iterator->getNext();
        }
        //check the last Metadata node
        if (iterator->isFree() == true) {
            if (iterator->getSize() >= size) {
                iterator->setIsFree(false);
                return (iterator + MetaDataSize);
            }
        }

    //didn't find a free block, allocates a new one
    if (!iterator->getNext()){
        void * newBlock = smallocAux((MetaDataSize + size));
        if (!newBlock){
            return NULL;
        }
        Metadata * newMetaData = (Metadata*)newBlock;
        *newMetaData = Metadata(size);
        newMetaData->setPrev(iterator);
        iterator->setNext(newMetaData);
        return (newMetaData + MetaDataSize);
    }
};


void* scalloc(size_t num, size_t size){
    if (size == 0){
        return NULL;
    }
    size_t desiredSize = num * size;

    Metadata* iterator = &metalistHead;
    while (iterator->getNext()){
        if(iterator->isFree() == true) {
            if (iterator->getSize() >= desiredSize) {
                iterator->setIsFree(false);
                return (iterator + MetaDataSize);
            }
        }
        iterator = iterator->getNext();
    }
    //check the last Metadata node
    if (iterator->isFree() == true) {
        if (iterator->getSize() >= desiredSize) {
            iterator->setIsFree(false);
            return (iterator + MetaDataSize);
        }
    }

    //didn't find a free block, allocates a new one
    if (!iterator->getNext()){
        void * newBlock = smallocAux((MetaDataSize + desiredSize));
        Metadata * newMetaData = (Metadata*)newBlock;
        *newMetaData = Metadata(desiredSize);
        newMetaData->setPrev(iterator);
        iterator->setNext(newMetaData);

        std::memset(newMetaData + MetaDataSize, 0, desiredSize);

        return (newMetaData + MetaDataSize);
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


    //searching for a free block (big enough)
    Metadata* iterator = &metalistHead;
   while (iterator->getNext()) {
       if (iterator->isFree() == true){
           if (iterator->getSize() >= size){
               iterator->setIsFree(false);//changing the new block
               //need to mark the oldp block as free now, since we moved it to a new block
               if (oldp != nullptr){
                   Metadata *oldpMetaData = ((Metadata *) oldp - MetaDataSize);
                   oldpMetaData->setIsFree(true);
                   return std::memcpy(iterator + MetaDataSize, oldp, size);
               }
           }
       }
       iterator = iterator->getNext();
   }
   //check the last Metadata node
   if(iterator->isFree() == true){
       if (iterator->getSize() >= size){
           iterator->setIsFree(false);//changing the new block
           //need to mark the oldp block as free now, since we moved it to a new block
           if (oldp != nullptr){
               Metadata *oldpMetaData = ((Metadata *) oldp - MetaDataSize);
               oldpMetaData->setIsFree(true);
               return std::memcpy(iterator + MetaDataSize, oldp, size);
           }

           return (iterator + MetaDataSize);
       }
   }


    //didn't find a free block, allocates a new one
    void * newBlock = smallocAux((MetaDataSize + size));
    Metadata * newMetaData = (Metadata*)newBlock;
    *newMetaData = Metadata(size);
    newMetaData->setPrev(iterator);
    iterator->setNext(newMetaData);

    //need to mark the oldp block as free now, since we moved it to a new block
    if (oldp != nullptr){
        Metadata *oldpMetaData = ((Metadata *) oldp - MetaDataSize);
        oldpMetaData->setIsFree(true);
        return std::memcpy(newMetaData + MetaDataSize, oldp, size);
    }

    return (newMetaData + MetaDataSize);
};




int main() {
    std::cout << "Hello, World!" << std::endl;
    std::cout << sbrk(0) << std::endl;
    char * arr = (char *)smalloc((sizeof(char))*3);
    if (!arr){
        return -2;
    }

    std::cout << sbrk(0) << std::endl;

    for (int i = 0; i < 3; i++){
        std::cout << arr[i] << ", "<< i << std::endl;
    }

    sfree(arr);
    std::cout << sbrk(0) << std::endl;

    if (metalistHead.getNext()) {
        if (metalistHead.getNext()->isFree() == true){
            std::cout << "is free? true";
        }
        else{
            std::cout << " is free? false";
        }
        std::cout << " size? " << metalistHead.getNext()->getSize()
                  << std::endl;
        if(metalistHead.getNext()->getNext() == nullptr){
            std::cout << "next is null" << std::endl;
        }
    }

    std::cout << sbrk(0) << std::endl;
    char * arr2 = (char *)smalloc((sizeof(char))*2);
    if (!arr){
        return -2;
    }

    std::cout << sbrk(0) << std::endl;


    if (metalistHead.getNext()) {
        if (metalistHead.getNext()->isFree() == true){
            std::cout << "is free? true";
        }
        else{
            std::cout << " is free? false";
        }
        std::cout << " size? " << metalistHead.getNext()->getSize()
                  << std::endl;
        if(metalistHead.getNext()->getNext() == nullptr){
            std::cout << "next is null" << std::endl;
        }
    }

    std::cout << sbrk(0) << std::endl;
    char * arr3 = (char *)smalloc((sizeof(char))*5);
    if (!arr){
        return -2;
    }

    std::cout << sbrk(0) << std::endl;

    Metadata* iterator = &metalistHead;
    while(iterator->getNext()) {
            if (iterator->getNext()->isFree() == true) {
                std::cout << "is free? true";
            } else {
                std::cout << " is free? false";
            }
            std::cout << " size? " << iterator->getNext()->getSize()
                      << std::endl;
            if (iterator->getNext()->getNext() == nullptr) {
                std::cout << "next is null" << std::endl;
            }
            iterator = iterator->getNext();
    }

    std::cout << "this is free of arr3 size 5"<<std::endl;
    sfree(arr3);
    std::cout << sbrk(0) << std::endl;

    iterator = &metalistHead;
    while(iterator->getNext()) {
        if (iterator->getNext()->isFree() == true) {
            std::cout << "is free? true";
        } else {
            std::cout << " is free? false";
        }
        std::cout << " size? " << iterator->getNext()->getSize()
                  << std::endl;
        if (iterator->getNext()->getNext() == nullptr) {
            std::cout << "next is null" << std::endl;
        }
        iterator = iterator->getNext();
    }

    char * arr6 = (char *) srealloc(arr2,(sizeof(char))*4);
    if (!arr){
        return -2;
    }

    iterator = &metalistHead;
    while(iterator->getNext()) {
        if (iterator->getNext()->isFree() == true) {
            std::cout << "is free? true";
        } else {
            std::cout << " is free? false";
        }
        std::cout << " size? " << iterator->getNext()->getSize()
                  << std::endl;
        if (iterator->getNext()->getNext() == nullptr) {
            std::cout << "next is null" << std::endl;
        }
        iterator = iterator->getNext();
    }

    std::cout << sbrk(0) << std::endl;

    std::cout << "_num_free_blocks "  << _num_free_blocks() << std::endl;
    std::cout << " _num_free_bytes() "  <<  _num_free_bytes() << std::endl;
    std::cout << "_num_allocated_blocks() "  << _num_allocated_blocks() << std::endl;
    std::cout << "_num_allocated_bytes() "  << _num_allocated_bytes() << std::endl;
    std::cout << "_num_meta_data_bytes() "  << _num_meta_data_bytes() << std::endl;
    std::cout << "t _size_meta_data() "  << _size_meta_data() << std::endl;

    return 0;
}
