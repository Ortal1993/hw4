#include <iostream>
#include <unistd.h>

///remove static wehn submitting. this is just for be baling to test the other cpps
static void* smalloc(size_t size){ ///how do we know if we use only the heap and not the mmap?
    if(size == 0 || size > 100000000){
        return NULL;
    }
    void* p = sbrk(size);
    if(p == (void*)-1){
        return NULL;
    }
    return p;
}

/*int main() {
    std::cout << "Hello, World!" << std::endl;
    std::cout << sbrk(0) << std::endl;
    int * arr = (int *)smalloc((sizeof(int))*3);
    //int * arr = new int[3];
    if (!arr){
        return -2;
    }

    std::cout << sbrk(0) << std::endl;

    arr[0] = 0;
    arr[1] = 1;
    arr[2] = 2;
    for (int i = 0; i < 100; i++){
        std::cout << arr[i] << ", "<< i << std::endl;
    }
    arr[4005] = 3;

    return 0;
}*/
