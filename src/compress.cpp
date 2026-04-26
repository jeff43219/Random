#include "include/compress.hpp"

#include <iostream>
#include <filesystem>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace fs = std::filesystem;

// ─── Helpers ────────────────────────────────────────────────────────────────

bool is_supported(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (auto& c : ext) c = tolower(c);
    for (const auto& e : SUPPORTED_EXTENSIONS)
        if (e == ext) return true;
    return false;
}

std::vector<std::string> collect_files(const std::string& path, bool recursive) {
    std::vector<std::string> files;
    if (fs::is_regular_file(path)) {
        if (is_supported(path)) files.push_back(path);
        return files;
    }
    if (recursive) {
        for (const auto& e : fs::recursive_directory_iterator(path))
            if (e.is_regular_file() && is_supported(e.path().string()))
                files.push_back(e.path().string());
    } else {
        for (const auto& e : fs::directory_iterator(path))
            if (e.is_regular_file() && is_supported(e.path().string()))
                files.push_back(e.path().string());
    }
    return files;
}

// ─── Pick hardware decoder based on codec ───────────────────────────────────

static const char* pick_hw_decoder(AVCodecID codec_id) {
    switch (codec_id) {
        case AV_CODEC_ID_H264:       return "h264_cuvid";
        case AV_CODEC_ID_HEVC:       return "hevc_cuvid";
        case AV_CODEC_ID_AV1:        return "av1_cuvid";
        case AV_CODEC_ID_VP9:        return "vp9_cuvid";
        case AV_CODEC_ID_MPEG2VIDEO: return "mpeg2_cuvid";
        case AV_CODEC_ID_MPEG4:      return "mpeg4_cuvid";
        default:                     return nullptr;
    }
}

// ─── Core ───────────────────────────────────────────────────────────────────

