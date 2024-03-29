Updated this tutorial to eliminate dependency on SPIFFS (as it's cumbersome in Arduino IDE 2) and utilize the Preferences library instead.
This tutorial leverages a segment of the ESP32's built-in non-volatile memory (NVS) for data storage. 
This data persists even through system restarts and power loss events.

Additionally, a feature has been incorporated to allow resetting of this memory by holding down the BOOT button for 10 seconds
(or 3 seconds for a standard reset).

see https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/ for the original code
