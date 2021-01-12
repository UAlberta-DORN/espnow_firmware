//#include "capstone_settings.h" // we can include a setting file specially made for this script, else compliler will use the default settings
#include <custom_capstone_lib.h>
int resend_counter=10;

//StaticJsonDocument<EEPROM_SIZE> eeprom_doc;
DynamicJsonDocument eeprom_doc(EEPROM_SIZE);

String getAllHeap(){
    char temp[300];
    sprintf(temp, "Heap: Free:%i, Min:%i, Size:%i, Alloc:%i", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap());
    return temp;
}

DynamicJsonDocument data_json(DEFAULT_DOC_SIZE);
//StaticJsonDocument<DEFAULT_DOC_SIZE> data_json;

void setup() {
  Serial.begin(115200);
  
  data_json=init_doc(data_json);
  
  serializeJson(data_json, Serial);
  Serial.println();
  
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(50);
    ESP.restart();
  }

  eeprom_doc=read_doc_from_EEPROM(0);

  if (!eeprom_doc.containsKey("header")){
    eeprom_doc=data_json;
    write_doc_to_EEPROM(0, eeprom_doc);
    
  } else {
    data_json["peers"]=eeprom_doc["peers"];
    }
  Serial.println("EEPROM Setup Finished");
  uint8_t hub_mac_addr = data_json["peers"]["hub"]["header"]["DEVICE_ID"];
  if ((type_of(hub_mac_addr)!="uint8_t") || hub_mac_addr == 0){
//  if (true){
    //    if we dont know the hub address, we need to broadcast ourselfs 
    Serial.println("Satellite:");
    //Set device in AP mode to begin with
    WiFi.mode(WIFI_AP);
    // configure device AP mode
    configDeviceAP();
    
    // This is the mac address of the Master in Station Mode
    Serial.print("AP MAC: "); Serial.println(WiFi.macAddress());
  } else {
    Serial.print("Hub MAC address: ");Serial.println(hub_mac_addr);
    }
  
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv and send CB to
  // get recv/send packer info.
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  Serial.print("freeMemory() after setup = ");
  Serial.println(getAllHeap());
}


  
// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  Serial.print("freeMemory() OnDataRecv = ");
  Serial.println(getAllHeap());
  
  Serial.println(*data);
  Serial.println((char*) data);
  
  char macStr[18];
  Serial.print(mac_addr[0]);Serial.print(":");
  Serial.print(mac_addr[1]);Serial.print(":");
  Serial.print(mac_addr[2]);Serial.print(":");
  Serial.print(mac_addr[3]);Serial.print(":");
  Serial.print(mac_addr[4]);Serial.print(":");
  Serial.print(mac_addr[5]);Serial.println("");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
           
//  StaticJsonDocument<400> message_doc;
  DynamicJsonDocument message_doc(400);
  
  message_doc=decode_espnow_message(data, data_len);
  Serial.print("message_doc: ");
  serializeJson(message_doc, Serial);
  Serial.println();
  String message = message_doc["json"].as<String>();
  Serial.println(message);
  Serial.println("Test 2");
  
  if (resend_counter < 0){
      resend_counter = 10;
    }
  
  if (resend_counter>0){
    if (check_package_hash(message_doc)){
        DeserializationError err =  deserializeJson(message_doc, message);
        Serial.print("message_doc: ");
        serializeJson(message_doc, Serial); Serial.println();
        if (err) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(err.c_str());
          }
        
        resend_counter=0;
      } else {
        Serial.println("Asking for resend...");
        ask_for_resend(mac_addr,data_json);
        return;
      }
    resend_counter--;
  } 
  
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  if (resend_counter==0){
    Serial.println("Error: ckecksum not matching");
    Serial.print("Last Packet Recv Data: "); 
    Serial.println(*data);
    Serial.println("");
    } else {
    Serial.print("Decoded Message:"); 
    serializeJson(message_doc, Serial);
    Serial.println("");
    Serial.println("");
    serializeJson(message_doc["peers"]["hub"], Serial);
    Serial.println("");
    resend_counter = 10;
   }  
  if (message_doc["header"]["DEVICE_TYPE"]==0){
    data_json["peers"]["hub"] = message_doc["header"];
    eeprom_doc=data_json;
    write_doc_to_EEPROM(0, eeprom_doc);
  }
  
  String command = message_doc["command"].as<String>();
  
  if (command.indexOf("sleep")>=0){
    sleep(extract_uint(message_doc["command"]));
  } else if (command.indexOf("B")>=0){
//    Do B
  } else if (command.indexOf("Pair")>=0){
      data_json["peers"]["hub"]["DEVICE_ID"] = message_doc["header"]["DEVICE_ID"];
      eeprom_doc=data_json;
      write_doc_to_EEPROM(0, eeprom_doc);
  } else if (command.indexOf("Callback")>=0){
      data_json["command"] = "Callback";
      Serial.print("Sending commands to: "); Serial.println(*mac_addr);
      sendData(mac_addr, package_json(data_json));
      Serial.println("Commands sent");
  } else {
//    some thing went wrong, ask for clarification  
    Serial.print("Cannot understand command: "); 
//    Serial.println(message_doc["command"]);

    uint8_t mac_addr = data_json["peers"]["hub"]["DEVICE_ID"];
    command_clarification(&mac_addr,data_json);
  }
  
}

// callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void loop() {

}
