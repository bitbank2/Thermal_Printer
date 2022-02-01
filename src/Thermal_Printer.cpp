//
// Thermal_Printer Arduino library
//
// Copyright (c) 2020 BitBank Software, Inc.
// Written by Larry Bank (bitbank@pobox.com)
// Project started 1/6/2020
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
#include <Arduino.h>
// uncomment this line to see debug info on the serial monitor
//#define DEBUG_OUTPUT

// Two sets of code - one for ESP32
#ifdef HAL_ESP32_HAL_H_
#include <BLEDevice.h>
#endif

#ifdef ARDUINO_ARDUINO_NANO33BLE
#include <ArduinoBLE.h>
#endif

#ifdef ARDUINO_NRF52_ADAFRUIT
#include <bluefruit.h>
#endif

#include "Thermal_Printer.h"

static char szPrinterName[32];
volatile uint8_t ucPrinterType; // one of PRINTER_MTP2, PRINTER_CAT, etc
static int bb_width, bb_height; // back buffer width and height in pixels
static int tp_wrap, bb_pitch;
static int iCursorX = 0;
static int iCursorY = 0;
static uint8_t bWithResponse = 0; // default to not wait for a response
static uint8_t *pBackBuffer = NULL;
static uint8_t bConnected = 0;
static void tpWriteData(uint8_t *pData, int iLen);
extern "C" {
extern unsigned char ucFont[], ucBigFont[];
};
static uint8_t tpFindPrinterName(char *szName);
static void tpPreGraphics(int iWidth, int iHeight);
static void tpPostGraphics(void);
static void tpSendScanline(uint8_t *pSrc, int iLen);

// Names and types of supported printers
const char *szBLENames[] = {(char *)"PT-210", (char *)"MTP-2", (char *)"MPT-II", (char *)"MTP-3",(char *)"MTP-3F",(char *)"GT01",(char *)"GT02",(char *)"GB01",(char *)"GB02", (char *)"YHK-A133", (char *)"PeriPage+",(char *)"PeriPage_","T02",NULL};
const uint8_t ucBLETypes[] = {PRINTER_MTP2, PRINTER_MTP2, PRINTER_MTP2, PRINTER_MTP3, PRINTER_MTP3, PRINTER_CAT, PRINTER_CAT, PRINTER_CAT, PRINTER_CAT, PRINTER_CAT, PRINTER_PERIPAGEPLUS, PRINTER_PERIPAGE, PRINTER_FOMEMO};
const int iPrinterWidth[] = {384, 576, 384, 576, 384, 384};
const uint8_t PeriPrefix[] = {0x10,0xff,0xfe,0x01};
const char *szServiceNames[] = {(char *)"18f0", (char *)"18f0", (char *)"ae30", (char *)"ff00",(char *)"ff00", (char *)"ff00"}; // 16-bit UUID of the printer services we want
const char *szCharNames[] = {(char *)"2af1", (char *)"2af1", (char *)"ae01",(char *)"ff02", (char *)"ff02", (char *)"ff02"}; // 16-bit UUID of printer data characteristics we want
// Command sequences for the 'cat' printer
const int8_t getDevState[] = {81, 120, -93, 0, 1, 0, 0, 0, -1};
const int8_t setQ200DPI[] = {81, 120, -92, 0, 1, 0, 50, -98, -1};
const int8_t latticeStart[] = {81, 120, -90, 0, 11, 0, -86, 85, 23,
                        56, 68, 95, 95, 95, 68, 56, 44, -95, -1};
const int8_t latticeEnd[] = {81, 120, -90, 0, 11, 0, -86, 85, 23, 0, 0, 0, 0, 0, 0, 0, 23, 17, -1};
const uint8_t paperFeed[] = {0x51, 0x78, 0xa1, 0, 2, 0, 30, 90, 0xff, 0xff};
int i;
const int8_t setPaper[] = {81, 120, -95, 0, 2, 0, 48, 0, -7, -1};
const int8_t printImage[] = {81, 120, -66, 0, 1, 0, 0, 0, -1};
const int8_t printText[] = {81, 120, -66, 0, 1, 0, 1, 7, -1};
const int8_t cChecksumTable[] = {0, 7, 14, 9, 28, 27, 18, 21, 56, 63, 54, 49, 36, 35, 42, 45, 112, 119, 126, 121, 108, 107, 98, 101, 72, 79, 70, 65, 84, 83, 90, 93, -32, -25, -18, -23, -4, -5, -14, -11, -40, -33, -42, -47, -60, -61, -54, -51, -112, -105, -98, -103, -116, -117, -126, -123, -88, -81, -90, -95, -76, -77, -70, -67, -57, -64, -55, -50, -37, -36, -43, -46, -1, -8, -15, -10, -29, -28, -19, -22, -73, -80, -71, -66, -85, -84, -91, -94, -113, -120, -127, -122, -109, -108, -99, -102, 39, 32, 41, 46, 59, 60, 53, 50, 31, 24, 17, 22, 3, 4, 13, 10, 87, 80, 89, 94, 75, 76, 69, 66, 111, 104, 97, 102, 115, 116,
                     125, 122, -119, -114, -121, -128, -107, -110, -101, -100, -79, -74, -65, -72, -83, -86, -93, -92, -7, -2, -9, -16, -27, -30, -21, -20, -63, -58, -49, -56, -35, -38, -45, -44, 105, 110, 103, 96, 117, 114, 123, 124, 81, 86, 95, 88, 77, 74, 67, 68, 25, 30, 23, 16, 5, 2, 11, 12, 33, 38, 47, 40, 61, 58, 51, 52, 78, 73, 64, 71, 82, 85, 92, 91, 118, 113, 120, 127, 106, 109, 100, 99, 62, 57, 48, 55, 34, 37, 44, 43, 6, 1, 8, 15, 26, 29, 20, 19, -82, -87, -96, -89, -78, -75, -68, -69, -106, -111, -104, -97, -118, -115, -124, -125, -34, -39, -48, -41, -62, -59, -52, -53, -26, -31, -24, -17, -6, -3, -12, -13};

/* Table of byte flip values to mirror-image incoming CCITT data */
const unsigned char ucMirror[256]=
     {0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
      8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
      4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
      12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
      2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
      10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
      6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
      14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
      1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
      9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
      5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
      13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
      3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
      11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
      7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
      15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};

#ifdef ARDUINO_NRF52_ADAFRUIT
// Bluetooth support for Adafruit nrf52 boards
//#define myServiceUUID 0xFEA0
//#define myDataUUID 0xFEA1
static ble_gap_evt_adv_report_t the_report;
static uint16_t the_conn_handle;
static int bNRFFound;
//BLEClientCharacteristic myDataChar(myDataUUID);
//BLEClientService myService(myServiceUUID);
BLEClientService myService; //(0x18f0);
BLEClientCharacteristic myDataChar; //(0x2af1);

/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
static void connect_callback(uint16_t conn_handle)
{
#ifdef DEBUG_OUTPUT
  Serial.println("Connected!");
  Serial.print("Discovering Service ... ");
#endif
    the_conn_handle = conn_handle;
  // If Service UUID is not found, disconnect and return
  if ( !myService.discover(conn_handle) )
  {
#ifdef DEBUG_OUTPUT
    Serial.println("Didn't find our service, disconnecting...");
#endif
      // disconect since we couldn't find our service
    Bluefruit.disconnect(conn_handle);
    return;
  }
 
  // Once FEA0 service is found, we continue to discover its characteristics
  if ( !myDataChar.discover() )
  {
    // Data char is mandatory, if it is not found (valid), then disconnect
#ifdef DEBUG_OUTPUT
    Serial.println("Data characteristic is mandatory but not found");
#endif
    Bluefruit.disconnect(conn_handle);
    return;
  }
    bConnected = 1; // success!
} /* connect_callback() */
/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason
 */
