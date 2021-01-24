//#include "capstone_settings.h" // we can include a setting file specially made for this script, else compliler will use the default settings
#include <custom_capstone_lib.h>

int resend_counter=0;
DynamicJsonDocument data_json(DEFAULT_DOC_SIZE);

void setup() {
  Serial.begin(115200);
  Serial.println("Hub:");
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_AP);

  // This is the mac address of the Master in Station Mode
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
   
  
  data_json=init_doc(data_json);

  serializeJson(data_json, Serial);
  Serial.println();
  
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
}


  
// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  Serial.println("Data Recived!");Serial.println("Data Recived!");Serial.println("Data Recived!");Serial.println("Data Recived!");
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
           
  DynamicJsonDocument message_doc(400);
  message_doc=decode_espnow_message(data, data_len);
  
  if (resend_counter==0){
      resend_counter = 10;
    }
  
  if (resend_counter>0){
    if (check_package_hash(message_doc)){
        message_doc=unpackage_json(message_doc);
        resend_counter=0;
      } else {
        ask_for_resend(mac_addr,data_json);
      }
    resend_counter--;
  } 
  
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  if (resend_counter==-1){
    Serial.println("Error: ckecksum not matching");
    Serial.print("Last Packet Recv Data: "); 
    Serial.println(*data);
    Serial.println("");
    } else {
    Serial.print("Decoded Message:"); 
    serializeJson(message_doc, Serial);
    Serial.println("");
    resend_counter=0;
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

esp_now_peer_info_t slaves[NUMSLAVES] = {};
int SlaveCnt = 0; 
int loop_counter =0;
void loop() {
  Serial.print("Loop number "); Serial.println(loop_counter);
  // In the loop we scan for slave
  *slaves = ScanForSlave();
  // If Slave is found, it would be populate in `slave` variable
  // We will check if `slave` is defined and then we proceed further
  SlaveCnt=count_slaves(slaves);
  // TODO: implement eeprom 
  Serial.print("Found "); Serial.print(SlaveCnt); Serial.println(" slave(s)");
  Serial.print("Slave addr = "); Serial.println(*(slaves[0].peer_addr));
  if (SlaveCnt > 0 & loop_counter>5) { // check if slave channel is defined
    loop_counter = 0;
    // `slave` is defined
    // Add slave as peer if it has not been added already
    manageSlave(slaves);
    // pair success or already paired
    // Send data to device
    for (int i = 0; i < SlaveCnt; i++) {
      data_json["command"] = "Callback";
      uint8_t broadcastAddress[] = {0x7C, 0x9E, 0xBD, 0xF4, 0x06, 0x69};
//      Serial.print("Sending commands to: "); Serial.println(*(slaves[i].peer_addr));
//      sendData(slaves[i].peer_addr, package_json(data_json));

      Serial.print("Sending commands to: "); Serial.println(*(broadcastAddress));
      sendData(slaves[i].peer_addr, package_json(data_json));
      Serial.println("Commands sent");
    }
  } else {
    // No slave found to process
  }

  loop_counter++;
}
