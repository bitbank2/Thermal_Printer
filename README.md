Bluetooth Low Energy<br>
Thermal Printer Library<br>
Copyright (c) 2020 BitBank Software, Inc.<br>
Written by Larry Bank<br>
bitbank@pobox.com<br>
<br>
![printer demo](/ble_prt.jpg?raw=true "Thermal_Printer")

<br>
This Arduino library allows you to easily generate text and graphics
and send them to various BLE thermal printers. I can support the printers that I currently own; if your model is not yet supported, the best way to encourage me to support it is to send me funds to buy it.<br>
There are many different
BLE APIs depending on the board manufacturer, I support the three more
popular ones: ESP32, Adafruit and Arduino (e.g. Nano BLE 33). The three main features of
thermal printers are supported by this code - plain text, barcodes (1D+2D) and dot addressable
graphics. The graphics are treated like a display driver - you define a RAM buffer and draw text, dots, lines and bitmaps into it, then send it to the printer. Text output supports the various built-in font size+attribute options of some printers as well as Adafruit_GFX format bitmap fonts (sent as graphics).
See the Wiki: https://github.com/bitbank2/Thermal_Printer/wiki/Public-API for a description of each function.
<br>

<br>
Features<br>
========<br>
- Supports the GOOJPRT PT-210, MTP-3, PeriPage+ and 'cat' printers (so far)<br>
- Compiles on ESP32, Adafruit nRF52 and Arduino boards with bluetooth<br>
- Supports graphics (dots, lines, text, bitmaps), 1D + 2D barcodes, and plain text output<br>
- Allows printing Adafruit_GFX fonts one line at a time or drawing them into a RAM buffer<br>
- Can scan/connect to printers by BLE name or auto-detect the supported models<br>
- Doesn't depend on any other 3rd party code<br>
<br>

Here is a subjective chart of the printer models I've tested and are supported by this code. Please feel free to send me info about other models that work and additional comments about these printers.<br>

<br>
<p align="center">
  <img width="939" height="203" src="https://github.com/bitbank2/Thermal_Printer/blob/master/printer_chart.jpg?raw=true">
</p>
<br>

If you find this code useful, please consider becoming a Github Sponsor or sending a single donation.

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SR4F44J2UR8S4)
