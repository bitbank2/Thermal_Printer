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
#define bleWriteValue(buffer, size, response) pRemoteCharacteristicData->writeValue(buffer, size, response);
const bool noResponseSupported = true;
#endif

#ifdef ARDUINO_ARDUINO_NANO33BLE
#include <ArduinoBLE.h>
#define bleWriteValue(buffer, size, response) pRemoteCharacteristicData.writeValue(buffer, size, response);
const bool noResponseSupported = true;
#endif

#ifdef ARDUINO_NRF52_ADAFRUIT
#include <bluefruit.h>
#define bleWriteValue(buffer, size, response) myDataChar.write((const void *)buffer, (uint16_t)size);
const bool noResponseSupported = false;
#endif

#include "Thermal_Printer.h"

static char szPrinterName[32];
static int bb_width, bb_height; // back buffer width and height in pixels
static int tp_wrap, bb_pitch;
static int iCursorX = 0;
static int iCursorY = 0;
static uint8_t bWithResponse = noResponseSupported ? MODE_WITHOUT_RESPONSE : 1; // default no wait if supported by API
static uint8_t *pBackBuffer = NULL;
static uint8_t bConnected = 0;
static void tpWriteData(uint8_t *pData, int iLen);
extern "C" {
extern unsigned char ucFont[], ucBigFont[];
};

//these are printer dependent. please expand for each new printer
const int PACKET_SIZE = 20;     //max bytes per packet (usually API should negotiate bigger packets)
const int PACKET_DELAY = 4;     //millis to wait after each packet for the printer to take care. If printer gets stuck increase this. (5: conservative, 2: 50% fail, 4: 10% fail)

#ifdef ARDUINO_NRF52_ADAFRUIT
// Bluetooth support for Adafruit nrf52 boards
const uint8_t myServiceUUID[16] = {0x55, 0xe4, 0x05, 0xd2, 0xaf, 0x9f, 0xa9, 0x8f, 0xe5, 0x4a, 0x7d, 0xfe, 0x43, 0x53, 0x53, 0x49};
const uint8_t myDataUUID[16] = {0xb3, 0x9b, 0x72, 0x34, 0xbe, 0xec, 0xd4, 0xa8, 0xf4, 0x43, 0x41, 0x88, 0x43, 0x53, 0x53, 0x49};
//#define myServiceUUID 0xFEA0
//#define myDataUUID 0xFEA1
static ble_gap_evt_adv_report_t the_report;
static uint16_t the_conn_handle;
static int bNRFFound;
//BLEClientCharacteristic myDataChar(myDataUUID);
//BLEClientService myService(myServiceUUID);
BLEClientService myService(0x18f0);
BLEClientCharacteristic myDataChar(0x2af1);
/**
 * Callback invoked when an connection is established
 * @param conn_handle
 */
