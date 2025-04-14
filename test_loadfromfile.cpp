#include "code/SharedMemoryStruct.h"
#include <iostream>

int main() {
    // Test with file that has index-value pairs but no explicit format declaration
    {
        EMP::LocalData data;
        bool result = data.loadFromFile("test_auto_format.txt");
        
        std::cout << "Test 1 (index-value pairs):\n";
        std::cout << "Load result: " << (result ? "Success" : "Failed") << "\n";
        std::cout << "isSequentiallyMatchedWithMesh: " << (data.isSequentiallyMatchedWithMesh ? "true" : "false") << "\n";
        std::cout << "Data size: " << data.data.size() << "\n";
        std::cout << "Index size: " << data.index.size() << "\n";
        
        // Print first few values
        std::cout << "First few values:\n";
        for (size_t i = 0; i < std::min(data.data.size(), size_t(5)); ++i) {
            std::cout << "Index: " << data.index[i] << ", Value: " << data.data[i] << "\n";
        }
        std::cout << "\n";
    }
    
    // Test with file that has only values but no explicit format declaration
    {
        EMP::LocalData data;
        bool result = data.loadFromFile("test_auto_format2.txt");
        
        std::cout << "Test 2 (values only):\n";
        std::cout << "Load result: " << (result ? "Success" : "Failed") << "\n";
        std::cout << "isSequentiallyMatchedWithMesh: " << (data.isSequentiallyMatchedWithMesh ? "true" : "false") << "\n";
        std::cout << "Data size: " << data.data.size() << "\n";
        std::cout << "Index size: " << data.index.size() << "\n";
        
        // Print first few values
        std::cout << "First few values:\n";
        for (size_t i = 0; i < std::min(data.data.size(), size_t(5)); ++i) {
            std::cout << "Index: " << data.index[i] << ", Value: " << data.data[i] << "\n";
        }
        std::cout << "\n";
    }
    
    return 0;
}