static void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
    bConnected = 0;
//  Serial.println("Disconnected");
} /* disconnect_callback() */

void ParseDeviceName(uint8_t *s, char *szOut)
{
  int bDone = 0, i = 0;
  int iLen;
  szOut[0] = 0; // assume we won't find a device name
  while (i < 64 && !bDone) {
    iLen = s[i++];
    if (s[i] == 0x09) { // device name
      i++; // skip type
      memcpy(szOut, &s[i], iLen-1);
      szOut[iLen-1] = 0; // zero terminate
      bDone = 1;
    } else {
      i += iLen; // skip this data field
    }
  } 
} /* ParseDeviceName() */

static void scan_callback(ble_gap_evt_adv_report_t* report)
{
char szTemp[32];

   ParseDeviceName(report->data.p_data, szTemp);

//    Serial.printf("found something %s\n", report->data.p_data);
//  if (Bluefruit.Scanner.checkReportForUuid(report, myServiceUUID))
//    char *name = (char *)report->data.p_data;
//    int i;
//    for (i=0; i<report->data.len; i++)
//       if (name[i] == szPrinterName[0]) break; // "parse" for the name in the adv data
//  if (name && memcmp(&name[i], szPrinterName, strlen(szPrinterName)) == 0)
#ifdef DEBUG_OUTPUT
       Serial.println("Found something!");
       Serial.print("device name = ");
       Serial.println(szTemp);
#endif
    ucPrinterType = tpFindPrinterName(szTemp);
    if (ucPrinterType > PRINTER_COUNT) { // nothing supported found
       Bluefruit.Scanner.resume();
    } else { // we can stop scanning
      bNRFFound = 1;
      ParseDeviceName(report->data.p_data, szPrinterName); // allow query by user
      Bluefruit.Scanner.stop();
      memcpy(&the_report, report, sizeof(ble_gap_evt_adv_report_t));
#ifdef DEBUG_OUTPUT
      Serial.println("Found a supported printer!");
#endif
    }
//  else // keep looking
//  {
    // For Softdevice v6: after received a report, scanner will be paused
    // We need to call Scanner resume() to continue scanning
//    Bluefruit.Scanner.resume();
//  }
} /* scan_callback() */

static void notify_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len)
{
} /* notify_callback() */

#endif // Adafruit nrf52

#ifdef HAL_ESP32_HAL_H_
static BLEUUID SERVICE_UUID0("49535343-FE7D-4AE5-8FA9-9FAFD205E455");
static BLEUUID CHAR_UUID_DATA0 ("49535343-8841-43F4-A8D4-ECBE34729BB3");
//static BLEUUID SERVICE_UUID1("0000AE30-0000-1000-8000-00805F9B34FB"); //Service
//static BLEUUID CHAR_UUID_DATA1("0000AE01-0000-1000-8000-00805F9B34FB"); // data characteristic
static BLEUUID SERVICE_UUID1(BLEUUID ((uint16_t)0xae30));
static BLEUUID CHAR_UUID_DATA1(BLEUUID((uint16_t)0xae01));
static BLEUUID SERVICE_UUID2(BLEUUID ((uint16_t)0xff00));
static BLEUUID CHAR_UUID_DATA2(BLEUUID((uint16_t)0xff02));

static String Scanned_BLE_Address;
static BLEScanResults foundDevices;
static BLEAddress *Server_BLE_Address;
static BLERemoteCharacteristic* pRemoteCharacteristicData;
static BLEScan *pBLEScan;
static BLEClient* pClient;
static char Scanned_BLE_Name[32];
#endif

#ifdef ARDUINO_ARDUINO_NANO33BLE
static BLEDevice peripheral;
static BLEService prtService;
static BLECharacteristic pRemoteCharacteristicData;
#endif

void tpSetWriteMode(uint8_t bWriteMode)
{
   bWithResponse = bWriteMode;
} /* tpSetWriteMode() */

#ifdef HAL_ESP32_HAL_H_
// Called for each device found during a BLE scan by the client
class tpAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      int iLen = strlen(szPrinterName);
#ifdef DEBUG_OUTPUT
      Serial.printf("Scan Result: %s \n", advertisedDevice.toString().c_str());
#endif
      if (iLen > 0 && memcmp(advertisedDevice.getName().c_str(), szPrinterName, iLen) == 0)
      { // this is what we want
        Server_BLE_Address = new BLEAddress(advertisedDevice.getAddress());
        Scanned_BLE_Address = Server_BLE_Address->toString().c_str();
        strcpy(Scanned_BLE_Name, advertisedDevice.getName().c_str());
#ifdef DEBUG_OUTPUT
        Serial.println("A match!");
        Serial.println((char *)Scanned_BLE_Address.c_str());
        Serial.println(Scanned_BLE_Name);
#endif
      } else if (iLen == 0) { // check for supported printers
        char szName[32];
        uint8_t ucType;
        strcpy(szName, advertisedDevice.getName().c_str());
        ucType = tpFindPrinterName(szName);
        if (ucType < PRINTER_COUNT) { // found a valid one!
            Server_BLE_Address = new BLEAddress(advertisedDevice.getAddress());
            Scanned_BLE_Address = Server_BLE_Address->toString().c_str();
            ucPrinterType = ucType;
            strcpy(Scanned_BLE_Name, advertisedDevice.getName().c_str());
            strcpy(szPrinterName, Scanned_BLE_Name); // allow user to query this
#ifdef DEBUG_OUTPUT
            Serial.print("A match! - ");
            Serial.print((char *)Scanned_BLE_Address.c_str());
            Serial.print(" - ");
            Serial.println(Scanned_BLE_Name);
#endif
        }
      } // if auto-detecting printers
    }
}; // class tpAdvertisedDeviceCallbacks
#endif

// Provide a back buffer for your printer graphics
// This allows you to manage the RAM used on
// embedded platforms like Arduinos
// The memory is laid out horizontally (384/576 pixels across = 48/72 bytes)
// So a 384x384 buffer would need to be 48x384 = 18432 bytes
//
void tpSetBackBuffer(uint8_t *pBuffer, int iWidth, int iHeight)
{
  pBackBuffer = pBuffer;
  bb_width = iWidth;
  bb_height = iHeight;
  bb_pitch = (iWidth + 7) >> 3;
} /* tpSetBackBuffer() */

