#ifndef _BMPIMAGE_H
#define _BMPIMAGE_H
#include "Arduino.h"
#include "wled.h"

/*
 * This class describes a bitmap image. Each object refers to a bmp file on
 * filesystem fatfs.
 * To initialize, call init(), passign to it name of a bitmap file
 * at the root of fatfs filesystem:
 *
 * BMPimage myImage;
 * myImage.init("logo.bmp");
 *
 * For performance reasons, before actually usign the image, you need to load
 * it from filesystem to RAM:
 * myImage.load();
 * All load() operations use the same reserved buffer in RAM, so you can only
 * have one file loaded at a time. Before loading a new file, always unload the
 * previous one:
 * myImage.unload();
 */

class BMPimage {
    public:
        int height()    {return _height; }
        int width()     {return _width;  }
        int rowSize()   {return _rowSize;}
        bool isLoaded() {return _loaded; }
        bool load();
        void unload()   {_loaded=false;  }
        byte * line(uint16_t n);
        uint32_t pixelColor(uint16_t x,uint16_t  y);
        bool init(const char* fn);
        void clear();
        char * getFilename() {return filename;};

    private:
        char filename[WLED_MAX_SEGNAME_LEN+1]="";
        int _width=0;
        int _height=0;
        int _rowSize=0;
        int _imageOffset=0;
        bool _loaded=false;
        bool _valid=false;
};

extern byte * _buffer;

#endif
