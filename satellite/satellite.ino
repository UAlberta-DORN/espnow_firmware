#include "satellite_config.h" // we can include a setting file specially made for this script, else compliler will use the default settings
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

esp_now_peer_info_t peerInfo;
bool hub_paired = false;

void setup() {
  delay(10);
  Serial.begin(115200);

  // we need to broadcast ourselfs anyway
  Serial.println("Satellite:");
  // Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the device in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());

  // Init ESPNow with a fallback logic
  InitESPNow();
  
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
  Serial.println("eeprom_doc is:");
  serializeJson(eeprom_doc, Serial);Serial.println("");
  if (!eeprom_doc.containsKey("header")){
    eeprom_doc=data_json;
    write_doc_to_EEPROM(0, eeprom_doc);
    
  } else {
    data_json["peers"]=eeprom_doc["peers"];
    data_json["hub_id"]=eeprom_doc["hub_id"];
    }
  Serial.println("EEPROM Setup Finished");
  uint8_t hub_mac_addr[6] = {};
  hub_mac_addr[0] = data_json["hub_id"][0].as<unsigned int>();
  hub_mac_addr[1] = data_json["hub_id"][1].as<unsigned int>();
  hub_mac_addr[2] = data_json["hub_id"][2].as<unsigned int>();
  hub_mac_addr[3] = data_json["hub_id"][3].as<unsigned int>();
  hub_mac_addr[4] = data_json["hub_id"][4].as<unsigned int>();
  hub_mac_addr[5] = data_json["hub_id"][5].as<unsigned int>();
  
  Serial.print("Hub MAC address: ");
  Serial.print(hub_mac_addr[0]);Serial.print(":");
  Serial.print(hub_mac_addr[1]);Serial.print(":");
  Serial.print(hub_mac_addr[2]);Serial.print(":");
  Serial.print(hub_mac_addr[3]);Serial.print(":");
  Serial.print(hub_mac_addr[4]);Serial.print(":");
  Serial.print(hub_mac_addr[5]);Serial.println("");
 

  
  if (data_json["hub_id"].is<int>() || (hub_mac_addr[0] == 0 && hub_mac_addr[1] == 0 && hub_mac_addr[2] == 0 && hub_mac_addr[3] == 0 && hub_mac_addr[4] == 0 && hub_mac_addr[5] == 0) ){
//  if (true){

  } else {
    Serial.print("Hub MAC address: ");
    Serial.print(hub_mac_addr[0]);Serial.print(":");
    Serial.print(hub_mac_addr[1]);Serial.print(":");
    Serial.print(hub_mac_addr[2]);Serial.print(":");
    Serial.print(hub_mac_addr[3]);Serial.print(":");
    Serial.print(hub_mac_addr[4]);Serial.print(":");
    Serial.print(hub_mac_addr[5]);Serial.println("");
    
    // Register peer
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, &hub_mac_addr, sizeof(uint8_t[6]));
    peerInfo.ifidx = ESP_IF_WIFI_AP;
    peerInfo.channel = CHANNEL;
    peerInfo.encrypt = false;



    
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
    }
    else {
      Serial.println("Hub added as peer at start up");
      hub_paired = true;
      }
  }
  

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
  DynamicJsonDocument message_doc(DEFAULT_DOC_SIZE);
  
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
    serializeJson(message_doc["header"]["DEVICE_ID"], Serial);
    Serial.println("");
    resend_counter = 10;
   }  
  if (message_doc["header"]["DEVICE_TYPE"]==0 && hub_paired == false){
    data_json["peers"]["hub"] = message_doc["header"];
    eeprom_doc=data_json;
    write_doc_to_EEPROM(0, eeprom_doc);
    
    // Register Hub as peer
    Serial.print("hub_mac_addr : "); 
    Serial.print(mac_addr[0]);Serial.print(":");
    Serial.print(mac_addr[1]);Serial.print(":");
    Serial.print(mac_addr[2]);Serial.print(":");
    Serial.print(mac_addr[3]);Serial.print(":");
    Serial.print(mac_addr[4]);Serial.print(":");
    Serial.print(mac_addr[5]);Serial.println("");
  
    Serial.println(peerInfo.peer_addr[0]);
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, mac_addr, sizeof(uint8_t[6]));
    peerInfo.channel = CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx = ESP_IF_WIFI_AP;
    
    data_json["hub_id"][0] = mac_addr[0];
    data_json["hub_id"][1] = mac_addr[1];
    data_json["hub_id"][2] = mac_addr[2];
    data_json["hub_id"][3] = mac_addr[3];
    data_json["hub_id"][4] = mac_addr[4];
    data_json["hub_id"][5] = mac_addr[5];

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
    }
    else {
      Serial.println("Hub added as peer at OnDataRecv");
      Serial.println(peerInfo.peer_addr[5]);
    }
  }
  
  write_doc_to_EEPROM(0, data_json);
  Serial.println("Written Doc to EEPROM");
  eeprom_doc=read_doc_from_EEPROM(0);
  uint8_t hub_mac_addr[6] = {};
  hub_mac_addr[0] = eeprom_doc["hub_id"][0].as<unsigned int>();
  hub_mac_addr[1] = eeprom_doc["hub_id"][1].as<unsigned int>();
  hub_mac_addr[2] = eeprom_doc["hub_id"][2].as<unsigned int>();
  hub_mac_addr[3] = eeprom_doc["hub_id"][3].as<unsigned int>();
  hub_mac_addr[4] = eeprom_doc["hub_id"][4].as<unsigned int>();
  hub_mac_addr[5] = eeprom_doc["hub_id"][5].as<unsigned int>();
  
  Serial.println("Double Check EEPROM Written");
  Serial.print("Hub MAC address: ");
  Serial.print(hub_mac_addr[0]);Serial.print(":");
  Serial.print(hub_mac_addr[1]);Serial.print(":");
  Serial.print(hub_mac_addr[2]);Serial.print(":");
  Serial.print(hub_mac_addr[3]);Serial.print(":");
  Serial.print(hub_mac_addr[4]);Serial.print(":");
  Serial.print(hub_mac_addr[5]);Serial.println("");
  
  String command = message_doc["command"].as<String>();
  if (command.indexOf("Sleep")>=0){
    int sleep_duration = extract_uint(message_doc["command"]);
    if (sleep_duration < 0){
      sleep_duration = DEFAULT_SLEEP_DURATION;
      }
    sleep(sleep_duration);
    
  } else if (command.indexOf("B")>=0){
//    Do B
  } else if (command.indexOf("Pair")>=0){
      data_json["peers"]["hub"]["DEVICE_ID"] = message_doc["header"]["DEVICE_ID"];
      eeprom_doc=data_json;
      write_doc_to_EEPROM(0, eeprom_doc);
  } else if (command.indexOf("Callback")>=0){
      data_json["command"] = "Callback";
      Serial.print("Sending commands to: "); Serial.println(*mac_addr);
      sendData(peerInfo.peer_addr, package_json(data_json));
      Serial.println("Commands sent");
  } else {
//    some thing went wrong, ask for clarification  
    Serial.print("Cannot understand command: "); 
//    Serial.println(message_doc["command"]);
 
    command_clarification(mac_addr,data_json);
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
