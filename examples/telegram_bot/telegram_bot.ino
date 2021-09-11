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
// Proportional fonts for 203dpi and 304dpi output
#include "Open_Sans_Bold_22.h"
#include "Open_Sans_Bold_32.h"
const char *APP_NAME = "Shopping_List"; // Preferences name

// Wifi network station credentials
// change these to your local AP credentials
#define WIFI_SSID "my_ssid"
#define WIFI_PASSWORD "my_password"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "xxxxxxx"

// List of user names that are authorized to use this bot
// make sure the list terminates with NULL
// For more security, this can be changed to use chat_id's that are unique
char *szValidUsers[] {"Isabella", "Gabriella", "Marice", "Larry", "Brian", NULL};

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

Preferences preferences;
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;          // last time messages' scan has been done
bool Start = false;

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

void handleNewMessages(int numNewMessages)
{
int i, j;
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
    if (text.charAt(0) == '/') { // it's a command
      if (text == "/list") {
        int i, iCount = GetStringCount();
        char szMsg[128];
        for (i=0; i<iCount; i++) {
          sprintf(szMsg, "%d - %s", i+1, GetString(i));
          bot.sendMessage(chat_id, szMsg);
        } // for i
        bot.sendMessage(chat_id, "end of list");
      } else if (text == "/deleteall") {
        DeleteAll();
        bot.sendMessage(chat_id, "List deleted\n");
      } else if (text.substring(0, 7) == "/delete") { // delete a single item
        int iCount, iItem = atoi(text.substring(8).c_str());
        char szMsg[64];
        DeleteString(iItem-1);
        iCount = GetStringCount();
        if (iItem > iCount || iItem < 0) {
          sprintf(szMsg, "Invalid item number; list length = %d\n", iCount);
          bot.sendMessage(chat_id, szMsg);
          continue;
        }
        sprintf(szMsg, "Item %d deleted; list length = %d\n", iItem, iCount);
        bot.sendMessage(chat_id, szMsg);
      } else if (text == "/print") {
        int bSucceeded = 0;
        bot.sendMessage(chat_id, "Printing...\n");
        // Need to temporarily disconnect WiFi or the system can 
        // crash due to a stack overrun caused by WiFi & BLE being used simultaneously
        WiFi.disconnect();
        // Search (scan), then connect to the BLE printer and send the text
        if (tpScan()) {
            Serial.println((char *)"Found a printer!, connecting...");
            if (tpConnect()) {
              Serial.println((char *)"Connected!, ready to print");
              GFXfont *pFont;
              int i, iCount = GetStringCount();
              char szMsg[128];
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
          // failed to connect; notify user
        }
        WiFi.reconnect(); // reconnect to WiFi network
        if (bSucceeded)
          bot.sendMessage(chat_id, "Printing completed successfully\n");
        else
          bot.sendMessage(chat_id, "Failed to print; see your sys admin for further assistance.\n");
      } else if (text == "/start" || text == "/help") {
        ShowHelpMessage(chat_id);
      } else {
        String errormsg = "Unrecognized command: " + text + "\n";
        errormsg += "Type /help for a list of valid commands\n";
        bot.sendMessage(chat_id, errormsg);
      }
    } // commands
    else { // add to the list
      char szMsg[32];
      int i = AddString((char *)text.c_str());
      sprintf(szMsg, "Item added; list length = %d\n", i);
      bot.sendMessage(chat_id, szMsg);
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

void setup()
{
  int iTimeout = 0;
  
  Serial.begin(115200);
  while (!Serial && iTimeout < 5) {
    delay(1000);
    iTimeout++;
  };

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
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

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
  Serial.println("Starting Telegram bot...");
} /* setup() */

void loop()
{
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
 //   vTaskDelay(100 / portTICK_PERIOD_MS); // allow the CPU to cool off a little while we wait
  }
}
