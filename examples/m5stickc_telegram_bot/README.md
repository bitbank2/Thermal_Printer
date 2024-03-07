# M5StickC Telegram bot

A [Telegram bot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot) for your ESP32 [M5StickC](https://shop.m5stack.com/products/stick-c) to print shopping or to-do lists
on BLE Thermal Printers

## Arduino sketch

## Parts
* Any ESP32
* Thermal Printer: GOOJPRT PT-210, MPT-3, PeriPage+

## Links
Brian Lough
* [GitHub Telegram Library](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)
* [YouTube](https://www.youtube.com/brianlough)
* [Tindie](https://www.tindie.com/stores/brianlough)
* [Twitter](https://twitter.com/witnessmenow)

Larry Bank
* [Github Thermal Printer library](https://github.com/bitbank2)
* [Twitter](https://twitter.com/fast_code_r_us)

## Dependencies
Arduino librairies
 * [UniversalTelegramBot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)
 * [bb_spi_lcd](https://github.com/bitbank2/bb_spi_lcd)

## Advice 
N.B
The ESP32 partitioning (Tools menu) must be set to "No OTA" to fit this sketch
in FLASH because it uses both the WiFi and BLE libraries which will pass 1MB
    
