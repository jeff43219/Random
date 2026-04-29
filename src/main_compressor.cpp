#include "include/compress.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

static void print_usage(const char* exe) {
    std::cout << "Usage: " << exe << " <path> [options]\n"
              << "\n"
              << "  <path>          File or directory to compress\n"
              << "\n"
              << "Options:\n"
              << "  -cq <1-51>      Quality (default: 28, lower = better)\n"
              << "  -preset <p1-p7> NVENC preset (default: p4)\n"
              << "  -r              Recursive directory scan\n"
              << "  -overwrite      Replace original with compressed output\n"
              << "\n"
              << "Examples:\n"
              << "  compressor.exe video.mp4\n"
              << "  compressor.exe C:\\Videos -r -cq 24 -overwrite\n";
}

static std::string human_size(int64_t bytes) {
    double mb = bytes / (1024.0 * 1024.0);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << mb << " MB";
    return ss.str();
}

std::string g_current_output;

int main(int argc, char* argv[]) {
    std::vector<std::string> args;

#ifdef _WIN32
    // Windows: Retrieve command line arguments in UTF-16 and convert to UTF-8
    int wargc;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv) {
        for (int i = 0; i < wargc; i++) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
            std::string arg(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &arg[0], size_needed, nullptr, nullptr);
            arg.pop_back(); // Remove the null terminator added by WideCharToMultiByte
            args.push_back(arg);
        }
        LocalFree(wargv);
    } else {
        std::cerr << "Failed to parse command line arguments.\n";
        return 1;
    }
#else
    // Linux/macOS: argv is already UTF-8
    for (int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }
#endif

    if (args.size() < 2) { print_usage(args[0].c_str()); return 1; }

    std::string path = args[1];
    CompressOptions opts;

    for (size_t i = 2; i < args.size(); i++) {
        std::string arg = args[i];
        if (arg == "-cq" && i + 1 < args.size())
            opts.cq = std::stoi(args[++i]);
        else if (arg == "-preset" && i + 1 < args.size())
            opts.preset = args[++i];
        else if (arg == "-r")
            opts.recursive = true;
        else if (arg == "-overwrite")
            opts.overwrite = true;
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    auto files = collect_files(path, opts.recursive);
    if (files.empty()) {
        std::cerr << "No supported video files found.\n";
        return 1;
    }

    std::cout << "Found " << files.size() << " file(s). Starting compression...\n"
              << "CQ: " << opts.cq << "  Preset: " << opts.preset
              << "  Overwrite: " << (opts.overwrite ? "yes" : "no") << "\n\n";

    int success = 0, failed = 0;
    int64_t total_saved = 0;

    for (size_t i = 0; i < files.size(); i++) {
        const auto& f = files[i];
        std::cout << "[" << (i+1) << "/" << files.size() << "] " << f << "\n";

        std::atomic<bool> done(false);
        CompressResult res;
        std::thread worker([&]() {
            res = compress_file(f, opts);
            done = true;
        });

        auto t0 = std::chrono::steady_clock::now();
        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t0).count();
            std::cout << "\r  Elapsed: " << std::fixed << std::setprecision(1)
                      << elapsed << "s   " << std::flush;
        }
        worker.join();
        std::cout << "\r                    \r";

        if (res.success) {
            int64_t saved = res.original_size - res.output_size;
            double  pct   = 100.0 * saved / res.original_size;
            total_saved  += saved;
            success++;
            std::cout << "  OK  "
                      << human_size(res.original_size) << " -> "
                      << human_size(res.output_size)
                      << "  saved " << std::fixed << std::setprecision(1) << pct << "%"
                      << "  (" << std::setprecision(1) << res.elapsed_sec << "s)\n";
        } else {
            failed++;
            std::cout << "  FAIL  " << res.error_msg << "\n";
        }
    }

    std::cout << "\n── Summary ──────────────────────────\n"
              << "  Done:   " << success << "\n"
              << "  Failed: " << failed  << "\n"
              << "  Saved:  " << human_size(total_saved) << "\n";
    std::cout << "\nPress Enter to close...";
    std::cin.get();
    
    return failed > 0 ? 1 : 0;
}