#ifndef _POV_H
#define _POV_H
#include "bmpimage.h"


class POV {
    public:
        POV();
        
        /* Shows a column directly from image memory (for horizontal POV)
         * Uses image width/height/rowSize to compute pixel positions
         */
        void showColumn(uint16_t colIndex);

        /* Reads from file an image and making it current image */
        bool loadImage(const char * filename);
        
        BMPimage * currentImage() {return &image;}

        char * getFilename() {return image.getFilename();}

    private:
        BMPimage image;
};



#endif
