#include <Thermal_Printer.h>

static const int WIDTH = 384;
static const int HEIGHT = 96;
static uint8_t printBuffer[(WIDTH / 8) * HEIGHT];

void setup()
{
  Serial.begin(115200);
  while (!Serial) {}

  tpSetBackBuffer(printBuffer, WIDTH, HEIGHT);
  tpFill(0);
  tpDrawText(8, 8, (char *)"MXW01", FONT_LARGE, 0);
  tpDrawLine(0, 0, WIDTH - 1, 0, 1);
  tpDrawLine(0, HEIGHT - 1, WIDTH - 1, HEIGHT - 1, 1);

  Serial.println("Scanning for MXW01...");
  if (!tpScan("MXW01", 10) || !tpConnect()) {
    Serial.println("MXW01 not found or connection failed");
    return;
  }

  tpSetEnergy(0x5D);
  Serial.println("Printing...");
  tpPrintBuffer();
  tpDisconnect();
  Serial.println("Done");
}

void loop()
{
}
