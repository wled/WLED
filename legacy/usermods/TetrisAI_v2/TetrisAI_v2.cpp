#include "wled.h"
#include "FX.h"
#include "fcn_declare.h"

#include "tetrisaigame.h"
// By: muebau

typedef struct TetrisAI_data
{
  unsigned long lastTime = 0;
  TetrisAIGame tetris;
  uint8_t   intelligence;
  uint8_t   rotate;
  bool      showNext;
  bool      showBorder;
  uint8_t   colorOffset;
  uint8_t   colorInc;
  uint8_t   mistaceCountdown;
  uint16_t segcols;
  uint16_t segrows;
  uint16_t segOffsetX;
  uint16_t segOffsetY;
  uint16_t effectWidth;
  uint16_t effectHeight;
} tetrisai_data;

void drawGrid(TetrisAIGame* tetris, TetrisAI_data* tetrisai_data)
{
  SEGMENT.fill(SEGCOLOR(1));

  //GRID
  for (auto index_y = 4; index_y < tetris->grid.height; index_y++)
  {
    for (auto index_x = 0; index_x < tetris->grid.width; index_x++)
    {
      CRGB color;
      if (*tetris->grid.getPixel(index_x, index_y) == 0)
      {
        //BG color
        color = SEGCOLOR(1);
      }
      //game over animation
      else if(*tetris->grid.getPixel(index_x, index_y) == 254)
      {
        //use fg
        color = SEGCOLOR(0);
      }
      else
      {
        //spread the color over the whole palette
        uint8_t colorIndex = *tetris->grid.getPixel(index_x, index_y) * 32;
        colorIndex += tetrisai_data->colorOffset;
        color = ColorFromPalette(SEGPALETTE, colorIndex, 255, NOBLEND);
      }

      SEGMENT.setPixelColorXY(tetrisai_data->segOffsetX + index_x, tetrisai_data->segOffsetY + index_y - 4, color);
    }
  }
  tetrisai_data->colorOffset += tetrisai_data->colorInc;

  //NEXT PIECE AREA
  if (tetrisai_data->showNext)
  {
    //BORDER
    if (tetrisai_data->showBorder)
    {
      //draw a line 6 pixels from right with the border color
      for (auto index_y = 0; index_y < tetrisai_data->effectHeight; index_y++)
      {
        SEGMENT.setPixelColorXY(tetrisai_data->segOffsetX + tetrisai_data->effectWidth - 6, tetrisai_data->segOffsetY + index_y, SEGCOLOR(2));
      }
    }

    //NEXT PIECE
    int piecesOffsetX = tetrisai_data->effectWidth - 4;
    int piecesOffsetY = 1;
    for (uint8_t nextPieceIdx = 1; nextPieceIdx < tetris->nLookAhead; nextPieceIdx++)
    {
      uint8_t pieceNbrOffsetY = (nextPieceIdx - 1) * 5;

      Piece piece(tetris->bag.piecesQueue[nextPieceIdx]);

      for (uint8_t pieceY = 0; pieceY < piece.getRotation().height; pieceY++)
      {
        for (uint8_t pieceX = 0; pieceX < piece.getRotation().width; pieceX++)
        {
          if (piece.getPixel(pieceX, pieceY))
          {
            uint8_t colIdx = ((piece.pieceData->colorIndex * 32) + tetrisai_data->colorOffset);
            SEGMENT.setPixelColorXY(tetrisai_data->segOffsetX + piecesOffsetX + pieceX, tetrisai_data->segOffsetY + piecesOffsetY + pieceNbrOffsetY + pieceY, ColorFromPalette(SEGPALETTE, colIdx, 255, NOBLEND));
          }
        }
      }
    }
  }
}

