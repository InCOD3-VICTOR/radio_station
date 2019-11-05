#include "audio_publicer.h"

AudioPublicer::AudioPublicer()
{
    av_register_all();
    //Network
    avformat_network_init();
    //Output
    av_log_set_level(AV_LOG_DEBUG);

    audio_frame_buffer_ = AudioFrameBuffer::Create(50);
    capture_event_ = CreateEvent(NULL, false, false, NULL);
    running_ = false;
    pts = 0;
    src_sample_rate_ = 8000;
    dst_sample_rate_ = 44100;
}

AudioPublicer::~AudioPublicer()
{
    /* close output */
    if (icecast_fmt_ctx_ && !(output_fmt_->flags & AVFMT_NOFILE))
        avio_close(icecast_fmt_ctx_->pb);
    avformat_free_context(icecast_fmt_ctx_);

    av_frame_free(&pFrame);
    av_frame_free(&pSrcFrame);

    CloseHandle(capture_event_);

    swr_free(&swr_ctx_);
}

bool AudioPublicer::Init()
{
    if(InitEncoder()< 0)
    {
        printf("Init Encoder failed!\n");
        return false;
    }
    if (InitResample()< 0)
    {
        printf("Init Resample failed!\n");
        return false;
    }
    if (InitPublicer()< 0)
    {
        printf("Init Publicer failed!\n");
        return false;
    }

    return true;
}


