//
// Thermal Printer Library
// written by Larry Bank
// Copyright (c) 2020 BitBank Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __THERMAL_PRINTER_H__
#define __THERMAL_PRINTER_H__

#define FONT_SMALL 0
#define FONT_LARGE 1
#define FONT_12x24 0
#define FONT_9x17  1

enum {
  ALIGN_LEFT=0x30,
  ALIGN_CENTER=0x31,
  ALIGN_RIGHT=0x32
};

enum {
  BARCODE_TEXT_NONE=0x30,
  BARCODE_TEXT_ABOVE=0x31,
  BARCODE_TEXT_BELOW=0x32,
  BARCODE_TEXT_BOTH=0x33
};

enum {
  BARCODE_UPCA=0,
  BARCODE_UPCE=0x01,
  BARCODE_EAN13=0x02,
  BARCODE_EAN8=0x03,
  BARCODE_CODE39=0x04,
  BARCODE_ITF=0x05,
  BARCODE_CODABAR=0x06,
  BARCODE_CODE93=0x48,
  BARCODE_CODE128=0x49,
  BARCODE_GS1_128=0x50,
  BARCODE_GS1_DATABAR_OMNI=0x51,
  BARCODE_GS1_DATABAR_TRUNCATED=0x52,
  BARCODE_GS1_DATABAR_LIMITED=0x53,
  BARCODE_GS1_DATABAR_EXPANDED=0x54,
  BARCODE_CODE128_AUTO=0x55
};

enum {
  PRINTER_MTP2=0,
  PRINTER_MTP3,
  PRINTER_CAT,
  PRINTER_PERIPAGEPLUS,
  PRINTER_PERIPAGE,
  PRINTER_FOMEMO,
  PRINTER_COUNT
};

// Proportional font data taken from Adafruit_GFX library
/// Font data stored PER GLYPH
#if !defined( _ADAFRUIT_GFX_H ) && !defined( _GFXFONT_H_ )
#define _GFXFONT_H_
typedef struct {
  uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
  uint8_t width;         ///< Bitmap dimensions in pixels
  uint8_t height;        ///< Bitmap dimensions in pixels
  uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
  int8_t xOffset;        ///< X dist from cursor pos to UL corner
  int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

/// Data stored for FONT AS A WHOLE
typedef struct {
  uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
  GFXglyph *glyph;  ///< Glyph array
  uint8_t first;    ///< ASCII extents (first char)
  uint8_t last;     ///< ASCII extents (last char)
  uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;
#endif // _ADAFRUIT_GFX_H
//
// Return the printer width in pixels
// The printer needs to be connected to get this info
//
int tpGetWidth(void);
//
// Returns the BLE name of the connected printer
// as a zero terminated c-string
// Returns NULL if not connected
//
char *tpGetName(void);

// Feed the paper in scanline increments
//
void tpFeed(int iLines);
//
// tpSetEnergy Set Energy - switch between eco and nice images :) 
//
void tpSetEnergy(int iEnergy);
//
// Return the measurements of a rectangle surrounding the given text string
// rendered in the given font
//
void tpGetStringBox(GFXfont *pFont, char *szMsg, int *width, int *top, int *bottom);
//
// Draw a string of characters in a custom font into the gfx buffer
//
int tpDrawCustomText(GFXfont *pFont, int x, int y, char *szMsg);
//
// Print a string of characters in a custom font to the connected printer
//
int tpPrintCustomText(GFXfont *pFont, int x, char *szMsg);

//
// Send raw data to printer
//
void tpWriteRawData(uint8_t *pData, int iLen);

// Select one of 2 available text fonts along with attributes
// FONT_12x24 or FONT_9x17
// Each option is either 0 (disabled) or 1 (enabled)
// These are the text attributes offered by the standard printer spec
//
void tpSetFont(int iFont, int iUnderline, int iDoubleWide, int iDoubleTall, int iEmphasized);

//
// Provide a back buffer for your printer graphics
// This allows you to manage the RAM used on
// embedded platforms like Arduinos
// The memory is laid out horizontally (384 pixels across = 48 bytes)
// So a 384x384 buffer would need to be 48x384 = 18432 bytes
//
void tpSetBackBuffer(uint8_t *pBuffer, int iWidth, int iHeight);
//
// Print plain text immediately
//
// Pass a C-string (zero terminated char array)
// If the text doesn't reach the end of the line
// it will not be printed until the printer receives
// a CR (carriage return) or new text which forces
// it to wrap around
//
int tpPrint(char *pString);
//
// Print plain text immediately
// Pass a C-string (zero terminated char array)
// A CR (carriage return) will be added at the end
// to cause the printer to print the text and advance
// the paper one line
//
int tpPrintLine(char *pString);

#define MODE_WITH_RESPONSE 1
#define MODE_WITHOUT_RESPONSE 0
//
// Set the BLE write mode
// MODE_WITH_RESPONSE asks the receiver to ack each packet
// it will be slower, but might be necessary to successfully transmit
// every packet. The default is to wait for a response for each write
//
void tpSetWriteMode(uint8_t bWriteMode);
//
// Draw text into the graphics buffer
//
int tpDrawText(int x, int y, char *pString, int iFontSize, int bInvert);
//
// Load a 1-bpp Windows bitmap into the back buffer
// Pass the pointer to the beginning of the BMP file
// along with a x and y offset (upper left corner)
//
int tpLoadBMP(uint8_t *pBMP, int bInvert, int iXOffset, int iYOffset);

//
// Fill the frame buffer with a byte pattern
// e.g. all off (0x00) or all on (0xff)
//
void tpFill(unsigned char ucData);
//
// Set (or clear) an individual pixel
//
int tpSetPixel(int x, int y, uint8_t ucColor);
//
// Send the graphics to the printer (must be connected over BLE first)
//
void tpPrintBuffer(void);
//
// Same as tpPrintBuffer, but output will be rotated by 90 degrees
//
void tpPrintBufferSide(void);
//
// Draw a line between 2 points
//
void tpDrawLine(int x1, int y1, int x2, int y2, uint8_t ucColor);
//
// Scan for compatible printers
// returns true if found
// and stores the printer address internally
// for use with the tpConnect() function
// szName is the printer device name to match
// iSeconds = how many seconds to scan for devices
//
int tpScan(const char *szName, int iSeconds);
//
// connect to a printer with a macaddress
// returns 1 if successful, 0 for failure
//
int tpConnect(const char *szMacAddress);
//
// Set the text and barcode alignment
// Use ALIGN_LEFT, ALIGN_CENTER or ALIGN_RIGHT
//
void tpAlign(uint8_t ucAlign);
//
// Print a 2D (QR) barcode
//
void tpQRCode(char *szText);
//
// Print a 2D (QR) barcode
// iSize = starting from 1 / standard is 3
//
void tpQRCode(char *szText, int iSize);
//
// Print a 1D barcode
//
void tp1DBarcode(int iType, int iHeight, char *szData, int iTextPos);
//
// Parameterless version
// finds supported printers automatically
//
int tpScan(void);
//
// After a successful scan, connect to the printer
// returns 1 if successful, 0 for failure
//
int tpConnect(void);
void tpDisconnect(void);
int tpIsConnected(void);
#endif // __THERMAL_PRINTER_H__
