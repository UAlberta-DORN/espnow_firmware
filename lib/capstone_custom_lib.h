//need arduinojson lib
#include <esp_now.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <SerialDebug.h>

/* if no specific setting file include, include the default setting  */
#ifndef DEVICE_TYPE
#include <capstone_default_settings.h>
#endif

#if DEBUG == true    //Macros are usually in all capital letters.
  #define printd(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define printlnd(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
  #define json_printd(...)  serializeJson(__VA_ARGS__, Serial);Serial.println("");
#else
  #define printd(...)     //now defines a blank line
  #define printlnd(...)   //now defines a blank line
  #define json_printd(...)   
#endif

unsigned long hash(String str_in){
    char *str=&str_in[sizeof(str_in)];
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    printlnd(hash);
    return hash;
}

void sendData(const uint8_t *mac_addr, String package);

String package_json (DynamicJsonDocument json_doc){
  json_doc.remove("peers");
  json_doc.remove("hub_id");
  json_doc.remove("children");
  
  String json_str;
  String packed_package;
  unsigned long json_hash;
 
  DynamicJsonDocument package(DEFAULT_DOC_SIZE);
  
  serializeJson(json_doc, json_str);
  json_hash=hash(json_str);
  package["json"]=json_str;
  package["hash"]=json_hash;
  serializeJson(package, packed_package);

  return packed_package; 
  
  }

bool check_package_hash (DynamicJsonDocument json_doc){
  printlnd();
  printd("Calc. hash: "); printlnd(hash(json_doc["json"]));
  printd("Inc, hash: "); json_printd(json_doc["hash"]); printlnd();
  printlnd();
  
  if (String(hash(json_doc["json"]))!=(json_doc["hash"]).as<String>()){
      return false;    
    }
  printlnd("hash matched");
  return true; 
    
  }
  
void ask_for_resend (const uint8_t *mac_addr, DynamicJsonDocument json_doc){
  json_doc["command"]="resend";
  String package;
  package = package_json (json_doc);
  
  sendData(mac_addr,package);

  }
  
void command_clarification (const uint8_t *mac_addr, DynamicJsonDocument json_doc){
  json_doc["command"]="command_clarification";
  String package;
  package = package_json (json_doc);
  
  sendData(mac_addr,package);

  }

void mac_addr_decode(String addr_str, uint8_t addr_arr[6]) {
  char str[addr_str.length()+1];
  addr_str.toCharArray(str, addr_str.length()+1);
  char* ptr;
  
  addr_arr[0] = strtol( strtok(str,":"), &ptr, HEX );
  for( int i = 1; i < 6; i++ ){
    addr_arr[i] = strtol( strtok( NULL,":"), &ptr, HEX );
    }
}

bool add_peer (uint8_t mac_addr[6], esp_now_peer_info_t peerInfo){
  Serial.print("processing mac_addr:");Serial.println(mac_addr[5]);
//  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
  memcpy(peerInfo.peer_addr, mac_addr, sizeof(uint8_t[6]));
  peerInfo.ifidx = ESP_IF_WIFI_AP;
  peerInfo.channel = CHANNEL;
  peerInfo.encrypt = false;
//  esp_err_t add_peer_status = esp_now_add_peer(&peerInfo);
//  
//  if (add_peer_status != ESP_OK || add_peer_status != ESP_ERR_ESPNOW_EXIST){
//    Serial.println("Failed to add peer");
//    Serial.println(add_peer_status);
//    return false;
//  }
//  else {
//    Serial.println("Device Paired");
//    return true;
//  }
  bool exists = esp_now_is_peer_exist(peerInfo.peer_addr);
  if (exists) {
    // Slave already paired.
    printlnd("Already Paired");
    return true;
  } else {
    // Slave not paired, attempt pair
    esp_err_t addStatus = esp_now_add_peer(&peerInfo);
    if (addStatus == ESP_OK) {
      // Pair success
      printlnd("Pair success");

      bool exists = esp_now_is_peer_exist(peerInfo.peer_addr);
      Serial.print("Status of peer:");Serial.println(exists);
      
      return true;
    } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
      // How did we get so far!!
      printlnd("ESPNOW Not Init");
    } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
      printlnd("Add Peer - Invalid Argument");
    } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
      printlnd("Peer list full");
    } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
      printlnd("Out of memory");
    } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
      printlnd("Peer Exists");
    } else {
      printlnd("Not sure what happened");
    }
    return false;
    delay(50);
  }
}



DynamicJsonDocument unpackage_json (DynamicJsonDocument json_doc){
  DynamicJsonDocument out_doc(DEFAULT_DOC_SIZE);
  printd("Deserialize: ");
  deserializeJson(out_doc, json_doc["json"]);
  return out_doc; 
  }

