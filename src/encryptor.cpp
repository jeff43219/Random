#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

void processFile(const std::string& filePath, const std::string& key) {
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error: Could not open file at " << filePath << std::endl;
        return;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(inFile)), 
                            std::istreambuf_iterator<char>());
    inFile.close();

    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key.length()];
    }

    std::ofstream outFile(filePath, std::ios::binary | std::ios::trunc);
    outFile.write(data.data(), data.size());
    outFile.close();

    std::cout << "Successfully processed: " << fs::path(filePath).filename() << std::endl;
}

// --- UPDATED: Added argc and argv ---
int main(int argc, char* argv[]) {
    std::string pathInput;
    std::string password;

    std::cout << "--- C++ PRO ENCRYPTOR (In-Place) ---" << std::endl;
    
    // Check if the path was passed via command line
    if (argc > 1) {
        pathInput = argv[1];
    } else {
        std::cout << "Enter FULL PATH to file: ";
        std::getline(std::cin, pathInput);
    }

    if (!pathInput.empty() && pathInput.front() == '"') {
        pathInput = pathInput.substr(1, pathInput.length() - 2);
    }

    if (!fs::exists(pathInput)) {
        std::cout << "Error: That path does not exist!" << std::endl;
        return 1;
    }

    // Check if the password was passed as a second argument
    if (argc > 2) {
        password = argv[2];
    } else {
        std::cout << "Enter your Secret Password: ";
        std::getline(std::cin, password);
    }

    if (password.empty()) {
        std::cout << "Password cannot be empty!" << std::endl;
        return 1;
    }

    processFile(pathInput, password);

    return 0;
}