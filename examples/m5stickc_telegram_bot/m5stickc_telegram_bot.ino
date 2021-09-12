/*******************************************************************
    A telegram bot for your ESP32 to print shopping or to-do lists
    on BLE Thermal Printers

    Arduino sketch written by Larry Bank
    
    Parts:
    Any ESP32, GOOJPRT PT-210, MPT-3, PeriPage+ (so far)

    Telegram Library written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow

    Thermal Printer library written by Larry Bank
    Twitter: https://twitter.com/fast_code_r_us
    Github: https://github.com/bitbank2

    N.B.: The ESP32 partitioning (Tools menu) must be set to "No OTA" to fit this sketch
          in FLASH because it uses both the WiFi and BLE libraries which will pass 1MB
          
 *******************************************************************/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Thermal_Printer.h>
#include <Preferences.h>
#include <bb_spi_lcd.h>
#include <Wire.h>

#define TFT_CS 5
#define TFT_RST 18
#define TFT_DC 23
#define TFT_CLK 13
#define TFT_MOSI 15
#define BUTTON_A 37
#define BUTTON_B 39

// Proportional fonts for 203dpi and 304dpi output
#include "Open_Sans_Bold_22.h"
#include "Open_Sans_Bold_32.h"
const char *APP_NAME = "Shopping_List"; // Preferences name
const char *Spaces = "                    ";
static int bPrint = 0;

// Wifi network station credentials
// Replace these with your actual credentials
#define WIFI_SSID "my_ssid"
#define WIFI_PASSWORD "my_password"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "use_the_token_from_botfather"

// List of user names that are authorized to use this bot
// make sure the list terminates with NULL
// For more security, this can be changed to use chat_id's that are unique
char *szValidUsers[] {(char *)"Larry", (char *)"Brian", NULL};
static uint8_t txBuffer[4096];
const unsigned long BOT_MTBS = 1000; // mean time between scan messages
SPILCD lcd;
Preferences preferences;
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;          // last time messages' scan has been done
bool Start = false;

// M5StickC-Plus init code
void Write1Byte( uint8_t Addr ,  uint8_t Data )
{
    Wire1.beginTransmission(0x34);
    Wire1.write(Addr);
    Wire1.write(Data);
    Wire1.endTransmission();
}
uint8_t Read8bit( uint8_t Addr )
{
    Wire1.beginTransmission(0x34);
    Wire1.write(Addr);
    Wire1.endTransmission();
    Wire1.requestFrom(0x34, 1);
    return Wire1.read();
}
void AxpBrightness(uint8_t brightness)
{
    if (brightness > 12)
    {
        brightness = 12;
    }
    uint8_t buf = Read8bit( 0x28 );
    Write1Byte( 0x28 , ((buf & 0x0f) | (brightness << 4)) );
}
void AxpPowerUp()
{
    Wire1.begin(21, 22);
    Wire1.setClock(400000);
    // Set LDO2 & LDO3(TFT_LED & TFT) 3.0V
    Write1Byte(0x28, 0xcc);

    // Set ADC sample rate to 200hz
    Write1Byte(0x84, 0b11110010);

    // Set ADC to All Enable
    Write1Byte(0x82, 0xff);

    // Bat charge voltage to 4.2, Current 100MA
    Write1Byte(0x33, 0xc0);

    // Depending on configuration enable LDO2, LDO3, DCDC1, DCDC3.
    byte buf = (Read8bit(0x12) & 0xef) | 0x4D;
//    if(disableLDO3) buf &= ~(1<<3);
//    if(disableLDO2) buf &= ~(1<<2);
//    if(disableDCDC3) buf &= ~(1<<1);
//    if(disableDCDC1) buf &= ~(1<<0);
    Write1Byte(0x12, buf);
     // 128ms power on, 4s power off
    Write1Byte(0x36, 0x0C);

    if (1) //if(!disableRTC)
    {
        // Set RTC voltage to 3.3V
        Write1Byte(0x91, 0xF0);

        // Set GPIO0 to LDO
        Write1Byte(0x90, 0x02);
    }

    // Disable vbus hold limit
    Write1Byte(0x30, 0x80);

    // Set temperature protection
    Write1Byte(0x39, 0xfc);

    // Enable RTC BAT charge
//    Write1Byte(0x35, 0xa2 & (disableRTC ? 0x7F : 0xFF));
    Write1Byte(0x35, 0xa2);
     // Enable bat detection
    Write1Byte(0x32, 0x46);

    // Set Power off voltage 3.0v
    Write1Byte(0x31 , (Read8bit(0x31) & 0xf8) | (1 << 2));

} /* AxpPowerUp() */

