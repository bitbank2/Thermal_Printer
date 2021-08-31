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
  PRINTER_MTP2=0,
  PRINTER_MTP3,
  PRINTER_CAT,
  PRINTER_PERIPAGE,
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
// Feed the paper in scanline increments
//
void tpFeed(int iLines);
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
