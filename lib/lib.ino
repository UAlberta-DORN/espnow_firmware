#include "capstone_custom_lib.h"
#include "capstone_default_settings.h"

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
char val = 'C';
void loop() {
//   put your main code here, to run repeatedly:
  write_doc_to_EEPROM(0, data_json);
  Serial.println("Written Doc to EEPROM:");
  serializeJson(data_json, Serial);Serial.println("");
  
  eeprom_doc=read_doc_from_EEPROM(0);
  Serial.println("Read from EEPROM:");
  serializeJson(eeprom_doc, Serial);Serial.println("");
//  Serial.print("val = ");Serial.print(val);Serial.println("");
//  EEPROM.write(0, val);
//  EEPROM.commit();
//  EEPROM.write(1, val);
//  EEPROM.commit();
//  Serial.println("Stored in EEPROM");
//  Serial.print("read_val = ");Serial.print(byte(EEPROM.read(0)));Serial.println("");
//  Serial.print("read_val = ");Serial.print(byte(EEPROM.read(1)));Serial.println("");
  Serial.println("Going to sleep");
  esp_sleep_enable_timer_wakeup(1000);
  esp_deep_sleep_start();
}