void ShowHelpMessage(String chat_id)
{
      String welcome = "Welcome to the Thermal Printer Shopping List bot\n\n";
      welcome += "This is an ESP32 listening for text and commands.\n\n";
      welcome += "Type /help to see this message again.\n";
      welcome += "The following commands are recognized:\n";
      welcome += "/list - show current shopping list\n";
      welcome += "/deleteall - delete the current shopping list\n";
      welcome += "/delete <number> - delete a specific item\n";
      welcome += "/print - print the list on a local thermal printer\n";
      welcome += "Any other text will be added to the list as an item\n";
      welcome += "Enjoy!\n";
      bot.sendMessage(chat_id, welcome);
} /* ShowHelpMessage() */

void PrintList(String chat_id)
{
        int bSucceeded = 0;
        char szMsg[128];
        
        if (chat_id.length()) bot.sendMessage(chat_id, "Printing...\n");
        sprintf(szMsg, "Printing...%s", Spaces);
        spilcdWriteString(&lcd, 0,16,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
        sprintf(szMsg, "Looking for printers%s", Spaces);
        spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);

        // Need to temporarily disconnect WiFi or the system can 
        // crash due to a stack overrun caused by WiFi & BLE being used simultaneously
        WiFi.disconnect();
        // Search (scan), then connect to the BLE printer and send the text
        if (tpScan()) {
            sprintf(szMsg, "Found a printer!%s", Spaces);
            spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
            Serial.println(szMsg);
            if (tpConnect()) {
              sprintf(szMsg, "Connected to %s%s", tpGetName(), Spaces);
              spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
              Serial.println(szMsg);
              GFXfont *pFont;
              int i, iCount = GetStringCount();
              if (tpGetWidth() == 384) // low res printer
                 pFont = (GFXfont *)&Open_Sans_Bold_22;
              else // high res printer
                 pFont = (GFXfont *)&Open_Sans_Bold_32;
              for (i=0; i<iCount; i++) {
                tpPrintCustomText(pFont, 0, GetString(i));
              } // for i
              tpFeed(64);
              tpDisconnect(); // done!
              bSucceeded = 1;
            } // connected
        } else {
          // failed to find a printer; notify user
            sprintf(szMsg, "No printers found%s", Spaces);
            spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
        }
        WiFi.reconnect(); // reconnect to WiFi network
        if (bSucceeded) {
          if (chat_id.length()) bot.sendMessage(chat_id, "Printing completed successfully\n");
          sprintf(szMsg, "Printing Succeeded!%s", Spaces);
          spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
        } else {
          if (chat_id.length()) bot.sendMessage(chat_id, "Failed to print; see your sys admin for further assistance.\n");
          sprintf(szMsg, "Printing Failed!%s", Spaces);
          spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
        }
} /* PrintList() */