void process_rpi_command (DynamicJsonDocument rpi_command, DynamicJsonDocument data_json, esp_now_peer_info_t peerInfo){
    for (int i=0;i<rpi_command.size();i++){
      Serial.print("i=");Serial.println(i);
      Serial.print("rpi_command[i]['id'].size()=");Serial.println(rpi_command[i]["id"].size());
      String command = rpi_command[i]["command"].as<String>();
      for (int j=0;j<rpi_command[i]["id"].size();j++){
        data_json["command"] = command;
        Serial.print("j=");Serial.println(j);
        uint8_t mac_addr[6];
        mac_addr_decode(rpi_command[i]["id"][j], mac_addr);
//          uint8_t mac_addr[] = {0x7C, 0x9E, 0xBD, 0xF4, 0x06, 0x68};
        //  Serial.println(*hub_mac_addr);
        Serial.print("Target MAC address: ");
        Serial.print(mac_addr[0]);Serial.print(":");
        Serial.print(mac_addr[1]);Serial.print(":");
        Serial.print(mac_addr[2]);Serial.print(":");
        Serial.print(mac_addr[3]);Serial.print(":");
        Serial.print(mac_addr[4]);Serial.print(":");
        Serial.print(mac_addr[5]);Serial.println("");

        add_peer(mac_addr, peerInfo);
        bool exists = esp_now_is_peer_exist(mac_addr);
        Serial.print("Status of peer:");Serial.println(exists);
        
        sendData(mac_addr, package_json(data_json));
        data_json["command"] = "";
        delay(50);
      }
   }
}

void write_doc_to_EEPROM(unsigned long address, DynamicJsonDocument eeprom_json_doc){
  printlnd("executing write_doc_to_EEPROM......");
  String eeprom_str;
  serializeJson(eeprom_json_doc, eeprom_str);
  address = address;
  unsigned long eeprom_size = EEPROM_SIZE - 1;
  
  printd("EEPROM Start Address: ");printlnd(address);
  printd("EEPROM End Address: ");printlnd(eeprom_size);
  
  for(int i=0;i<eeprom_size;i++)
  {
    EEPROM.write(address+i, eeprom_str[i]);
//    delay(100);
//    printlnd("");
//    printd(address+i);
//    printd(" : ");
//    printd(eeprom_str[i]);
//    printlnd("");
  }
  EEPROM.write(address + eeprom_size,'\0');
  EEPROM.commit();
}


DynamicJsonDocument read_doc_from_EEPROM(unsigned long address)
{
  printlnd("executing read_doc_from_EEPROM......");
  char eeprom_str[EEPROM_SIZE];
  int i=0;
  unsigned char k;
  address = address;
  unsigned long eeprom_size = EEPROM_SIZE;
  k = EEPROM.read(address);
  eeprom_str[i] = byte(k);
  printd(k);
  while(i < eeprom_size - 1){
    k = EEPROM.read(address + i);
    eeprom_str[i] = byte(k);
//    printlnd("");
//    printd(address+i);
//    printd(" : ");
//    printd(eeprom_str[i]);
//    printlnd("");
    i++;
//    delay(100);
  }
  eeprom_str[i]='\0';
  
  DynamicJsonDocument eeprom_json_doc(EEPROM_SIZE);
  deserializeJson(eeprom_json_doc, String(eeprom_str));
  
  return eeprom_json_doc;
}

DynamicJsonDocument init_doc(DynamicJsonDocument doc) {
  doc["header"]["DEVICE_TYPE"] = DEVICE_TYPE;
  doc["header"]["POWER_SOURCE"] = POWER_SOURCE;
  doc["header"]["DEVICE_ID"] = DEVICE_ID;
  doc["header"]["LOCAL_TIME"] = millis();
  doc["peers"]["hub"]="";
  doc["data"]["temp"] = FILLER_VAL;
  doc["data"]["light"] = FILLER_VAL;
  
  return doc;
  }

DynamicJsonDocument decode_espnow_message (const uint8_t* message, int message_len){
  printlnd("decode_espnow_message...");
  DynamicJsonDocument out_doc(DEFAULT_DOC_SIZE);
  printlnd("message_str...");
//  printlnd(message);

  char* message_char[message_len];
  *message_char = (char*) message;  
  String message_str;
  message_str = *message_char;
  printlnd(message_str);
  printd("message_str: ");printlnd(message_str);
  printlnd("deserializeJson...");
  deserializeJson(out_doc, message_str);
  return out_doc;
  }

int count_slaves(esp_now_peer_info_t *slaves){
  int slave_count=0;
  bool count_finished=false;
  for (int i = 0; i < NUMSLAVES; i++) {
    int zero_combo=0;
//    printlnd(*slaves[i].peer_addr);
//    if (esp_now_is_peer_exist(slaves[i].peer_addr)){
    if ((*slaves[i].peer_addr) != 0){
        slave_count++;
      }
//    else{return slave_count;}
  }
  return slave_count;
}

int extract_uint (String str) {
  char charBuf[str.length()+1];
  str.toCharArray(charBuf, str.length()+1);

  String out_int = "";
  bool active = false;
  
  for (int i=0; i<str.length()+1; i++ ){
     if (isDigit(charBuf[i])){
        out_int+=charBuf[i];
        active = true;
      } else {
        if (active){
            return out_int.toInt();
          }
      }
    }
  return -1;
}

