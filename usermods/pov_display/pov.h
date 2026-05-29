#ifndef _POV_H
#define _POV_H
#include "bmpimage.h"


class POV {
    public:
        POV();

        /* Shows one line. line should be pointer to array which holds  pixel colors
         * (3 bytes per pixel, in BGR order). Note: 3, not 4!!!
         *  size should be size of array (number of pixels, not number of bytes)
         */
        void showLine(const byte * line, uint16_t size);

        /* Reads from file an image and making it current image */
        bool loadImage(const char * filename);

        /* Show next line of active image
           Retunrs the index of next line to be shown (not yet shown!)
           If it retunrs 0, it means we have completed showing the image and
            next call will start again
        */
        int16_t showNextLine();

        //time since strip was last updated, in micro sec
        uint32_t timeSinceUpdate() {return (micros()-lastLineUpdate);}


        BMPimage * currentImage() {return &image;}

        char * getFilename() {return image.getFilename();}

    private:
        BMPimage image;
        int16_t  currentLine=0;     //next line to be shown
        uint32_t lastLineUpdate=0; //time in microseconds
};



#endif
