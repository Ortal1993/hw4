#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

#define BYTES_NUM 128
#define K_BYTE 1024
#define OVER_SIZE 100000000

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
Metadata * metalistTail = &metalistHead;

Metadata mmapedHead(0);
Metadata * mmapedTail = &mmapedHead;

Metadata* bins[BYTES_NUM] = {};

size_t _num_free_blocks();
size_t  _num_free_bytes();
size_t _num_allocated_blocks();
size_t  _num_allocated_bytes();
size_t  _num_meta_data_bytes();
size_t _size_meta_data();
Metadata* Wildrness(size_t size);

static void* smallocAux(size_t size){
    if(size == 0 || size > OVER_SIZE){
        return NULL;
    }
    void* p = sbrk(size);
    if(p == (void*)-1){
        return NULL;
    }
    return p;
}

static void* mmapedAux(size_t size){//will be used only with size > 128kb
    void* p = mmap(NULL, size + MetaDataSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED){
        return nullptr;
    }
    Metadata* newMapMetaData = (Metadata*)p;
    *newMapMetaData = Metadata(size);
    newMapMetaData->setPrev(mmapedTail);
    if(mmapedTail != &mmapedHead){
        mmapedTail->setNext(newMapMetaData);
    }else{//list empty
        mmapedHead.setNext(newMapMetaData);
    }
    newMapMetaData->setNext(nullptr);
    mmapedTail = newMapMetaData;
    void* ptrBlock = (void*)((size_t)newMapMetaData + MetaDataSize);
    return ptrBlock;
}

static void unmmapedAux(void * adr, size_t size){//will be used only with size > 128kb
    ///no need to check if adr is null?
    Metadata* toDelete = (Metadata*)adr - MetaDataSize;
    Metadata* prev = toDelete->getPrev();
    Metadata* next = toDelete->getNext();
    if(prev){
        prev->setNext(toDelete->getNext());
    }
    if (next) {
        next->setPrev(toDelete->getPrev());
    }
    toDelete->setPrev(nullptr);
    toDelete->setNext(nullptr);
    int err = munmap(toDelete, size);
    if (err == -1){
        if(prev){
            prev->setNext(toDelete);
        }
        if(next) {
            next->setPrev(toDelete->getPrev());
        }
        toDelete->setPrev(prev);
        toDelete->setNext(next);
        return;
    }
    if (next == nullptr){//it was the last, need to update the tail
        mmapedTail = prev;
    }
}

static void* isOnMap(void * p){
    Metadata* iterator = &mmapedHead;
    while (iterator){
        void* ptrBlock = (void*)((size_t)iterator + MetaDataSize);
        if(ptrBlock == p){
            return p;
        }
        iterator = iterator->getNext();
    }
}

static void* isOnHeap(void * p){
    Metadata* iterator = &metalistHead;
    while (iterator){
        void* ptrBlock = (void*)((size_t)iterator + MetaDataSize);
        if(ptrBlock == p){
            return p;
        }
        iterator = iterator->getNext();
    }
}

void InsertToHist(Metadata* metadataToInsert){
    size_t sizeBlock = metadataToInsert->getSize();
    int index = sizeBlock / K_BYTE;
    Metadata* iteratorHist = bins[index];
    //list is empty
    if(iteratorHist == nullptr){
        bins[index] = metadataToInsert;
        return;
    }

    while (iteratorHist){
        if(iteratorHist->getSize() < sizeBlock){
            iteratorHist = iteratorHist->getHistoNext();
        }else{
            break;
        }
    }

    //insert metadata in the middle of the list
    if (iteratorHist != nullptr){
        metadataToInsert->setHistoNext(iteratorHist);
        metadataToInsert->setHistoPrev(iteratorHist->getHistoPrev());
        iteratorHist->setHistoPrev(metadataToInsert);
        if(iteratorHist == bins[index]){
            bins[index] = metadataToInsert;
        }
        return;
    }

    //insert metadata in the end of the list
    metadataToInsert->setHistoNext(nullptr);
    metadataToInsert->setHistoPrev(iteratorHist);
    iteratorHist->setHistoNext(metadataToInsert);

}

