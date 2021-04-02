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
  printlnd("Satellite:");
  // Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the device in AP Mode
  printd("AP MAC: "); printlnd(WiFi.softAPmacAddress());

  // Init ESPNow with a fallback logic
  InitESPNow();
  
  data_json=init_doc(data_json);
  
  json_printd(data_json, Serial);
  printlnd();
  
  if (!EEPROM.begin(EEPROM_SIZE)) {
    printlnd("Failed to initialise EEPROM");
    printlnd("Restarting...");
    delay(50);
    ESP.restart();
  }

  eeprom_doc=read_doc_from_EEPROM(0);
  printlnd("eeprom_doc is:");
  json_printd(eeprom_doc, Serial);printlnd("");
  if (!eeprom_doc.containsKey("header")){
    eeprom_doc=data_json;
    write_doc_to_EEPROM(0, eeprom_doc);
    
  } else {
    data_json["peers"]=eeprom_doc["peers"];
    data_json["hub_id"]=eeprom_doc["hub_id"];
    }
  printlnd("EEPROM Setup Finished");
  uint8_t hub_mac_addr[6] = {};
  hub_mac_addr[0] = data_json["hub_id"][0].as<unsigned int>();
  hub_mac_addr[1] = data_json["hub_id"][1].as<unsigned int>();
  hub_mac_addr[2] = data_json["hub_id"][2].as<unsigned int>();
  hub_mac_addr[3] = data_json["hub_id"][3].as<unsigned int>();
  hub_mac_addr[4] = data_json["hub_id"][4].as<unsigned int>();
  hub_mac_addr[5] = data_json["hub_id"][5].as<unsigned int>();
  
  printd("Hub MAC address: ");
  printd(hub_mac_addr[0]);printd(":");
  printd(hub_mac_addr[1]);printd(":");
  printd(hub_mac_addr[2]);printd(":");
  printd(hub_mac_addr[3]);printd(":");
  printd(hub_mac_addr[4]);printd(":");
  printd(hub_mac_addr[5]);printlnd("");
 

  
  if (data_json["hub_id"].is<int>() || (hub_mac_addr[0] == 0 && hub_mac_addr[1] == 0 && hub_mac_addr[2] == 0 && hub_mac_addr[3] == 0 && hub_mac_addr[4] == 0 && hub_mac_addr[5] == 0) ){
//  if (true){

  } else {
    printd("Hub MAC address: ");
    printd(hub_mac_addr[0]);printd(":");
    printd(hub_mac_addr[1]);printd(":");
    printd(hub_mac_addr[2]);printd(":");
    printd(hub_mac_addr[3]);printd(":");
    printd(hub_mac_addr[4]);printd(":");
    printd(hub_mac_addr[5]);printlnd("");
    
    // Register peer
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, &hub_mac_addr, sizeof(uint8_t[6]));
    peerInfo.ifidx = ESP_IF_WIFI_AP;
    peerInfo.channel = CHANNEL;
    peerInfo.encrypt = false;



    
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      printlnd("Failed to add peer");
    }
    else {
      printlnd("Hub added as peer at start up");
      hub_paired = true;
      }
  }
  

  // Once ESPNow is successfully Init, we will register for recv and send CB to
  // get recv/send packer info.
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  printd("freeMemory() after setup = ");
  printlnd(getAllHeap());
}


  
// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  printd("freeMemory() OnDataRecv = ");
  printlnd(getAllHeap());
  
  printlnd(*data);
  printlnd((char*) data);
  
  char macStr[18];
  printd(mac_addr[0]);printd(":");
  printd(mac_addr[1]);printd(":");
  printd(mac_addr[2]);printd(":");
  printd(mac_addr[3]);printd(":");
  printd(mac_addr[4]);printd(":");
  printd(mac_addr[5]);printlnd("");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
           
