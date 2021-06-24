#include <iostream>
#include <unistd.h>

void* smalloc(size_t size){ ///how do we know if we use only the heap and not the mmap?
    if(size == 0 || size > 100000000){
        return NULL;
    }
    void* p = sbrk(size);
    if(p == (void*)-1){
        return NULL;
    }
    return p;
}

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
