#define DEVICE_TYPE 0 // Hub = 0, Seneor = 1, switch = 2, 
#define POWER_SOURCE 0 // wall power= 0, battery = 1
#define DEVICE_ID String(random(0xffff), HEX) // use mac address in the future: WiFi.macAddress()
#define FILLER_VAL -273.15
#define DEFAULT_DOC_SIZE 256
#define EEPROM_SIZE 512
#define CHANNEL 1
#define NUMSLAVES 20
#define PRINTSCANRESULTS 0
