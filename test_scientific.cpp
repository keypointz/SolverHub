#include "code/SharedMemoryStruct.h"
#include <iostream>
#include <fstream>

int main() {
    // Create test data with values that will show scientific notation
    EMP::LocalData data("scientific_test", "test_mesh");
    data.isFieldData = true;
    data.type = EMP::DataGeoType::VertexData;
    data.t = 0.00000123456789; // Very small number

    // Add component data with various magnitudes
    data.addComponent("small", { 0.00000000123, 0.00000000456, 0.00000000789 }, "m");
    data.addComponent("medium", { 123.456789, 456.789123, 789.123456 }, "m");
    data.addComponent("large", { 1234567890.12, 4567891234.56, 7891234567.89 }, "m");

    // Add index
    data.index = { 1, 2, 3 };

    // Save to file
    bool result = data.saveToFile("scientific_test.txt");
    if (result) {
        std::cout << "File saved successfully. Check scientific_test.txt for scientific notation." << std::endl;

        // Display the first few lines of the file
        std::ifstream file("scientific_test.txt");
        if (file.is_open()) {
            std::string line;
            int lineCount = 0;
            while (std::getline(file, line) && lineCount < 50) {
                std::cout << line << std::endl;
                lineCount++;
            }
            file.close();
        }
    } else {
        std::cout << "Failed to save file." << std::endl;
    }

    return 0;
}
