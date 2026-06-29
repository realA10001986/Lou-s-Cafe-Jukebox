/*
 * AudioFileSourceLoop
 * Read SD/SPIFFS/LittleFS file to be used by AudioGenerator
 * Reads file in a loop (for looped playback)
 * 
 * Thomas Winischhofer (A10001986), 2023/2026
 *
 * Based on AudioFileSourceSD by Earle F. Philhower, III
 *
 */

#include "jb_global.h"
#include "AudioFileSourceLoop.h"

AudioFileSourceLoop::~AudioFileSourceLoop()
{
    if(f) f.close();
}

uint32_t AudioFileSourceLoop::read(void *data, uint32_t len)
{
    uint32_t glen = 0, rlen = len, g;
    
    switch(ftype) {
    case 1:
        glen = f.read((uint8_t *)data, len);
        if(!doPlayLoop || glen == len) return glen;
        seek(startPos, SEEK_SET);
        glen += f.read(((uint8_t *)data) + glen, len - glen);
        break;
    }

    return glen;
}

bool AudioFileSourceLoop::seek(int32_t pos, int dir)
{
    if(!f) return false;
    if(dir == SEEK_SET)      return f.seek(pos);
    else if(dir == SEEK_CUR) return f.seek(f.position() + pos);
    else if(dir == SEEK_END) return f.seek(f.size() + pos);
    return false;
}

// SD -----------------------------------------------

AudioFileSourceSDLoop::AudioFileSourceSDLoop()
{
}

/*
AudioFileSourceSDLoop::AudioFileSourceSDLoop(const char *filename)
{
    open(filename);
}
*/

bool AudioFileSourceSDLoop::open(const char *filename)
{
    f = SD.open(filename, FILE_READ);
    ftype = 1;
    return f;
}

// FlashFS -------------------------------------------

AudioFileSourceFSLoop::AudioFileSourceFSLoop()
{
}

/*
AudioFileSourceFSLoop::AudioFileSourceFSLoop(const char *filename)
{
    open(filename);
}
*/

bool AudioFileSourceFSLoop::open(const char *filename)
{
    f = LittleFS.open(filename, FILE_READ);
    ftype = 1;
    return f;
}
