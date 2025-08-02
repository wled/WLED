/******************************************************************************
  * @file           : pieces.h
  * @brief          : contains the tetris pieces with their colors indecies
  ******************************************************************************
  * @attention
  *
  * Copyright (c) muebau 2022
  * All rights reserved.</center></h2>
  *
  ******************************************************************************
*/

#ifndef __PIECES_H__
#define __PIECES_H__

#include <stdint.h>
#include <stdbool.h>

#include <bitset>
#include <cstddef>
#include <cassert>
#include <iostream>

#define numPieces 7

struct PieceRotation
{
    uint8_t width;
    uint8_t height;
    uint16_t rows;
};

struct PieceData
{
    uint8_t rotCount;
    uint8_t colorIndex;
    PieceRotation rotations[4];
};

PieceData piecesData[numPieces] = {
    // I
    {
            2,
            1,
            {
                { 1, 4, 0b0001000100010001},
                { 4, 1, 0b0000000000001111}
            }
    },
    // O
    {
            1,
            2,
            {
                { 2, 2, 0b0000000000110011}
            }
    },
    // Z
    {
            2,
            3,
            {
                { 3, 2, 0b0000000001100011},
                { 2, 3, 0b0000000100110010}
            }
    },
    // S
    {
            2,
            4,
            {
                { 3, 2, 0b0000000000110110},
                { 2, 3, 0b0000001000110001}
            }
    },
    // L
    {
            4,
            5,
            {
                { 2, 3, 0b0000001000100011},
                { 3, 2, 0b0000000001110100},
                { 2, 3, 0b0000001100010001},
                { 3, 2, 0b0000000000010111}
            }
    },
    // J
    {
            4,
            6,
            {
                { 2, 3, 0b0000000100010011},
                { 3, 2, 0b0000000001000111},
                { 2, 3, 0b0000001100100010},
                { 3, 2, 0b0000000001110001}
            }
    },
    // T
    {
            4,
            7,
            {
                { 3, 2, 0b0000000001110010},
                { 2, 3, 0b0000000100110001},
                { 3, 2, 0b0000000000100111},
                { 2, 3, 0b0000001000110010}
            }
    },
};

class Piece
{
private:
public:
    uint8_t x;
    uint8_t y;
    PieceData* pieceData;
    uint8_t rotation;
    uint8_t landingY;

    Piece(uint8_t pieceIndex = 0):
        x(0),
        y(0),
        rotation(0),
        landingY(0)
    {
        this->pieceData = &piecesData[pieceIndex];
    }

    void reset()
    {
        this->rotation = 0;
        this->x = 0;
        this->y = 0;
        this->landingY = 0;
    }

    uint32_t getGridRow(uint8_t x, uint8_t y, uint8_t width)
    {
        if (x < width)
        {
            //shift the row with the "top-left" position to the "x" position
            auto shiftx = (width - 1) - x;
            auto topleftx = (getRotation().width - 1);

            auto finalShift = shiftx - topleftx;
            auto row = getRow(y);
            auto finalResult = row << finalShift;

            return finalResult;
        }
        return 0xffffffff;
    }

    uint8_t getRow(uint8_t y)
    {
        if (y < 4)
        {
            return (getRotation().rows >> (12 - (4 * y))) & 0xf;
        }
        return 0xf;
    }

    bool getPixel(uint8_t x, uint8_t y)
    {
        if(x > getRotation().width - 1 || y > getRotation().height - 1 )
        {
            return false;
        }
        
        if (x < 4 && y < 4)
        {
            return (getRow((4 - getRotation().height) + y) >> (3 - ((4 - getRotation().width) + x))) & 0x1;
        }
        return false;
    }

    PieceRotation getRotation()
    {
        return this->pieceData->rotations[rotation];
    }
};

#endif /* __PIECES_H__ */
