#include <Windows.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>
#include <io.h>
#include <fcntl.h>

namespace fs = std::filesystem;

// ----------------------------------------------------------------------
// Global settings (set from command line)
bool g_dryRun       = false;
bool g_quiet        = false;
bool g_force        = false;   // skip "are you sure?" prompt
bool g_onlyBrowsers = false;
bool g_onlyTemp     = false;

// Stats
uintmax_t g_totalBytes = 0;
size_t    g_totalFiles = 0;

// Own executable path (filled early)
fs::path g_exePath;

// ----------------------------------------------------------------------
// Return true if a path points to our own running executable
bool isOwnExe(const fs::path& p) {
    std::error_code ec;
    return fs::equivalent(p, g_exePath, ec);
}

// ----------------------------------------------------------------------
// Recursively calculate total size of a file or directory
uintmax_t calculateSize(const fs::path& p) {
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) {
        return fs::file_size(p, ec);
    }
    if (fs::is_directory(p, ec)) {
        uintmax_t size = 0;
        for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::none, ec);
             it != fs::recursive_directory_iterator(); ++it) {
            if (it->is_regular_file(ec)) {
                size += it->file_size(ec);
            }
        }
        return size;
    }
    return 0;
}

// ----------------------------------------------------------------------
// Delete a single file or directory, with safety checks
// Returns true if deletion succeeded (or would have succeeded in dry‑run)
bool deletePath(const fs::path& p, const std::wstring& description) {
    if (!fs::exists(p)) return false;

    // Never delete our own exe
    if (isOwnExe(p)) {
        if (!g_quiet) std::wcout << L"Skipping own executable: " << p.wstring() << L"\n";
        return false;
    }

    uintmax_t sz = calculateSize(p);
    if (g_dryRun) {
        if (!g_quiet) std::wcout << L"[DRY] Would delete: " << p.wstring() << L"  (" << sz << L" bytes)\n";
        g_totalBytes += sz;
        g_totalFiles++;
        return true;
    }

    // Real deletion
    std::error_code ec;
    if (fs::is_directory(p, ec)) {
        fs::remove_all(p, ec);   // remove_all does NOT follow directory symlinks
    } else {
        fs::remove(p, ec);
    }

    if (ec) {
        if (!g_quiet) std::wcerr << L"Failed to delete " << p.wstring() << L": " << ec.message().c_str() << L"\n";
        return false;
    }

    g_totalBytes += sz;
    g_totalFiles++;
    if (!g_quiet) std::wcout << L"Deleted: " << p.wstring() << L"\n";
    return true;
}

// ----------------------------------------------------------------------
// Delete all entries inside a directory (but not the directory itself)
void deleteDirectoryContents(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        deletePath(entry.path(), L"");
    }
}

// ----------------------------------------------------------------------
// Section cleaners
void cleanUserTemp() {
    wchar_t* tmp = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &tmp))) {
        fs::path userTemp = fs::path(tmp).parent_path().parent_path() / L"AppData" / L"Local" / L"Temp";
        CoTaskMemFree(tmp);
        if (!g_quiet) std::wcout << L"Cleaning User Temp: " << userTemp.wstring() << L"\n";
        if (fs::exists(userTemp)) {
            deleteDirectoryContents(userTemp);
        }
    }
}

void cleanSystemTemp(bool isAdmin) {
    if (!isAdmin) {
        if (!g_quiet) std::wcout << L"Skipping C:\\Windows\\Temp (requires Administrator)\n";
        return;
    }
    fs::path systemTemp = L"C:\\Windows\\Temp";
    if (!g_quiet) std::wcout << L"Cleaning System Temp: " << systemTemp.wstring() << L"\n";
    if (fs::exists(systemTemp)) {
        deleteDirectoryContents(systemTemp);
    }
}

void cleanPrefetch(bool isAdmin) {
    if (!isAdmin) {
        if (!g_quiet) std::wcout << L"Skipping Prefetch (requires Administrator)\n";
        return;
    }
    fs::path prefetchDir = L"C:\\Windows\\Prefetch";
    if (!g_quiet) std::wcout << L"Cleaning Prefetch: " << prefetchDir.wstring() << L"\n";
    if (!fs::exists(prefetchDir)) return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(prefetchDir, ec)) {
        if (entry.path().extension() == L".pf" || entry.path().filename() == L"Layout.ini") {
            deletePath(entry.path(), L"");
        }
    }
}

void cleanBraveCaches() {
    wchar_t* localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData))) return;
    fs::path braveBase = fs::path(localAppData) / L"BraveSoftware" / L"Brave-Browser" / L"User Data";
    CoTaskMemFree(localAppData);

    if (!fs::exists(braveBase)) {
        if (!g_quiet) std::wcout << L"Brave user data not found.\n";
        return;
    }
    if (!g_quiet) std::wcout << L"Cleaning Brave caches...\n";

    std::error_code ec;
    for (const auto& profileEntry : fs::directory_iterator(braveBase, ec)) {
        if (!profileEntry.is_directory(ec)) continue;

        // Standard cache folders that are safe to delete
        const std::vector<std::wstring> cacheDirs = {L"Cache", L"Code Cache", L"GPUCache", L"DawnCache"};
        for (const auto& cd : cacheDirs) {
            fs::path cachePath = profileEntry.path() / cd;
            if (fs::exists(cachePath)) {
                deletePath(cachePath, L"");
            }
        }
    }
}

