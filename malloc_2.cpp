#include <iostream>
#include <unistd.h>

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

static void* smallocAux(size_t size){ ///how do we know if we use only the heap and not the mmap?
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
    //didn't find a free block, allocates a new one
    if (!iterator->getNext()){
        Metadata* newMetatData = (Metadata*)smallocAux(MetaDataSize);
        if (!newMetatData){
            return NULL;
        }
        void* p = smallocAux(size);
        if (!p){
            return NULL;
            ///maybe need to free the newMetatData? but on the other hand, don't care about fragmentation/optimizations now
        }
        newMetatData->setSize(size);
        newMetatData->setPrev(iterator);
        iterator->setNext(newMetatData);
        return p;
    }
};

void* scalloc(size_t num, size_t size){};///Amit
void sfree(void* p){};///Ortal
void* srealloc(void* oldp, size_t size){};///Ortal


static size_t _num_free_blocks(){};///Amit

static size_t  _num_free_bytes(){};///Amit

static size_t _num_allocated_blocks(){};///Amit

static size_t  _num_allocated_bytes(){};///Ortal

static size_t  _num_meta_data_bytes(){};///Ortal

static size_t _size_meta_data(){};///Ortal
