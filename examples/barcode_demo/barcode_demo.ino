//
// 1D & 2D Barcode demo
//
// Some thermal printers have internal support for generating 1D and 2D barcodes
// from a string of ASCII characters. Two functions within the Thermal_Printer library
// allow you to access this functionality in a simple manner.
// Even if the printer supports barcode printing, it probably doesn't support
// the complete list of code types defined by the ESC/POS printer commands.
// In the case of the GOOJPRT PT210/MTP-2, it only supports a small subset of 1D codes
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
 
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println((char *)"Scanning for BLE printer");
  if (tpScan()) // Scan for any supported printer name
  {
    Serial.println((char *)"Found a printer!, connecting...");
    if (tpConnect())
    {
      int iWidth;
      char *szName = tpGetName();
      Serial.println((char *)"Connected!");
      iWidth = tpGetWidth();
      Serial.print("Printer pixel width = ");
      Serial.println(iWidth, DEC);
      Serial.print("Reported printer name = ");
      Serial.println(szName);
      // optionally speed up BLE data throughput if your MCU supports it
//      tpSetWriteMode(MODE_WITHOUT_RESPONSE);
      tpAlign(ALIGN_LEFT);
      tpPrint("This should print a 2D QR code\n\rwith the URL of my website\n\r");
      tpAlign(ALIGN_CENTER);
      tpQRCode((char *)"https://bitbanksoftware.com");
      tpAlign(ALIGN_LEFT);
      tpPrint("\n\rThis should print a CODE128\n\rof the string '123456789'\n\r");
      // The call below specifies that the code should be 64 pixels tall
      // and that the text encoded by the bars should be displayed below it
      // Not all text position options work on all printers.
      tpAlign(ALIGN_CENTER);
      tp1DBarcode(BARCODE_CODE128, 64, (char *)"123456789", BARCODE_TEXT_BELOW);
      tpAlign(ALIGN_LEFT);
      tpFeed(24); // feed the paper out a little from the print head to see what was printed
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
