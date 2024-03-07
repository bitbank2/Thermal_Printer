# M5StickC Telegram bot

A telegram bot for your ESP32 [M5StickC](https://shop.m5stack.com/products/stick-c) to print shopping or to-do lists
on BLE Thermal Printers

## Arduino sketch

## Parts
* Any ESP32
* Thermal Printer: GOOJPRT PT-210, MPT-3, PeriPage+

## Links
Brian Lough
* Telegram Library written by Brian Lough
* YouTube: https://www.youtube.com/brianlough
* Tindie: https://www.tindie.com/stores/brianlough/
* Twitter: https://twitter.com/witnessmenow

Larry Bank: Thermal Printer library written by Larry Bank
* Twitter: https://twitter.com/fast_code_r_us
* Github: https://github.com/bitbank2

## Dependencies
Arduino librairies
 * UniversalTelegramBot
 * bb_spi_lcd

## Advice 
N.B
The ESP32 partitioning (Tools menu) must be set to "No OTA" to fit this sketch
in FLASH because it uses both the WiFi and BLE libraries which will pass 1MB
    
