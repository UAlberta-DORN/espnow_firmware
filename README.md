# ESPnow Firmware

To code ESP32 on the Arduino IDE, follow [this tutorial](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)

## Additional Dependencies:

#### installation required
- [ArduinoJson](https://arduinojson.org/)
  - install it via Arduino IDE's Library Manager

#### regular dependencies:
  - esp_now
  - EEPROM
  - WiFi

## Install the Custom Capstone Library
Click on the "install_and_update_lib.bat" file (it copies the files to the Arduino library folder)

***This needs to be done for every single update.***

## Sample Response From the Hub
This is a sample of what will be sent to the Raspberry Pi:
```yaml
{
  "header": {
    "DEVICE_TYPE": 1,
    "POWER_SOURCE": 0,
    "DEVICE_ID": "5A:9E:AD:F5:06:69"
  },
  "data": {
    "temp": -273.15,
    "light": -273.15
  },
  "children": {
    "9C:9E:BD:F4:06:69": {
      "header": {
        "DEVICE_TYPE": 1,
        "POWER_SOURCE": 0,
        "DEVICE_ID": "9C:9E:BD:F4:06:69"
      },
      "data": {
        "temp": -273.15,
        "light": -273.15
      }
    },
    "7C:9E:BD:F4:06:68": {
      "header": {
        "DEVICE_TYPE": 1,
        "POWER_SOURCE": 0,
        "DEVICE_ID": "7C:9E:BD:F4:06:68"
      },
      "data": {
        "temp": -273.15,
        "light": -273.15
      }
    }
  }
}
```
