#include <Thermal_Printer.h>
static uint8_t ucBuf[48 * 384];
#define WIDTH 384
#define HEIGHT 384

void setup() {
  int i;
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Preparing image buffer...");
  tpSetBackBuffer(ucBuf, WIDTH, HEIGHT);
  tpFill(0);
  for (i=0; i<WIDTH; i += 32)
  {
    tpDrawLine(i, 0, WIDTH-1-i, HEIGHT-1, 1);
  }
  for (i=0; i<HEIGHT; i+= 8)
  {
    tpDrawLine(WIDTH-1, i, 0, HEIGHT-1-i, 1);
  }
  tpDrawText(0,0,(char *)"BitBank Thermal Printer", FONT_LARGE, 0);
  Serial.println("Scanning for BLE printer");
  if (tpScan("MTP-2",5))
  {
    Serial.println("Found a printer!, connecting...");
    if (tpConnect())
    {
      Serial.println("Connected!, printing graphics");
      tpPrintBuffer();
      Serial.println("Disconnecting");
      tpDisconnect();
      Serial.println("Done!");
      while (1) {};      
    }
  }
  else
  {
    Serial.println("Didn't find a printer :( ");
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
