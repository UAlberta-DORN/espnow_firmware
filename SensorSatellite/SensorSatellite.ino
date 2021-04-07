#include "sensor_config.h" // we can include a setting file specially made for this script, else compliler will use the default settings
#include <custom_capstone_lib.h>

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// Owen's:
#include <Wire.h>

// volatile int alarm;

// Device addresses
const int OPT3001_Address = 0x44;
const int TMP116_Address = 0x49;

// Hexadecimal addresses for various OPT3001 registers
const int OPT_Reg = 0x00;                // OPTical register
const int OPT_Config_Reg = 0x01;         // Configuration register
const int OPT_Low_Lim_Reg = 0x02;        // High limit register
const int OPT_High_Lim_Reg = 0x03;       // Low limit register
const int OPT_Device_ID_Reg = 0x7F;      // Device ID register

// Hexadecimal addresses for various TMP116 registers
const int Temp_Reg = 0x00;               // Temperature register
const int Temp_Config_Reg = 0x01;        // Configuration register
const int Temp_High_Lim_Reg = 0x02;      // High limit register
const int Temp_Low_Lim_Reg = 0x03;       // Low limit register
const int Temp_EEPROM_Unlock_Reg = 0x04; // EEPROM unlock register
const int Temp_Device_ID_Reg = 0x0F;     // Device ID register

// Set OPTical threshold
const uint8_t OPT_highlimH = B10111111;  // High byte of high lim
const uint8_t OPT_highlimL = B11111111;  // Low byte of high lim
const uint8_t OPT_lowlimH = B00000000;   // High byte of low lim
const uint8_t OPT_lowlimL = B00000001;   // Low byte of low lim

// Set temperature threshold
const uint8_t Temp_highlimH = B00001101; // High byte of high lim
const uint8_t Temp_highlimL = B10000000; // Low byte of high lim   - High 27 C
const uint8_t Temp_lowlimH = B00001100;  // High byte of low lim
const uint8_t Temp_lowlimL = B00000000;  // Low byte of low lim    - Low 24 C


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////






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

  ////////////////////////////////////////////////////////////////////////////
  // Owen's setups
  Wire.begin(21,22);

  // Write limits to OPT register
  I2Cwrite(OPT3001_Address, OPT_High_Lim_Reg, OPT_highlimH, OPT_highlimL);
  I2Cwrite(OPT3001_Address, OPT_Low_Lim_Reg, OPT_lowlimH, OPT_lowlimL);

  // Write limits to TMP register
  I2Cwrite(TMP116_Address, Temp_High_Lim_Reg, Temp_highlimH, Temp_highlimL);
  I2Cwrite(TMP116_Address, Temp_Low_Lim_Reg, Temp_lowlimH, Temp_lowlimL);

  // Sets Pin 13 as output, Pin 2 as input (active low)
  pinMode(13, OUTPUT);
  pinMode(2, INPUT_PULLUP);

  // Sets up Pin 2 to trigger "alert" ISR when pin changes H->L and L->H
  attachInterrupt(digitalPinToInterrupt(2), alert, CHANGE);

  Serial.println("Light Intensity (Lux)\tTemperature (C)");
  
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
  ///////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////
  // Owen's Loop
  // One-shot setting sent to config register
  I2Cwrite(OPT3001_Address, OPT_Config_Reg, 0xC2, 0x00);
  // Request and calculate OPTical lux reading
  double OPT_data = ReadOPTSensor();
  Serial.print(OPT_data);
  Serial.print("\t\t\t");

  delay(10);

  // One-shot setting sent to config register
  I2Cwrite(TMP116_Address, Temp_Config_Reg, 0x0C, 0x20);
  // Request and calculate temperature reading
  double temp_data = ReadTempSensor();
  Serial.println(temp_data);

  delay(500);
  ////////////////////////////////////////////////////////////////////////////
  //  update the data:
  data_json["data"]["temp"] = temp_data;
  data_json["data"]["light"] = OPT_data;
}


///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
// Owen's Functions 


double I2Cwrite(int dev, int reg, int H, int L){

  // Takes in 4 variables:
  // device address, register address,
  // high and low bytes of data to transmit
  Wire.beginTransmission(dev);
  Wire.write(reg);
  Wire.write(H);
  Wire.write(L);
  Wire.endTransmission();
  delay(10);

}

void alert(){

//  alarm = digitalRead(2);

}

double ReadOPTSensor(void){

  // Device addresses
  //int OPT3001_Address = 0x48;

  // Data array to store 2-bytes from I2C line
  uint8_t data[2];
  // Combination of 2-byte data into 16-bit data
  int16_t datac;
  // Mantissa and exponent from register
  int16_t iExponent, iMantissa;
  // Calculated lux from sensor data
  float fLux;

  // Points to device & begins transmission
  Wire.beginTransmission(OPT3001_Address);
  // Points to OPTical register to read/write data
  Wire.write(OPT_Reg);
  // Ends data transfer and transmits data from register
  Wire.endTransmission();

  // Delay to allow sufficient conversion time
  delay(10);

  // Requests 2-byte temperature data from device
  Wire.requestFrom(OPT3001_Address,2);

  // Checks if data received matches the requested 2-bytes
  if(Wire.available() <= 2){
    // Stores each byte of data from temperature register
    data[0] = Wire.read();
    data[1] = Wire.read();

    // Combines data to make 16-bit binary number
    datac = ((data[0] << 8) | data[1]);

    // Extract mantissa and exponent
    iMantissa = datac & 0x0FFF;
    iExponent = (datac & 0xF000) >> 12;

    // Caculate lux based on (3) in datasheet
    return 0.01 * pow(2, iExponent) * iMantissa;

  }
}

double ReadTempSensor(void){

  // Device addresses
  //int TMP116_Address = 0x48;

  // Data array to store 2-bytes from I2C line
  uint8_t data[2];
  // Combination of 2-byte data into 16-bit data
  int16_t datac;

  // Points to device & begins transmission
  Wire.beginTransmission(TMP116_Address);
  // Points to temperature register to read/write data
  Wire.write(Temp_Reg);
  // Ends data transfer and transmits data from register
  Wire.endTransmission();

  // Delay to allow sufficient conversion time
  delay(10);

  // Requests 2-byte temperature data from device
  Wire.requestFrom(TMP116_Address,2);

  // Checks if data received matches the requested 2-bytes
  if(Wire.available() <= 2){
    // Stores each byte of data from temperature register
    data[0] = Wire.read();
    data[1] = Wire.read();

    // Combines data to make 16-bit binary number
    datac = ((data[0] << 8) | data[1]);

    // Convert to Celcius (7.8125 mC resolution) and return
    return datac*0.0078125;

  }
}
