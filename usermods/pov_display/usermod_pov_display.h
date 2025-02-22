#pragma once
#include "wled.h"
#include <PNGdec.h>

PNG png_decoder;
File pov_image;
static const char _data_FX_MODE_POV_IMAGE[] PROGMEM = "POV Image@!;;;1";

void * PovOpenFile(const char *filename, int32_t *size) {
    pov_image = WLED_FS.open(filename);
    *size = pov_image.size();
    return &pov_image;
}

void PovCloseFile(void *handle) {
    if (pov_image) pov_image.close();
}

int32_t PovReadFile(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
	iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
	return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
}

int32_t PovSeekFile(PNGFILE *pFile, int32_t iPosition)
{
    int i = micros();
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    i = micros() - i;
    return pFile->iPos;
}

void PovDraw(PNGDRAW *pDraw) {
    uint16_t usPixels[SEGLEN];
    png_decoder.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    for(int x=0; x < SEGLEN; x++) {
	uint16_t color = usPixels[x];
	byte r = ((color >> 11) & 0x1F);
	byte g = ((color >> 5) & 0x3F);
	byte b = (color & 0x1F);
	SEGMENT.setPixelColor(x, RGBW32(r,g,b,0));
    }
    strip.show();
}

uint16_t mode_pov_image(void) {
    char filepath[WLED_MAX_SEGNAME_LEN + 1] = "/";
    strncpy(filepath + 1, SEGMENT.name, WLED_MAX_SEGNAME_LEN);
    int rc = png_decoder.open(filepath, PovOpenFile, PovCloseFile, PovReadFile, PovSeekFile, PovDraw);
    if (rc == PNG_SUCCESS) {
	rc = png_decoder.decode(NULL, 0);
	png_decoder.close();
	return FRAMETIME;
    }
    return FRAMETIME;
}

class PovDisplayUsermod : public Usermod {
  protected:
        bool enabled = false; //WLEDMM
        const char *_name; //WLEDMM
        bool initDone = false; //WLEDMM
        unsigned long lastTime = 0; //WLEDMM

  public:
    PovDisplayUsermod(const char *name, bool enabled) {
	this->_name = name;
	this->enabled = enabled;
    } //WLEDMM

    void setup() {
	strip.addEffect(255, &mode_pov_image, _data_FX_MODE_POV_IMAGE);
	initDone = true;
    }

    void loop() {
    }

    uint16_t getId()
    {
      return USERMOD_ID_POV_DISPLAY;
    }

    void connected() {}
};
