/*
    James William Fletcher (github.com/mrbid)
        September 2021
    
            Borg ER-0
*/
#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>

#ifdef __linux__
    #include <sys/stat.h>
#endif

#include "sdl_extra.h"
#include "synth.h"
#include "res.h"

#define TOTAL_DIALS   2
#define SAMPLE_RATE   44100
#define MAXFREQUENCY  1800 //20000
#define MAXAMPLITUDE  255
#define MAXSAMPLELEN  33

SDL_Window *window = NULL;
char *basedir, *appdir;

SDL_Surface* bb; // backbuffer
SDL_Surface* s_bg;
SDL_Surface* s_icon;

Uint32 bgcolor = 0xFFE8A200;
Uint32 outlinecolor = 0xFF000000;
Uint32 scopecolor = 0xFF363636;
Uint32 select_lightness = 44;

Uint32 themeon = 0;
Uint8 select_mode = 0;
Uint8 selected_bank = 0;
Sint32 selected_dial = -1;
Uint8 envelope_enabled = 0;
Uint8 osc_enabled = 0;

SDL_Rect screen_rect = {0, 0, 1069, 283};
SDL_Rect bankl_rect = {4, 265, 12, 14};
SDL_Rect bankr_rect = {64, 265, 12, 14};
SDL_Rect load_rect = {78, 265, 46, 14};
SDL_Rect save_rect = {126, 265, 46, 14};
SDL_Rect secl_rect = {897, 265, 12, 14};
SDL_Rect secr_rect = {957, 265, 12, 14};
SDL_Rect export_rect = {971, 265, 46, 14};
SDL_Rect play_rect = {1019, 265, 46, 14};
SDL_Rect envelope_rect = {4, 4, 800, 258};
SDL_Rect osc_rect = {807, 4, 258, 258};

SDL_Rect dial_rect[2];

float dial_scale[] = {MAXFREQUENCY, MAXAMPLITUDE};

struct ssynth
{
    Uint8 seclen;
    float osctable[256]; // 0-1
    float envelope[798]; // 0-1
    float dial_state[2]; // 0-1
};
struct ssynth synth[256];

void saveState()
{
    char file[256];
    sprintf(file, "%sbank.save", appdir);
    FILE* f = fopen(file, "wb");
    if(f != NULL)
    {
        if(fwrite(&synth[0], sizeof(struct ssynth), 256, f) != 256)
            printf("fwrite() wrote the wrong amount of bytes, save corrupted.\n");
        fclose(f);
    }
}

void loadState()
{
    char file[256];
    sprintf(file, "%sbank.save", appdir);
    FILE* f = fopen(file, "rb");
    if(f != NULL)
    {
        unsigned int strikeout = 0;
        while(fread(&synth[0], sizeof(struct ssynth), 256, f) != 256)
        {
            printf("Read bank failed... Trying again.\n");
            strikeout++;
            if(strikeout > 3333)
            {
                printf("Loading your data totally failed. ¯\\_(ツ)_/¯ Maybe it's corrupted? :(\n");
                break;
            }
        }
        fclose(f);
    }
}

void doSynth(Uint8 play)
{
    //memset(&sample, 0x00, MAX_SAMPLE);
    setSampleLen(synth[selected_bank].seclen);
    
    Uint32 eic = 0;
    Uint32 envelope_offset = 0;
    
    const Uint32 samstep = sample_len / (envelope_rect.w-2);
    const float r_samstep = 1.f/(float)samstep;
    const float reciprocal_sample_rate = 1.f/(float)SAMPLE_RATE;
    const float f = synth[selected_bank].dial_state[0] * dial_scale[0];
    const float a = synth[selected_bank].dial_state[1] * dial_scale[1];
    float phase = 0.f;
    float osc = 0.f;
    for(int i = 0; i < SAMPLE_RATE*synth[selected_bank].seclen; i++)
    {
        osc = (oscillator(phase, &synth[selected_bank].osctable[0])-0.5f) * a;
        osc *= wlerp(synth[selected_bank].envelope[envelope_offset], synth[selected_bank].envelope[envelope_offset+1], ((float)eic)*r_samstep);
        sample[i] = quantise_float(osc);

        // step phase
        phase += Hz(f)*reciprocal_sample_rate;

        // increment envelope stepper
        eic++;
        if(eic > samstep)
        {
            eic = 0;
            if(envelope_offset < envelope_rect.w-3)
                envelope_offset++;
        }
    }
    
    if(play == 1)
        playSample();
}

