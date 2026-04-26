#pragma once

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

// Supported input extensions
static const std::vector<std::string> SUPPORTED_EXTENSIONS = {
    ".mp4", ".mkv", ".avi", ".mov", ".wmv",
    ".flv", ".webm", ".ts", ".m4v"
};

struct CompressOptions {
    int cq        = 28;       // Quality level (lower = better). Range: 1-51
    std::string preset = "p2"; // NVENC preset (p1=fast/worse .. p7=slow/best)
    bool recursive = false;    // Scan subdirectories
    bool overwrite = false;    // Replace original on success
};

struct CompressResult {
    std::string filepath;
    int64_t original_size  = 0;
    int64_t output_size    = 0;
    double  elapsed_sec    = 0.0;
    bool    success        = false;
    std::string error_msg;
};

// Check if file extension is supported
bool is_supported(const std::string& path);

// Compress a single file, returns result
CompressResult compress_file(const std::string& input_path, const CompressOptions& opts);

// Collect all supported files from a path (file or directory)
std::vector<std::string> collect_files(const std::string& path, bool recursive);
// Global current output path (for signal handler cleanup)
extern std::string g_current_output;