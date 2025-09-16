/******************************************************************************
  * @file           : gridcolor.h
  * @brief          : contains the tetris grid as 8bit indexed color version
  ******************************************************************************
  * @attention
  *
  * Copyright (c) muebau 2023
  * All rights reserved.</center></h2>
  *
  ******************************************************************************
*/

#ifndef __GRIDCOLOR_H__
#define __GRIDCOLOR_H__
#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include "gridbw.h"
#include "gridcolor.h"

using namespace std;

class GridColor
{
private:
public:
    uint8_t width;
    uint8_t height;
    GridBW gridBW;
    std::vector<uint8_t> pixels;

    GridColor(uint8_t width, uint8_t height):
        width(width),
        height(height),
        gridBW(width, height),
        pixels(width* height)
    {}

    void clear()
    {
        for (uint8_t y = 0; y < height; y++)
        {
            gridBW.pixels[y] = 0x0;
            for (int8_t x = 0; x < width; x++)
            {
                *getPixel(x, y) = 0;
            }
        }
    }

    void placePiece(Piece* piece, uint8_t x, uint8_t y)
    {
        for (uint8_t pieceY = 0; pieceY < piece->getRotation().height; pieceY++)
        {
            for (uint8_t pieceX = 0; pieceX < piece->getRotation().width; pieceX++)
            {
                if (piece->getPixel(pieceX, pieceY))
                {
                    *getPixel(x + pieceX, y + pieceY) = piece->pieceData->colorIndex;
                }
            }
        }
    }

    void erasePiece(Piece* piece, uint8_t x, uint8_t y)
    {
        for (uint8_t pieceY = 0; pieceY < piece->getRotation().height; pieceY++)
        {
            for (uint8_t pieceX = 0; pieceX < piece->getRotation().width; pieceX++)
            {
                if (piece->getPixel(pieceX, pieceY))
                {
                    *getPixel(x + pieceX, y + pieceY) = 0;
                }
            }
        }
    }

    void cleanupFullLines()
    {
        uint8_t offset = 0;
        //from "height - 1" to "0", so from bottom row to top
        for (uint8_t y = height; y-- > 0; )
        {
            if (gridBW.isLineFull(y))
            {
                offset++;
                for (uint8_t x = 0; x < width; x++)
                {
                    pixels[y * width + x] = 0;
                }
                continue;
            }

            if (offset > 0)
            {
                if (gridBW.pixels[y])
                {
                    for (uint8_t x = 0; x < width; x++)
                    {
                        pixels[(y + offset) * width + x] = pixels[y * width + x];
                        pixels[y * width + x] = 0;
                    }
                }
            }
        }
        gridBW.cleanupFullLines();
    }

    uint8_t* getPixel(uint8_t x, uint8_t y)
    {
        return &pixels[y * width + x];
    }

    void sync()
    {
        for (uint8_t y = 0; y < height; y++)
        {
            gridBW.pixels[y] = 0x0;
            for (int8_t x = 0; x < width; x++)
            {
                gridBW.pixels[y] <<= 1;
                if (*getPixel(x, y) != 0)
                {
                    gridBW.pixels[y] |= 0x1;
                }
            }
        }
    }

    void reset()
    {
        gridBW.reset();
        pixels.clear();
        pixels.resize(width* height);
        clear();
    }
};

#endif /* __GRIDCOLOR_H__ */