//
// Fill the frame buffer with a byte pattern
// e.g. all off (0x00) or all on (0xff)
//
void tpFill(unsigned char ucData)
{
  if (pBackBuffer != NULL)
    memset(pBackBuffer, ucData, bb_pitch * bb_height);
} /* tpFill() */
//
// Turn text wrap on or off for the oldWriteString() function
//
void tpSetTextWrap(int bWrap)
{
  tp_wrap = bWrap;
} /* tpSetTextWrap() */
//
// Invert font data
//
static void InvertBytes(uint8_t *pData, uint8_t bLen)
{
uint8_t i;
   for (i=0; i<bLen; i++)
   {
      *pData = ~(*pData);
      pData++;
   }
} /* InvertBytes() */
//
// Return the measurements of a rectangle surrounding the given text string
// rendered in the given font
//
void tpGetStringBox(GFXfont *pFont, char *szMsg, int *width, int *top, int *bottom)
{
int cx = 0;
int c, i = 0;
GFXglyph *pGlyph;
int miny, maxy;

   if (width == NULL || top == NULL || bottom == NULL || pFont == NULL || szMsg == NULL) return; // bad pointers
   miny = 100; maxy = 0;
   while (szMsg[i]) {
      c = szMsg[i++];
      if (c < pFont->first || c > pFont->last) // undefined character
         continue; // skip it
      c -= pFont->first; // first char of font defined
      pGlyph = &pFont->glyph[c];
      cx += pGlyph->xAdvance;
      if (pGlyph->yOffset < miny) miny = pGlyph->yOffset;
      if (pGlyph->height+pGlyph->yOffset > maxy) maxy = pGlyph->height+pGlyph->yOffset;
   }
   *width = cx;
   *top = miny;
   *bottom = maxy;
} /* tpGetStringBox() */

