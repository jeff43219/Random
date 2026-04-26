#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

int main() {
    std::string pathInput, targetName;
    std::vector<fs::path> matches;

    std::cout << "--- C++ Folder & File Nuker ---\n";
    std::cout << "Enter folder path: ";
    std::getline(std::cin, pathInput);

    if (!pathInput.empty() && pathInput.front() == '"') {
        pathInput = pathInput.substr(1, pathInput.length() - 2);
    }

    fs::path root(pathInput);

    if (!fs::exists(root)) {
        std::cerr << "Error: Path not found!\n";
        return 1;
    }

    std::cout << "Enter name to find (case-insensitive): ";
    std::getline(std::cin, targetName);

    std::string searchLower = toLower(targetName);

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            std::string nameLower = toLower(entry.path().filename().string());

            if (nameLower.find(searchLower) != std::string::npos) {
                matches.push_back(entry.path());
                std::string type = entry.is_directory() ? "FOLDER" : "FILE";
                std::cout << "[MATCH] " << type << ": " << entry.path().filename().string() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << "\n";
        return 1;
    }

    if (matches.empty()) {
        std::cout << "Nothing found.\n";
        return 0;
    }

    std::cout << "\nFound " << matches.size() << " items. Delete all? (y/n): ";
    char confirm;
    std::cin >> confirm;

    if (confirm == 'y' || confirm == 'Y') {
        int deleted = 0;
        for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
            try {
                if (fs::exists(*it)) {
                    deleted += fs::remove_all(*it);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error deleting " << *it << ": " << e.what() << "\n";
            }
        }
        std::cout << "Done. Items removed: " << deleted << "\n";
    } else {
        std::cout << "Cancelled.\n";
    }

    return 0;
}