struct sui
{
    Uint8 bankl_hover;
    Uint8 bankr_hover;
    Uint8 load_hover;
    Uint8 save_hover;
    Uint8 secl_hover;
    Uint8 secr_hover;
    Uint8 export_hover;
    Uint8 play_hover;
    Uint8 dial_hover[2];
};
struct sui ui;

void loadAssets(SDL_Surface* screen)
{
    memset(&ui, 0x00, sizeof(struct sui));
    memset(&synth, 0x00, sizeof(struct ssynth));

    for(int i = 0; i < 256; i++)
    {
        for(int j = 0; j < envelope_rect.w-2; j++)
            synth[i].envelope[j] = 0.5f;
        const float su = 6.283185482f / (float)(osc_rect.w-2);
        for(int j = 0; j < osc_rect.w-2; j++)
            synth[i].osctable[j] = (sinf(su*j)+1.0f)*0.5f;
        synth[i].seclen = 3;
        synth[i].dial_state[0] = 0.04f;
        synth[i].dial_state[1] = 1.f;
    }

    dial_rect[0] = (SDL_Rect){1011, 12, 19, 19};
    dial_rect[1] = (SDL_Rect){1037, 12, 19, 19};

    bb = SDL_RGBA32Surface(screen->w, screen->h);
    s_bg = surfaceFromData((Uint32*)&bg_image.pixel_data[0], screen->w, screen->h);
    s_icon = surfaceFromData((Uint32*)&icon_image.pixel_data[0], 16, 16);
}