////////////////////////////
//     2D Tetris AI       //
////////////////////////////
uint16_t mode_2DTetrisAI()
{
  if (!strip.isMatrix || !SEGENV.allocateData(sizeof(tetrisai_data)))
  {
    // not a 2D set-up
    SEGMENT.fill(SEGCOLOR(0));
    return 350;
  }
  TetrisAI_data* tetrisai_data = reinterpret_cast<TetrisAI_data*>(SEGENV.data);

  const uint16_t cols = SEGMENT.virtualWidth();
  const uint16_t rows = SEGMENT.virtualHeight();

  //range 0 - 1024ms => 1024/255 ~ 4
  uint16_t msDelayMove = 1024 - (4 * SEGMENT.speed);
  int16_t msDelayGameOver = msDelayMove / 4;

  //range 0 - 2 (not including current)
  uint8_t nLookAhead = SEGMENT.intensity ? (SEGMENT.intensity >> 7) + 2 : 1;
  //range 0 - 16
  tetrisai_data->colorInc = SEGMENT.custom2 >> 4;

  if (tetrisai_data->tetris.nLookAhead != nLookAhead
    || tetrisai_data->segcols != cols
    || tetrisai_data->segrows != rows
    || tetrisai_data->showNext != SEGMENT.check1
    || tetrisai_data->showBorder != SEGMENT.check2
  )
  {
    tetrisai_data->segcols = cols;
    tetrisai_data->segrows = rows;
    tetrisai_data->showNext = SEGMENT.check1;
    tetrisai_data->showBorder = SEGMENT.check2;

    //not more than 32 columns and 255 rows as this is the limit of this implementation
    uint8_t gridWidth = cols > 32 ? 32 : cols;
    uint8_t gridHeight = rows > 255 ? 255 : rows;

    tetrisai_data->effectWidth = 0;
    tetrisai_data->effectHeight = 0;

    // do we need space for the 'next' section?
    if (tetrisai_data->showNext)
    {
      //does it get to tight?
      if (gridWidth + 5 > cols)
      {
        // yes, so make the grid smaller
        // make space for the piece and one pixel of space
        gridWidth = (gridWidth - ((gridWidth + 5) - cols));
      }
      tetrisai_data->effectWidth += 5;

      // do we need space for a border?
      if (tetrisai_data->showBorder)
      {
        if (gridWidth + 5 + 1 > cols)
        {
          gridWidth -= 1;
        }
        tetrisai_data->effectWidth += 1;
      }
    }

    tetrisai_data->effectWidth += gridWidth;
    tetrisai_data->effectHeight += gridHeight;

    tetrisai_data->segOffsetX = cols > tetrisai_data->effectWidth ? ((cols - tetrisai_data->effectWidth) / 2) : 0;
    tetrisai_data->segOffsetY = rows > tetrisai_data->effectHeight ? ((rows - tetrisai_data->effectHeight) / 2) : 0;

    tetrisai_data->tetris = TetrisAIGame(gridWidth, gridHeight, nLookAhead, piecesData, numPieces);
    tetrisai_data->tetris.state = TetrisAIGame::States::INIT;
    SEGMENT.fill(SEGCOLOR(1));
  }

  if (tetrisai_data->intelligence != SEGMENT.custom1)
  {
    tetrisai_data->intelligence = SEGMENT.custom1;
    float dui = 0.2f - (0.2f * (tetrisai_data->intelligence / 255.0f));

    tetrisai_data->tetris.ai.aHeight = -0.510066f + dui;
    tetrisai_data->tetris.ai.fullLines = 0.760666f - dui;
    tetrisai_data->tetris.ai.holes = -0.35663f + dui;
    tetrisai_data->tetris.ai.bumpiness = -0.184483f + dui;
  }

  if (tetrisai_data->tetris.state == TetrisAIGame::ANIMATE_MOVE)
  {
    
    if (strip.now - tetrisai_data->lastTime > msDelayMove)
    {
      drawGrid(&tetrisai_data->tetris, tetrisai_data);
      tetrisai_data->lastTime = strip.now;
      tetrisai_data->tetris.poll();
    }
  }
  else if (tetrisai_data->tetris.state == TetrisAIGame::ANIMATE_GAME_OVER)
  {
    if (strip.now - tetrisai_data->lastTime > msDelayGameOver)
    {
      drawGrid(&tetrisai_data->tetris, tetrisai_data);
      tetrisai_data->lastTime = strip.now;
      tetrisai_data->tetris.poll();
    }
  }
  else if (tetrisai_data->tetris.state == TetrisAIGame::FIND_BEST_MOVE)
  {
    if (SEGMENT.check3)
    {
      if(tetrisai_data->mistaceCountdown == 0)
      {
        tetrisai_data->tetris.ai.findWorstMove = true;
        tetrisai_data->tetris.poll();
        tetrisai_data->tetris.ai.findWorstMove = false;
        tetrisai_data->mistaceCountdown = SEGMENT.custom3;
      }
      tetrisai_data->mistaceCountdown--;      
    }
    tetrisai_data->tetris.poll();
  }
  else
  {
    tetrisai_data->tetris.poll();
  }

  return FRAMETIME;
} // mode_2DTetrisAI()
static const char _data_FX_MODE_2DTETRISAI[] PROGMEM = "Tetris AI@!,Look ahead,Intelligence,Rotate color,Mistake free,Show next,Border,Mistakes;Game Over,!,Border;!;2;sx=127,ix=64,c1=255,c2=0,c3=31,o1=1,o2=1,o3=0,pal=11";

class TetrisAIUsermod : public Usermod
{

private:

public:
  void setup()
  {
    strip.addEffect(255, &mode_2DTetrisAI, _data_FX_MODE_2DTETRISAI);
  }

  void loop()
  {

  }

  uint16_t getId()
  {
    return USERMOD_ID_TETRISAI;
  }
};


static TetrisAIUsermod tetrisai_v2;
REGISTER_USERMOD(tetrisai_v2);