//
// Draw a string of characters in a custom font into the gfx buffer
//
int tpDrawCustomText(GFXfont *pFont, int x, int y, char *szMsg)
{
int i, end_y, dx, dy, tx, ty, c, iBitOff;
uint8_t *s, *d, bits, ucMask, ucClr, uc;
GFXglyph glyph, *pGlyph;

   if (pBackBuffer == NULL || pFont == NULL || x < 0 || y > bb_height)
      return -1;
   pGlyph = &glyph;

   i = 0;
   while (szMsg[i] && x < bb_width)
   {
      c = szMsg[i++];
      if (c < pFont->first || c > pFont->last) // undefined character
         continue; // skip it
      c -= pFont->first; // first char of font defined
      memcpy_P(&glyph, &pFont->glyph[c], sizeof(glyph));
      dx = x + pGlyph->xOffset; // offset from character UL to start drawing
      dy = y + pGlyph->yOffset;
      s = pFont->bitmap + pGlyph->bitmapOffset; // start of bitmap data
      // Bitmap drawing loop. Image is MSB first and each pixel is packed next
      // to the next (continuing on to the next character line)
      iBitOff = 0; // bitmap offset (in bits)
      bits = uc = 0; // bits left in this font byte
      end_y = dy + pGlyph->height;
      if (dy < 0) { // skip these lines
          iBitOff += (pGlyph->width * (-dy));
          dy = 0;
      }
      for (ty=dy; ty<=end_y && ty < bb_height; ty++) {
         d = &pBackBuffer[ty * bb_pitch]; // internal buffer dest
         for (tx=0; tx<pGlyph->width; tx++) {
            if (uc == 0) { // need to read more font data
               tx += bits; // skip any remaining 0 bits
               uc = pgm_read_byte(&s[iBitOff>>3]); // get more font bitmap data
               bits = 8 - (iBitOff & 7); // we might not be on a byte boundary
               iBitOff += bits; // because of a clipped line
               uc <<= (8-bits);
               if (tx >= pGlyph->width) {
                  while(tx >= pGlyph->width) { // rolls into next line(s)
                     tx -= pGlyph->width;
                     ty++;
                  }
                  if (ty >= end_y) { // we're past the end
                     tx = pGlyph->width;
                     continue; // exit this character cleanly
                  }
                  d = &pBackBuffer[ty * bb_pitch];
               }
            } // if we ran out of bits
            if (uc & 0x80) { // set pixel
               ucMask = 0x80 >> ((dx+tx) & 7);
               d[(dx+tx)>>3] |= ucMask;
            }
            bits--; // next bit
            uc <<= 1;
         } // for x
      } // for y
      x += pGlyph->xAdvance; // width of this character
   } // while drawing characters
   return 0;
} /* tpDrawCustomText() */
//
// Print a string of characters in a custom font to the connected printer
//
int tpPrintCustomText(GFXfont *pFont, int startx, char *szMsg)
{
int i, x, y, end_y, dx, dy, tx, ty, c, iBitOff;
int maxy, miny, height;
uint8_t *s, *d, bits, ucMask, ucClr, uc;
GFXglyph glyph, *pGlyph;
uint8_t ucTemp[80]; // max width of 1 scan line (576 pixels)
int iPrintWidth = iPrinterWidth[ucPrinterType];

   if (!bConnected)
      return -1;
   if (pFont == NULL || x < 0)
      return -1;
   pGlyph = &glyph;

   // Get the size of the rectangle enclosing the text
//   tpGetStringBox(pFont, szMsg, &tx, &miny, &maxy);
//   height = (maxy - miny) + 1;

   tpPreGraphics(iPrintWidth, pFont->yAdvance);
   miny = 0 - (pFont->yAdvance * 2)/3; // 2/3 of char is above the baseline
   maxy = pFont->yAdvance + miny;
   for (y=miny; y<=maxy; y++)
   {
     i = 0;
     x = startx;
     memset(ucTemp, 0, sizeof(ucTemp));
     while (szMsg[i] && x < iPrintWidth)
     {
       c = szMsg[i++];
       if (c < pFont->first || c > pFont->last) // undefined character
         continue; // skip it
       c -= pFont->first; // first char of font defined
       memcpy_P(&glyph, &pFont->glyph[c], sizeof(glyph));
       dx = x + pGlyph->xOffset; // offset from character UL to start drawing
       dy = /*y +*/ pGlyph->yOffset;
       s = pFont->bitmap + pGlyph->bitmapOffset; // start of bitmap data
       // Bitmap drawing loop. Image is MSB first and each pixel is packed next
       // to the next (continuing on to the next character line)
       iBitOff = 0; // bitmap offset (in bits)
       bits = uc = 0; // bits left in this font byte
       end_y = dy + pGlyph->height;
       for (ty=dy; ty<=end_y; ty++) {
         for (tx=0; tx<pGlyph->width; tx++) {
            if (uc == 0) { // need to read more font data
               tx += bits; // skip any remaining 0 bits
               uc = pgm_read_byte(&s[iBitOff>>3]); // get more font bitmap data
               bits = 8 - (iBitOff & 7); // we might not be on a byte boundary
               iBitOff += bits; // because of a clipped line
               uc <<= (8-bits);
               if (tx >= pGlyph->width) {
                  while(tx >= pGlyph->width) { // rolls into next line(s)
                     tx -= pGlyph->width;
                     ty++;
                  }
                  if (ty >= end_y) { // we're past the end
                     tx = pGlyph->width;
                     continue; // exit this character cleanly
                  }
               }
            } // if we ran out of bits
            if (uc & 0x80 && ty == y) { // set pixel if we're drawing this line
               ucMask = 0x80 >> ((dx+tx) & 7);
               ucTemp[(dx+tx)>>3] |= ucMask;
            }
            bits--; // next bit
            uc <<= 1;
         } // for tx
      } // for ty
      x += pGlyph->xAdvance; // width of this character
    } // while drawing characters
    tpSendScanline(ucTemp, (iPrintWidth+7)/8); // send to printer 
  } // for each line of output
  tpPostGraphics();
  return 0;
} /* tpPrintCustomText() */
//
// Draw text into the graphics buffer
//
int tpDrawText(int x, int y, char *szMsg, int iFontSize, int bInvert)
{
int i, ty, iFontOff;
unsigned char c, *s, *d, ucTemp[64];

    if (x == -1 || y == -1) // use the cursor position
    {
      x = iCursorX; y = iCursorY;
    }
    else
    {
      iCursorX = x; iCursorY = y; // set the new cursor position
    }
    if (iCursorX >= bb_width || iCursorY >= bb_height-7)
       return -1; // can't draw off the display

    if (iFontSize == FONT_SMALL) // 8x8 font
    {
       i = 0;
       while (iCursorX < bb_width && szMsg[i] != 0 && iCursorY < bb_height)
       {
          c = (unsigned char)szMsg[i];
          iFontOff = (int)(c-32) * 8;
          memcpy(ucTemp, &ucFont[iFontOff], 8);
          if (bInvert) InvertBytes(ucTemp, 8);
          d = &pBackBuffer[(iCursorY * bb_pitch) + iCursorX/8];
          for (ty=0; ty<8; ty++)
          {
             d[0] = ucTemp[ty];
             d += bb_pitch;
          }
          iCursorX += 8;
          if (iCursorX >= bb_width && tp_wrap) // word wrap enabled?
          {
             iCursorX = 0; // start at the beginning of the next line
             iCursorY +=8;
          }
       i++;
       } // while
    return 0;
    } // 8x8
    else if (iFontSize == FONT_LARGE) // 16x32 font
    {
      i = 0;
      while (iCursorX < bb_width && iCursorY < bb_height-31 && szMsg[i] != 0)
      {
          s = (unsigned char *)&ucBigFont[(unsigned char)(szMsg[i]-32)*64];
          memcpy(ucTemp, s, 64);
          if (bInvert) InvertBytes(ucTemp, 64);
          d = &pBackBuffer[(iCursorY * bb_pitch) + iCursorX/8];
          for (ty=0; ty<32; ty++)
          {
             d[0] = s[0];
             d[1] = s[1];
             s += 2; d += bb_pitch;
          }
          iCursorX += 16;
          if (iCursorX >= bb_width && tp_wrap) // word wrap enabled?
          {
             iCursorX = 0; // start at the beginning of the next line
             iCursorY += 32;
          }
          i++;
       } // while
       return 0;
    } // 16x32
   return -1;
} /* tpDrawText() */
//
// Set (or clear) an individual pixel
//
int tpSetPixel(int x, int y, uint8_t ucColor)
{
uint8_t *d, mask;

  if (pBackBuffer == NULL)
     return -1;
  d = &pBackBuffer[(bb_pitch * y) + (x >> 3)];
  mask = 0x80 >> (x & 7);
  if (ucColor)
     d[0] |= mask;
  else
     d[0] &= ~mask;  
  return 0;
} /* tpSetPixel() */
//
// Load a 1-bpp Windows bitmap into the back buffer
// Pass the pointer to the beginning of the BMP file
// along with a x and y offset (upper left corner)
//
int tpLoadBMP(uint8_t *pBMP, int bInvert, int iXOffset, int iYOffset)
{
int16_t i16;
int iOffBits; // offset to bitmap data
int iPitch;
int16_t cx, cy, x, y;
uint8_t *d, *s, pix;
uint8_t srcmask, dstmask;
uint8_t bFlipped = false;

  i16 = pBMP[0] | (pBMP[1] << 8);
  if (i16 != 0x4d42) // must start with 'BM'
     return -1; // not a BMP file
  if (iXOffset < 0 || iYOffset < 0)
     return -1;
  cx = pBMP[18] + (pBMP[19] << 8);
  cy = pBMP[22] + (pBMP[23] << 8);
  if (cy > 0) // BMP is flipped vertically (typical)
     bFlipped = true;
  if (cx + iXOffset > bb_width || cy + iYOffset > bb_height) // too big
     return -1;
  i16 = pBMP[28] + (pBMP[29] << 8);
  if (i16 != 1) // must be 1 bit per pixel
     return -1;
  iOffBits = pBMP[10] + (pBMP[11] << 8);
  iPitch = (cx + 7) >> 3; // byte width
  iPitch = (iPitch + 3) & 0xfffc; // must be a multiple of DWORDS

  if (bFlipped)
  {
     iOffBits += ((cy-1) * iPitch); // start from bottom
     iPitch = -iPitch;
  }
  else
  {
     cy = -cy;
  }

// Send it to the gfx buffer
     for (y=0; y<cy; y++)
     {
         s = &pBMP[iOffBits + (y * iPitch)]; // source line
         d = &pBackBuffer[((iYOffset+y) * bb_pitch) + iXOffset/8];
         srcmask = 0x80; dstmask = 0x80 >> (iXOffset & 7);
         pix = *s++;
         if (bInvert) pix = ~pix;
         for (x=0; x<cx; x++) // do it a bit at a time
         {
           if (pix & srcmask)
              *d |= dstmask;
           else
              *d &= ~dstmask;
           srcmask >>= 1;
           if (srcmask == 0) // next pixel
           {
              srcmask = 0x80;
              pix = *s++;
              if (bInvert) pix = ~pix;
           }
           dstmask >>= 1;
           if (dstmask == 0)
           {
              dstmask = 0x80;
              d++;
           }
         } // for x
  } // for y
  return 0;
} /* tpLoadBMP() */
//
// Connection status
// true = connected, false = disconnected
//
int tpIsConnected(void)
{
  if (bConnected == 1) {
     // we are/were connected, check...
#ifdef HAL_ESP32_HAL_H_
     if (pClient && pClient->isConnected())
        return 1;
     bConnected = 0; // change status to disconnected
#endif // ESP32
  }
  return 0; // not connected
} /* tpIsConnected() */
//
// After a successful scan, connect to the printer
// returns 1 if successful, 0 for failure
//
int tpConnect(void)
{
#ifdef HAL_ESP32_HAL_H_
    pClient  = BLEDevice::createClient();
#ifdef DEBUG_OUTPUT
    Serial.printf(" - Created client, connecting to %s\n", Scanned_BLE_Address.c_str());
#endif
    // Connect to the BLE Server.
    pClient->connect(*Server_BLE_Address);
Serial.println("Came back from connect");
    if (!pClient->isConnected())
    {
      Serial.println("Connect failed");
      return false;
    }
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = NULL;
    if (ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3)
       pRemoteService = pClient->getService(SERVICE_UUID0);
    else if (ucPrinterType == PRINTER_CAT)
       pRemoteService = pClient->getService(SERVICE_UUID1);
    else if (ucPrinterType == PRINTER_FOMEMO || ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS)
       pRemoteService = pClient->getService(SERVICE_UUID2);
    if (pRemoteService != NULL)
    {
#ifdef DEBUG_OUTPUT
      Serial.println(" - Found our service");
#endif
      if (pClient->isConnected())
      {
        pRemoteCharacteristicData = NULL;
        if (ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3)
          pRemoteCharacteristicData = pRemoteService->getCharacteristic(CHAR_UUID_DATA0);
        else if (ucPrinterType == PRINTER_CAT)
          pRemoteCharacteristicData = pRemoteService->getCharacteristic(CHAR_UUID_DATA1);
        else if (ucPrinterType == PRINTER_FOMEMO || ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS)
          pRemoteCharacteristicData = pRemoteService->getCharacteristic(CHAR_UUID_DATA2);
        if (pRemoteCharacteristicData != NULL)
        {
#ifdef DEBUG_OUTPUT
          Serial.println("Got data transfer characteristic!");
#endif
          bConnected = 1;
          return 1;
        }
      } // if connected
    } // if service found
    else
    {
        bConnected = 0;
#ifdef DEBUG_OUTPUT
        Serial.println("Data service not found");
#endif
    }
  return 0;
#endif
#ifdef ARDUINO_ARDUINO_NANO33BLE // Arduino BLE
    if (!peripheral)
    {
#ifdef DEBUG_OUTPUT
        Serial.println("No peripheral");
#endif
        return 0; // scan didn't succeed or wasn't run
    }
    
    // Connect to the BLE Server.
#ifdef DEBUG_OUTPUT
    Serial.println("connection attempt...");
#endif
    if (peripheral.connect())
    {
#ifdef DEBUG_OUTPUT
        Serial.println("Connected!");
#endif
        // you MUST discover the service or you won't be able to access it
        if (peripheral.discoverService(szServiceNames[ucPrinterType])) {
#ifdef DEBUG_OUTPUT
          Serial.print("Service 0x");
          Serial.print(szServiceNames[ucPrinterType]);
          Serial.println(" discovered");
#endif
        } else {
#ifdef DEBUG_OUTPUT
          Serial.println("Service discovery failed");
#endif
          peripheral.disconnect();
          while (1);
        }
        // Obtain a reference to the service we are after in the remote BLE server.
#ifdef DEBUG_OUTPUT
        Serial.println("Trying to get service");
#endif
        prtService = peripheral.service(szServiceNames[ucPrinterType]); // get the printer service
        if (prtService)
        {
#ifdef DEBUG_OUTPUT
            Serial.println("Got the service");
#endif
            pRemoteCharacteristicData = prtService.characteristic(szCharNames[ucPrinterType]);
            if (pRemoteCharacteristicData)
            {
#ifdef DEBUG_OUTPUT
                Serial.println("Got the characteristic");
#endif
                bConnected = 1;
                return 1;
            }
        }
        else
        {
#ifdef DEBUG_OUTPUT
            Serial.println("Didn't get the service");
#endif
        }
    }
#ifdef DEBUG_OUTPUT
    Serial.println("connection failed");
#endif
    return 0;
#endif // NANO33
#ifdef ARDUINO_NRF52_ADAFRUIT
    Bluefruit.Central.connect(&the_report);
    long ulTime = millis();
    while (!bConnected && (millis() - ulTime) < 4000) // allow 4 seconds for the connection to occur
    {
        delay(20);
    }
    return bConnected;
#endif // ADAFRUIT
} /* tpConnect() */