void render(SDL_Surface* screen)
{
    Uint32 ih = 0; // is hover
    char val[256]; // temporary string buffer

    // blit bg
    SDL_BlitSurface(s_bg, NULL, bb, NULL);

    // draw bank selection
    if(selected_bank < 10)
    {
        sprintf(val, "%d", selected_bank);
        drawText(bb, val, 38, 267, 0);
    }
    else if(selected_bank < 100)
    {
        sprintf(val, "%d", selected_bank);
        drawText(bb, val, 35, 267, 0);
    }
    else
    {
        sprintf(val, "%d", selected_bank);
        drawText(bb, val, 30, 267, 0);
    }

    // draw sec len
    if(synth[selected_bank].seclen < 10)
    {
        sprintf(val, "%d Sec", synth[selected_bank].seclen);
        drawText(bb, val, 919, 267, 0);
    }
    else if(synth[selected_bank].seclen < 100)
    {
        sprintf(val, "%d Sec", synth[selected_bank].seclen);
        drawText(bb, val, 916, 267, 0);
    }

    // oscillator
    Uint32 eby = osc_rect.y+osc_rect.h-2;
    Uint32 ofh = (osc_rect.h-3);
    linergb(bb, osc_rect.x, eby-(ofh/2), osc_rect.x, eby-(ofh*synth[selected_bank].osctable[0]), 220, 95, 117);
    for(int i = 1; i < osc_rect.w-2; i++)
    {
        linergb(bb, osc_rect.x+i, eby-(ofh*synth[selected_bank].osctable[i-1]), osc_rect.x+i+1, eby-(ofh*synth[selected_bank].osctable[i]), 220, 95, 117);
    }
    linergb(bb, osc_rect.x+osc_rect.w-1, eby-(ofh*synth[selected_bank].osctable[osc_rect.w-3]), osc_rect.x+osc_rect.w-1, eby-(ofh/2), 220, 95, 117);
    if(osc_enabled == 1)
        setColourLightness(bb, osc_rect, scopecolor, 33);

    // envelope
    eby = envelope_rect.y+envelope_rect.h-2;
    for(int i = 0; i < envelope_rect.w-2; i++)
    {
        linergb(bb, envelope_rect.x+i+1, eby, envelope_rect.x+i+1, eby-((envelope_rect.h-3)*synth[selected_bank].envelope[i]), 220, 95, 117);
    }
    if(envelope_enabled == 1)
        setColourLightness(bb, envelope_rect, scopecolor, 33);

    // dial hover & state
    const Uint32 hh = dial_rect[0].h/2; // no point making this static (hack: all the same height)
    for(int i = 0; i < TOTAL_DIALS; i++)
    {
        const Uint32 sx = dial_rect[i].x+hh;
        const Uint32 sy = dial_rect[i].y+hh;
        const float radius = (float)(hh-2);
        const float c1 = (cosf(1.570796371f + (synth[selected_bank].dial_state[i] * 6.283185482f)) * radius)+0.5f;
        const float s1 = (sinf(1.570796371f + (synth[selected_bank].dial_state[i] * 6.283185482f)) * radius)+0.5f;
        const Uint32 ex = sx + c1;
        const Uint32 ey = sy + s1;
        linergb(bb, sx, sy, ex, ey, 255,255,255);

        if(ui.dial_hover[i] == 1)
        {
            ih=1;
            
            if(i == 0)
                sprintf(val, "Frequency: %+.2f", synth[selected_bank].dial_state[i] * dial_scale[i]);
            else
                sprintf(val, "Amplitude: %+.2f", synth[selected_bank].dial_state[i] * dial_scale[i]);
            drawText(bb, val, 811, 249, 1);

            if(select_mode == 0)
            {
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 10, 108, 108, 108);
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 11, 108, 108, 108);
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 12, 255, 255, 255);
            }
            else if(select_mode == 1)
            {
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 10, 0, 163, 232);
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 11, 0, 163, 232);
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 12, 255, 255, 255);
            }
            else if(select_mode == 2)
            {
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 10, 220, 95, 117);
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 11, 220, 95, 117);
                circlergb(bb, dial_rect[i].x+hh, dial_rect[i].y+hh, 12, 255, 255, 255);
            }
        }
    }

    // button hover
    if(ui.bankl_hover == 1){ih=1; setAreaLightness(bb, bankl_rect, select_lightness);}
    else if(ui.bankr_hover == 1){ih=1; setAreaLightness(bb, bankr_rect, select_lightness);}
    else if(ui.load_hover == 1){ih=1; setAreaLightness(bb, load_rect, select_lightness);}
    else if(ui.save_hover == 1){ih=1; setAreaLightness(bb, save_rect, select_lightness);}
    else if(ui.secl_hover == 1){ih=1; setAreaLightness(bb, secl_rect, select_lightness);}
    else if(ui.secr_hover == 1){ih=1; setAreaLightness(bb, secr_rect, select_lightness);}
    else if(ui.export_hover == 1){ih=1; setAreaLightness(bb, export_rect, select_lightness);}
    else if(ui.play_hover == 1){ih=1; setAreaLightness(bb, play_rect, select_lightness);}

    // colourise theme for export feedback
    if(themeon > 0)
    {
        static float h = 0;
        setHueSat(bb, export_rect, h, 0.5f);
        h += 0.002f;
        if(h >= 1.0f)
        {
            h = 0.f;
            themeon = 0;
        }
    }

    // blit to screen
    SDL_BlitSurface(bb, NULL, screen, NULL);
    SDL_UpdateWindowSurface(window);

    // cursor
    if(ih == 1)
        SDL_CursorPointer(1);
    else
        SDL_CursorPointer(0);
}

