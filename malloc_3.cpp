#include <iostream>
#include <unistd.h>
#include <cstring>

#define BYTES_NUM 128
#define K_BYTE 1024

class Metadata {
private:
    size_t size;
    bool is_free;
    Metadata* next;
    Metadata* prev;
    Metadata* histoNext;
    Metadata* histoPrev;
public:
    Metadata(size_t size, bool is_free = false, Metadata* next = NULL, Metadata* prev = NULL, Metadata* histoNext = NULL,
             Metadata* histoPrev = NULL):
            size(size), is_free(is_free), next(next), prev(prev), histoNext(histoNext), histoPrev(histoPrev){};
    ~Metadata() = default;///?
    Metadata* getNext(){return this->next;};
    Metadata* getPrev(){return this->prev;};
    Metadata* getHistoNext(){return this->histoNext;};
    Metadata* getHistoPrev(){return this->histoPrev;};
    size_t getSize(){return this->size;};
    bool isFree(){return this->is_free;};
    void setNext(Metadata* next){this->next = next;};
    void setPrev(Metadata* prev){this->prev = prev;};
    void setHistoNext(Metadata* histoNext){this->histoNext = histoNext;};
    void setHistoPrev(Metadata* histoPrev){this->histoPrev = histoPrev;};
    void setSize(size_t size){this->size = size;};
    void setIsFree(bool arg){this->is_free = arg;};
};

Metadata metalistHead(0);
size_t MetaDataSize = sizeof(Metadata);

Metadata* bins[BYTES_NUM] = {};

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

void InsertToHist(Metadata* metadataToInsert){
    size_t sizeBlock = metadataToInsert->getSize();
    int index = sizeBlock / K_BYTE;
    Metadata* iteratorHist = bins[index];
    if(iteratorHist == nullptr){
        Metadata binsHeadList(0);
        bins[index] = &binsHeadList;
        iteratorHist = bins[index];
    }

    while (iteratorHist->getSize() < sizeBlock){
        iteratorHist = iteratorHist->getHistoNext();
    }

    //insert metadata in the middle of the list
    if (iteratorHist != nullptr){
        metadataToInsert->setHistoNext(iteratorHist);
        metadataToInsert->setHistoPrev(iteratorHist->getHistoPrev());
        iteratorHist->setHistoPrev(metadataToInsert);
    }

    //insert metadata in the end of the list
    metadataToInsert->setHistoNext(nullptr);
    metadataToInsert->setHistoPrev(iteratorHist);
    iteratorHist->setHistoNext(metadataToInsert);

}

void RemoveFromHist(Metadata* metadataToRemove, size_t originalSize){
    int index = originalSize / K_BYTE;
    Metadata* iteratorHist = bins[index];

    while (iteratorHist != metadataToRemove){
        iteratorHist = iteratorHist->getHistoNext();
    }

    //remove metadata from the middle of the list
    if (iteratorHist != nullptr){
        metadataToRemove->getHistoPrev()->setHistoNext(metadataToRemove->getHistoNext());
        metadataToRemove->getHistoNext()->setHistoPrev(metadataToRemove->getHistoPrev());
        iteratorHist->setHistoPrev(nullptr);
        iteratorHist->setHistoNext(nullptr);
    }

    //remove metadata from the end of the list
    metadataToRemove->getHistoPrev()->setHistoNext(nullptr);
    iteratorHist->setHistoPrev(nullptr);
    iteratorHist->setHistoNext(nullptr);
}

void SplitBlock(Metadata* currMetadata, size_t size){
    size_t originalBlockSize = currMetadata->getSize();
    currMetadata->setSize(size);

    Metadata* newMetadata = (Metadata*)smallocAux(MetaDataSize);
    size_t remainingSize = originalBlockSize - size - MetaDataSize;
    *newMetadata = Metadata(remainingSize);
    if (!newMetadata){
        return;
    }
    void* p = smallocAux(size);
    if (!p){
        newMetadata->setSize(0);
        return;
        ///maybe need to free the newMetadata? but on the other hand, don't care about fragmentation/optimizations now
    }
    newMetadata->setIsFree(true);///maybe now this field is not necessary cause all free are in the hist
    currMetadata->setIsFree(false);///maybe now this field is not necessary cause all free are in the hist
    newMetadata->setPrev(currMetadata);
    newMetadata->setNext(currMetadata->getNext());
    currMetadata->setNext(newMetadata);

    InsertToHist(newMetadata);
    RemoveFromHist(currMetadata, originalBlockSize);

}

Metadata* FindLast(){
    Metadata* iterator = &metalistHead;
    while (iterator->getNext()){
        iterator = iterator->getNext();
    }
    return iterator;
}

void* smalloc(size_t size){
    //find free block in histogram
    for(int index = size / K_BYTE; index < BYTES_NUM; index++){
        Metadata* iteratorHist = bins[index];
        if(iteratorHist == nullptr){
            Metadata binsHeadList(0);
            bins[index] = &binsHeadList;
            continue;
        }
        while (iteratorHist->getHistoNext()){
            if (iteratorHist->getSize() >= size + BYTES_NUM + MetaDataSize && iteratorHist->isFree()){///maybe now iteratorHist->isFree() is not necessary cause all free are in the hist
                SplitBlock(iteratorHist, size);
                return iteratorHist;
            }
            iteratorHist = iteratorHist->getNext();
        }
        //check the last Metadata node
        if (iteratorHist->getSize() >= size + BYTES_NUM + MetaDataSize && iteratorHist->isFree()){///maybe now iteratorHist->isFree() is not necessary cause all free are in the hist
            SplitBlock(iteratorHist, size);
            return iteratorHist;
        }
    }

    //didn't find a free block in binsHist, allocates a new one
    Metadata* lastMetadata = FindLast();
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
    lastMetadata->setNext(newMetaData);
    newMetaData->setPrev(lastMetadata);
    newMetaData->setNext(nullptr);
    return p;
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
