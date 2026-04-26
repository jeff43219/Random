#include "compress.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
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

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    std::string path = argv[1];
    CompressOptions opts;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-cq" && i + 1 < argc)
            opts.cq = std::stoi(argv[++i]);
        else if (arg == "-preset" && i + 1 < argc)
            opts.preset = argv[++i];
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

    return failed > 0 ? 1 : 0;
}