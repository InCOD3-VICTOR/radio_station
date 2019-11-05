// radio_station.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdio.h>
#include "audio_publicer.h"
using namespace std;
int main(int argc, char* argv[])
{

    AudioPublicer audio_publicer;

    audio_publicer.Init();
    audio_publicer.SetUrl("icecast://source:hackme@localhost:8000/stream");
    audio_publicer.StartUp();

    int sample_size = audio_publicer.GetSampleSampleSize();

    //FILE *fp = fopen("f:\\NocturneNo2inEflat_44.1k_s16le.pcm", "rb");
    FILE *fp = fopen("f:\\rec_1002.pcm", "rb");
    char* buffer = new char[sample_size];
    while (1)
    {

        int read_size;
        read_size = fread(buffer, 1, sample_size, fp);
        if (read_size == 0){
            fprintf(stdout, "end of file\n");
            break;
            fseek(fp, 0, SEEK_SET);
        }

        audio_publicer.Send(buffer, read_size);
        _sleep(10);
    }
    audio_publicer.ShutDown();
    delete buffer;
    fclose(fp);
}

