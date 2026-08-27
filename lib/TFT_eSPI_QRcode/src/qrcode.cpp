#include <Arduino.h>
#include <cstdlib>
#include "qrcode.h"
#include "qrencode.h"

int offsetsX;
int offsetsY;
int screenwidth;
int screenheight;
int multiply = 2;

namespace {
constexpr size_t kEncoderWorkBufferSize = 600;
constexpr size_t kEncoderInputBufferSize = 260;

bool encodeFrame(const String &message)
{
  message.toCharArray((char *)strinbuf, kEncoderInputBufferSize);
  qrframe = (unsigned char *)malloc(kEncoderWorkBufferSize);
  if (!qrframe)
    return false;

  qrencode();
  return true;
}

void releaseFrame()
{
  free(qrframe);
  qrframe = 0;
}
} // namespace

QRcode::QRcode(tft_display *tft)
{
  this->tft = tft;
}

void QRcode::init()
{
  screenwidth = tft->width();
  screenheight = tft->height();
  int min = screenwidth;
  if (screenheight < screenwidth)
    min = screenheight;
  multiply = min / WD;
  offsetsX = (screenwidth - (WD * multiply)) / 2;
  offsetsY = (screenheight - (WD * multiply)) / 2;
}

void QRcode::render(int x, int y, int color)
{
  x = (x * multiply) + offsetsX;
  y = (y * multiply) + offsetsY;
  if (color == 1)
  {
    tft->drawPixel(x, y, TFT_BLACK);
    if (multiply > 1)
    {
      tft->fillRect(x, y, multiply, multiply, TFT_BLACK);
    }
  }
  else
  {
    tft->drawPixel(x, y, TFT_WHITE);
    if (multiply > 1)
    {
      tft->fillRect(x, y, multiply, multiply, TFT_WHITE);
    }
  }
}

bool QRcode::create(const String &message)
{
  // create QR code
  tft->fillScreen(TFT_WHITE);
  if (!encodeFrame(message))
    return false;

  // print QR Code
  for (byte x = 0; x < WD; x += 2)
  {
    for (byte y = 0; y < WD; y++)
    {
      if (QRBIT(x, y) && QRBIT((x + 1), y))
      {
        // black square on top of black square
        render(x, y, 1);
        render((x + 1), y, 1);
      }
      if (!QRBIT(x, y) && QRBIT((x + 1), y))
      {
        // white square on top of black square
        render(x, y, 0);
        render((x + 1), y, 1);
      }
      if (QRBIT(x, y) && !QRBIT((x + 1), y))
      {
        // black square on top of white square
        render(x, y, 1);
        render((x + 1), y, 0);
      }
      if (!QRBIT(x, y) && !QRBIT((x + 1), y))
      {
        // white square on top of white square
        render(x, y, 0);
        render((x + 1), y, 0);
      }
    }
  }
  releaseFrame();
  return true;
}

bool QRcode::encode(const String &message, uint8_t *packed, size_t capacity, uint8_t &matrixSizeOut)
{
  matrixSizeOut = 0;
  const size_t required = packedSize();
  if (!packed || capacity < required)
    return false;

  if (!encodeFrame(message))
    return false;

  memset(packed, 0, required);
  size_t bitIndex = 0;
  for (uint8_t y = 0; y < WD; ++y)
  {
    for (uint8_t x = 0; x < WD; ++x, ++bitIndex)
    {
      if (QRBIT(x, y))
        packed[bitIndex >> 3] |= (uint8_t)(0x80U >> (bitIndex & 7U));
    }
  }

  matrixSizeOut = WD;
  releaseFrame();
  return true;
}

uint8_t QRcode::matrixSize()
{
  return WD;
}

size_t QRcode::packedSize()
{
  return ((size_t)WD * WD + 7U) / 8U;
}
