# dashcam-parkmode

Targets an ESP32 feather with an optoisolator hooked to car acc power on pin 33 and a power output on pin 32.

Basically piggybacks on top of a b-124x.

Draws around 0.3ma when idle.

Make sure to cut ground wire and data lines on the USB cable powering the ESP32, and tie D- to ground (ESP32 side) with a 10k for power savings.
