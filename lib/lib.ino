#include "capstone_default_settings.h"
#include "capstone_custom_lib.h"


//DynamicJsonDocument eeprom_doc(600);
DynamicJsonDocument data_json(DEFAULT_DOC_SIZE);
StaticJsonDocument<EEPROM_SIZE> eeprom_doc;
void setup() {
  // Initialize Serial port
  Serial.begin(115200);
  while (!Serial) continue;

  
//  EEPROM.begin(EEPROM_SIZE);
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(50);
    ESP.restart();
  }
  
  eeprom_doc=read_doc_from_EEPROM(0);
  Serial.println("Wake Up, Read from EEPROM: ");
  serializeJson(eeprom_doc, Serial);Serial.println("");
  Serial.println("");
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

}