void tpDisconnect(void)
{
  if (!bConnected) return; // nothing to do
#ifdef HAL_ESP32_HAL_H_
   if (pClient != NULL)
   {
      pClient->disconnect();
      bConnected = 0;
   }
#endif
#ifdef ARDUINO_ARDUINO_NANO33BLE
    if (peripheral)
    {
        if (peripheral.connected())
        {
            peripheral.disconnect();
            bConnected = 0;
        }
    }
#endif
#ifdef ARDUINO_NRF52_ADAFRUIT
    {
        bConnected = 0;
        Bluefruit.disconnect(the_conn_handle);
    }
#endif
} /* tpDisconnect() */
//
// Find the printer name from our supported names list
//
static uint8_t tpFindPrinterName(char *szName)
{
int i = 0;

   szName[9] = 0; // Need to chop off the name after 'PeriPage+'
                  // because it includes 2 bytes of the BLE MAC address
   while (szBLENames[i] != NULL) {
     if (strcmp(szName, szBLENames[i]) == 0) { // found a supported printer
     Serial.print("Found a match for ");
     Serial.println(szName);
     Serial.print("Printer type = ");
     Serial.println(ucBLETypes[i], DEC);
        return ucBLETypes[i];
     } else {
       i++;
     }
   } // while searching our name list
   return 255; // invalid name
} /* tpFindPrinterName() */
//
// Parameterless version
// finds supported printers automatically
//
int tpScan(void)
{
  return tpScan("", 5);
} /* tpScan() */