CompressResult compress_file(const std::string& input_path, const CompressOptions& opts) {
    CompressResult result;
    result.filepath = input_path;

    auto t_start = std::chrono::steady_clock::now();

    fs::path in_path(input_path);
    fs::path out_path = in_path.parent_path() /
        (in_path.stem().string() + "_compressed" + in_path.extension().string());
    std::string out_str = out_path.string();

    result.original_size = (int64_t)fs::file_size(in_path);

    // ── Open input ──
    AVFormatContext* in_fmt_ctx = nullptr;
    if (avformat_open_input(&in_fmt_ctx, input_path.c_str(), nullptr, nullptr) < 0) {
        result.error_msg = "Failed to open input file";
        return result;
    }
    if (avformat_find_stream_info(in_fmt_ctx, nullptr) < 0) {
        avformat_close_input(&in_fmt_ctx);
        result.error_msg = "Failed to read stream info";
        return result;
    }

    // ── Open output ──
    AVFormatContext* out_fmt_ctx = nullptr;
    if (avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, out_str.c_str()) < 0) {
        avformat_close_input(&in_fmt_ctx);
        result.error_msg = "Failed to allocate output context";
        return result;
    }

    // ── Find video stream ──
    int video_idx = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_idx < 0) {
        avformat_close_input(&in_fmt_ctx);
        avformat_free_context(out_fmt_ctx);
        result.error_msg = "No video stream found";
        return result;
    }

    AVStream* in_video = in_fmt_ctx->streams[video_idx];
    AVCodecID src_codec_id = in_video->codecpar->codec_id;

    // ── Try hardware decoder, fall back to software ──
    const char* hw_dec_name = pick_hw_decoder(src_codec_id);
    const AVCodec* dec = nullptr;

    if (hw_dec_name)
        dec = avcodec_find_decoder_by_name(hw_dec_name);
    if (!dec)
        dec = avcodec_find_decoder(src_codec_id);
    if (!dec) {
        avformat_close_input(&in_fmt_ctx);
        avformat_free_context(out_fmt_ctx);
        result.error_msg = "No decoder found";
        return result;
    }

    AVCodecContext* dec_ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(dec_ctx, in_video->codecpar);
    dec_ctx->thread_count = 0;
    if (avcodec_open2(dec_ctx, dec, nullptr) < 0) {
        avcodec_free_context(&dec_ctx);
        dec = avcodec_find_decoder(src_codec_id);
        dec_ctx = avcodec_alloc_context3(dec);
        avcodec_parameters_to_context(dec_ctx, in_video->codecpar);
        dec_ctx->thread_count = 0;
        if (avcodec_open2(dec_ctx, dec, nullptr) < 0) {
            avformat_close_input(&in_fmt_ctx);
            avformat_free_context(out_fmt_ctx);
            avcodec_free_context(&dec_ctx);
            result.error_msg = "Failed to open decoder";
            return result;
        }
    }

    // ── Setup stream map ──
    AVCodecContext* enc_ctx = nullptr;
    std::vector<int> stream_map(in_fmt_ctx->nb_streams, -1);
    int out_stream_idx = 0;

    for (unsigned i = 0; i < in_fmt_ctx->nb_streams; i++) {
        AVStream* in_stream = in_fmt_ctx->streams[i];
        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT) {
            stream_map[i] = -1;
            continue;
        }

        AVStream* out_stream = avformat_new_stream(out_fmt_ctx, nullptr);
        if (!out_stream) {
            result.error_msg = "Failed to create output stream";
            goto cleanup;
        }
        stream_map[i] = out_stream_idx++;

        if ((int)i == video_idx) {
            const AVCodec* enc = avcodec_find_encoder_by_name("hevc_nvenc");
            if (!enc) { result.error_msg = "hevc_nvenc not found"; goto cleanup; }

            enc_ctx = avcodec_alloc_context3(enc);
            if (!enc_ctx) { result.error_msg = "Failed to alloc encoder context"; goto cleanup; }

            enc_ctx->width               = in_stream->codecpar->width;
            enc_ctx->height              = in_stream->codecpar->height;
            enc_ctx->sample_aspect_ratio = in_stream->codecpar->sample_aspect_ratio;
            enc_ctx->pix_fmt             = AV_PIX_FMT_YUV420P;
            enc_ctx->time_base           = in_stream->time_base;
            enc_ctx->framerate           = in_stream->r_frame_rate;

            av_opt_set(enc_ctx->priv_data, "rc",          "constqp",           0);
            av_opt_set_int(enc_ctx->priv_data, "qp",       opts.cq,             0);
            av_opt_set(enc_ctx->priv_data, "preset",       opts.preset.c_str(), 0);
            av_opt_set(enc_ctx->priv_data, "profile",      "main",              0);
            av_opt_set(enc_ctx->priv_data, "tune",         "hq",                0);
            av_opt_set_int(enc_ctx->priv_data, "lookahead", 8,                  0);
            av_opt_set_int(enc_ctx->priv_data, "surfaces",  64,                 0);

            if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            if (avcodec_open2(enc_ctx, enc, nullptr) < 0) {
                result.error_msg = "Failed to open encoder";
                goto cleanup;
            }
            avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            out_stream->time_base = enc_ctx->time_base;
        } else {
            avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            out_stream->codecpar->codec_tag = 0;
            out_stream->time_base = in_stream->time_base;
        }
    }

    // ── Open output file ──
    if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_fmt_ctx->pb, out_str.c_str(), AVIO_FLAG_WRITE) < 0) {
            result.error_msg = "Failed to open output file for writing";
            goto cleanup;
        }
    }
    if (avformat_write_header(out_fmt_ctx, nullptr) < 0) {
        result.error_msg = "Failed to write output header";
        goto cleanup;
    }

    // ── Pipelined decode + encode (single writer on main thread) ──
    {
        constexpr int QUEUE_MAX = 16;

        std::queue<AVFrame*>  frame_queue;
        std::mutex            fq_mutex;
        std::condition_variable fq_cv;
        std::atomic<bool>     decode_done(false);

        std::queue<AVPacket*> out_queue;
        std::mutex            oq_mutex;
        std::condition_variable oq_cv;
        std::atomic<bool>     encode_done(false);

        SwsContext* sws_ctx = nullptr;
        int src_w = in_video->codecpar->width;
        int src_h = in_video->codecpar->height;

        AVPacket* pkt = av_packet_alloc();

        // ── Decoder thread ──
        std::thread decoder_thread([&]() {
            AVFrame* frame    = av_frame_alloc();
            AVFrame* sw_frame = av_frame_alloc();

            auto push_frame = [&](AVFrame* src) {
                if (!sws_ctx) {
                    sws_ctx = sws_getContext(
                        src->width, src->height, (AVPixelFormat)src->format,
                        src_w, src_h, AV_PIX_FMT_YUV420P,
                        SWS_BILINEAR, nullptr, nullptr, nullptr
                    );
                }
                AVFrame* conv = av_frame_alloc();
                conv->format  = AV_PIX_FMT_YUV420P;
                conv->width   = src_w;
                conv->height  = src_h;
                av_frame_get_buffer(conv, 0);
                sws_scale(sws_ctx,
                    src->data, src->linesize, 0, src->height,
                    conv->data, conv->linesize);
                conv->pts       = src->pts;
                conv->duration  = src->duration;
                conv->pict_type = AV_PICTURE_TYPE_NONE;
                {
                    std::unique_lock<std::mutex> lock(fq_mutex);
                    fq_cv.wait(lock, [&]{ return (int)frame_queue.size() < QUEUE_MAX; });
                    frame_queue.push(conv);
                }
                fq_cv.notify_all();
            };

            while (av_read_frame(in_fmt_ctx, pkt) >= 0) {
                if ((int)pkt->stream_index == video_idx) {
                    if (avcodec_send_packet(dec_ctx, pkt) == 0) {
                        while (avcodec_receive_frame(dec_ctx, frame) == 0) {
                            AVFrame* src = frame;
                            if (frame->format == AV_PIX_FMT_CUDA ||
                                frame->format == AV_PIX_FMT_NV12) {
                                sw_frame->format = AV_PIX_FMT_NV12;
                                if (av_hwframe_transfer_data(sw_frame, frame, 0) >= 0) {
                                    sw_frame->pts      = frame->pts;
                                    sw_frame->duration = frame->duration;
                                    src = sw_frame;
                                }
                            }
                            push_frame(src);
                            av_frame_unref(sw_frame);
                            av_frame_unref(frame);
                        }
                    }
                } else if (stream_map[pkt->stream_index] >= 0) {
                    // Audio/subtitle — push to output queue (written by main thread)
                    AVPacket* copy  = av_packet_clone(pkt);
                    AVStream* in_s  = in_fmt_ctx->streams[copy->stream_index];
                    AVStream* out_s = out_fmt_ctx->streams[stream_map[copy->stream_index]];
                    av_packet_rescale_ts(copy, in_s->time_base, out_s->time_base);
                    copy->stream_index = stream_map[copy->stream_index];
                    {
                        std::unique_lock<std::mutex> lock(oq_mutex);
                        out_queue.push(copy);
                    }
                    oq_cv.notify_one();
                }
                av_packet_unref(pkt);
            }

            // Flush decoder
            avcodec_send_packet(dec_ctx, nullptr);
            while (avcodec_receive_frame(dec_ctx, frame) == 0) {
                AVFrame* src = frame;
                if (frame->format == AV_PIX_FMT_CUDA ||
                    frame->format == AV_PIX_FMT_NV12) {
                    sw_frame->format = AV_PIX_FMT_NV12;
                    if (av_hwframe_transfer_data(sw_frame, frame, 0) >= 0) {
                        sw_frame->pts = frame->pts;
                        src = sw_frame;
                    }
                }
                push_frame(src);
                av_frame_unref(sw_frame);
                av_frame_unref(frame);
            }

            decode_done = true;
            fq_cv.notify_all();
            av_frame_free(&frame);
            av_frame_free(&sw_frame);
        });

        // ── Encoder thread ──
        std::thread encoder_thread([&]() {
            AVPacket* out_pkt = av_packet_alloc();

            while (true) {
                AVFrame* conv = nullptr;
                {
                    std::unique_lock<std::mutex> lock(fq_mutex);
                    fq_cv.wait(lock, [&]{
                        return !frame_queue.empty() || decode_done.load();
                    });
                    if (!frame_queue.empty()) {
                        conv = frame_queue.front();
                        frame_queue.pop();
                    } else {
                        break;
                    }
                }
                fq_cv.notify_all();

                if (avcodec_send_frame(enc_ctx, conv) == 0) {
                    while (avcodec_receive_packet(enc_ctx, out_pkt) == 0) {
                        AVPacket* copy = av_packet_clone(out_pkt);
                        copy->stream_index = stream_map[video_idx];
                        av_packet_rescale_ts(copy, enc_ctx->time_base,
                            out_fmt_ctx->streams[stream_map[video_idx]]->time_base);
                        {
                            std::unique_lock<std::mutex> lock(oq_mutex);
                            out_queue.push(copy);
                        }
                        oq_cv.notify_one();
                        av_packet_unref(out_pkt);
                    }
                }
                av_frame_free(&conv);
            }

            // Flush encoder
            avcodec_send_frame(enc_ctx, nullptr);
            while (avcodec_receive_packet(enc_ctx, out_pkt) == 0) {
                AVPacket* copy = av_packet_clone(out_pkt);
                copy->stream_index = stream_map[video_idx];
                av_packet_rescale_ts(copy, enc_ctx->time_base,
                    out_fmt_ctx->streams[stream_map[video_idx]]->time_base);
                {
                    std::unique_lock<std::mutex> lock(oq_mutex);
                    out_queue.push(copy);
                }
                oq_cv.notify_one();
                av_packet_unref(out_pkt);
            }

            encode_done = true;
            oq_cv.notify_all();
            av_packet_free(&out_pkt);
        });

        // ── Main thread: sole writer ──
        while (true) {
            AVPacket* wpkt = nullptr;
            {
                std::unique_lock<std::mutex> lock(oq_mutex);
                oq_cv.wait(lock, [&]{
                    return !out_queue.empty() || encode_done.load();
                });
                if (!out_queue.empty()) {
                    wpkt = out_queue.front();
                    out_queue.pop();
                } else {
                    break;
                }
            }
            if (wpkt) {
                av_interleaved_write_frame(out_fmt_ctx, wpkt);
                av_packet_free(&wpkt);
            }
        }

        decoder_thread.join();
        encoder_thread.join();

        av_write_trailer(out_fmt_ctx);

        if (sws_ctx) sws_freeContext(sws_ctx);
        av_packet_free(&pkt);
        avcodec_free_context(&dec_ctx);
    }

    result.output_size = (int64_t)fs::file_size(out_path);
    result.success = true;

    if (opts.overwrite) {
        fs::remove(in_path);
        fs::rename(out_path, in_path);
    }

cleanup:
    if (enc_ctx) avcodec_free_context(&enc_ctx);
    if (out_fmt_ctx) {
        if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&out_fmt_ctx->pb);
        avformat_free_context(out_fmt_ctx);
    }
    avformat_close_input(&in_fmt_ctx);

    auto t_end = std::chrono::steady_clock::now();
    result.elapsed_sec = std::chrono::duration<double>(t_end - t_start).count();

    if (!result.success && fs::exists(out_path))
        fs::remove(out_path);

    return result;
}