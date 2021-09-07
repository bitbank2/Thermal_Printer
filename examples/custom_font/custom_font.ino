//
// Custom Font Demo
//
// Print Adafruit_GFX bitmap fonts on inexpensive thermal printers
// as line-at-a-time graphics output which doesn't require any bitmap buffers
//
// written by Larry Bank
// Copyright (c) 2021 BitBank Software, Inc.
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

#include <Thermal_Printer.h>
#include "FreeSerif12pt7b.h"
#include "OpenSansBold64.h"
 
void setup() {
  int iWidth;
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);
  Serial.println((char *)"Scanning for BLE printer");
  if (tpScan()) // Scan for any supported printer name
  {
    Serial.println((char *)"Found a printer!, connecting...");
    if (tpConnect())
    {
      char *szName = tpGetName();
      Serial.println((char *)"Connected!");
      iWidth = tpGetWidth();
      Serial.print("Printer pixel width = ");
      Serial.println(iWidth, DEC);
      Serial.print("Reported printer name = ");
      Serial.println(szName);
      // optionally speed up BLE data throughput if your MCU supports it
      //    tpSetWriteMode(MODE_WITHOUT_RESPONSE);
      // Draw text with a custom proportional font
      Serial.println("Printing custom fonts");
      // You can use the tpDrawCustomText to draw these same fonts
      // into your graphics buffer instead of sending directly to the printer
      tpPrintCustomText((GFXfont *)&FreeSerif12pt7b, 0, (char *)"You too can print nice looking fonts");
      tpPrintCustomText((GFXfont *)&FreeSerif12pt7b, 0, (char *)"with Adafruit_GFX bitmap format.");
      tpPrintCustomText((GFXfont *)&Open_Sans_Bold_64, 0, (char *)"Huge fonts!");
      tpFeed(48); // feed the paper out a little from the print head to see what was printed
      Serial.println((char *)"Disconnecting");
      tpDisconnect();
      Serial.println((char *)"Done!");
      while (1) {};      
    } // if connected
  } // successful scan
  else
  {
    Serial.println((char *)"Didn't find a printer :( ");
  }
} /* setup() */

void loop() {
 // nothing going on here :)
}