void handleNewMessages(int numNewMessages)
{
int i, j;
char szMsg[128];

//  Serial.println("handleNewMessages");
//  Serial.println(String(numNewMessages));

  for (i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

//  Serial.print("Got text: ");
//  Serial.println(text);
  
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";
    Serial.print("From user: ");
    Serial.println(from_name);
   // validate the user
   j = 0;
   while (szValidUsers[j] != NULL) {
      if (strcmp(from_name.c_str(), szValidUsers[j]) == 0)
         break;
      j++;
   }
   if (szValidUsers[j] == NULL) { // not found, don't act on command
      bot.sendMessage(chat_id, "Unauthorized user, ignoring\n");
      continue; // process next msg in the queue
   }
    
//    if (text == "/send_test_action")
//    {
//      bot.sendChatAction(chat_id, "typing");
//      delay(4000);
//      bot.sendMessage(chat_id, "Did you see the action message?");

      // You can't use own message, just choose from one of bellow

      //typing for text messages
      //upload_photo for photos
      //record_video or upload_video for videos
      //record_audio or upload_audio for audio files
      //upload_document for general files
      //find_location for location data

      //more info here - https://core.telegram.org/bots/api#sendchataction
//    }
    sprintf(szMsg, "User: %s%s", from_name.c_str(), Spaces);
    spilcdWriteString(&lcd, 0,16,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
    
    if (text.charAt(0) == '/') { // it's a command
      sprintf(szMsg, "Sent cmd: %s%s", text.c_str(), Spaces);
      spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
      if (text == "/list") {
        int i, iCount = GetStringCount();
        for (i=0; i<iCount; i++) {
          sprintf(szMsg, "%d - %s", i+1, GetString(i));
          bot.sendMessage(chat_id, szMsg);
        } // for i
        bot.sendMessage(chat_id, "end of list");
      } else if (text == "/deleteall") {
        DeleteAll();
        bot.sendMessage(chat_id, "List deleted\n");
        ShowItemCount();
      } else if (text.substring(0, 7) == "/delete") { // delete a single item
        int iCount, iItem = atoi(text.substring(8).c_str());
        iCount = GetStringCount();
        if (iItem > iCount || iItem < 0) {
          sprintf(szMsg, "Invalid item number; list length = %d\n", iCount);
          bot.sendMessage(chat_id, szMsg);
          continue;
        }
        DeleteString(iItem-1);
        sprintf(szMsg, "Item %d deleted; list length = %d\n", iItem, iCount);
        bot.sendMessage(chat_id, szMsg);
        ShowItemCount();
      } else if (text == "/print") {
        bPrint = 1; // allow Telegram msg queue to finish, then print
        bot.sendMessage(chat_id, "Printing...");
      } else if (text == "/start" || text == "/help") {
        ShowHelpMessage(chat_id);
      } else {
        String errormsg = "Unrecognized command: " + text + "\n";
        errormsg += "Type /help for a list of valid commands\n";
        bot.sendMessage(chat_id, errormsg);
      }
    } // commands
    else { // add to the list
      int i = AddString((char *)text.c_str());
      sprintf(szMsg, "Item added; list length = %d\n", i);
      bot.sendMessage(chat_id, szMsg);
      sprintf(szMsg, "Added: %s%s", text.c_str(), Spaces);
      spilcdWriteString(&lcd, 0,40,szMsg,0x07e0,0, FONT_12x16, DRAW_TO_LCD);
      ShowItemCount();
    }
  } // for each message
} /* handleNewMessages() */

void DeleteString(int iString)
{
int iCount;
char szName[32], szTemp[128];

      preferences.begin(APP_NAME, false);
      iCount = preferences.getInt("list_count", 0);
      if (iString >= iCount) {
        preferences.end();
        return; // invalid string to delete
      }
      sprintf(szName, "name%d", iCount-1); // get last string in the list
      preferences.getString(szName, szTemp, sizeof(szTemp));
      sprintf(szName, "name%d", iString); // to replace the one we want to delete
      preferences.putString(szName, szTemp);
      iCount--; // delete the last name
      preferences.putInt("list_count", iCount);
      preferences.end();
  
} /* DeleteString() */

char * GetString(int iString)
{
static char szTemp[128];
char szName[32];
int iCount;

  preferences.begin(APP_NAME, false);
  iCount = preferences.getInt("list_count", 0);
  if (iString >= iCount) { // invalid
     return NULL;
     preferences.end();
  }
  sprintf(szName, "name%d", iString);
  preferences.getString(szName, szTemp, sizeof(szTemp));
  preferences.end();
  return szTemp;
} /* GetString() */

void DeleteAll(void)
{
      preferences.begin(APP_NAME, false);
      preferences.putInt("list_count", 0);
      preferences.end();  
} /* DeleteAll() */

int GetStringCount(void)
{
int iCount;

      preferences.begin(APP_NAME, false);
      iCount = preferences.getInt("list_count", 0);
      preferences.end();
      return iCount;
} /* GetStringCount() */

int AddString(char *newname)
{
int iCount;
char szTemp[32];

      preferences.begin(APP_NAME, false);
      iCount = preferences.getInt("list_count", 0);
      // save the data
      sprintf(szTemp, "name%d", iCount);
      preferences.putString(szTemp, newname);
      iCount++;
      preferences.putInt("list_count", iCount);
      preferences.end();
      return iCount; // return new list length 
} /* AddString() */

void lightSleep(uint64_t time_in_us)
{
  esp_sleep_enable_timer_wakeup(time_in_us);
  esp_light_sleep_start();
} /* lightSleep() */

void ShowReady(void)
{
  spilcdWriteString(&lcd, 0,16,(char *)"Telegram bot ready!  ", 0xffe0,0,FONT_12x16, DRAW_TO_LCD);
  spilcdWriteString(&lcd, 0,40,(char *)"Press A to print   ", 0xffe0,0,FONT_12x16, DRAW_TO_LCD);
} /* ShowReady() */

void ShowItemCount(void)
{
int iCount;
char szTemp[32];

   preferences.begin(APP_NAME, false);
   iCount = preferences.getInt("list_count", 0);
   preferences.end();
   sprintf(szTemp, "Item Count: %d   ", iCount);
   spilcdWriteString(&lcd, 0,112,szTemp, 0xffe0,0,FONT_12x16, DRAW_TO_LCD);  
} /* ShowItemCount() */

void setup()
{
  int iTimeout = 0;
  char szTemp[32];

  AxpPowerUp();
  AxpBrightness(9); // turn on backlight (0-12)
  pinMode(BUTTON_A, INPUT);
  pinMode(BUTTON_B, INPUT);
  
  Serial.begin(115200);
  while (!Serial && iTimeout < 5) {
    delay(1000);
    iTimeout++;
  };

  spilcdSetTXBuffer(txBuffer, sizeof(txBuffer));
  spilcdInit(&lcd, LCD_ST7789_135, FLAGS_NONE, 40000000, TFT_CS, TFT_DC, TFT_RST, -1, -1, TFT_MOSI, TFT_CLK); // M5Stick-C plus pin numbering, 40Mhz
  spilcdSetOrientation(&lcd, LCD_ORIENTATION_90);
  spilcdFill(&lcd, 0, DRAW_TO_LCD);
  spilcdWriteString(&lcd, 16,2,(char *)"Telegram Bot Shopping List", 0xffff,0,FONT_8x8, DRAW_TO_LCD);
  spilcdWriteString(&lcd, 0,16,(char *)"Connecting to WiFi...", 0x7e0,0,FONT_8x8, DRAW_TO_LCD);
  // Attempt to connect to Wifi network:
  Serial.print("\nConnecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  sprintf(szTemp, "WiFi addr: %s", WiFi.localIP().toString().c_str());
  Serial.println("Connected!");
  Serial.print(szTemp);
  spilcdWriteString(&lcd, 0,16,szTemp, 0xffe0,0,FONT_8x8, DRAW_TO_LCD);

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  ShowReady();
  ShowItemCount();
} /* setup() */

void loop()
{
  String empty = "";
  
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  } else {
    if (digitalRead(BUTTON_A) == LOW || bPrint) { //  start printing
      bPrint = 0;
      PrintList(empty);
      delay(2000);
      ShowReady();
    }
  }
}