//
// Scan for compatible printers
// returns true if found
// and stores the printer address internally
// for use with the tpConnect() function
// iSeconds = how many seconds to scan for devices
//
int tpScan(const char *szName, int iSeconds)
{
unsigned long ulTime;
int bFound = 0;
int iLen = strlen(szName);
    
    strcpy(szPrinterName, szName); // only use the given name
#ifdef HAL_ESP32_HAL_H_
    Scanned_BLE_Name[0] = 0;
    ucPrinterType = 255;
    BLEDevice::init("ESP32");
    pBLEScan = BLEDevice::getScan(); //create new scan
    if (pBLEScan != NULL)
    {
      pBLEScan->setAdvertisedDeviceCallbacks(new tpAdvertisedDeviceCallbacks()); //Call the class that is defined above
      pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
      bConnected = false;
      Server_BLE_Address = NULL;
      pBLEScan->start(iSeconds); //Scan for N seconds
    }
    ulTime = millis();
    while (!bFound && (millis() - ulTime) < iSeconds*1000L)
    {
       if (iLen == 0 && ucPrinterType < PRINTER_COUNT) { // found a supported printer
          pBLEScan->stop();
          bFound = 1;
#ifdef DEBUG_OUTPUT
          Serial.print("Found a compatible device - ");
          Serial.println(Scanned_BLE_Name);
#endif     
       } else if (iLen > 0 && memcmp(Scanned_BLE_Name,szPrinterName, iLen) == 0) // found a device we want
       {
#ifdef DEBUG_OUTPUT
           Serial.println("Found Device :-)");
#endif
           pBLEScan->stop(); // stop scanning
           bFound = 1;
           ucPrinterType = tpFindPrinterName(Scanned_BLE_Name);
       }
       else
       {
          delay(10); // if you don't add this, the ESP32 will reset due to watchdog timeout
       }
    }
#endif
#ifdef ARDUINO_ARDUINO_NANO33BLE // Arduino API
    // initialize the BLE hardware
    BLE.begin();
    // start scanning for the printer service UUID
//    BLE.scanForUuid("49535343-FE7D-4AE5-8FA9-9FAFD205E455", true);
    if (iLen != 0) { // if there's a name given
       Serial.println("Scanning for a specific name");
       BLE.scanForName(szPrinterName, true);
    }
    else { // scan for EVERYTHING
       Serial.println("Scanning without a specific name");
       BLE.scan(true);
    }
    ulTime = millis();
    while (!bFound && (millis() - ulTime) < (unsigned)iSeconds*1000UL)
    {
    // check if a peripheral has been discovered
        peripheral = BLE.available();
        if (peripheral)
        {
        // discovered a peripheral, print out address, local name, and advertised service
#ifdef DEBUG_OUTPUT
            Serial.print("Found ");
            Serial.print(peripheral.address());
            Serial.print(" '");
            Serial.print(peripheral.localName());
            Serial.print("' ");
            Serial.print(peripheral.advertisedServiceUuid());
            Serial.println();
#endif
            if (iLen > 0 && memcmp(peripheral.localName().c_str(), szPrinterName, iLen) == 0)
            { // found the one we're looking for
               // stop scanning
                BLE.stopScan();
                bFound = 1;
                // determine the printer
                ucPrinterType = tpFindPrinterName((char *)peripheral.localName().c_str());
            } else if (iLen == 0 && strlen(peripheral.localName().c_str()) > 0) { // compare the name with our supported ones
              ucPrinterType = tpFindPrinterName((char *)peripheral.localName().c_str());
              if (ucPrinterType < PRINTER_COUNT) {
                   BLE.stopScan();
                   bFound = 1; 
                   strcpy(szPrinterName, peripheral.localName().c_str());
#ifdef DEBUG_OUTPUT
                   Serial.print("Found a matching device - ");
                   Serial.println(peripheral.localName().c_str());
#endif
              }
            }
        } // if peripheral located
        else
        {
            delay(50); // give time for scanner to find something
        }
    } // while scanning
#endif
#ifdef ARDUINO_NRF52_ADAFRUIT
    bConnected = 0;
    // Initialize Bluefruit with maximum connections as Peripheral = 0, Central = 1
    // SRAM usage required by SoftDevice will increase dramatically with number of connections
    Bluefruit.begin(0, 1);
    /* Set the device name */
    Bluefruit.setName("Bluefruit52");
    /* Set the LED interval for blinky pattern on BLUE LED */
    Bluefruit.setConnLedInterval(250);
//    Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
//    Bluefruit.configCentralBandwidth(BANDWIDTH_MAX);
    /* Start Central Scanning
     * - Enable auto scan if disconnected
     * - Filter out packet with a min rssi
     * - Interval = 100 ms, window = 50 ms
     * - Use active scan (used to retrieve the optional scan response adv packet)
     * - Start(0) = will scan forever since no timeout is given
     */
    bNRFFound = 0;
    Bluefruit.Scanner.setRxCallback(scan_callback);
    Bluefruit.Scanner.restartOnDisconnect(true);
//    Bluefruit.Scanner.filterRssi(-72);
//    Bluefruit.Scanner.filterUuid(myService.uuid);
    Bluefruit.Scanner.setInterval(160, 80);       // in units of 0.625 ms
    Bluefruit.Scanner.useActiveScan(true);        // Request scan response data
    Bluefruit.Scanner.start(0);                   // 0 = Don't stop
    // allow the timeout for the scan
    ulTime = millis();
    while (!bNRFFound && (millis() - ulTime) < (unsigned)iSeconds*1000UL)
    {
        delay(10);
    }
    Bluefruit.Scanner.stop();
#ifdef DEBUG_OUTPUT
    Serial.println("Stopping the scan");
#endif
  if (ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3) {
    myService = BLEClientService(0x18f0);
    myDataChar = BLEClientCharacteristic(0x2af1);
  } else if (ucPrinterType == PRINTER_CAT) {
    myService = BLEClientService(0xae30);
    myDataChar = BLEClientCharacteristic(0xae01);
  } else if (ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS) {
    myService = BLEClientService(0xff00);
    myDataChar = BLEClientCharacteristic(0xff02);
  }

    myService.begin(); // start my client service
    // Initialize client characteristics
    // Note: Client Chars will be added to the last service that is begin()ed.
    myDataChar.setNotifyCallback(notify_callback);
    myDataChar.begin();
    // Callbacks for Central
    Bluefruit.Central.setConnectCallback(connect_callback);
    Bluefruit.Central.setDisconnectCallback(disconnect_callback);
    bFound = bNRFFound;
#endif // ADAFRUIT
    return bFound;
} /* tpScan() */
//
// Write data to the printer over BLE
//
static void tpWriteData(uint8_t *pData, int iLen)
{
    if (!bConnected) // || !pRemoteCharacteristicData)
        return;
    // Write BLE data without response, otherwise the printer
    // stutters and takes much longer to print
#ifdef HAL_ESP32_HAL_H_
    // For some reason the ESP32 sends some corrupt data if we ask it
    // to write more than 20 bytes at a time (used to be 48)
    while (iLen > 20) {
       pRemoteCharacteristicData->writeValue(pData, 20, bWithResponse);
       if (!bWithResponse) delay(4);
       pData += 20;
       iLen -= 20;
    }
    if (iLen) {
      pRemoteCharacteristicData->writeValue(pData, iLen, bWithResponse);
    }
#endif
#ifdef ARDUINO_ARDUINO_NANO33BLE
    pRemoteCharacteristicData.writeValue(pData, iLen, bWithResponse);
#endif
#ifdef ARDUINO_NRF52_ADAFRUIT
    myDataChar.write((const void *)pData, (uint16_t)iLen);
#endif
} /* tpWriteData() */
//
// Select one of 2 available text fonts along with attributes
// FONT_12x24 or FONT_9x17
//
void tpSetFont(int iFont, int iUnderline, int iDoubleWide, int iDoubleTall, int iEmphasized)
{
uint8_t ucTemp[16];
int i;

  if (iFont < FONT_12x24 || iFont > FONT_9x17) return;

  if (ucPrinterType == PRINTER_FOMEMO || ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3 || ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS) {
     i = 0;
     if (ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS) {
        ucTemp[i++] = 0x10; ucTemp[i++] = 0xff;
        ucTemp[i++] = 0xfe; ucTemp[i++] = 0x01;
     }
     ucTemp[i++] = 0x1b; // ESC
     ucTemp[i++] = 0x21; // !
     ucTemp[i] = (uint8_t)iFont;
     if (iUnderline)
        ucTemp[i] |= 0x80;
     if (iDoubleWide)
        ucTemp[i] |= 0x20;
     if (iDoubleTall)
        ucTemp[i] |= 0x10;
     if (iEmphasized)
        ucTemp[i] |= 0x8;
     tpWriteData(ucTemp, i+1);
  }
} /* tpSetFont() */
//
// Checksum
//
static uint8_t CheckSum(uint8_t *pData, int iLen)
{
int i;
uint8_t cs = 0;

    for (i=0; i<iLen; i++)
        cs = cChecksumTable[(cs ^ pData[i])];
    return cs;
} /* CheckSum() */
//
// Set printer energy
//
static uint8_t *tpSetEnergy(int iEnergy)
{
static int8_t cEnergy[]  = {81, 120, -81, 0,2,0,-1,-1,0,-1};

   cEnergy[6] = (int8_t)(iEnergy >> 8);
   cEnergy[7] = (int8_t)(iEnergy & 0xff);
   cEnergy[7] = CheckSum((uint8_t *)&cEnergy[6], 2); 
   return (uint8_t *)cEnergy;

} /* tpSetEnergy() */
//
// Set the text and barcode alignment
// Use ALIGN_LEFT, ALIGN_CENTER or ALIGN_RIGHT
//
void tpAlign(uint8_t ucAlign)
{
uint8_t ucTemp[4];

    if (!bConnected || ucAlign < ALIGN_LEFT || ucAlign > ALIGN_RIGHT)
       return; // invalid
    ucTemp[0] = 0x1b; // ESC
    ucTemp[1] = 'a';
    ucTemp[2] = ucAlign;
    tpWriteData(ucTemp, 3);

} /* tpAlign() */

//
// Print a 2D (QR) code
//
void tpQRCode(char *szText)
{
	tpQRCode(szText, 0x03);
}