void sleep (int seconds_to_sleep) {
  printd("Going to sleep for ");printd(seconds_to_sleep);printlnd(" seconds");
  esp_sleep_enable_timer_wakeup(seconds_to_sleep*1000*1000);
  esp_deep_sleep_start();
}
  
/////////////////////////////////////////////////////////////////////////////////////////////////
//from espnow examples:

// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    printlnd("ESPNow Init Success");
  }
  else {
    printlnd("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  String Prefix = "Slave:";
  String Mac = DEVICE_ID;
  String SSID = Prefix + DEVICE_ID;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
  if (!result) {
    printlnd("AP Config failed.");
  } else {
    printlnd("AP Config Success. Broadcasting with AP: " + String(SSID));
  }
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// modified from the espnow examples

esp_now_peer_info_t ScanForSlave() {

  int8_t scanResults = WiFi.scanNetworks();
  //reset slaves
  esp_now_peer_info_t slaves[NUMSLAVES] = {};
  memset(slaves, 0, sizeof(slaves));
  int SlaveCnt = 0;
  
  printlnd("");
  if (scanResults == 0) {
    printlnd("No WiFi devices in AP Mode found");
  } else {
    printd("Found "); printd(scanResults); printlnd(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        printd(i + 1); printd(": "); printd(SSID); printd(" ["); printd(BSSIDstr); printd("]"); printd(" ("); printd(RSSI); printd(")"); printlnd("");
      }
      delay(10);
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Slave") == 0) {
        // SSID of interest
        printd(i + 1); printd(": "); printd(SSID); printd(" ["); printd(BSSIDstr); printd("]"); printd(" ("); printd(RSSI); printd(")"); printlnd("");
        // Get BSSID => Mac Address of the Slave
        int mac[6];

        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slaves[SlaveCnt].peer_addr[ii] = (uint8_t) mac[ii];
          }
        }
        slaves[SlaveCnt].channel = CHANNEL; // pick a channel
        slaves[SlaveCnt].encrypt = 0; // no encryption
        SlaveCnt++;
      }
    }
  }

  if (SlaveCnt > 0) {
    printd(SlaveCnt); printlnd(" Slave(s) found, processing..");
  } else {
    printlnd("No Slave Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
  
  return *slaves;
}

// Check if the slave is already paired with the master.
// If not, pair the slave with master
void manageSlave(esp_now_peer_info_t* slaves) {
  int SlaveCnt = count_slaves(slaves);
  char str[8];
  if (SlaveCnt > 0) {
    for (int i = 0; i < SlaveCnt; i++) {
      printd("Processing: ");
      for (int ii = 0; ii < 6; ++ii ) {
        sprintf(str,"%x",(uint8_t) slaves[i].peer_addr[ii]);
        printd(str);
        if (ii != 5) printd(":");
      }
      printd(" Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(slaves[i].peer_addr);
      if (exists) {
        // Slave already paired.
        printlnd("Already Paired");
      } else {
        // Slave not paired, attempt pair
        esp_err_t addStatus = esp_now_add_peer(&slaves[i]);
        if (addStatus == ESP_OK) {
          // Pair success
          printlnd("Pair success");
        } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
          // How did we get so far!!
          printlnd("ESPNOW Not Init");
        } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
          printlnd("Add Peer - Invalid Argument");
        } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
          printlnd("Peer list full");
        } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
          printlnd("Out of memory");
        } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
          printlnd("Peer Exists");
        } else {
          printlnd("Not sure what happened");
        }
        delay(100);
      }
    }
  } else {
    // No slave found to process
    printlnd("No Slave found to process");
  }
}

// send data
void sendData(const uint8_t *mac_addr, String package) {
  
  printd("Sending: ");
  printlnd(package);
  //encode the string package
  int str_len = package.length() + 1;
  char package_char_array[str_len];
  package.toCharArray(package_char_array, str_len);
//  uint8_t* encoded_package[str_len] = {reinterpret_cast<const uint8_t*>(package.c_str())};  

  uint8_t encoded_package[str_len] = {};
//  encoded_package = (uint8_t*) &package_char_array;
//  memcpy(&encoded_package, &package_char_array, str_len);
  for (int i = 0; i < str_len; i++) {
    encoded_package[i] = (uint8_t)(package_char_array[i]);
  }
//  printd("package_char_array: "); printlnd(package_char_array);
//  printd("encoded_package: "); printlnd((char*) encoded_package);
  printd("Sending to mac_addr : "); 
  printd(mac_addr[0]);printd(":");
  printd(mac_addr[1]);printd(":");
  printd(mac_addr[2]);printd(":");
  printd(mac_addr[3]);printd(":");
  printd(mac_addr[4]);printd(":");
  printd(mac_addr[5]);printlnd("");
  
  printd("encoded_package size: "); printlnd(sizeof(encoded_package));
  
  esp_err_t result = esp_now_send(mac_addr, encoded_package, sizeof(encoded_package));
  printd("Send Status: ");
  if (result == ESP_OK) {
    printlnd("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    printlnd("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    printlnd("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    printlnd("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    printlnd("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    printlnd("Peer not found.");
  } else {
    printlnd("Not sure what happened");
    }
    delay(100);
}
/////////////////////////////////////////////////////////
//