void cleanFirefoxCaches() {
    wchar_t* roaming = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roaming))) return;
    fs::path profilesDir = fs::path(roaming) / L"Mozilla" / L"Firefox" / L"Profiles";
    CoTaskMemFree(roaming);

    if (!fs::exists(profilesDir)) {
        if (!g_quiet) std::wcout << L"Firefox profiles not found.\n";
        return;
    }
    if (!g_quiet) std::wcout << L"Cleaning Firefox caches...\n";

    std::error_code ec;
    for (const auto& profileEntry : fs::directory_iterator(profilesDir, ec)) {
        if (!profileEntry.is_directory(ec)) continue;

        // Cache folders that are safe to delete
        const std::vector<std::wstring> cacheFolders = {L"cache2", L"startupCache", L"thumbnails"};
        for (const auto& cf : cacheFolders) {
            fs::path cachePath = profileEntry.path() / cf;
            if (fs::exists(cachePath)) {
                deletePath(cachePath, L"");
            }
        }
    }
}

// ----------------------------------------------------------------------
// Elevation helper
bool elevateIfNeeded(bool needAdmin) {
    if (needAdmin && !IsUserAnAdmin()) {
        // Ask user (unless --quiet – in quiet mode we just skip admin tasks)
        if (g_quiet) return false;   // can't elevate silently
        std::wcout << L"Some tasks require Administrator privileges. Elevate? (y/n): ";
        wchar_t answer;
        std::wcin >> answer;
        if (answer != L'y' && answer != L'Y') {
            std::wcout << L"Continuing without admin – system tasks will be skipped.\n";
            return false;
        }

        // Relaunch self as admin
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring params;
        // Pass original arguments (except we will reconstruct)
        // Simpler: pass all original command line arguments
        LPWSTR cmdLine = GetCommandLineW();
        // Re-run with ShellExecute "runas"
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.lpParameters = cmdLine;
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) {
            ExitProcess(0);   // old process exits after successful elevation launch
        } else {
            std::wcerr << L"Elevation failed or was denied.\n";
            return false;
        }
    }
    return IsUserAnAdmin();   // now admin or was already admin
}

// ----------------------------------------------------------------------
// Parse command line arguments
void parseArgs(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--dry-run") {
            g_dryRun = true;
        } else if (arg == L"--quiet") {
            g_quiet = true;
        } else if (arg == L"--force") {
            g_force = true;
        } else if (arg == L"--only-browsers") {
            g_onlyBrowsers = true;
        } else if (arg == L"--only-temp") {
            g_onlyTemp = true;
        } else {
            std::wcerr << L"Unknown option: " << arg << L"\n";
            std::wcerr << L"Usage: TempPurge.exe [--dry-run] [--quiet] [--force] [--only-browsers] [--only-temp]\n";
            ExitProcess(1);
        }
    }
}

// ----------------------------------------------------------------------
int wmain(int argc, wchar_t* argv[]) {
    // Enable full Unicode output
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);   // optional, safe
    // std::wcout.imbue(std::locale(""));

    // Store own exe path
    wchar_t exeBuf[MAX_PATH];
    GetModuleFileNameW(NULL, exeBuf, MAX_PATH);
    g_exePath = exeBuf;

    parseArgs(argc, argv);

    // Determine if we need admin privileges
    bool needAdmin = !g_onlyBrowsers;   // system temp + prefetch require admin

    if (!elevateIfNeeded(needAdmin)) {
        // Not admin – system tasks will be skipped automatically
    }
    bool isAdmin = IsUserAnAdmin();

    // Show what will run
    if (!g_quiet) {
        std::wcout << L"Temp-Purger\n";
        if (g_dryRun) std::wcout << L"*** DRY RUN MODE – no files will be deleted ***\n";
        std::wcout << L"--------------------------------------------\n";
    }

    // Confirmation prompt before real deletion (unless --dry-run or --force)
    if (!g_dryRun && !g_force) {
        std::wcout << L"Proceed with deletion? (y/n): ";
        wchar_t confirm;
        std::wcin >> confirm;
        if (confirm != L'y' && confirm != L'Y') {
            std::wcout << L"Aborted.\n";
            return 0;
        }
    }

    // Execute cleanups
    if (!g_onlyBrowsers) {
        cleanUserTemp();
        cleanSystemTemp(isAdmin);
        cleanPrefetch(isAdmin);
    }
    if (!g_onlyTemp) {
        cleanBraveCaches();
        cleanFirefoxCaches();
    }

    // Summary
    if (!g_quiet) {
        std::wcout << L"--------------------------------------------\n";
        if (g_dryRun) {
            std::wcout << L"Would have freed: " << g_totalBytes << L" bytes in " << g_totalFiles << L" items.\n";
        } else {
            std::wcout << L"Freed: " << g_totalBytes << L" bytes in " << g_totalFiles << L" items.\n";
        }
    }

    return 0;
}