void RemoveFromHist(Metadata* metadataToRemove, size_t originalSize){
    int index = originalSize / K_BYTE;
    Metadata* iteratorHist = bins[index];
    if(iteratorHist == NULL){//hasn't been inserted yet
        return;
    }

    while(iteratorHist){
        if (iteratorHist != metadataToRemove){
            iteratorHist = iteratorHist->getHistoNext();
        }else{
            break;
        }
    }

    //first and only
    if (iteratorHist->getHistoNext() == nullptr && iteratorHist->getHistoPrev() == nullptr && iteratorHist == metadataToRemove){
        bins[index] = nullptr;
    }

        //first but not only
    else if (iteratorHist->getHistoPrev() == nullptr){
        bins[index] = iteratorHist->getHistoNext();
        iteratorHist->getHistoNext()->setHistoPrev(nullptr);
        metadataToRemove->setHistoNext(nullptr);
    }

        //remove metadata from the middle of the list
    else if (iteratorHist->getHistoNext() != nullptr && iteratorHist->getHistoPrev() != nullptr){
        metadataToRemove->getHistoPrev()->setHistoNext(metadataToRemove->getHistoNext());
        metadataToRemove->getHistoNext()->setHistoPrev(metadataToRemove->getHistoPrev());
        iteratorHist->setHistoPrev(nullptr);
        iteratorHist->setHistoNext(nullptr);
    }

        //remove metadata from the end of the list
    else {
        metadataToRemove->getHistoPrev()->setHistoNext(nullptr);
        iteratorHist->setHistoPrev(nullptr);
        iteratorHist->setHistoNext(nullptr);
    }
}

void SplitBlock(Metadata* currMetadata, size_t size){
    size_t originalBlockSize = currMetadata->getSize();
    currMetadata->setSize(size);

    void* ptrMetadata = (void*)((size_t)currMetadata + MetaDataSize + size);
    Metadata* newMetadata = (Metadata*)ptrMetadata;
    size_t remainingSize = originalBlockSize - size - MetaDataSize;
    *newMetadata = Metadata(remainingSize);

    newMetadata->setIsFree(true);
    currMetadata->setIsFree(false);
    newMetadata->setPrev(currMetadata);
    newMetadata->setNext(currMetadata->getNext());
    currMetadata->setNext(newMetadata);

    InsertToHist(newMetadata);
    RemoveFromHist(currMetadata, originalBlockSize);
}

Metadata* Wildrness(size_t size){
    size_t enlarge = size - metalistTail->getSize();
    void * addition = smallocAux(enlarge);
    if (!addition){
        return NULL;
    }
    //std::cout << "num of free blocks before " << _num_free_blocks() << std::endl;
    RemoveFromHist(metalistTail, metalistTail->getSize());
    //std::cout << "num of free blocks after " << _num_free_blocks() << std::endl;
    metalistTail->setSize(size);
    metalistTail->setIsFree(false);
    return metalistTail;
}

void* smalloc(size_t size){
    if(size > BYTES_NUM * K_BYTE){
        return mmapedAux(size);
    }

    //find free block in histogram
    for(int index = size / K_BYTE; index < BYTES_NUM; index++){
        Metadata* iteratorHist = bins[index];
        if(iteratorHist == nullptr){
            continue;
        }
        while (iteratorHist){
            if (iteratorHist->getSize() >= size){
                if (iteratorHist->getSize() >= size + BYTES_NUM + MetaDataSize && iteratorHist->isFree()) {///maybe now iteratorHist->isFree() is not necessary cause all free are in the hist
                    SplitBlock(iteratorHist, size);
                }else{
                    iteratorHist->setIsFree(false);
                    RemoveFromHist(iteratorHist, iteratorHist->getSize());

                }
                void* ptrBlock = (void*)((size_t)iteratorHist + MetaDataSize);
                return ptrBlock;
            }
            iteratorHist = iteratorHist->getHistoNext();
        }
    }

    //'wildrness'
    if (metalistTail->isFree()){
        void* ptrBlock = (void*)((size_t)Wildrness(size) + MetaDataSize);
        return ptrBlock;
    }

    //didn't find a free block in binsHist, allocates a new one
    void * newBlock = smallocAux((MetaDataSize + size));
    if (!newBlock){
        return NULL;
    }
    Metadata * newMetaData = (Metadata*)newBlock;
    *newMetaData = Metadata(size);
    newMetaData->setPrev(metalistTail);
    if(metalistTail != &metalistHead){
        metalistTail->setNext(newMetaData);
    }else{
        metalistHead.setNext(newMetaData);
    }
    newMetaData->setNext(nullptr);
    metalistTail = newMetaData; //updating the last one*/
    void* ptrBlock = (void*)((size_t)newMetaData + MetaDataSize);
    return ptrBlock;

};

