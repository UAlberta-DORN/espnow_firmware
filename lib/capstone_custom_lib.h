//need arduinojson lib
#include <esp_now.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <WiFi.h>

/* if no specific setting file include, include the default setting  */
#ifndef DEVICE_TYPE
#include <capstone_default_settings.h>
#endif

unsigned long hash(String str_in){
    char *str=&str_in[sizeof(str_in)];
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    Serial.println(hash);
    return hash;
}


void sendData(const uint8_t *mac_addr, String package);

//String type_of(String a) { return "String"; }
//String type_of(int a) { return "int"; }
//String type_of(char *a) { return "char"; }
//String type_of(float a) { return "float"; }
//String type_of(bool a) { return "bool"; }
//String type_of(StaticJsonDocument<DEFAULT_DOC_SIZE> a) { return "Json"; }
String type_of(uint8_t a) { return "uint8_t"; }
//String type_of(const uint8_t a) { return "uint8_t"; }

String package_json (StaticJsonDocument<DEFAULT_DOC_SIZE> json_doc){
  String json_str;
  String packed_package;
  unsigned long json_hash;
  StaticJsonDocument<DEFAULT_DOC_SIZE> package;
  
  serializeJson(json_doc, json_str);
  json_hash=hash(json_str);
  package["json"]=json_str;
  package["hash"]=json_hash;
  serializeJson(package, packed_package);

  return packed_package; 
  
  }

bool check_package_hash (StaticJsonDocument<DEFAULT_DOC_SIZE> json_doc){
  if (hash(json_doc["json"])!=json_doc["hash"]){
      return false;    
    }
  return true; 
    
  }
  
void ask_for_resend (const uint8_t *mac_addr, StaticJsonDocument<DEFAULT_DOC_SIZE> json_doc){
  json_doc["command"]="resend";
  String package;
  package = package_json (json_doc);
  
  sendData(mac_addr,package);

  }
  
void command_clarification (uint8_t *mac_addr, StaticJsonDocument<DEFAULT_DOC_SIZE> json_doc){
  json_doc["command"]="command_clarification";
  String package;
  package = package_json (json_doc);
  
  sendData(mac_addr,package);

  }
  
StaticJsonDocument<DEFAULT_DOC_SIZE> unpackage_json (StaticJsonDocument<DEFAULT_DOC_SIZE> json_doc){
  StaticJsonDocument <DEFAULT_DOC_SIZE> out_doc;
  deserializeJson(out_doc, json_doc["json"]);
  return out_doc; 
  }

void write_doc_to_EEPROM(int address, StaticJsonDocument<EEPROM_SIZE> eeprom_json_doc){
  String eeprom_str;
  serializeJson(eeprom_json_doc, eeprom_str);
  
  for(int i=0;i<EEPROM_SIZE;i++)
  {
    EEPROM.write(address+i, eeprom_str[i]);
  }
  EEPROM.write(address + EEPROM_SIZE,'\0');
  EEPROM.commit();
}


StaticJsonDocument<EEPROM_SIZE> read_doc_from_EEPROM(int address)
{
  char eeprom_str[EEPROM_SIZE];
  int i=0;
  unsigned char k;
  k = EEPROM.read(address);
  while(k != '\0' && i < EEPROM_SIZE){
    k = EEPROM.read(address + i);
    eeprom_str[i] = k;
    i++;
  }
  eeprom_str[i]='\0';
  
  StaticJsonDocument<EEPROM_SIZE> eeprom_json_doc;
  deserializeJson(eeprom_json_doc, String(eeprom_str));
  
  return eeprom_json_doc;
}

StaticJsonDocument <DEFAULT_DOC_SIZE> init_doc(StaticJsonDocument<DEFAULT_DOC_SIZE> doc) {
  doc["header"]["DEVICE_TYPE"] = DEVICE_TYPE;
  doc["header"]["POWER_SOURCE"] = POWER_SOURCE;
  doc["header"]["DEVICE_ID"] = DEVICE_ID;
  doc["peers"]["hub"]="";
  doc["data"]["temp"] = FILLER_VAL;
  doc["data"]["light"] = FILLER_VAL;
  
  return doc;
  }

StaticJsonDocument <200> decode_espnow_message (const uint8_t message){
  StaticJsonDocument <200> out_doc;
  String message_str((char*) message);
  deserializeJson(out_doc, message_str);
  return out_doc;
  }

int count_slaves(esp_now_peer_info_t* slaves){
  int slave_count=0;
  bool count_finished=false;
  for (int i = 0; i < NUMSLAVES; i++) {
    int zero_combo=0;
    if (esp_now_is_peer_exist(slaves[i].peer_addr)){
        slave_count++;
      }
//    else{return slave_count;}
  }
  return slave_count;
}



/////////////////////////////////////////////////////////////////////////////////////////////////
//from espnow examples:

// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  String Prefix = "Slave:";
  String Mac = WiFi.macAddress();
  String SSID = Prefix + Mac;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
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
  
  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
      }
      delay(10);
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Slave") == 0) {
        // SSID of interest
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
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
    Serial.print(SlaveCnt); Serial.println(" Slave(s) found, processing..");
  } else {
    Serial.println("No Slave Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
  
  return *slaves;
}

// Check if the slave is already paired with the master.
// If not, pair the slave with master
void manageSlave(esp_now_peer_info_t* slaves) {
  int SlaveCnt = count_slaves(slaves);
  if (SlaveCnt > 0) {
    for (int i = 0; i < SlaveCnt; i++) {
      Serial.print("Processing: ");
      for (int ii = 0; ii < 6; ++ii ) {
        Serial.print((uint8_t) slaves[i].peer_addr[ii], HEX);
        if (ii != 5) Serial.print(":");
      }
      Serial.print(" Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(slaves[i].peer_addr);
      if (exists) {
        // Slave already paired.
        Serial.println("Already Paired");
      } else {
        // Slave not paired, attempt pair
        esp_err_t addStatus = esp_now_add_peer(&slaves[i]);
        if (addStatus == ESP_OK) {
          // Pair success
          Serial.println("Pair success");
        } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
          // How did we get so far!!
          Serial.println("ESPNOW Not Init");
        } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
          Serial.println("Add Peer - Invalid Argument");
        } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
          Serial.println("Peer list full");
        } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
          Serial.println("Out of memory");
        } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
          Serial.println("Peer Exists");
        } else {
          Serial.println("Not sure what happened");
        }
        delay(100);
      }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
  }
}

// send data
void sendData(const uint8_t *mac_addr, String package) {
  
    Serial.print("Sending: ");
    Serial.println(package);
  //encode the string package
  const uint8_t* encoded_package = reinterpret_cast<const uint8_t*>(package.c_str());  
  esp_err_t result = esp_now_send(mac_addr, encoded_package, sizeof(encoded_package));
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
    }
    delay(100);
}
/////////////////////////////////////////////////////////
//
