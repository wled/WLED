#pragma once
#include "wled.h"
#include <PNGdec.h>

void * openFile(const char *filename, int32_t *size) {
    f = WLED_FS.open(filename);
    *size = f.size();
    return &f;
}

void closeFile(void *handle) {
    if (f) f.close();
}

int32_t readFile(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen)
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

int32_t seekFile(PNGFILE *pFile, int32_t iPosition)
{
    int i = micros();
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    i = micros() - i;
    return pFile->iPos;
}

void draw(PNGDRAW *pDraw) {
    uint16_t usPixels[SEGLEN];
    png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
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
    const char * filepath = SEGMENT.name;
    int rc = png.open(filepath, openFile, closeFile, readFile, seekFile, draw);
    if (rc == PNG_SUCCESS) {
	rc = png.decode(NULL, 0);
	png.close();
	return FRAMETIME;
    }
    return FRAMETIME;
}

class PovDisplayUsermod : public Usermod
{
  public:
    static const char _data_FX_MODE_POV_IMAGE[] PROGMEM = "POV Image@!;;;1";

    PNG png;
    File f;

    void setup() {
	strip.addEffect(255, &mode_pov_image, _data_FX_MODE_POV_IMAGE);
    }

    void loop() {
    }

    uint16_t getId()
    {
      return USERMOD_ID_POV_DISPLAY;
    }

    void connected() {}
};
