#include "capstone_custom_lib.h"
#include "capstone_default_settings.h"

void setup() {
  // Initialize Serial port
  Serial.begin(9600);
  while (!Serial) continue;

  StaticJsonDocument <EEPROM_SIZE> eeprom_doc;
//  EEPROM.begin(EEPROM_SIZE);
  eeprom_doc = EEPROM.read(0);
  
  
  StaticJsonDocument<DEFAULT_DOC_SIZE> data_json;
  data_json=init_doc(data_json);
  
  data_json["data"]["temp"] = 35;
  data_json["data"]["light"] = 250;
  serializeJson(data_json, Serial);
  Serial.println();



  String output;
  serializeJson(data_json, output);
  Serial.println(output);

  Serial.println(sizeof(package_json (data_json)));
  
//  serializeJsonPretty(doc, Serial);
}

void loop() {
  // put your main code here, to run repeatedly:

}
