/******************************************************************************
  * @file           : tetrisbag.h
  * @brief          : the tetris implementation of a random piece generator
  ******************************************************************************
  * @attention
  *
  * Copyright (c) muebau 2022
  * All rights reserved.</center></h2>
  *
  ******************************************************************************
*/

#ifndef __TETRISBAG_H__
#define __TETRISBAG_H__

#include <stdint.h>
#include <vector>
#include <algorithm>

#include "tetrisbag.h"

class TetrisBag
{
private:
public:
    uint8_t nPieces;
    uint8_t nBagLength;
    uint8_t queueLength;
    uint8_t bagIdx;
    std::vector<uint8_t> bag;
    std::vector<Piece> piecesQueue;

    TetrisBag(uint8_t nPieces, uint8_t nBagLength, uint8_t queueLength):
        nPieces(nPieces),
        nBagLength(nBagLength),
        queueLength(queueLength),
        bag(nPieces * nBagLength),
        piecesQueue(queueLength)
    {
        init();
    }

    void init()
    {
        //will shuffle the bag at first use
        bagIdx = nPieces - 1;

        for (uint8_t bagIndex = 0; bagIndex < nPieces * nBagLength; bagIndex++)
        {
            bag[bagIndex] = bagIndex % nPieces;
        }

        //will init the queue
        for (uint8_t index = 0; index < piecesQueue.size(); index++)
        {
            queuePiece();
        }
    }

    void shuffleBag()
    {
        uint8_t temp;
        uint8_t swapIdx;
        for (int index = nPieces - 1; index > 0; index--)
        {
            //get candidate to swap
            swapIdx = rand() % index;

            //swap it!
            temp = bag[swapIdx];
            bag[swapIdx] = bag[index];
            bag[index] = temp;
        }
    }

    Piece getNextPiece()
    {
        bagIdx++;
        if (bagIdx >= nPieces)
        {
            shuffleBag();
            bagIdx = 0;
        }
        return Piece(bag[bagIdx]);
    }

    void queuePiece()
    {
        //move vector to left
        std::rotate(piecesQueue.begin(), piecesQueue.begin() + 1, piecesQueue.end());
        piecesQueue[piecesQueue.size() - 1] = getNextPiece();
    }

    void queuePiece(uint8_t idx)
    {
        //move vector to left
        std::rotate(piecesQueue.begin(), piecesQueue.begin() + 1, piecesQueue.end());
        piecesQueue[piecesQueue.size() - 1] = Piece(idx % nPieces);
    }

    void reset()
    {
        bag.clear();
        bag.resize(nPieces * nBagLength);
        piecesQueue.clear();
        piecesQueue.resize(queueLength);
        init();
    }
};

#endif /* __TETRISBAG_H__ */