static void connect_callback(uint16_t conn_handle)
{
//  Serial.println("Connected!");
//  Serial.print("Discovering FEA0 Service ... ");
    the_conn_handle = conn_handle;
  // If FEA0 is not found, disconnect and return
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

static void scan_callback(ble_gap_evt_adv_report_t* report)
{
//    Serial.printf("found something %s\n", report->data.p_data);
//  if (Bluefruit.Scanner.checkReportForUuid(report, myServiceUUID))
//    char *name = (char *)report->data.p_data;
//    int i;
//    for (i=0; i<report->data.len; i++)
//       if (name[i] == szPrinterName[0]) break; // "parse" for the name in the adv data
//  if (name && memcmp(&name[i], szPrinterName, strlen(szPrinterName)) == 0)
  {
#ifdef DEBUG_OUTPUT
     Serial.println("Found Printer!");
#endif
      bNRFFound = 1;
      Bluefruit.Scanner.stop();
//      Serial.print("RemoteDisplay UUID detected. Connecting ... ");
      memcpy(&the_report, report, sizeof(ble_gap_evt_adv_report_t));
//      Bluefruit.Central.connect(report);
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
static BLEUUID SERVICE_UUID("49535343-FE7D-4AE5-8FA9-9FAFD205E455");
static BLEUUID CHAR_UUID_DATA ("49535343-8841-43F4-A8D4-ECBE34729BB3");
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
      if (memcmp(advertisedDevice.getName().c_str(), szPrinterName, iLen) == 0)
      { // this is what we want
        Server_BLE_Address = new BLEAddress(advertisedDevice.getAddress());
        Scanned_BLE_Address = Server_BLE_Address->toString().c_str();
        strcpy(Scanned_BLE_Name, advertisedDevice.getName().c_str());
#ifdef DEBUG_OUTPUT
        Serial.println("A match!");
        Serial.println((char *)Scanned_BLE_Address.c_str());
        Serial.println(Scanned_BLE_Name);
#endif
      }
    }
}; // class tpAdvertisedDeviceCallbacks
#endif

//
// Provide a back buffer for your printer graphics
// This allows you to manage the RAM used on
// embedded platforms like Arduinos
// The memory is laid out horizontally (384 pixels across = 48 bytes)
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
    Serial.printf(" - Created client, connecting to %s", Scanned_BLE_Address.c_str());
#endif
    // Connect to the BLE Server.
    pClient->connect(*Server_BLE_Address);
//    if (!pClient->isConnected())
//    {
//      Serial.println("Connect failed");
//      return false;
//    }
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService != NULL)
    {
//      Serial.println(" - Found our service");
      if (pClient->isConnected())
      {
        pRemoteCharacteristicData = pRemoteService->getCharacteristic(CHAR_UUID_DATA);
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
//      Serial.println("data Service not found");
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
        if (peripheral.discoverService("18f0")) {
#ifdef DEBUG_OUTPUT
          Serial.println("0x18f0 discovered");
#endif
        } else {
#ifdef DEBUG_OUTPUT
          Serial.println("0x18f0 disc failed");
#endif
          peripheral.disconnect();
          while (1);
        }
        // Obtain a reference to the service we are after in the remote BLE server.
#ifdef DEBUG_OUTPUT
        Serial.println("Trying to get service 18f0");
#endif
        prtService = peripheral.service("18f0"); // get the printer service
        if (prtService)
        {
#ifdef DEBUG_OUTPUT
            Serial.println("Got the service");
#endif
            pRemoteCharacteristicData = prtService.characteristic("2af1");
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
#ifdef HAL_ESP32_HAL_H_
   if (bConnected && pClient != NULL)
   {
      pClient->disconnect();
      bConnected = 0;
   }
#endif
#ifdef ARDUINO_ARDUINO_NANO33BLE
    if (peripheral && bConnected)
    {
        if (peripheral.connected())
        {
            peripheral.disconnect();
            bConnected = 0;
        }
    }
#endif
#ifdef ARDUINO_NRF52_ADAFRUIT
    if (bConnected)
    {
        bConnected = 0;
        Bluefruit.disconnect(the_conn_handle);
    }
#endif
} /* tpDisconnect() */

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
    
    strcpy(szPrinterName, szName);
#ifdef HAL_ESP32_HAL_H_
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
       if (memcmp(Scanned_BLE_Name,szPrinterName, iLen) == 0) // found a device we want
       {
#ifdef DEBUG_OUTPUT
           Serial.println("Found Device :-)");
#endif
           pBLEScan->stop(); // stop scanning
           bFound = 1;
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
    BLE.scanForName(szPrinterName, true);
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
            if (memcmp(peripheral.localName().c_str(), szPrinterName, iLen) == 0)
            { // found the one we're looking for
               // stop scanning
                BLE.stopScan();
                bFound = 1;
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
    Bluefruit.Scanner.filterUuid(myService.uuid);
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
//    Serial.println("Stopping the scan");
    myService.begin(); // start my client service
    // Initialize client characteristics of VirtualDisplay.
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

    //might also write empty packets. no clue if this is ever needed
    for(int i = 0; i < iLen / PACKET_SIZE; i++)
    {
        bleWriteValue(pData, PACKET_SIZE, bWithResponse);
        pData += PACKET_SIZE;
        if(bWithResponse == MODE_WITHOUT_RESPONSE) delay(PACKET_DELAY);
    }
    if(iLen % PACKET_SIZE || iLen == 0)
    {
        bleWriteValue(pData, iLen % PACKET_SIZE, bWithResponse);
        if(bWithResponse == MODE_WITHOUT_RESPONSE) delay(PACKET_DELAY);
    }
} /* tpWriteData() */
//
// Select one of 2 available text fonts along with attributes
// FONT_12x24 or FONT_9x17
//
void tpSetFont(int iFont, int iUnderline, int iDoubleWide, int iDoubleTall, int iEmphasized)
{
uint8_t ucTemp[4];
  if (iFont < FONT_12x24 || iFont > FONT_9x17) return;

  ucTemp[0] = 0x1b; // ESC
  ucTemp[1] = 0x21; // !
  ucTemp[2] = (uint8_t)iFont;
  if (iUnderline)
     ucTemp[2] |= 0x80;
  if (iDoubleWide)
     ucTemp[2] |= 0x20;
  if (iDoubleTall)
     ucTemp[2] |= 0x10;
  if (iEmphasized)
     ucTemp[2] |= 0x8;
  tpWriteData(ucTemp, 3);

} /* tpSetFont() */
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
  if (pString)
  {
    iLen = strlen(pString);
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
    uint8_t cr = 0xd;
    tpWriteData(&cr, 1);
    return 1;
  }
  return 0;
} /* tpPrintLine() */
//
// Send the graphics to the printer (must be connected over BLE first)
//
void tpPrintBuffer(void)
{
uint8_t *s, ucTemp[8];
int y;

  if (!bConnected)
    return;
// The printer command for graphics is laid out like this:
// 0x1d 'v' '0' '0' xLow xHigh yLow yHigh <x/8 * y data bytes>
  ucTemp[0] = 0x1d; ucTemp[1] = 'v';
  ucTemp[2] = '0'; ucTemp[3] = '0';
  ucTemp[4] = (bb_width+7)>>3; ucTemp[5] = 0;
  ucTemp[6] = (uint8_t)bb_height; ucTemp[7] = (uint8_t)(bb_height >> 8);
  tpWriteData(ucTemp, 8);
// Now write the graphics data
  tpWriteData(pBackBuffer, bb_pitch * bb_height);
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
