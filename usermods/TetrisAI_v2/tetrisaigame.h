/******************************************************************************
  * @file           : tetrisaigame.h
  * @brief          : main tetris functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) muebau 2022
  * All rights reserved.</center></h2>
  *
  ******************************************************************************
*/

#ifndef __TETRISAIGAME_H__
#define __TETRISAIGAME_H__

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include "pieces.h"
#include "gridcolor.h"
#include "tetrisbag.h"
#include "tetrisai.h"

using namespace std;

class TetrisAIGame
{
private:
    bool animateFallOfPiece(Piece* piece, bool skip)
    {
        if (skip || piece->y >= piece->landingY)
        {
            piece->y = piece->landingY;
            grid.gridBW.placePiece(piece, piece->x, piece->landingY);
            grid.placePiece(piece, piece->x, piece->y);
            return false;
        }
        else
        {
            // eraese last drawing
            grid.erasePiece(piece, piece->x, piece->y);

            //move piece down
            piece->y++;

            // draw piece
            grid.placePiece(piece, piece->x, piece->y);

            return true;
        }
    }

public:
    uint8_t width;
    uint8_t height;
    uint8_t nLookAhead;
    uint8_t nPieces;
    TetrisBag bag;
    GridColor grid;
    TetrisAI ai;
    Piece curPiece;
    PieceData* piecesData;
    enum States { INIT, TEST_GAME_OVER, GET_NEXT_PIECE, FIND_BEST_MOVE, ANIMATE_MOVE, ANIMATE_GAME_OVER } state = INIT;

    TetrisAIGame(uint8_t width, uint8_t height, uint8_t nLookAhead, PieceData* piecesData, uint8_t nPieces):
        width(width),
        height(height),
        nLookAhead(nLookAhead),
        nPieces(nPieces),
        bag(nPieces, 1, nLookAhead),
        grid(width, height + 4),
        ai(),
        piecesData(piecesData)
    {
    }

    void nextPiece()
    {
        grid.cleanupFullLines();
        bag.queuePiece();
    }

    void findBestMove()
    {
        ai.findBestMove(grid.gridBW, &bag.piecesQueue);
    }

    bool animateFall(bool skip)
    {
        return animateFallOfPiece(&(bag.piecesQueue[0]), skip);
    }

    bool isGameOver()
    {
        //if there is something in the 4 lines of the hidden area the game is over
        return grid.gridBW.pixels[0] || grid.gridBW.pixels[1] || grid.gridBW.pixels[2] || grid.gridBW.pixels[3];
    }

    void poll()
    {
        switch (state)
        {
            case INIT:
                reset();
                state = TEST_GAME_OVER;
                break;
            case TEST_GAME_OVER:
                if (isGameOver())
                {
                    state = ANIMATE_GAME_OVER;
                }
                else
                {
                    state = GET_NEXT_PIECE;
                }
                break;
            case GET_NEXT_PIECE:
                nextPiece();
                state = FIND_BEST_MOVE;
                break;
            case FIND_BEST_MOVE:
                findBestMove();
                state = ANIMATE_MOVE;
                break;
            case ANIMATE_MOVE:
                if (!animateFall(false))
                {
                    state = TEST_GAME_OVER;
                }
                break;
            case ANIMATE_GAME_OVER:
                static auto curPixel = grid.pixels.size();
                grid.pixels[curPixel] = 254;

                if (curPixel == 0)
                {
                    state = INIT;
                    curPixel = grid.pixels.size();
                }
                curPixel--;
                break;
        }
    }

    void reset()
    {
        grid.width = width;
        grid.height = height + 4;
        grid.reset();
        bag.reset();
    }
};

#endif /* __TETRISAIGAME_H__ */
