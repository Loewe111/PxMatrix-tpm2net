# PxMatrix TPM2.NET Program

This is a simple program to display data from TPM2.NET on a PxMatrix display.

## Requirements

You need to have PlatformIO installed in order to build this project.

## Building

First edit the credentials for your WiFi network in the `platformio.ini` file.
If you are using a different size display, you need to change the values in the `main.cpp` file. 

Then run `pio run -t upload` to build and upload the program to your ESP32, or press the upload button in VSCode.

## Usage

The program will connect to your WiFi network and then start the TPM2.NET server. You can software like [Jinx!](http://www.live-leds.de/) to send data to the display.