void* scalloc(size_t num, size_t size){
    if(size > BYTES_NUM * K_BYTE){
        void * p = mmapedAux(size);
        if (p){
            return std::memset(p, 0, size);
        }
        return nullptr;
    }

    if (size == 0){
        return NULL;
    }

    size_t desiredSize = num * size;
    if(desiredSize > OVER_SIZE){
        return NULL;
    }

    for(int index = desiredSize / K_BYTE; index < BYTES_NUM; index++){
        Metadata* iteratorHist = bins[index];
        if(iteratorHist == nullptr){
            continue;
        }
        while (iteratorHist){
            if (iteratorHist->getSize() >= desiredSize + BYTES_NUM + MetaDataSize && iteratorHist->isFree()){///maybe now iteratorHist->isFree() is not necessary cause all free are in the hist
                SplitBlock(iteratorHist, desiredSize);
                void* ptrBlock = (void*)((size_t)iteratorHist + MetaDataSize);
                std::memset(ptrBlock, 0, desiredSize);
                return ptrBlock;
            }
            iteratorHist = iteratorHist->getHistoNext();
        }
    }

    //'wildrness'
    if (metalistTail->isFree()){
        Metadata* metadata = Wildrness(size);
        void* ptrBlock = (void*)((size_t)metadata + MetaDataSize);
        std::memset(ptrBlock, 0, desiredSize);
        return ptrBlock;
    }

    //didn't find a free block in binsHist, allocates a new one
    void * newBlock = smallocAux((MetaDataSize + size));
    if (!newBlock){
        return NULL;
    }
    Metadata * newMetaData = (Metadata*)newBlock;
    *newMetaData = Metadata(size);
    newMetaData->setPrev(metalistTail);
    if(metalistTail != &metalistHead){
        metalistTail->setNext(newMetaData);
    }else{
        metalistHead.setNext(newMetaData);
    }
    newMetaData->setNext(nullptr);
    metalistTail = newMetaData; //updating the last one*/
    void* ptrBlock = (void*)((size_t)newMetaData + MetaDataSize);
    return ptrBlock;
};

void MergeForward(Metadata* iterator){
    int sizeOfNext = 0;
    if(iterator->getNext() != nullptr){
        if(iterator->getNext()->isFree()){
            sizeOfNext = iterator->getNext()->getSize();
            RemoveFromHist(iterator->getNext(), sizeOfNext);
            if(iterator->getNext()->getNext() != nullptr){
                iterator->getNext()->getNext()->setPrev(iterator);
            }
            iterator->setNext(iterator->getNext()->getNext());
            iterator->setSize(iterator->getSize() + MetaDataSize + sizeOfNext);
        }
    }
}

Metadata* MergeBackward(Metadata* iterator){
    int sizeOfPrev = 0;
    if(iterator->getPrev() != nullptr){
        if(iterator->getPrev()->isFree()){
            sizeOfPrev = iterator->getPrev()->getSize();
            RemoveFromHist(iterator->getPrev(), sizeOfPrev);
            iterator->getPrev()->setNext(iterator->getNext());
            if(iterator->getNext() != nullptr){
                iterator->getNext()->setPrev(iterator->getPrev());
            }
            size_t currentSize = iterator->getSize();
            void* ptrIterator = (void*)((size_t)iterator - sizeOfPrev - MetaDataSize);
            iterator = (Metadata*)ptrIterator;
            iterator->setSize(currentSize + MetaDataSize + sizeOfPrev);
            return iterator;
        }
    }
    return iterator;
}

void sfree(void* p){///p points to the block after metadata
    if(p == nullptr){
        return;
    }
    void * ptr = isOnHeap(p);
    if (ptr != nullptr){
        void* ptrMetadata = (void*)((size_t)ptr - MetaDataSize);
        Metadata* metadata = (Metadata*)ptrMetadata;
        //std::cout << "addr of metadata in sfree: " << metadata << std::endl;
        if (metadata->isFree()){
            return;
        }else{
            metadata->setIsFree(true);
            MergeForward(metadata);
            metadata = MergeBackward(metadata);///problem??
            InsertToHist(metadata);
            return;
        }
    }
    else{
        ptr = isOnMap(p);
        void* ptrMetadata = (void*)((size_t)ptr - MetaDataSize);
        Metadata* mapData = (Metadata*)ptrMetadata;
        unmmapedAux(p, mapData->getSize());
    }
};