//  StaticJsonDocument<400> message_doc;
  DynamicJsonDocument message_doc(DEFAULT_DOC_SIZE);
  
  message_doc=decode_espnow_message(data, data_len);
  printd("message_doc: ");
  json_printd(message_doc, Serial);
  printlnd();
  String message = message_doc["json"].as<String>();
  printlnd(message);
  printlnd("Test 2");
  
  if (resend_counter < 0){
      resend_counter = 10;
    }
  
  if (resend_counter>0){
    if (check_package_hash(message_doc)){
        DeserializationError err =  deserializeJson(message_doc, message);
        printd("message_doc: ");
        json_printd(message_doc, Serial); printlnd();
        if (err) {
          printd(F("deserializeJson() failed: "));
          printlnd(err.c_str());
          }
        
        resend_counter=0;
      } else {
        printlnd("Asking for resend...");
        ask_for_resend(mac_addr,data_json);
        return;
      }
    resend_counter--;
  } 
  
  printd("Last Packet Recv from: "); printlnd(macStr);
  if (resend_counter==0){
    printlnd("Error: ckecksum not matching");
    printd("Last Packet Recv Data: "); 
    printlnd(*data);
    printlnd("");
    } else {
    printd("Decoded Message:"); 
    json_printd(message_doc, Serial);
    printlnd("");
    printlnd("");
    json_printd(message_doc["peers"]["hub"], Serial);
    printlnd("");
    json_printd(message_doc["header"]["DEVICE_ID"], Serial);
    printlnd("");
    resend_counter = 10;
   }  
  if (message_doc["header"]["DEVICE_TYPE"]==0 && hub_paired == false){
    data_json["peers"]["hub"] = message_doc["header"];
    eeprom_doc=data_json;
    write_doc_to_EEPROM(0, eeprom_doc);
    
    // Register Hub as peer
    printd("hub_mac_addr : "); 
    printd(mac_addr[0]);printd(":");
    printd(mac_addr[1]);printd(":");
    printd(mac_addr[2]);printd(":");
    printd(mac_addr[3]);printd(":");
    printd(mac_addr[4]);printd(":");
    printd(mac_addr[5]);printlnd("");
  
    printlnd(peerInfo.peer_addr[0]);
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
      printlnd("Failed to add peer");
    }
    else {
      printlnd("Hub added as peer at OnDataRecv");
      printlnd(peerInfo.peer_addr[5]);
    }
  }
  
  write_doc_to_EEPROM(0, data_json);
  printlnd("Written Doc to EEPROM");
  eeprom_doc=read_doc_from_EEPROM(0);
  uint8_t hub_mac_addr[6] = {};
  hub_mac_addr[0] = eeprom_doc["hub_id"][0].as<unsigned int>();
  hub_mac_addr[1] = eeprom_doc["hub_id"][1].as<unsigned int>();
  hub_mac_addr[2] = eeprom_doc["hub_id"][2].as<unsigned int>();
  hub_mac_addr[3] = eeprom_doc["hub_id"][3].as<unsigned int>();
  hub_mac_addr[4] = eeprom_doc["hub_id"][4].as<unsigned int>();
  hub_mac_addr[5] = eeprom_doc["hub_id"][5].as<unsigned int>();
  
  printd("Hub MAC address: ");
  printd(hub_mac_addr[0]);printd(":");
  printd(hub_mac_addr[1]);printd(":");
  printd(hub_mac_addr[2]);printd(":");
  printd(hub_mac_addr[3]);printd(":");
  printd(hub_mac_addr[4]);printd(":");
  printd(hub_mac_addr[5]);printlnd("");

//  For testing: (always pair when connected)
  data_json["peers"]["hub"]["DEVICE_ID"] = message_doc["header"]["DEVICE_ID"];
  
  data_json["hub_id"][0] = mac_addr[0];
  data_json["hub_id"][1] = mac_addr[1];
  data_json["hub_id"][2] = mac_addr[2];
  data_json["hub_id"][3] = mac_addr[3];
  data_json["hub_id"][4] = mac_addr[4];
  data_json["hub_id"][5] = mac_addr[5];
  
  eeprom_doc=data_json;
  write_doc_to_EEPROM(0, data_json);
//  End testing 
  
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
      
      data_json["hub_id"][0] = mac_addr[0];
      data_json["hub_id"][1] = mac_addr[1];
      data_json["hub_id"][2] = mac_addr[2];
      data_json["hub_id"][3] = mac_addr[3];
      data_json["hub_id"][4] = mac_addr[4];
      data_json["hub_id"][5] = mac_addr[5];
      
      eeprom_doc=data_json;
      write_doc_to_EEPROM(0, data_json);
  } else if (command.indexOf("Callback")>=0){
      data_json["command"] = "Callback";
      printd("Sending commands to: "); printlnd(*mac_addr);
      sendData(peerInfo.peer_addr, package_json(data_json));
      printlnd("Commands sent");
  } else {
//    some thing went wrong, ask for clarification  
    printd("Cannot understand command: "); 
//    printlnd(message_doc["command"]);
 
    command_clarification(mac_addr,data_json);
  }
}

// callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  printd("Last Packet Sent to: "); printlnd(macStr);
  printd("Last Packet Send Status: "); printlnd(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  
}

void loop() {
  
}