//
// Print a 2D (QR) code
// iSize = starting from 1 / standard is 3
//
void tpQRCode(char *szText, int iSize)
{
// QR Code: Select the model
//              Hex     1D      28      6B      04      00      31      41      n1(x32)     n2(x00) - size of model
// set n1 [49 x31, model 1] [50 x32, model 2] [51 x33, micro qr code]
// https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=140
uint8_t modelQR[] = {0x1d, 0x28, 0x6b, 0x04, 0x00, 0x31, 0x41, 0x32, 0x00};

// QR Code: Set the size of module
// Hex      1D      28      6B      03      00      31      43      n
// n depends on the printer
// https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=141
uint8_t sizeQR[] = {0x1d, 0x28, 0x6b, 0x03, 0x00, 0x31, 0x43, iSize};

//          Hex     1D      28      6B      03      00      31      45      n
// Set n for error correction [48 x30 -> 7%] [49 x31-> 15%] [50 x32 -> 25%] [51 x33 -> 30%]
// https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=142
uint8_t errorQR[] = {0x1d, 0x28, 0x6b, 0x03, 0x00, 0x31, 0x45, 0x31};
// QR Code: Store the data in the symbol storage area
// Hex      1D      28      6B      pL      pH      31      50      30      d1...dk
// https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=143
//                        1D          28          6B         pL          pH  cn(49->x31) fn(80->x50) m(48->x30) d1…dk
uint8_t storeQR[] = {0x1d, 0x28, 0x6b, 0x00 /*store_pL*/, 0x00/*store_pH*/, 0x31, 0x50, 0x30};
// QR Code: Print the symbol data in the symbol storage area
// Hex      1D      28      6B      03      00      31      51      m
// https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=144
uint8_t printQR[] = {0x1d, 0x28, 0x6b, 0x03, 0x00, 0x31, 0x51, 0x30};
int store_len = strlen(szText) + 3;
uint8_t store_pL = (uint8_t)(store_len & 0xff);
uint8_t store_pH = (uint8_t)(store_len / 256);

    if (ucPrinterType != PRINTER_FOMEMO && ucPrinterType != PRINTER_MTP2 && ucPrinterType != PRINTER_MTP3)
       return; // only supported on these
    storeQR[3] = store_pL; storeQR[4] = store_pH;
//    tpWriteData(modelQR, sizeof(modelQR));
    tpWriteData(sizeQR, sizeof(sizeQR));
    tpWriteData(errorQR, sizeof(errorQR));
    tpWriteData(storeQR, sizeof(storeQR));
    tpWriteData((uint8_t *)szText, store_len);
    tpWriteData(printQR, sizeof(printQR));

} /* tpQRCode() */
//
// Print a 1D barcode
//
void tp1DBarcode(int iType, int iHeight, char *szData, int iTextPos)
{
uint8_t ucTemp[128];
uint8_t len;
int i=0;

   if (!bConnected || szData == NULL) return;
   len = (uint8_t)strlen(szData);
   ucTemp[i++] = 0x1d; ucTemp[i++] = 0x48; ucTemp[i++] = (uint8_t)iTextPos;
   ucTemp[i++] = 0x1d; ucTemp[i++] = 0x68; ucTemp[i++] = (uint8_t)iHeight;
   ucTemp[i++] = 0x1d; ucTemp[i++] = 0x77; ucTemp[i++] = 2; // width multiplier
   ucTemp[i++] = 0x1d; ucTemp[i++] = 0x6b; ucTemp[i++] = (uint8_t)iType;
   ucTemp[i++] = len;
   memcpy(&ucTemp[i], szData, len);
   tpWriteData(ucTemp, len + i);
} /* tp1DBarcode() */

//
// Print plain text immediately
//
// Pass a C-string (zero terminated char array)
// If the text doesn't reach the end of the line
// it will not be printed until the printer receives
// a CR (carriage return) or new text which forces
// it to wrap around
//
int tpPrint(char *pString)
{
int iLen;

  if (!bConnected || pString == NULL)
    return 0;

  if (ucPrinterType == PRINTER_CAT) {
     uint8_t ucTemp[128];
     uint8_t len = (uint8_t)strlen(pString);
     tpWriteData((uint8_t *)printText, sizeof(printText));
      ucTemp[0] = 0x51;
      ucTemp[1] = 0x78;
      ucTemp[2] = 0xa2; // data, uncompressed
      ucTemp[3] = 0;
      ucTemp[4] = (uint8_t)len; // data length
      ucTemp[5] = 0;
      for (int i=0; i<len; i++)
        ucTemp[6+i] = ucMirror[pString[i]];
      ucTemp[6 + len + 1] = 0xff;
      ucTemp[6 + len] = CheckSum(&ucTemp[6], len);
      tpWriteData(ucTemp, 8 + len);
     return 1;
  }
  if (ucPrinterType == PRINTER_FOMEMO || ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3 || ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS)
  {
    iLen = strlen(pString);
    if (ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS) {
        uint8_t ucTemp[8];
        ucTemp[0] = 0x10; ucTemp[1] = 0xff;
        ucTemp[2] = 0xfe; ucTemp[3] = 0x01;
        tpWriteData(ucTemp, 4);
    }
    tpWriteData((uint8_t*)pString, iLen);
    return 1;
  }
  return 0;
} /* tpPrint() */
//
// Print plain text immediately
// Pass a C-string (zero terminated char array)
// A CR (carriage return) will be added at the end
// to cause the printer to print the text and advance
// the paper one line
//
int tpPrintLine(char *pString)
{
  if (tpPrint(pString))
  {
    char cTemp[4] = {0xa, 0};
    tpPrint((char *)cTemp);
    return 1;
  }
  return 0;
} /* tpPrintLine() */
//
// Returns the BLE name of the connected printer
// as a zero terminated c-string
// Returns NULL if not connected
//
char *tpGetName(void)
{
   if (!bConnected) return NULL;
   return szPrinterName;
} /* tpGetName() */
//
// Return the printer width in pixels
// The printer needs to be connected to get this info
//
int tpGetWidth(void)
{
   if (!bConnected)
      return 0;
   return iPrinterWidth[ucPrinterType];
} /* tpGetWidth() */
//
// Feed the paper in scanline increments
//
void tpFeed(int iLines)
{
uint8_t ucTemp[16];

  if (!bConnected || iLines < 0 || iLines > 255)
    return;
  if (ucPrinterType == PRINTER_CAT) {
     memcpy(ucTemp, paperFeed, sizeof(paperFeed));
     iLines >>= 2; // uses a different measurement of lines to feed
     ucTemp[6] = (uint8_t)(iLines >> 8);
     ucTemp[7] = (uint8_t)iLines;
     ucTemp[8] = CheckSum(&ucTemp[6], 2);
     tpWriteData(ucTemp, sizeof(paperFeed));
  } else if (ucPrinterType == PRINTER_FOMEMO || ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3) {
   // The PT-210 doesn't have a "feed-by-line" command
   // so instead, we'll send 1 byte-wide graphics of a blank segment
   int i;
   for (i=0; i<iLines; i++) {
     ucTemp[0] = 0x1d; ucTemp[1] = 'v';
     ucTemp[2] = '0'; ucTemp[3] = '0';
     ucTemp[4] = 1; ucTemp[5] = 0; // width = 1 byte
     ucTemp[6] = 1; ucTemp[7] = 0; // height = 1 line
     ucTemp[8] = 0; // 8 blank pixels
     tpWriteData(ucTemp, 9);
     delay(5);
   }
  }
} /* tpFeed() */
//
// Send the preamble for transmitting graphics
//
static void tpPreGraphics(int iWidth, int iHeight)
{
uint8_t *s, ucTemp[16];

  if (ucPrinterType == PRINTER_CAT) {
//    tpWriteData((uint8_t *)getDevState, sizeof(getDevState));
//    tpWriteData((uint8_t *)setQ200DPI, sizeof(setQ200DPI));
//    tpWriteData((uint8_t *)latticeStart, sizeof(latticeStart));

//    s = tpSetEnergy(12000);
//    tpWriteData(s, 10);
    tpWriteData((uint8_t *)printImage, sizeof(printImage));
//    tpWriteData((uint8_t *)paperFeed, 9);
  } else if (ucPrinterType == PRINTER_FOMEMO || ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3) {
  // The printer command for graphics is laid out like this:
  // 0x1d 'v' '0' '0' xLow xHigh yLow yHigh <x/8 * y data bytes>
    ucTemp[0] = 0x1d; ucTemp[1] = 'v';
    ucTemp[2] = '0'; ucTemp[3] = '0';
    ucTemp[4] = (iWidth+7)>>3; ucTemp[5] = 0;
    ucTemp[6] = (uint8_t)iHeight; ucTemp[7] = (uint8_t)(iHeight >> 8);
    tpWriteData(ucTemp, 8);
  } else if (ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS) {
    ucTemp[0] = 0x10; ucTemp[1] = 0xff;
    ucTemp[2] = 0xfe; ucTemp[3] = 0x01; // start of command
    tpWriteData(ucTemp, 4);
    memset(ucTemp, 0, 12);
    tpWriteData(ucTemp, 12); // 12 0's (not sure why)
    ucTemp[0] = 0x1d; ucTemp[1] = 0x76;
    ucTemp[2] = 0x30; ucTemp[3] = 0x00;
    ucTemp[4] = (uint8_t)((iWidth+7)>>3); ucTemp[5] = 0x00; // width in bytes
    ucTemp[6] = (uint8_t)iHeight; ucTemp[7] = (uint8_t)(iHeight>>8); // height (little endian)
    tpWriteData(ucTemp, 8);
  }

} /* tpPreGraphics() */