static void* reallocAux(void * oldp, size_t size) {
    if(size == 0 || size > OVER_SIZE){
        return nullptr;
    }
    size_t currSize = 0;
    if (oldp) {
        void* ptrMetadata = (void*)((size_t)oldp - MetaDataSize);
        Metadata* oldpMetaData = (Metadata*)ptrMetadata;
        bool found = false;
        if ((oldpMetaData->getSize() >= size)){
            found = true;
        } else if (oldpMetaData->getPrev() && (oldpMetaData->getSize() + oldpMetaData->getPrev()->getSize()) >= size
                   && oldpMetaData->getPrev()->isFree() && oldpMetaData->getPrev()->isFree()) {///maybe now iteratorHist->isFree() is not necessary cause all free are in the hist
            oldpMetaData = MergeBackward(oldpMetaData);
            found = true;
        }else if (oldpMetaData->getNext() && (oldpMetaData->getSize() + oldpMetaData->getNext()->getSize()) >= size
                  && oldpMetaData->isFree() && oldpMetaData->getNext()->isFree()) {
            MergeForward(oldpMetaData);
            found = true;
        } else if(oldpMetaData->getNext() && oldpMetaData->getPrev() && (oldpMetaData->getSize() + oldpMetaData->getPrev()->getSize() + oldpMetaData->getNext()->getSize()) >= size
                  && oldpMetaData->isFree() && oldpMetaData->getPrev()->isFree() && oldpMetaData->getNext()->isFree()) {
            oldpMetaData = MergeBackward(oldpMetaData);
            MergeForward(oldpMetaData);
            found = true;
        }
        if (oldpMetaData->getSize() >= size + BYTES_NUM + MetaDataSize) {
            SplitBlock(oldpMetaData, size);
        }
        if(found){
            oldpMetaData->setIsFree(false);
            RemoveFromHist(oldpMetaData, oldpMetaData->getSize());///maybe a problem
            void* ptrBlock = (void*)((size_t)oldpMetaData + MetaDataSize);
            if(oldp){
                return std::memcpy(ptrBlock, oldp, size);
            }else{
                return ptrBlock;
            }
        }
    }//oldp is not good enough and also his neibours

    //searching for a free block that is good enough
    for(int index = 0; index < BYTES_NUM; index++){
        Metadata* iteratorHist = bins[index];
        if(iteratorHist == nullptr){
            continue;
        }
        while (iteratorHist){
            if (iteratorHist->getSize() >= size){
                if (iteratorHist->getSize() >= size + BYTES_NUM + MetaDataSize) {
                    SplitBlock(iteratorHist, size);
                }else{
                    iteratorHist->setIsFree(false);
                    RemoveFromHist(iteratorHist, iteratorHist->getSize());
                }
                void* ptrBlock = (void*)((size_t)iteratorHist + MetaDataSize);
                if(oldp){
                    ptrBlock = std::memcpy(ptrBlock, oldp, size);
                    sfree(oldp);
                }
                return ptrBlock;
            }
            iteratorHist = iteratorHist->getHistoNext();
        }
    }

    //'wildrness'
    if (metalistTail->isFree()){
        Metadata* metadata = Wildrness(size);
        void* ptrBlock = (void*)((size_t)metadata + MetaDataSize);
        if(oldp){
            ptrBlock = std::memcpy(ptrBlock, oldp, size);
            sfree(oldp);
        }else{
            return ptrBlock;
        }
    }

    //didn't find a free block, allocates a new one
    void * newBlock = smallocAux((MetaDataSize + size));
    if (!newBlock){
        return NULL;
    }
    Metadata * newMetaData = (Metadata*)newBlock;
    *newMetaData = Metadata(size);
    newMetaData->setPrev(metalistTail);
    if(metalistTail != &metalistHead){
        metalistTail->setNext(newMetaData);
    }else{
        metalistHead.setNext(newMetaData);
    }
    newMetaData->setNext(nullptr);
    metalistTail = newMetaData; //updating the last one*/
    void* ptrBlock = (void*)((size_t)newMetaData + MetaDataSize);
    //need to mark the oldp block as free now, since we moved it to a new block
    if (oldp){
        Metadata *oldpMetaData = ((Metadata *) oldp - MetaDataSize);
        oldpMetaData->setIsFree(true);
        InsertToHist(oldpMetaData);
        ptrBlock = std::memcpy(ptrBlock, oldp, size);
        sfree(oldp);
    }
    return ptrBlock;

}

