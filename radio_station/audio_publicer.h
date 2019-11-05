#pragma  once
#include <string>

//#ifdef _WIN64
//Windows
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include <libswresample/swresample.h>
};

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")


#include "audio_frame_buffer.h"
#include <thread>
#include <atomic>
#include <memory>

class AudioPublicer
{
public:

    AudioPublicer();
    ~AudioPublicer();

    bool Init();

    bool InitPublicer();
    bool StartUp();
    void ShutDown();

    int GetSampleSampleSize();

    void SetUrl(std::string url);
    void SendInner(char* buf, int size);
    int Encode(char* buf, int size);

    void Send(char* buf, int size);

    bool InitEncoder();

    bool InitResample();
private:

    AudioFrameBuffer* audio_frame_buffer_;
    HANDLE capture_event_;

    std::string url_;

    //媒体文件或媒体流上下文
    AVFormatContext *icecast_fmt_ctx_;
    AVOutputFormat *output_fmt_;

    //编码
    AVCodecContext* mp3_codec_ctx_;
    AVCodec* codec_;
    AVFrame* pFrame;
    uint8_t* frame_buf;
    int sample_buffer_size;

    AVFrame* pSrcFrame;
    uint8_t* src_frame_buf;
    int src_sample_buffer_size;
    
    AVPacket encoded_pkt_;


    //重采样
    SwrContext *swr_ctx_;
    AVFrame* input_frame_;

    int pts;
    int src_sample_rate_;
    int dst_sample_rate_;


public:
    std::shared_ptr<std::thread> process_thread_;
    std::atomic<bool> running_;
    void ProcessThread();
};