bool AudioPublicer::InitPublicer()
{
    
    avformat_alloc_output_context2(&icecast_fmt_ctx_, NULL, "mp3", url_.c_str());

    if (!icecast_fmt_ctx_) {
        printf("Could not create output context\n");
        return false;
    }
    output_fmt_ = icecast_fmt_ctx_->oformat;
    AVStream *out_stream = avformat_new_stream(icecast_fmt_ctx_, codec_);
    if (!out_stream) {
        printf("Failed allocating output stream\n");
        return false;
    }
    int ret;
    ret = avcodec_parameters_from_context(out_stream->codecpar, mp3_codec_ctx_);
    if (ret < 0)
    {
        printf("Failed to copy context from input to output stream codec context\n");
        return false;
    }

    out_stream->codec->codec_tag = 0;
    if (icecast_fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
        out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    //Dump Format------------------
    av_dump_format(icecast_fmt_ctx_, 0, url_.c_str(), 1);
    return true;

}

void AudioPublicer::SetUrl(std::string url)
{
    url_ = url;
}

bool AudioPublicer::StartUp()
{
    int ret;
    if (!(output_fmt_->flags & AVFMT_NOFILE)) {
        ret = avio_open(&icecast_fmt_ctx_->pb, url_.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            printf("Could not open output URL '%s'", url_.c_str());
            return false;
        }
    }
    //Write file header
    ret = avformat_write_header(icecast_fmt_ctx_, NULL);
    if (ret < 0) {
        printf("Error occurred when opening output URL\n");
        return false;
    }

    if (!running_)
    {
        running_ = true;
        process_thread_.reset(new std::thread(std::bind(&AudioPublicer::ProcessThread, this)));
        
    }
    
    
    return true;

}

void AudioPublicer::ShutDown()
{
    if (running_)
    {
        running_ = false;
        if (process_thread_->joinable())
            process_thread_->join();
    }
}



void AudioPublicer::SendInner(char* buf, int size)
{
    int ret;
    ret = Encode(buf, size);
    if (ret < 0)
    {
        printf("Error Encode packet\n");
        return;
    }
    
    ret = av_interleaved_write_frame(icecast_fmt_ctx_, &encoded_pkt_);
    if (ret < 0) {
        printf("Error muxing packet\n");
        return;
    }
    av_free_packet(&encoded_pkt_);
}


void AudioPublicer::Send(char* buf, int size)
{
    AudioFrame* audio_buffer = AudioFrame::Create((uint8_t*)buf, size);
    audio_frame_buffer_->PushFrame(audio_buffer);

    SetEvent(capture_event_);
}



bool AudioPublicer::InitEncoder()
{
    codec_ = avcodec_find_encoder(AV_CODEC_ID_MP3);
    //pCodec = avcodec_find_encoder_by_name("libfdk_aac");
    if (!codec_){
        printf("Can not find encoder!\n");
        return -1;
    }

    mp3_codec_ctx_ = avcodec_alloc_context3(codec_);
    mp3_codec_ctx_->codec_id = AV_CODEC_ID_MP3;
    mp3_codec_ctx_->codec_type = AVMEDIA_TYPE_AUDIO;
    mp3_codec_ctx_->sample_fmt = AV_SAMPLE_FMT_S16P;
    mp3_codec_ctx_->sample_rate = src_sample_rate_;// 44100;
    mp3_codec_ctx_->channel_layout = AV_CH_LAYOUT_STEREO;
    mp3_codec_ctx_->channels = av_get_channel_layout_nb_channels(mp3_codec_ctx_->channel_layout);
    //mp3_codec_ctx_->bit_rate = 64000;
    //mp3_codec_ctx_->time_base = { 1, 1000000 };

    //av_dump_format(mp3_codec_ctx_, 0, out_file, 1);
    
    if (avcodec_open2(mp3_codec_ctx_, codec_, NULL) < 0){
        printf("Failed to open encoder!\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrame->nb_samples = mp3_codec_ctx_->frame_size;
    pFrame->format = mp3_codec_ctx_->sample_fmt;

    pFrame->sample_rate = mp3_codec_ctx_->sample_rate;
    pFrame->channels = mp3_codec_ctx_->channels;
    pFrame->channel_layout = mp3_codec_ctx_->channel_layout;
    
    sample_buffer_size = av_samples_get_buffer_size(NULL, mp3_codec_ctx_->channels, mp3_codec_ctx_->frame_size, mp3_codec_ctx_->sample_fmt, 1);
    frame_buf = (uint8_t *)av_malloc(sample_buffer_size);



    pSrcFrame = av_frame_alloc();
    pSrcFrame->nb_samples = mp3_codec_ctx_->frame_size;
    pSrcFrame->format = AV_SAMPLE_FMT_S16;

    pSrcFrame->sample_rate = mp3_codec_ctx_->sample_rate;
    //pSrcFrame->sample_rate = src_sample_rate_;
    pSrcFrame->channels = mp3_codec_ctx_->channels;
    pSrcFrame->channel_layout = mp3_codec_ctx_->channel_layout;

    src_sample_buffer_size = av_samples_get_buffer_size(NULL, mp3_codec_ctx_->channels, mp3_codec_ctx_->frame_size, AV_SAMPLE_FMT_S16, 1);
    src_frame_buf = (uint8_t *)av_malloc(src_sample_buffer_size);
    

    return 0;
}

int AudioPublicer::GetSampleSampleSize()
{
    return src_sample_buffer_size;
}


int AudioPublicer::Encode(char* buf, int size)
{
    av_new_packet(&encoded_pkt_, sample_buffer_size);

    //填充frame
    memcpy(src_frame_buf, buf, size);
    int ret;
    ret = avcodec_fill_audio_frame(pSrcFrame, mp3_codec_ctx_->channels, AV_SAMPLE_FMT_S16, (const uint8_t*)src_frame_buf, src_sample_buffer_size, 1);
    if (ret < 0) {
        return -1;
    }

    //重采样将AV_SAMPLE_FMT_S16 转成 AV_SAMPLE_FMT_S16P
    ret = swr_convert_frame(swr_ctx_, pFrame, pSrcFrame);
    if (ret < 0) {
        printf("Swr Failed to Resample!\n");
        return -1;
    }
    
    //编码
    int got_frame = 0;
    pFrame->pts = pts++;
    ret = avcodec_encode_audio2(mp3_codec_ctx_, &encoded_pkt_, pFrame, &got_frame);
    if (ret < 0){
        printf("Failed to encode!\n");
        av_free_packet(&encoded_pkt_);
        return -1;
    }
    
    if (got_frame == 1){
#ifdef _DEBUG
        printf("Succeed to encode 1 frame! \tsize:%5d\n", encoded_pkt_.size);
#endif
        return 0;
    }
    else
    {
        av_free_packet(&encoded_pkt_);
        return -1;
    }



}


bool AudioPublicer::InitResample()
{
    swr_ctx_ = swr_alloc();
    swr_ctx_ = swr_alloc_set_opts(NULL,  // we're allocating a new context
        AV_CH_LAYOUT_STEREO,  // out_ch_layout
        AV_SAMPLE_FMT_S16P,    // out_sample_fmt
        src_sample_rate_,                // out_sample_rate
        AV_CH_LAYOUT_STEREO, // in_ch_layout
        AV_SAMPLE_FMT_S16,   // in_sample_fmt
        src_sample_rate_,                // in_sample_rate
        0,                    // log_offset
        NULL);                // log_ctx

    int ret;
    ret = swr_init(swr_ctx_);
    if (ret != 0) {
        printf("swr_init failed\n");
    }
    else {
        printf("swr_init success\n");
    }


    // 计算转换后的sample个数 a * b / c
    //int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples, frame->sample_rate, frame->sample_rate, AVRounding(1));
    // 转换，返回值为转换后的sample个数
    //int nb = swr_convert(swr_ctx, &audio_buf, dst_nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
    //data_size = frame->channels * nb * av_get_bytes_per_sample(dst_format); //data_size中保存的是转换的数据的字节数：通道数 * sample个数 * 每个sample的字节数
    
    return true;
}


void AudioPublicer::ProcessThread()
{
    while (running_)
    {
        static const int kThreadWaitTimeMs = 5;
        if (WaitForSingleObject(capture_event_, kThreadWaitTimeMs) == WAIT_TIMEOUT) {
            AudioFrame* audio_frame = nullptr;
            if (audio_frame_buffer_->PopFrame(&audio_frame) != -1)
            {
                
                SendInner((char*)audio_frame->frame_data_, audio_frame->lenght_);
                delete audio_frame;
            }
        }
    }
}





