/******************************************************************************
  * @file           : rating.h
  * @brief          : contains the tetris rating of a grid
  ******************************************************************************
  * @attention
  *
  * Copyright (c) muebau 2022
  * All rights reserved.</center></h2>
  *
  ******************************************************************************
*/

#ifndef __RATING_H__
#define __RATING_H__

#include <stdint.h>
#include <float.h>
#include <stdbool.h>
#include <math.h>
#include <vector>
#include "rating.h"

using namespace std;

class Rating
{
private:
public:
    uint8_t minHeight;
    uint8_t maxHeight;
    uint16_t holes;
    uint8_t fullLines;
    uint16_t bumpiness;
    uint16_t aggregatedHeight;
    float score;
    uint8_t width;
    std::vector<uint8_t> lineHights;

    Rating(uint8_t width):
        width(width),
        lineHights(width)
    {
        reset();
    }

    void reset()
    {
        this->minHeight = 0;
        this->maxHeight = 0;

        for (uint8_t line = 0; line < this->width; line++)
        {
            this->lineHights[line] = 0;
        }

        this->holes = 0;
        this->fullLines = 0;
        this->bumpiness = 0;
        this->aggregatedHeight = 0;
        this->score = -FLT_MAX;
    }
};

#endif /* __RATING_H__ */
