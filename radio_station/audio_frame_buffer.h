#pragma once
#include <windows.h>
#include <cstdint>
#include <list>
#include <mutex>

class AudioFrame
{
public:
    static AudioFrame* Create(uint8_t* data, int length)
    {
        return new AudioFrame(data, length);
    }

    AudioFrame(uint8_t* data, int length)
    {
        lenght_ = length;
        frame_data_ = new uint8_t[lenght_];
        memcpy(frame_data_, data, length);
    }
    AudioFrame()
    {
        frame_data_ = nullptr;
        lenght_ = 0;
    }
    ~AudioFrame()
    {
        delete[] frame_data_;
        frame_data_ = nullptr;
        lenght_ = 0;
    }


    uint8_t* frame_data_;
    int lenght_;
};





class AudioFrameBuffer
{

public:
    static AudioFrameBuffer* Create(int max_size)
    {
        return new AudioFrameBuffer(max_size);
    }

    AudioFrameBuffer(int max_size) :
        max_size_(max_size)
    {
        audio_frames_.clear();
    }


    int PushFrame(AudioFrame* audio_buffer)
    {
        std::lock_guard<std::mutex> lock_guard(mutxt_);
        if (audio_frames_.size() > max_size_)
            return -1;
        audio_frames_.push_back(audio_buffer);
        return 0;
    }

    int PopFrame(AudioFrame** audio_buffer)
    {
        std::lock_guard<std::mutex> lock_guard(mutxt_);
        if (audio_frames_.size() == 0)
            return -1;
        *audio_buffer = audio_frames_.front();
        audio_frames_.pop_front();
        return 0;
    }


    std::list<AudioFrame*> audio_frames_;
    std::mutex mutxt_;
    int max_size_;

};