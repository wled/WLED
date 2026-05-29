#include "bmpimage.h"
#define BUF_SIZE 64000

byte * _buffer = nullptr;

uint16_t read16(File &f) {
  uint16_t result;
  f.read((uint8_t *)&result,2);
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  f.read((uint8_t *)&result,4);
  return result;
}

bool BMPimage::init(const char * fn) {
    File bmpFile;
    int bmpDepth;
    //first, check if filename exists
    if (!WLED_FS.exists(fn)) {
      return false;
    }
    
    bmpFile = WLED_FS.open(fn);
    if (!bmpFile) {
      _valid=false;
      return false;
    }

    //so, the file exists and is opened
    // Parse BMP header
    uint16_t header = read16(bmpFile);
    if(header != 0x4D42) { // BMP signature
      _valid=false;
      bmpFile.close();
      return false;
    }

    //read and ingnore file size
    read32(bmpFile);
    (void)read32(bmpFile); // Read & ignore creator bytes
    _imageOffset = read32(bmpFile); // Start of image data
    // Read DIB header
    read32(bmpFile);
    _width  = read32(bmpFile);
    _height = read32(bmpFile);
    if(read16(bmpFile) != 1) { // # planes -- must be '1'
        _valid=false;
        bmpFile.close();
        return false;
    }
    bmpDepth = read16(bmpFile); // bits per pixel
    if((bmpDepth != 24) || (read32(bmpFile) != 0)) { // 0 = uncompressed {
        _width=0;
        _valid=false;
        bmpFile.close();
        return false;
    }
    // If _height is negative, image is in top-down order.
    // This is not canon but has been observed in the wild.
    if(_height < 0) {
        _height = -_height;
    }
    //now, we have successfully got all the basics
    // BMP rows are padded (if needed) to 4-byte boundary
    _rowSize = (_width * 3 + 3) & ~3;
    //check image size - if it is too large, it will be unusable
    if (_rowSize*_height>BUF_SIZE) {
      _valid=false;
      bmpFile.close();
      return false;
    }

    bmpFile.close();
    // Ensure filename fits our buffer (segment name length constraint).
    size_t len = strlen(fn);
    if (len > WLED_MAX_SEGNAME_LEN) {
      return false;
    }
    strncpy(filename, fn, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';
    _valid = true;
    return true;
}

void BMPimage::clear(){
    strcpy(filename, "");
    _width=0;
    _height=0;
    _rowSize=0;
    _imageOffset=0;
    _loaded=false;
    _valid=false;
}

bool BMPimage::load(){
    const size_t size = (size_t)_rowSize * (size_t)_height;
    if (size > BUF_SIZE) {
        return false;
    }
    File bmpFile = WLED_FS.open(filename);
    if (!bmpFile) {
        return false;
    }

    if (_buffer != nullptr) free(_buffer);
    _buffer = (byte*)malloc(size);
    if (_buffer == nullptr) return false;

    bmpFile.seek(_imageOffset);
    const size_t readBytes = bmpFile.read(_buffer, size);
    bmpFile.close();
    if (readBytes != size) {
        _loaded = false;
        return false;
    }
    _loaded = true;
    return true;
}

byte* BMPimage::line(uint16_t n){
    if (_loaded) {
        return (_buffer+n*_rowSize);
    } else {
        return NULL;
    }
}

uint32_t BMPimage::pixelColor(uint16_t x, uint16_t  y){
    uint32_t pos;
    byte b,g,r; //colors
    if (! _loaded) {
      return 0;
    }
    if ( (x>=_width) || (y>=_height) ) {
      return 0;
    }
    pos=y*_rowSize + 3*x;
    //get colors. Note that in BMP files, they go in BGR order
    b= _buffer[pos++];
    g= _buffer[pos++];
    r= _buffer[pos];
    return (r<<16|g<<8|b);
}