static void tpPostGraphics(void)
{
   if (ucPrinterType == PRINTER_CAT) {
//      tpWriteData((uint8_t *)setPaper, sizeof(setPaper));
//      tpWriteData((uint8_t *)setPaper, sizeof(setPaper));
//      tpWriteData((uint8_t *)latticeEnd, sizeof(latticeEnd));
//      tpWriteData((uint8_t *)getDevState, sizeof(getDevState));
   } else if (ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS) {
 //     uint8_t ucTemp[] = {0x1b, 0x4a, 0x40, 0x10, 0xff, 0xfe, 0x45};
 //     tpWriteData(ucTemp, sizeof(ucTemp));
   }
} /* tpPostGraphics() */

static void tpSendScanline(uint8_t *s, int iLen)
{
  if (ucPrinterType == PRINTER_CAT) {
      uint8_t ucTemp[64+8];
      ucTemp[0] = 0x51;
      ucTemp[1] = 0x78;
      ucTemp[2] = 0xa2; // gfx, uncompressed
      ucTemp[3] = 0;
      ucTemp[4] = (uint8_t)iLen; // data length
      ucTemp[5] = 0;
      for (i=0; i<iLen; i++) { // reverse the bits
        ucTemp[6+i] = ucMirror[s[i]];
      } // for each byte to mirror
      ucTemp[6 + iLen] = 0;
      ucTemp[6 + iLen + 1] = 0xff;
      ucTemp[6 + iLen] = CheckSum(&ucTemp[6], iLen);
      tpWriteData(ucTemp, 8 + iLen);
  } else if (ucPrinterType == PRINTER_FOMEMO || ucPrinterType == PRINTER_MTP2 || ucPrinterType == PRINTER_MTP3 || ucPrinterType == PRINTER_PERIPAGE || ucPrinterType == PRINTER_PERIPAGEPLUS) {
      tpWriteData(s, iLen);
  }
    // NB: To reliably send lots of data over BLE, you either use WRITE with
    // response (which waits for each packet to be acknowledged), or you add your
    // own delays to give it time to physically print the data.
    // In my testing, the forced delays actually goes faster since you don't
    // have to wait for each packet to be acknowledged and the extra delays
    // BLE adds between sending and receiving.
    // Without this delay, data will be lost and you may leave the printer
    // stuck waiting for a graphics command to finish.
    // For the ESP32, we break up the packets and add the delays in tpWriteData()
#ifndef HAL_ESP32_HAL_H_
    if (!bWithResponse) {
      delay(1+(bb_pitch/8));
    }
#endif
} /* tpSendScanline() */

//
// Send the graphics to the printer (must be connected over BLE first)
//
void tpPrintBuffer(void)
{
uint8_t *s, ucTemp[8];
int y;
int i;

  if (!bConnected)
    return;

  tpPreGraphics(bb_width, bb_height);

  // Print the graphics
  s = pBackBuffer;
  for (y=0; y<bb_height; y++) {
    tpSendScanline(s, bb_pitch);
    s += bb_pitch;
  } // for y
  
  tpPostGraphics();

} /* tpPrintBuffer() */
//
// Draw a line between 2 points
//
void tpDrawLine(int x1, int y1, int x2, int y2, uint8_t ucColor)
{
  int temp;
  int dx = x2 - x1;
  int dy = y2 - y1;
  int error;
  uint8_t *p, mask;
  int xinc, yinc;

  if (x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0 || x1 >= bb_width || x2 >= bb_width || y1 >= bb_height || y2 >= bb_height)
     return;

  if(abs(dx) > abs(dy)) {
    // X major case
    if(x2 < x1) {
      dx = -dx;
      temp = x1;
      x1 = x2;
      x2 = temp;
      temp = y1;
      y1 = y2;
      y2 = temp;
    }

    dy = (y2 - y1);
    error = dx >> 1;
    yinc = 1;
    if (dy < 0)
    {
      dy = -dy;
      yinc = -1;
    }
    p = &pBackBuffer[(y1 * bb_pitch) + (x1 >> 3)]; // point to current spot in back buffer
    mask = 0x80 >> (x1 & 7); // current bit offset
    for(; x1 <= x2; x1++) {
      if (ucColor)
        *p |= mask; // set pixel and increment x pointer
      else
        *p &= ~mask;
      mask >>= 1;
      if (mask == 0) {
         mask = 0x80;
         p++;
      }
      error -= dy;
      if (error < 0)
      {
        error += dx;
        if (yinc > 0)
           p += bb_pitch;
        else
           p -= bb_pitch;
      }
    } // for x1
  }
  else {
    // Y major case
    if(y1 > y2) {
      dy = -dy;
      temp = x1;
      x1 = x2;
      x2 = temp;
      temp = y1;
      y1 = y2;
      y2 = temp;
    }

    p = &pBackBuffer[(y1 * bb_pitch) + (x1 >> 3)]; // point to current spot in back buffer
    mask = 0x80 >> (x1 & 7); // current bit offset
    dx = (x2 - x1);
    error = dy >> 1;
    xinc = 1;
    if (dx < 0)
    {
      dx = -dx;
      xinc = -1;
    }
    for(; y1 <= y2; y1++) {
      if (ucColor)
         *p |= mask; // set the pixel
      else
         *p &= ~mask;
      p += bb_pitch; // y++
      error -= dx;
      if (error < 0)
      {
        error += dy;
        x1 += xinc;
        if (xinc > 0)
        {
          mask >>= 1;
          if (mask == 0) // change the byte
          {
             p++;
             mask = 0x80;
          }
        } // positive delta x
        else // negative delta x
        {
          mask <<= 1;
          if (mask == 0)
          {
             p--;
             mask = 1;
          }
        }
      }
    } // for y
  } // y major case
} /* tpDrawLine() */
