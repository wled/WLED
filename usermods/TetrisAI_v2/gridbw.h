/******************************************************************************
  * @file           : gridbw.h
  * @brief          : contains the tetris grid as binary so black and white version
  ******************************************************************************
  * @attention
  *
  * Copyright (c) muebau 2023
  * All rights reserved.</center></h2>
  *
  ******************************************************************************
*/

#ifndef __GRIDBW_H__
#define __GRIDBW_H__

#include <vector>
#include "pieces.h"

using namespace std;

class GridBW
{
private:
public:
    uint8_t width;
    uint8_t height;
    std::vector<uint32_t> pixels;
    // When a row fills, we mark it here first so it can flash before being
    // fully removed.
    std::vector<bool> clearingRows;
    // True when a line clearing flashing effect is over and we're ready to
    // fully clean up the lines
    bool clearedLinesReadyForRemoval = false;

    GridBW(uint8_t width, uint8_t height):
        width(width),
        height(height),
        pixels(height),
        clearingRows(height)
    {
        if (width > 32)
        {
            this->width = 32;
        }
    }

    void placePiece(Piece* piece, uint8_t x, uint8_t y)
    {
        for (uint8_t row = 4 - piece->getRotation().height; row < 4; row++)
        {
            pixels[y + (row - (4 - piece->getRotation().height))] |= piece->getGridRow(x, row, width);
        }
    }

    void erasePiece(Piece* piece, uint8_t x, uint8_t y)
    {
        for (uint8_t row = 4 - piece->getRotation().height; row < 4; row++)
        {
            pixels[y + (row - (4 - piece->getRotation().height))] &= ~piece->getGridRow(x, row, width);
        }
    }

    bool noCollision(Piece* piece, uint8_t x, uint8_t y)
    {
        //if it touches a wall it is a collision
        if (x > (this->width - piece->getRotation().width) || y > this->height - piece->getRotation().height)
        {
            return false;
        }

        for (uint8_t row = 4 - piece->getRotation().height; row < 4; row++)
        {
            if (piece->getGridRow(x, row, width) & pixels[y + (row - (4 - piece->getRotation().height))])
            {
                return false;
            }
        }
        return true;
    }

    void findLandingPosition(Piece* piece)
    {
        // move down until the piece bumps into some occupied pixels or the 'wall'
        while (noCollision(piece, piece->x, piece->landingY))
        {
            piece->landingY++;
        }

        //at this point the positon is 'in the wall' or 'over some occupied pixel'
        //so the previous position was the last correct one (clamped to 0 as minimum).
        piece->landingY = piece->landingY > 0 ? piece->landingY - 1 : 0;
    }

    bool hasClearingRows()
    {
        for (bool rowClearing : clearingRows)
        {
            if (rowClearing)
            {
                return true;
            }
        }
        return false;
    }

    void cleanupFullLines()
    {
        // Skip cleanup if there are rows clearing
        if (hasClearingRows() && !clearedLinesReadyForRemoval) {
            return;
        }
        uint8_t offset = 0;
        bool doneRemovingClearedLines = false;

        //from "height - 1" to "0", so from bottom row to top
        for (uint8_t row = height; row-- > 0; )
        {
            //full line?
            if (isLineFull(row))
            {
                if (clearedLinesReadyForRemoval) {
                    offset++;
                    pixels[row] = 0x0;
                    doneRemovingClearedLines = true;
                } else {
                    clearingRows[row] = true;
                }
                continue;
            }

            if (offset > 0)
            {
                pixels[row + offset] = pixels[row];
                pixels[row] = 0x0;
            }
        }
        if (doneRemovingClearedLines) {
            clearingRows.assign(height, false);
            clearedLinesReadyForRemoval = false;
        }
    }

    bool isLineFull(uint8_t y)
    {
        return pixels[y] == (width >= 32 ? UINT32_MAX : (1U << width) - 1);
    }

    bool isLineReadyForRemoval(uint8_t y)
    {
        return clearedLinesReadyForRemoval && isLineFull(y);
    }

    void reset()
    {
        if (width > 32)
        {
            width = 32;
        }

        pixels.clear();
        pixels.resize(height);
        clearingRows.assign(height, false);
        clearedLinesReadyForRemoval = false;
    }
};

#endif /* __GRIDBW_H__ */