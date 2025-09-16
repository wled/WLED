/******************************************************************************
  * @file           : ai.h
  * @brief          : contains the heuristic
  ******************************************************************************
  * @attention
  *
  * Copyright (c) muebau 2023
  * All rights reserved.</center></h2>
  *
  ******************************************************************************
*/

#ifndef __AI_H__
#define __AI_H__

#include "gridbw.h"
#include "rating.h"

using namespace std;

class TetrisAI
{
private:
public:
    float aHeight;
    float fullLines;
    float holes;
    float bumpiness;
    bool findWorstMove = false;

    uint8_t countOnes(uint32_t vector)
    {
        uint8_t count = 0;
        while (vector)
        {
            vector &= (vector - 1);
            count++;
        }
        return count;
    }

    void updateRating(GridBW grid, Rating* rating)
    {
        rating->minHeight = 0;
        rating->maxHeight = 0;
        rating->holes = 0;
        rating->fullLines = 0;
        rating->bumpiness = 0;
        rating->aggregatedHeight = 0;
        fill(rating->lineHights.begin(), rating->lineHights.end(), 0);

        uint32_t columnvector = 0x0;
        uint32_t lastcolumnvector = 0x0;
        for (uint8_t row = 0; row < grid.height; row++)
        {
            columnvector |= grid.pixels[row];

            //first (highest) column makes it
            if (rating->maxHeight == 0 && columnvector)
            {
                rating->maxHeight = grid.height - row;
            }

            //if column vector is full we found the minimal height (or it stays zero)
            if (rating->minHeight == 0 && (columnvector == (uint32_t)((1 << grid.width) - 1)))
            {
                rating->minHeight = grid.height - row;
            }

            //line full if all ones in mask :-)
            if (grid.isLineFull(row))
            {
                rating->fullLines++;
            }

            //holes are basically a XOR with the "full" columns
            rating->holes += countOnes(columnvector ^ grid.pixels[row]);

            //calculate the difference (XOR) between the current column vector and the last one
            uint32_t columnDelta = columnvector ^ lastcolumnvector;

            //process every new column
            uint8_t index = 0;
            while (columnDelta)
            {
                //if this is a new column
                if (columnDelta & 0x1)
                {
                    //update hight of this column
                    rating->lineHights[(grid.width - 1) - index] = grid.height - row;

                    // update aggregatedHeight
                    rating->aggregatedHeight += grid.height - row;
                }
                index++;
                columnDelta >>= 1;
            }
            lastcolumnvector = columnvector;
        }

        //compare every two columns to get the difference and add them up
        for (uint8_t column = 1; column < grid.width; column++)
        {
            rating->bumpiness += abs(rating->lineHights[column - 1] - rating->lineHights[column]);
        }

        rating->score = (aHeight * (rating->aggregatedHeight)) + (fullLines * (rating->fullLines)) + (holes * (rating->holes)) + (bumpiness * (rating->bumpiness));
    }

    TetrisAI(): TetrisAI(-0.510066f, 0.760666f, -0.35663f, -0.184483f)
    {}

    TetrisAI(float aHeight, float fullLines, float holes, float bumpiness):
        aHeight(aHeight),
        fullLines(fullLines),
        holes(holes),
        bumpiness(bumpiness)
    {}

    void findBestMove(GridBW grid, Piece *piece)
    {
        vector<Piece> pieces = {*piece};
        findBestMove(grid, &pieces);
        *piece = pieces[0];
    }

    void findBestMove(GridBW grid, std::vector<Piece> *pieces)
    {
        findBestMove(grid, pieces->begin(), pieces->end());
    }

    void findBestMove(GridBW grid, std::vector<Piece>::iterator start, std::vector<Piece>::iterator end)
    {
        Rating bestRating(grid.width);
        findBestMove(grid, start, end, &bestRating);
    }

    void findBestMove(GridBW grid, std::vector<Piece>::iterator start, std::vector<Piece>::iterator end, Rating* bestRating)
    {
        grid.cleanupFullLines();
        Rating curRating(grid.width);
        Rating deeperRating(grid.width);
        Piece piece = *start;

        // for every rotation of the piece
        for (piece.rotation = 0; piece.rotation < piece.pieceData->rotCount; piece.rotation++)
        {
            // put piece to top left corner
            piece.x = 0;
            piece.y = 0;

            //test for every column
            for (piece.x = 0; piece.x <= grid.width - piece.getRotation().width; piece.x++)
            {
                //todo optimise by the use of the previous grids height
                piece.landingY = 0;
                //will set landingY to final position
                grid.findLandingPosition(&piece);

                // draw piece
                grid.placePiece(&piece, piece.x, piece.landingY);

                if(start == end - 1)
                {
                    //at the deepest level
                    updateRating(grid, &curRating);
                }
                else
                {
                    //go deeper to take another piece into account
                    findBestMove(grid, start + 1, end, &deeperRating);
                    curRating = deeperRating;
                }

                // eraese piece
                grid.erasePiece(&piece, piece.x, piece.landingY);

                if(findWorstMove)
                {
                    //init rating for worst
                    if(bestRating->score == -FLT_MAX)
                    {
                        bestRating->score = FLT_MAX;
                    }

                    // update if we found a worse one
                    if (bestRating->score > curRating.score)
                    {
                        *bestRating = curRating;
                        (*start) = piece;
                    }
                }
                else
                {
                    // update if we found a better one
                    if (bestRating->score < curRating.score)
                    {
                        *bestRating = curRating;
                        (*start) = piece;
                    }
                }
            }
        }
    }
};

#endif /* __AI_H__ */