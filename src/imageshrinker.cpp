#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <future>
#include <mutex>
#include <limits>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;
std::mutex mtx;

bool isImage(const std::string& ext) {
    std::string lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    return (lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".png" || lowerExt == ".bmp");
}

void compressImage(fs::path inputPath, fs::path rootPath, fs::path tempDir, int quality) {
    int width, height, channels;
    unsigned char* img = stbi_load(inputPath.string().c_str(), &width, &height, &channels, 0);
    
    if (img == nullptr) return;

    fs::path relative = fs::relative(inputPath, rootPath);
    fs::path outputPath = tempDir / relative;
    outputPath.replace_extension(".jpg"); 

    {
        std::lock_guard<std::mutex> lock(mtx);
        fs::create_directories(outputPath.parent_path());
    }

    if (stbi_write_jpg(outputPath.string().c_str(), width, height, channels, img, quality)) {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "[SUCCESS]: Compressed " << relative.string() << std::endl;
    }

    stbi_image_free(img);
}

int main(int argc, char* argv[]) {
    std::string targetInput;
    int qualityLevel = 80; // Default fallback
    std::string confirmReplace = "n";

    std::cout << "--- C++ Ryzen Turbo In-Place Compressor ---" << std::endl;

    if (argc > 1) {
        targetInput = argv[1];
        std::cout << "Target received: " << targetInput << std::endl;
    } else {
        std::cout << "Enter file or folder path: ";
        std::getline(std::cin, targetInput);
    }
    
    if (!targetInput.empty() && (targetInput.front() == '"' || targetInput.front() == '\'')) {
        targetInput = targetInput.substr(1, targetInput.length() - 2);
    }

    fs::path targetPath(targetInput);

    if (!fs::exists(targetPath)) {
        std::cout << "Error: Path not found! Checked: " << targetInput << std::endl;
        return 1;
    }

    // --- NEW: Handle both file and directory logic ---
    fs::path rootPath;
    std::vector<fs::path> imagePaths;

    if (fs::is_regular_file(targetPath)) {
        if (!isImage(targetPath.extension().string())) {
            std::cout << "Error: The specified file is not a supported image format." << std::endl;
            return 1;
        }
        // If it's a file, the root path is the folder containing it
        rootPath = targetPath.parent_path();
        imagePaths.push_back(targetPath);
    } else if (fs::is_directory(targetPath)) {
        // If it's a folder, the root path is the folder itself
        rootPath = targetPath;
    } else {
        std::cout << "Error: Path is neither a regular file nor a directory." << std::endl;
        return 1;
    }

    // Quality argument
    if (argc > 2) {
        qualityLevel = std::stoi(argv[2]);
    } else {
        std::cout << "Enter quality (1-100): ";
        std::cin >> qualityLevel;
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    }

    // Confirmation argument
    if (argc > 3) {
        confirmReplace = argv[3];
    } else {
        std::cout << "REPLACE originals with compressed versions? (yes/no): ";
        std::getline(std::cin, confirmReplace);
    }
    std::transform(confirmReplace.begin(), confirmReplace.end(), confirmReplace.begin(), ::tolower);

    if (confirmReplace != "y" && confirmReplace != "yes") {
        std::cout << "Cancelled." << std::endl;
        return 0;
    }

    fs::path tempDir = rootPath / "temp_processing_bin";

    // If it was a directory, we need to crawl it to find the images
    if (fs::is_directory(targetPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            // Prevent the iterator from processing anything inside our temporary working bin
            if (entry.path().string().find(tempDir.string()) != std::string::npos) continue; 
            
            if (entry.is_regular_file() && isImage(entry.path().extension().string())) {
                imagePaths.push_back(entry.path());
            }
        }
    }

    if (imagePaths.empty()) {
        std::cout << "No valid images found to process!" << std::endl;
        return 0;
    }

    std::cout << "Engaging " << std::thread::hardware_concurrency() << " threads for " << imagePaths.size() << " images...\n";

    std::vector<std::future<void>> futures;
    for (const auto& path : imagePaths) {
        futures.push_back(std::async(std::launch::async, compressImage, path, rootPath, tempDir, qualityLevel));
        if (futures.size() >= 32) {
            for (auto& f : futures) f.wait();
            futures.clear();
        }
    }
    for (auto& f : futures) f.wait();

    std::cout << "\nSwapping files..." << std::endl;
    
    try {
        // Delete original images
        for (const auto& p : imagePaths) {
            fs::remove(p);
        }

        // Move new compressed images out of the temp bin
        for (const auto& entry : fs::recursive_directory_iterator(tempDir)) {
            if (entry.is_regular_file()) {
                fs::path relative = fs::relative(entry.path(), tempDir);
                fs::path finalPath = rootPath / relative;
                fs::create_directories(finalPath.parent_path());
                fs::rename(entry.path(), finalPath);
            }
        }

        // Clean up the temp bin
        fs::remove_all(tempDir);
        
        std::cout << "Done! Optimization complete." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error during file swap: " << e.what() << std::endl;
    }

    return 0;
}