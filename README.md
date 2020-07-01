# GMF Allocator | General minimal-fragmentation allocator

## This allocator implementation allows you to:
- allocate chunks of memory on any buffers (stack allocated, heap allocated)
- deallocate memory based on just a pointer

## Features:
- have minimal fragmentation during allocation
- perform allocation in logarithmic time - **log n**

## Usage
To use you will need `std::pmr::polymorphic_allocator<T>`which is available in the std library *only with C++17 or later*.
Here's an example of using a 1040 byte buffer to create a 256-element `std::vector`:
```cpp
size_t n = 1040;
std::byte* buffer = new std::byte[n]; // buffer creation

gmf_memory_resource mr { buffer, buffer + n }; // create gmf_memory_resource based on a given buffer
const std::pmr::polymorphic_allocator<int> allocator { &mr }; // create allocator based on a given memory resource

std::vector<int, std::pmr::polymorphic_allocator<int>> vec { allocator }; // creating a vector based on a given allocator 
vec.resize(256); // it's ok
```


## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License
[GPLv3](https://choosealicense.com/licenses/gpl-3.0/)
