/*
    James William Fletcher (james@voxdsp.com)
        September 2021
    
    Borg ER-0
*/
#ifndef SYNTH_H
#define SYNTH_H

#include <SDL2/SDL.h>
#include <math.h>

// generators
float oscillator(const float theta, const float* table);

// utility functions
#define wlerp(a, b, i) ((b - a) * i + a)
float Hz(float hz);
float squish(float f);
int fZero(float f);
Sint8 quantise_float(float f);

// init
int initMonoAudio(int samplerate);

// file
void writeWAV(const char* file);

// play
void setSampleLen(Uint32 seconds);
void playSample();
void stopSample();


/*
    functions bodies
*/
float oscillator(const float theta, const float* table)
{
    // normalise theta to a 0-1 range
    float norm = theta * 0.1591549367f; // 1/(PI*2)

    // preserve the fractional part that will be lopped off by the wrapping
    const float a = norm * 256.f; // scales normal range to wave table range
    const float fract = a-floor(a);

    // wrap it
    const unsigned char i = (unsigned char)(a); // this cast does the wrapping for us, it's fast.

    // check for end of array case for wrap-around lerp.
    if(i >= 255)
        return wlerp(table[255], table[0], fract);

    // do regular table lerp
    return wlerp(table[i], table[i+1], fract);
}

float Hz(float hz)
{
    return hz * 6.283185482f;
}

int fZero(float f)
{
    return f > -0.01 && f < 0.01 ? 1 : 0;
}

float squish(float f)
{
    return fabs(tanh(f));
}

Sint8 quantise_float(float f)
{
    if(f < 0)
        f -= 0.5f;
    else
        f += 0.5f;
    return (Sint8)f;
}

// vars
#define MAX_SAMPLE 1455300 //33*44100
SDL_AudioSpec sdlaudioformat;
Sint8 sample[MAX_SAMPLE];
Uint32 sample_index = 0;
Uint32 sample_len = 0;

void audioCallback(void* unused, Uint8* stream, int len)
{
    if(sample_index > sample_len)
        return;
    
    for(int i = 0; i < len; i++)
    {
        stream[i] = sample[sample_index];
        sample_index++;
        if(sample_index > sample_len)
        {
            SDL_PauseAudio(1);
            return;
        }
    }
}

void playSample()
{
    sample_index = 0;
    SDL_PauseAudio(0);
}

void stopSample()
{
    sample_index = 0;
    SDL_PauseAudio(1);
}

void setSampleLen(Uint32 seconds)
{
    sample_len = sdlaudioformat.freq * seconds;
    if(sample_len >= MAX_SAMPLE)
        sample_len = MAX_SAMPLE-1;
}

int initMonoAudio(int samplerate)
{
    // set audio format
    sdlaudioformat.freq = samplerate; // 44100 / 48000
    sdlaudioformat.format = AUDIO_S8; // AUDIO_U16
    sdlaudioformat.channels = 1;
    sdlaudioformat.samples = 4096;
    sdlaudioformat.callback = audioCallback;
    sdlaudioformat.userdata = NULL;

    // open audio device
    if(SDL_OpenAudio(&sdlaudioformat, 0) < 0)
        return -1;

    // success
    return 1;
}

void writeWAV(const char* file)
{
    // I have to convert the buffer to unsigned, madness.
    Uint8 usample[MAX_SAMPLE];
    for(int i = 0; i < sample_len; i++)
        usample[i] = sample[i]+128;
    
    // prep header
    const unsigned int wavedata_size = sample_len + 44;
    const unsigned int subchunk = 16;
    const unsigned short audioformat = 1;
    const unsigned short channels = 1;
    const unsigned int samplerate = sdlaudioformat.freq;
    const unsigned short bitspersample = 8;
    const unsigned int byterate = (samplerate * channels * bitspersample) / 8;
    const unsigned short blockalignment = (channels * bitspersample) / 8;

    // write wav
    FILE* f = fopen(file, "wb");
    if(f != NULL)
    {
        fwrite("RIFF", 4, 1, f);
        fwrite(&wavedata_size, 4, 1, f);
        fwrite("WAVE", 4, 1, f);
        fwrite("fmt ", 4, 1, f);
        fwrite(&subchunk, 4, 1, f);
        fwrite(&audioformat, 2, 1, f);
        fwrite(&channels, 2, 1, f);
        fwrite(&samplerate, 4, 1, f);
        fwrite(&byterate, 4, 1, f);
        fwrite(&blockalignment, 2, 1, f);
        fwrite(&bitspersample, 2, 1, f);
        fwrite("data", 4, 1, f);
        fwrite(&sample_len, 4, 1, f);
        fwrite(usample, sample_len, 1, f);
        fclose(f);
    }
}

#endif