void* srealloc(void* oldp, size_t size){
    if(oldp == nullptr && size > BYTES_NUM * K_BYTE){
        return mmapedAux(size);
    }

    void * foundP = isOnMap(oldp);
    if (foundP){ //it's on the mmap
        void* ptrMetadata = (void*)((size_t)foundP - MetaDataSize);
        Metadata* metadata = (Metadata*)ptrMetadata;
        if(size > BYTES_NUM * K_BYTE){ //new mmap
            void * newMmap = mmapedAux(size);
            if (!newMmap){
                return oldp; //realloc failed, so didn't free (unmapp) the oldp
            }
            std::memmove(newMmap,foundP,size);
            unmmapedAux(foundP, metadata->getSize());
            return newMmap;
        }//need to search/allocate on heap now (the given size is smaller then the block's size on mmap and smaller then 128kb)
        else {
            return reallocAux(oldp, size);
        }
    } else if ((foundP = isOnHeap(oldp))) {//it's on the heap
        if (size > BYTES_NUM * K_BYTE) {//need to allocate with mmap now
            void * newMmap = mmapedAux(size);
            if (!newMmap){
                return foundP; //realloc failed, so didn't free the oldp = foundP
            }
            std::memmove(newMmap,foundP,size);
            Metadata *oldpMetaData = ((Metadata *) foundP - MetaDataSize);
            oldpMetaData->setIsFree(true);
            InsertToHist(oldpMetaData);
            return newMmap;
        } else {//need to search/allocate on the heap (just as we did before)
            return reallocAux(oldp, size);
        }
    } else{
        return reallocAux(oldp, size);
    }
};


size_t _num_free_blocks(){
    Metadata* iterator = &metalistHead;
    int numOfBlocks = 0;
    for(int index = 0; index < BYTES_NUM; index++) {
        Metadata *iteratorHist = bins[index];
        if (iteratorHist == nullptr) {
            continue;
        }
        while (iteratorHist){
            numOfBlocks++;
            iteratorHist = iteratorHist->getHistoNext();
        }
    }
    return numOfBlocks;
};

size_t  _num_free_bytes(){
    Metadata* iterator = &metalistHead;
    int numOfBytes = 0;
    for(int index = 0; index < BYTES_NUM; index++) {
        Metadata *iteratorHist = bins[index];
        if (iteratorHist == nullptr) {
            continue;
        }
        while (iteratorHist){
            numOfBytes += iteratorHist->getSize();
            iteratorHist = iteratorHist->getHistoNext();
        }
    }
    return numOfBytes;
};

size_t _num_allocated_blocks(){

    Metadata* iterator = &metalistHead;
    int numUsedBlocks = 0;
    while (iterator){
        if (iterator){
            numUsedBlocks++;
        }
        iterator = iterator->getNext();
    }

    Metadata* iteratorMap = &mmapedHead;
    int numUsedBlocksMap = 0;
    while (iteratorMap){
        if (iteratorMap){
            numUsedBlocksMap++;
        }
        iteratorMap = iteratorMap->getNext();
    }

    return numUsedBlocks + numUsedBlocksMap - 2; //we counted 2 heads too, so need -2
};

size_t  _num_allocated_bytes(){
    Metadata* iterator = &metalistHead;
    int numUsedBytes = 0;
    while (iterator){
        if (iterator){
            numUsedBytes += iterator->getSize();
        }
        iterator = iterator->getNext();
    }

    Metadata* iteratorMap = &mmapedHead;
    int numUsedBytesMap = 0;
    while (iteratorMap){
        if (iteratorMap){
            numUsedBytesMap += iteratorMap->getSize();
        }
        iteratorMap = iteratorMap->getNext();
    }

    return numUsedBytes + numUsedBytesMap;
};

size_t  _num_meta_data_bytes(){
    size_t num = _num_allocated_blocks();
    return num * MetaDataSize;
};

size_t _size_meta_data(){
    return MetaDataSize;
};