int main(int argc, char *args[])
{
    // init sdl
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS) < 0)
    {
        fprintf(stderr, "ERROR: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    // create window
    window = SDL_CreateWindow("Borg ER-0", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_rect.w, screen_rect.h, SDL_WINDOW_SHOWN);
    if(window == NULL)
    {
        fprintf(stderr, "ERROR: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }

    // create screen
    SDL_Surface* screen = SDL_GetWindowSurface(window);
    if(screen == NULL)
    {
        fprintf(stderr, "ERROR: SDL_GetWindowSurface(): %s\n", SDL_GetError());
        return 1;
    }

    // credit
    printf("Borg ER-0 by James William Fletcher\n");

    // get app dir
    basedir = SDL_GetBasePath();
    appdir = SDL_GetPrefPath("voxdsp", "borger0");
    printf("basePath: %s\n", basedir);
    printf("prefPath: %s\n", appdir);
#ifdef __linux__
    printf("exportPath: %s/EXPORTS/Borg_ER-0\n\n", getenv("HOME"));
#endif

    // sdl version
    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    printf("Compiled against SDL version %u.%u.%u.\n", compiled.major, compiled.minor, compiled.patch);
    printf("Linked against SDL version %u.%u.%u.\n", linked.major, linked.minor, linked.patch);

    // instructions
    printf("Binds to play audio: spacebar, mouse3, mouse4\n");
    printf("Reset envelope/oscillator: right click on it\n");
    printf("Scroll dial sensitivity selection: right click, three sensitvity options\n\n");

    // load assets
    loadAssets(screen);

    // load bank
    loadState();

    //init audio
    initMonoAudio(SAMPLE_RATE);

    // themeing (this is the general idea of how to re-theme, I don't intend to use this in production)
    // but say someone made a modification, it might be a good idea to change the theme a little
    // so that users know which version they are using.
    // ----------------------------------------------
    // replaceColour(s_bg, screen_rect, outlinecolor, 0, 200, 0);
    // outlinecolor = getpixel(s_bg, 6, 14);
    // replaceColour(s_bg, screen_rect, bgcolor, 32, 77, 133);
    // bgcolor = getpixel(s_bg, 0, 0);
    // replaceColour(s_bg, screen_rect, scopecolor, 77, 133, 0);
    // scopecolor = getpixel(s_bg, 7, 155);

    // set icon
    SDL_SetWindowIcon(window, s_icon);

    // first render
    stopSample();
    doSynth(0);
    render(screen);

    // event loop
    while(1)
    {
        SDL_Event event;
        Sint32 rx, ry;
        while(SDL_WaitEvent(&event))
        {
            switch(event.type)
            {
                case SDL_WINDOWEVENT:
                {
                    render(screen);
                }
                break;

                case SDL_MOUSEMOTION:
                {
                    const Sint32 x = event.motion.x, y = event.motion.y;
                    static struct sui lui;
                    
                    // Adjust envelope
                    if(envelope_enabled == 1 && x >= envelope_rect.x && x <= envelope_rect.x+envelope_rect.w-3 && y > envelope_rect.y && y < envelope_rect.y+envelope_rect.h+1)
                    {
                        static Uint32 li2 = 0;
                        const Uint32 ni = x-envelope_rect.x;
                        const float nv = 1.f-((float)(y-envelope_rect.y) / (float)envelope_rect.h);
                        synth[selected_bank].envelope[ni] = nv;

                        // really simple but effective smoothing method
                        // otherwise the input code needs to be executed on
                        // a seperate thread where the render no longer blocks.
                        Uint32 di = ni-li2;
                        if(di < 6)
                        {
                            while(di > 0)
                            {
                                synth[selected_bank].envelope[li2+di] = nv;
                                di--;
                            }
                        }
                        
                        // tick envalope renders at 20 fps
                        static Uint32 lt = 0;
                        if(SDL_GetTicks() > lt)
                        {
                            render(screen);
                            lt = SDL_GetTicks() + 50;
                        }

                        li2 = ni;
                        break;
                    }

                    // render dial rotations or button hovers
                    if(selected_dial >= 0)
                    {
                        // scale dial sensitivity
                        float sense = 0.001f / dial_scale[selected_dial];
                        if(select_mode == 0)
                        {
                            if(dial_scale[selected_dial] == MAXFREQUENCY)
                                sense = 0.3f / dial_scale[selected_dial];
                            else if(dial_scale[selected_dial] == MAXAMPLITUDE)
                                sense = 0.1f / dial_scale[selected_dial];
                        }
                        else if(select_mode == 1)
                            sense = 0.001f / dial_scale[selected_dial];
                        else if(select_mode == 2)
                            sense = 1.6f / dial_scale[selected_dial];

                        // do rotation
                        SDL_GetRelativeMouseState(&rx, &ry);
                        if(ry != 0){synth[selected_bank].dial_state[selected_dial] -= ((float)(ry))*sense;}
                        if(synth[selected_bank].dial_state[selected_dial] >= 1.f)
                            synth[selected_bank].dial_state[selected_dial] = 1.f;
                        else if(synth[selected_bank].dial_state[selected_dial] < -1.f)
                            synth[selected_bank].dial_state[selected_dial] = -1.f;

                        // tick dial turn renders at 20 fps
                        static Uint32 lt = 0;
                        if(SDL_GetTicks() > lt)
                        {
                            render(screen);
                            lt = SDL_GetTicks() + 50;
                        }
                    }
                    else
                    {
                        // Adjust oscillator
                        if(osc_enabled == 1 && x >= osc_rect.x && x <= osc_rect.x+osc_rect.w-3 && y >= osc_rect.y && y < osc_rect.y+osc_rect.h)
                        {
                            static Uint32 li1 = 0;
                            const Uint32 ni = x-osc_rect.x;
                            const float nv = 1.f-((float)(y-osc_rect.y+1) / (float)osc_rect.h);
                            synth[selected_bank].osctable[ni] = nv;

                            // really simple but effective smoothing method
                            // otherwise the input code needs to be executed on
                            // a seperate thread where the render no longer blocks.
                            Uint32 di = ni-li1;
                            if(di < 6)
                            {
                                while(di > 0)
                                {
                                    synth[selected_bank].osctable[li1+di] = nv;
                                    di--;
                                }
                            }
                            
                            // tick oscillator renders at 20 fps
                            static Uint32 lt = 0;
                            if(SDL_GetTicks() > lt)
                            {
                                render(screen);
                                lt = SDL_GetTicks() + 50;
                            }

                            li1 = ni;
                            break;
                        }

                        // clear states
                        memset(&ui, 0x00, sizeof(struct sui));

                        // check for hover state and return
                        Uint8 sc = 0; // skip check
                        if(inrange(&ui.bankl_hover, &sc, x, y, bankl_rect) == 0)
                        if(inrange(&ui.bankr_hover, &sc, x, y, bankr_rect) == 0)
                        if(inrange(&ui.load_hover, &sc, x, y, load_rect) == 0)
                        if(inrange(&ui.save_hover, &sc, x, y, save_rect) == 0)
                        if(inrange(&ui.secl_hover, &sc, x, y, secl_rect) == 0)
                        if(inrange(&ui.secr_hover, &sc, x, y, secr_rect) == 0)
                        if(inrange(&ui.export_hover, &sc, x, y, export_rect) == 0)
                        if(inrange(&ui.play_hover, &sc, x, y, play_rect) == 0)
                        for(int i = 0; i < TOTAL_DIALS; i++)
                            if(inrange(&ui.dial_hover[i], &sc, x, y, dial_rect[i]) == 1)
                                break;

                        // render if there has been a change in state
                        if(memcmp(&lui, &ui, sizeof(struct sui)) != 0)
                        {
                            render(screen);
                            memcpy(&lui, &ui, sizeof(struct sui));
                        }
                    }
                }
                break;

                case SDL_KEYDOWN:
                {
                    if(event.key.keysym.sym == SDLK_SPACE)
                    {
                        doSynth(1);
                        render(screen);
                    }
                }
                break;

                case SDL_MOUSEBUTTONUP:
                {
                    static struct ssynth lsyn[256];
                    
                    if(osc_enabled == 1)
                    {
                        SDL_CaptureMouse(SDL_FALSE);
                        osc_enabled = 0;
                        doSynth(0);
                        render(screen);
                        memcpy(&lsyn, &synth[selected_bank], sizeof(struct sui));
                    }
                    if(envelope_enabled == 1)
                    {
                        SDL_CaptureMouse(SDL_FALSE);
                        envelope_enabled = 0;
                        doSynth(0);
                        render(screen);
                        memcpy(&lsyn, &synth[selected_bank], sizeof(struct sui));
                    }
                    else if(selected_dial >= 0)
                    {
                        SDL_GetRelativeMouseState(&rx, &ry);
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        selected_dial = -1;
                        doSynth(0);
                        render(screen);
                        memcpy(&lsyn, &synth[selected_bank], sizeof(struct sui));
                    }
                    else
                    {
                        if(memcmp(&lsyn, &synth[selected_bank], sizeof(struct sui)) != 0)
                        {
                            doSynth(0);
                            render(screen);
                            memcpy(&lsyn, &synth[selected_bank], sizeof(struct sui));
                        }
                    }
                }
                break;

                case SDL_MOUSEBUTTONDOWN:
                {
                    const Uint32 x = event.button.x, y = event.button.y;

                    if(event.button.button == SDL_BUTTON_RIGHT)
                    {
                        // dial sensitivity
                        Uint8 sc = 0; // skip check
                        for(int i = 0; i < TOTAL_DIALS; i++)
                        {
                            if(inrange(&ui.dial_hover[i], &sc, x, y, dial_rect[i]) == 1)
                            {
                                select_mode++;
                                if(select_mode >= 3)
                                    select_mode = 0;
                                break;
                            }
                        }
                        if(sc == 1)
                        {
                            render(screen);
                            break;
                        }

                        // reset envelope
                        if(x > envelope_rect.x && x < envelope_rect.x+envelope_rect.w && y > envelope_rect.y && y < envelope_rect.y+envelope_rect.h)
                        {
                            for(int i = 0; i < envelope_rect.w-2; i++)
                                synth[selected_bank].envelope[i] = 0.5f;
                            doSynth(0);
                            render(screen);
                            break;
                        }

                        // reset osc
                        if(x > osc_rect.x && x < osc_rect.x+osc_rect.w && y > osc_rect.y && y < osc_rect.y+osc_rect.h)
                        {
                            const float su = 6.283185482f / (float)(osc_rect.w-2);
                            for(int i = 0; i < osc_rect.w-2; i++)
                                synth[selected_bank].osctable[i] = (sinf(su*i)+1.0f)*0.5f;
                            doSynth(0);
                            render(screen);
                            break;
                        }
                        
                        render(screen);
                    }
                    else if(event.button.button == SDL_BUTTON_X1 || event.button.button == SDL_BUTTON_MIDDLE)
                    {
                        doSynth(1);
                        render(screen);
                    }
                    else if(event.button.button == SDL_BUTTON_LEFT)
                    {
                        Uint8 sc = 0; // skip check

                        for(int i = 0; i < TOTAL_DIALS; i++)
                        {
                            if(ui.dial_hover[i] == 1)
                            {
                                SDL_GetRelativeMouseState(&rx, &ry);
                                SDL_SetRelativeMouseMode(SDL_TRUE);
                                selected_dial = i;
                                break;
                            }
                        }

                        if(sc == 0)
                        {
                            if(x > envelope_rect.x && x < envelope_rect.x+envelope_rect.w && y > envelope_rect.y && y < envelope_rect.y+envelope_rect.h)
                            {
                                sc=1;
                                envelope_enabled = 1;
                                SDL_CaptureMouse(SDL_TRUE);
                            }
                            else if(x > osc_rect.x && x < osc_rect.x+osc_rect.w && y > osc_rect.y && y < osc_rect.y+osc_rect.h)
                            {
                                sc=1;
                                osc_enabled = 1;
                                SDL_CaptureMouse(SDL_TRUE);
                            }
                            else if(ui.play_hover == 1)
                            {
                                sc=1;
                                doSynth(1);
                            }
                            else if(ui.export_hover == 1)
                            {
                                sc=1;
                                char file[256];
#ifdef __linux__
                                sprintf(file, "%s/EXPORTS", getenv("HOME"));
                                mkdir(file, 0755);
                                sprintf(file, "%s/Borg_ER-0", file);
                                mkdir(file, 0755);
                                sprintf(file, "%s/bank-%d.wav", file, selected_bank);
                                printf("File written to: %s\n", file);
#else
                                sprintf(file, "bank-%d.wav", selected_bank);
#endif
                                writeWAV(file);

                                // some user feedback
                                themeon=1;
                                while(themeon == 1)
                                    render(screen);
                            }
                            else if(ui.load_hover == 1)
                            {
                                sc=1;
                                loadState();
                                doSynth(0);
                            }
                            else if(ui.save_hover == 1)
                            {
                                sc=1;
                                saveState();
                            }
                            else if(ui.bankl_hover == 1)
                            {
                                stopSample();
                                sc=1;
                                selected_bank--;
                                doSynth(0);
                            }
                            else if(ui.bankr_hover == 1)
                            {
                                stopSample();
                                sc=1;
                                selected_bank++;
                                doSynth(0);
                            }
                            else if(ui.secl_hover == 1)
                            {
                                sc=1;
                                synth[selected_bank].seclen--;
                                if(synth[selected_bank].seclen == 0)
                                    synth[selected_bank].seclen = MAXSAMPLELEN;
                                doSynth(0);
                                stopSample();
                            }
                            else if(ui.secr_hover == 1)
                            {
                                sc=1;
                                synth[selected_bank].seclen++;
                                if(synth[selected_bank].seclen > MAXSAMPLELEN)
                                    synth[selected_bank].seclen = 1;
                                doSynth(0);
                                stopSample();
                            }
                        }

                        render(screen);
                    }
                }
                break;
                
                case SDL_QUIT:
                {
                    saveState();
                    SDL_FreeSurface(bb);
                    SDL_FreeSurface(s_bg);
                    SDL_FreeSurface(s_icon);
                    SDL_CursorPointer(1337);
                    drawText(NULL, "*K", 0, 0, 0);
                    SDL_DestroyWindow(window);
                    SDL_CloseAudio();
                    SDL_Quit();
                    exit(0);
                }
                break;
            }
        }
    }

    //Done.
    return 0;
}

