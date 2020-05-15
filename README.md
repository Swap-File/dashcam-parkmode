# dashcam-parkmode

Targets an ESP32 feather with an optoisolator hooked to car acc power on pin 33 and a power output on pin 32.  Config jumper is on pin 27, short to ground to enable config.

Compile with PlatformIO, requires ESP32 IDF 1.0.9 for the Captive Portal to correctly work, newer versions crash.

Basically piggybacks on top of a b-124x.

Draws around 0.3ma when idle.

Make sure to cut ground wire and data lines on the USB cable powering the ESP32, and tie D- to ground (ESP32 side) with a 10k for power savings.

b-124x compatible cables can be found here: https://www.digikey.com/products/en?keywords=WM12449-ND
