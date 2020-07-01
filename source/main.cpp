#include <vector>

#include "gmf_memory_resource.h"

using namespace std;

void example() {
	size_t n = 1040;
	byte* buffer = new byte[n];

	gmf_memory_resource mr { buffer, buffer + n };
	const std::pmr::polymorphic_allocator<int> allocator { &mr };

	vector<int, std::pmr::polymorphic_allocator<int>> vec { allocator };

	vec.resize(256);
}

int main() {
	example();
	
	return 0;
}