#include <Thermal_Printer.h>
#include <JPEGDEC.h>

#include "eagle_576.h"
#include "dog_384.h"

uint8_t ucDither[576 * 16]; // buffer for the dithered pixel output
JPEGDEC jpg;
static int iWidth;

int JPEGDraw(JPEGDRAW *pDraw)
{
  int i, iCount;
  uint8_t *s = (uint8_t *)pDraw->pPixels;
  
  tpSetBackBuffer((uint8_t *)pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  // The output is inverted, so flip the bits
  iCount = (pDraw->iWidth * pDraw->iHeight)/8;
  for (i=0; i<iCount; i++)
     s[i] = ~s[i];
     
  tpPrintBuffer(); // Print this block of pixels
  return 1; // Continue decode
} /* JPEGDraw() */

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println((char *)"Scanning for BLE printer");
  if (tpScan()) // Scan for any supported printer name
  {
    Serial.println((char *)"Found a printer!, connecting...");
    if (tpConnect())
    {
      Serial.println("Connected!");
      tpSetWriteMode(MODE_WITHOUT_RESPONSE);
    }
    else
    {
      Serial.println("Failed to connected :(");
      while (1) {};
    }
  } // if scan
  else
  {
    Serial.println((char *)"Didn't find a printer :( ");
    while (1) {};
  }
} /* setup () */

void loop() {
uint8_t *pImage;
int iImageSize;

  iWidth = tpGetWidth(); // get the width of the printer in pixels
  if (iWidth == 384) {
     pImage = (uint8_t *)dog_384;
     iImageSize = (int)sizeof(dog_384);
  } else { // assume 576
     pImage = (uint8_t *)eagle_576;
     iImageSize = (int)sizeof(eagle_576);
  }
  if (jpg.openFLASH(pImage, iImageSize, JPEGDraw)) {
     jpg.setPixelType(ONE_BIT_DITHERED);
     jpg.decodeDither(ucDither, 0);
  }
  tpFeed(32); // advance the paper 32 scan lines
  tpDisconnect();
  Serial.println("Finished printing!");
  while (1) {};
} /* loop() */
