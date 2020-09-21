/*
  Project Name: Group 5 Client Communication
  
  Version: 14
  
  Summary: Handles local communication between different clients using a series of HTTP POST requests (from clients). Messages
           received by the Arduino is temporarily held in a message queue, which is then sent from the Arduino to the local client
           via JSON, whose response content can be detected and parsed by the app. 
       
  Default Web Server IP Address: 192.168.4.1
  
  Client Protocols: There are three main protocols shared by the arduino and the client for bidirectional communication.
      1. Client Initialization Protocol: /client_name:{your name here}/client_id:{your id here}
      2. Message Sending Protocol: /message:{your message here}/source_id:{source id}/target_id:{target id}
      3. Network Update and Message Reception Protocol: returns JSON information of clients and available messages.
                                                        When receiving the message, it will be in the form: 
                                                        message:{message here}/source_id:{source id here}

  Communication Protocol: The main protocol used for the communication system is a token buffer protocol. These tokens are sent along with 
                          radio broadcasts and allows only the nodes with a valid token to transmit messages. This terminates the possibility
                          of collision during data reception. Tokens have the form 1-2-3-4-5-....-N-0, where each number represents a node ID
                          within the network, and the 0 token represents a delay timer to allow new users into the network.
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <TinyGPS.h>
#include <Arduino.h>
#include <wiring_private.h>
#include <variant.h>
#define PIN_SERIAL_RX       (7)                // Pin description number for PIO_SERCOM on 7
#define PIN_SERIAL_TX       (6)                // Pin description number for PIO_SERCOM on 6
#define PAD_SERIAL_TX       (UART_TX_PAD_2)      // SERCOM pad 2 TX
#define PAD_SERIAL_RX       (SERCOM_RX_PAD_3)    // SERCOM pad 3 RX

/* Network Variables */
char ssid[] = "TestNet1";        // your network SSID (name)
int keyIndex = 0;                // your network key Index number (needed only for WEP)
byte mac[6];
int status = WL_IDLE_STATUS;
WiFiServer server(80);

/* Communication Protocol Variables */
String currToken = "";
String nodeID = "1";
unsigned long startMillis;
unsigned long currentMillis;
bool startTimer = true;
bool loneBroadcaster = true;
int randomInitialTimer = 0;

/* Client Protocol Variables */
String clientArray[10]={""}; // static array for client information (ip address and name) (currently handles 10 clients at max)
String messageQueue[50]={""}; // places all incoming messages in a queue, since Arduino operation is not asynchronous
String networkMacAddress[20][11];
String currMacAddress = "";
String messageSendList[30]={""}; // protocol is /message:(your message)/target_ip:(destination ip)/target_mac:(destination mac)/source_id:(source id)
String incomingMessage = "";
int networkMacAddressTime[20]; // this is in form client_name:(client name)/client_id:(client id)/client_ip:(client ip)/client_mac:(client mac)
int clientTime[10] = {NULL};
int currClientIndex = 0;
int currMessageIndex = 0;
int currMacIndex = 0;
int currMessageSendIndex = 0; 
int macTimer = 0; // counts up and checks all mac addresses

/* GPS and Emergency Protocol Variables*/
Uart gpsSerial(&sercom3, PIN_SERIAL_RX, PIN_SERIAL_TX,PAD_SERIAL_RX , PAD_SERIAL_TX); // Create the new UART instance assigning it to pin 0 and 1
TinyGPS gps;
String nodeLocation = "0.000000;0.000000"; // MUST FILL THIS NEW VARIABLE WITH LOCATION
String emergencyLocations[20];
bool broadcastEmergency = false;
bool buttonCounting = false;
float lat;
float lon;
int currEmergencyIndex = 0;
int emergencyPin = 4; // pin for emergency button
int beeperPin = 5; // pin for beeper
unsigned long startBeeperTimer;
unsigned long startEmergencyTimer;

/* Access Point Setup */
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  Serial1.begin(9600); 
  gpsSerial.begin(9600);
  pinMode(emergencyPin, INPUT);
  pinMode(beeperPin, OUTPUT);
  pinPeripheral(PIN_SERIAL_TX, PIO_SERCOM);
  pinPeripheral(PIN_SERIAL_RX, PIO_SERCOM_ALT);
  Serial.println("Access Point Web Server");
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  // Create open network. Change this line if you want to create an WEP network:
  status = WiFi.beginAP(ssid);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }
  // wait 10 seconds for connection:
  delay(10000);
  // start the web server on port 80
  server.begin();
  // you're connected now, so print out the status
  printWiFiStatus();

  /* Generate Mac Address for Arduino */
  WiFi.macAddress(mac);
  for(int i = 5; i >=0; i--){
    currMacAddress = currMacAddress + String(mac[i]);
    if(i !=0){
       currMacAddress = currMacAddress + ".";
    }
  }
  // immediately put current mac address as the first address in the array
  networkMacAddress[0][0] = currMacAddress;
  currMacIndex++;

  // Initially flush all Serial data to avoid error in reception
  while(Serial.available() > 0) {
    char t = Serial.read();
  }

  // check if any other nodes present in network
  int checkBroadcastTimer = 0;
  while(Serial.available() == 0 && checkBroadcastTimer >= 5000){
    checkBroadcastTimer++;
    delay(1);
  }
  readSerialMessage();
  if(incomingMessage.indexOf("/token:") != -1){
    loneBroadcaster = false; // if other tokens present in network, indicate this
    int tokenIndex = incomingMessage.indexOf("/token:");
    int finishIndex = incomingMessage.indexOf("/finish_protocol");
    currToken = incomingMessage.substring(tokenIndex + 7, finishIndex);
  }
  else{
    currToken = nodeID + "-0"; // initialize token for lone broadcaster: X-0
    Serial.println("I am alone");
  }
}

void loop() {
  if (status != WiFi.status()) {
    // it has changed update the variable
    status = WiFi.status();
    if (status == WL_AP_CONNECTED) {
      // a device has connected to the AP
      Serial.println("Device connected to AP");
    } else {
      // a device has disconnected from the AP, and we are back in listening mode
      Serial.println("Device disconnected from AP");
    }
  }
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {
    // obtain client IP in String Format
    String clientIP = "";
    for(int i = 0; i < 4; i++){
      clientIP = clientIP + client.remoteIP()[i];
      if(i!=3){
        clientIP = clientIP + ".";
      }
    }
    Serial.println("new client");
    String currentLine = "";  
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()){ 
        char c = client.read();
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank) {
          /* JSON Data and Parsing */
          /* 
           {"Type": "Users",
            "Data":[{"Name":"Name Here", "ID":"ID here", "Location":"Location here"}],
            "Messages":[{"Source ID":"Source ID Here", "Message":"Message Here"}],
            "Emergency":[{"Location":"Location here"}, {"Location":"Location here"}]
           }
          */
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: application/json");
          client.println("Access-Control-Allow-Origin: *");
          client.println("Refresh: 2");
          client.println("");
          
          // --- Obtains Message From Client ---
          // clientMessage has form: /message:(your message here)/source_id:(source id)/target_id:(target id)
          int startIndex = currentLine.indexOf("GET /");
          int endIndex = currentLine.indexOf("HTTP/1.1");
          String clientMessage = currentLine.substring(startIndex + 5, endIndex);
          clientMessage.replace("%20", " ");
          Serial.println(clientMessage);
          
          // --- Handles Client Request for Updated Data ---
          if(clientMessage.indexOf("update_data")!= -1){     
            bool deleteComma = false;
            // attaches all connected users in network to JSON
            String finalJson = "{\"Type\": \"Users\",\"Data\":[";
            for(int i = 0; i < currMacIndex; i++){
              for(int k = 1; k < 11; k++){
                // networkMacAddress in form client_name:(client name)/client_id:(client id)/client_ip:(client ip)/client_loc:(client location)/client_mac:(client mac)/ 
                if(networkMacAddress[i][k] != ""){
                  deleteComma = true;
                  int indexName = networkMacAddress[i][k].indexOf("client_name:");
                  int indexID = networkMacAddress[i][k].indexOf("client_id:");
                  int indexIP = networkMacAddress[i][k].indexOf("client_ip:");
                  int indexLoc = networkMacAddress[i][k].indexOf("client_loc:");
                  int indexMac = networkMacAddress[i][k].indexOf("/client_mac:");
                  String clientLocation = networkMacAddress[i][k].substring(indexLoc+11, indexMac);
                  String clientName = networkMacAddress[i][k].substring(indexName+12, indexID-1);
                  String clientID = networkMacAddress[i][k].substring(indexID+10,indexIP-1);
                  finalJson = finalJson + "{\"Name\":\"" + clientName + "\", \"ID\":\"" + clientID + "\", \"Location\":\"" + clientLocation + "\"},";
                }
              }
            }
            if(deleteComma){
              finalJson.remove(finalJson.length()-1);
              deleteComma = false;
            }
            finalJson = finalJson + "], \"Messages\":[";
            // Releases messageQueue to JSON requested by client
            // messageQueue of form /message:(your message)/target_ip:(destination ip)/target_mac:(destination mac)/source_id:(source id)
            // message output to app has general form /message:(your message)/target_id:(target id)
            // Need to search in client table for this target id and obtain corresponding ip and mac address
            for(int i = 0; i < currMessageIndex; i++){
              int indexIP = messageQueue[i].indexOf("target_ip:");
              int indexMac = messageQueue[i].indexOf("target_mac:");
              String targetIP = messageQueue[i].substring(indexIP+10, indexMac-1);
              targetIP.replace(" ", "");
              targetIP.replace("\r", "");
              targetIP.replace("/", "");
              if(targetIP == clientIP){
                deleteComma = true;
                int indexMessage = messageQueue[i].indexOf("message:");
                int indexID = messageQueue[i].indexOf("source_id:");
                String message = messageQueue[i].substring(indexMessage + 8, indexIP - 1);
                String sourceID = messageQueue[i].substring(indexID + 10);
                sourceID.replace(" ", "");
                sourceID.replace("\r", "");
                finalJson = finalJson + "{\"Source ID\":\"" + sourceID + "\", \"Message\":\"" + message + "\"},";
                for(int k = i; k < currMessageIndex; k++){
                  messageQueue[k] = messageQueue[k + 1];
                }
                // moves message queue forwards once it is sent to JSON
                messageQueue[currMessageIndex] = "";
                currMessageIndex--;
                i--;
              }
            }
            if(deleteComma){
              finalJson.remove(finalJson.length()-1);
              deleteComma = false;
            }
            if(currEmergencyIndex != 0){
              deleteComma = true; // if emergency list is not empty, delete last comma
            }
            finalJson = finalJson + "], \"Emergency\":[";
            for(int i = 0; i < currEmergencyIndex; i++){
              finalJson = finalJson + "{\"Location\":\"" + emergencyLocations[i] + "\"},";
            }
            if(deleteComma){
              finalJson.remove(finalJson.length()-1);
              deleteComma = false;
            }
            finalJson = finalJson + "]}";
            finalJson.replace("\n", "");
            Serial.println("final JSON: " + finalJson);
            client.println(finalJson);
          }
          // --- Handles Client Message Transmission ---
          // send message if it does not contain an automatic "favicon.ico" response from web server
          // messageQueue has the general form /message:(your message)/target_ip:(destination ip)/target_mac:(destination mac)/source_id:(source id)
          if(clientMessage.indexOf("favicon.ico") == -1 && clientMessage.indexOf("target_id:") != -1){ 
            int indexTargetID = clientMessage.indexOf("target_id:");
            String targetID = clientMessage.substring(indexTargetID + 10);
            targetID.replace(" ", "");
            String targetMac = "";
            String targetIP = "";
            // networkMacAddress has form /client_name:(client name)/client_id:(client id)/client_ip:(client ip)/client_loc:(client location)/client_mac:(client mac)
            for(int i = 0; i < currMacIndex; i++){
              for(int k = 1; k < 11; k++){
                if(networkMacAddress[i][k].indexOf(targetID) != -1){
                  int indexMac = networkMacAddress[i][k].indexOf("client_mac:");
                  int indexID = networkMacAddress[i][k].indexOf("client_id:");
                  int indexIP = networkMacAddress[i][k].indexOf("client_ip:");
                  int indexLoc = networkMacAddress[i][k].indexOf("client_loc:"); // CHANGED CHANGED CHANGED
                  targetMac = networkMacAddress[i][k].substring(indexMac+11);
                  targetMac.replace(" ", "");
                  targetIP = networkMacAddress[i][k].substring(indexIP+10,indexLoc-1);
                  break;
                }
              }
            }
            Serial.println("targetMac:" + targetMac);
            Serial.println("targetIp:" + targetIP);
            if(targetMac != "" && targetIP != ""){
              // the input clientMessage is /message:(your message)/source_id:(source id)/target_id:(target id)
              // clientMessage output here will now have the same protocol as in messageQueue
              // which is /message:(your message)/target_ip:(destination ip)/target_mac:(destination mac)/source_id:(source id)
              int indexSourceID = clientMessage.indexOf("source_id:");
              int indexTargetID = clientMessage.indexOf("target_id:");
              String sourceID = clientMessage.substring(indexSourceID+10,indexTargetID-1);
              clientMessage = clientMessage.substring(0, indexSourceID - 1);
              clientMessage = clientMessage + "/target_ip:" + targetIP + "/target_mac:" + targetMac + "/source_id:" + sourceID;
              clientMessage.replace("\n", "");
              // if the targetMac is equal to Current Mac Address, we store in local messageQueue
              if(targetMac == currMacAddress){
                messageQueue[currMessageIndex] = clientMessage;
                currMessageIndex++;
              }
              // if target mac is a different node, broadcast it out to other nodes
              else{
                messageSendList[currMessageSendIndex] = clientMessage;
                currMessageSendIndex++;
              }
            }
          }
          
          // --- Handles New Client to the Local Network ---
          // input is /client_name:(name)/client_id:(id)
          // clientArray  has form /client_name:(client name)/client_id:(client id)/client_ip:(client ip)/client_loc:(client location) CHANGED CHANGED CHANGED CHANGED CHANGED
          // clientMessage has the form client_name:(client name)/client_id:(client id)   
          if((clientMessage.indexOf("client_name:") != -1) && (clientMessage.indexOf("client_id:") != -1)){
            bool newClient = true;
            int indexID = clientMessage.indexOf("client_id:");
            String clientID = "client_id:" + clientMessage.substring(indexID + 10);
            String clientInfo = clientMessage + "/client_ip:" + clientIP;
            clientInfo.replace(" ", "");
            clientID.replace(" ", "");
            for(int k = 0; k < currClientIndex; k++){
              // if client already exists in list, do not add it again
              if(clientArray[k].indexOf(clientID) != -1){
                newClient = false;
              }
            }
            if(newClient){ // CHANGED CHANGED CHANGED CHANGED CHANGED
              clientInfo = clientInfo + "/client_loc:" + nodeLocation;
              clientArray[currClientIndex] = clientInfo;
              currClientIndex++;
            }
          }
          
          // --- Refreshes Local Client Connection Status ---
          for(int i = 0; i < currClientIndex; i++){
            if(clientArray[i].indexOf(clientIP) != -1){
              clientTime[i] = 0;
              break;
            }
          }
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          currentLineIsBlank = false;    
          currentLine += c;     
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
  }
  macTimer++;
  
  // --- Handles Incoming Message from Other Nodes ---
  incomingMessage = "";
  readSerialMessage();
  
  // --- Handles Incoming Mac Address Pings ---
  if(incomingMessage.indexOf("ping_mac:") != -1){
    int emergencyIndex = incomingMessage.indexOf("/emergency_broadcast:");
    int tokenIndex = incomingMessage.indexOf("/token:");
    int finishIndex = incomingMessage.indexOf("/finish_protocol");
    if(emergencyIndex != -1){
      String emergencyLoc = incomingMessage.substring(emergencyIndex+21, tokenIndex);
      bool found = false;
      for(int i = 0; i < currEmergencyIndex; i++){
        if(emergencyLocations[i] == emergencyLoc){
          found = true;
        }
      }
      if(!found){
        emergencyLocations[currEmergencyIndex] = emergencyLoc;
        currEmergencyIndex++;
      }
    }
    startTimer = true; // must reset this if multiple users try to join network
    currToken = incomingMessage.substring(tokenIndex + 7, finishIndex);
    Serial.println("Current Token: " + currToken);
    incomingMessage = incomingMessage.substring(0, tokenIndex);
    int indexEndMac = incomingMessage.indexOf("/");
    int indexStartPingMac = incomingMessage.indexOf("ping_mac:");
    String pingMac = incomingMessage.substring(indexStartPingMac + 9, indexEndMac);
    bool newMac = true;
    int indexRefreshMac;
    for(int i = 0; i < currMacIndex; i++){
      if(networkMacAddress[i][0] == pingMac){
        networkMacAddressTime[i] = 0; 
        newMac = false;
        indexRefreshMac = i;
      }
    }
    if(newMac){
      indexRefreshMac = currMacIndex;
    }
    networkMacAddress[indexRefreshMac][0] = pingMac;
    if(incomingMessage.indexOf("/client:") != -1){
      int clientIndexBottom = incomingMessage.indexOf("client:") + 7;
      int clientIndexTop = incomingMessage.indexOf("client:", clientIndexBottom);
      int i = 1;
      // will store in networkMacAddress in the form client_name:(client name)/client_ip:(client ip)/client_loc:(client location)/client_mac(client mac)
      while(clientIndexTop != -1){
        String newClientInfo = incomingMessage.substring(clientIndexBottom, clientIndexTop)+ "/client_mac:" + pingMac;
        newClientInfo.replace(" ", "");
        networkMacAddress[indexRefreshMac][i] = newClientInfo;
        clientIndexBottom = clientIndexTop + 7;
        clientIndexTop = incomingMessage.indexOf("client:", clientIndexBottom);
        i++;
      }
      for(int k = i; k < 11; k++){
        networkMacAddress[indexRefreshMac][k] = "";
      }
      int messageIndex = incomingMessage.indexOf("/message:");
      if(messageIndex == -1){
        networkMacAddress[indexRefreshMac][i] = incomingMessage.substring(clientIndexBottom)+ "/client_mac:" + pingMac;
      }
      else{
        networkMacAddress[indexRefreshMac][i] = incomingMessage.substring(clientIndexBottom, messageIndex)+ "/client_mac:" + pingMac;
      }
      networkMacAddressTime[indexRefreshMac] = 0;
    }
    if(newMac){
      currMacIndex++;
    }
  }

  // --- Handles Client Emergency Broadcast ---
  if(digitalRead(emergencyPin) == HIGH){
    if(!buttonCounting){
      buttonCounting = true;
      startBeeperTimer = millis();
    }
    currentMillis = millis();
    if(currentMillis - startBeeperTimer > 3000){ // must hold button for 3s
      broadcastEmergency = true;
      buttonCounting = false;
      startEmergencyTimer = millis();
    }
  }
  else{
    buttonCounting = false;
  }

  // --- Broadcasts Emergency Signal for 30 seconds ---
  if(broadcastEmergency){
    // maybe make LED light up to indicate you are broadcasting emergency?
    currentMillis = millis();
    if(currentMillis - startEmergencyTimer > 30000){
      broadcastEmergency = false;
    }
  }
  
  // --- Triggers Beeper if Emergency Detected --- 
  if(currEmergencyIndex > 0){
    digitalWrite(beeperPin, HIGH);
  }
  else{
    digitalWrite(beeperPin, LOW);
  }

  // --- Handles Message Inputs from Other Nodes ---
  // message input in form /message:(your message)/target_ip:(destination ip)/target_mac:(destination mac)/source_id:(source id)
  int messageIndexBottom = incomingMessage.indexOf("/message:");
  int messageIndexTop = incomingMessage.indexOf("/message:", messageIndexBottom + 7);
  while(messageIndexBottom != -1){
    int macIndexStart = incomingMessage.indexOf("target_mac:", messageIndexBottom);
    int macIndexEnd = incomingMessage.indexOf("source_id:", messageIndexBottom);
    String targetMac = incomingMessage.substring(macIndexStart + 11, macIndexEnd-1);
    String newMessage;
    if(messageIndexTop != -1){
      newMessage = incomingMessage.substring(messageIndexBottom, messageIndexTop);
    }
    else{
      newMessage = incomingMessage.substring(messageIndexBottom);
    }
    messageIndexBottom = messageIndexTop;
    messageIndexTop = incomingMessage.indexOf("/message:", messageIndexBottom + 7); 
    Serial.println("NEW MESSAGE");
    Serial.println(newMessage);
    targetMac.replace(" ", "");
    currMacAddress.replace(" ", "");
    if(targetMac == currMacAddress){
      messageQueue[currMessageIndex] = newMessage;
      currMessageIndex++;
    }
  }

  // --- Handles Token Push and Broadcasts ---
  if(currToken.startsWith(nodeID)){
    delay(100);
    broadcastPing(); // broadcasts that device is still in network
  }
  else if(currToken.indexOf(nodeID) == -1){
    if(currToken.startsWith("0")){
      if(startTimer){
        startTimer = false;
        randomInitialTimer = random(0, 3000); // random number between 0 and 3000 (0s to 3s);
        startMillis = millis();
      }
      currentMillis = millis();
      if(currentMillis - startMillis > randomInitialTimer){
        currToken = nodeID + "-" + currToken; // places in front of 0, so broadcast resets at 0, e.g. X-0-1-2-3-4-5-...
        broadcastPing();
      }
    }
  }
  // Pushes token forward and deletes idle node if it does not broadcast within 5s
  else{
    if(startTimer){
      startTimer = false;
      startMillis = millis();
    }
    currentMillis = millis();
    if(currentMillis - startMillis > 7000){ // gives 5 seconds delay before moving token forward
      // Tokens of form: 1-2-3-4-5-....-N-0
      if(currToken.startsWith("0")){
        currToken = currToken.substring(2) + "-" + currToken.substring(0,1); // moves token forward
      }
      else{
        currToken = currToken.substring(2); // Removes the Idle Token as well
      }
      startTimer = true;
    }
  }
  
  // --- Checks Network Nodes for Timeout ---
  if(macTimer%20000 == 0){
    for(int i = 1; i < currMacIndex; i++){ // don't check index 0, since it is current device
      networkMacAddressTime[i] = networkMacAddressTime[i] + 20000; 
    }
    // check if any mac address is beyond a certain time limit and removes it from network
    for(int i = 1; i < currMacIndex; i++){
      if(networkMacAddressTime[i] >= 50000){ // timeout set at around 30 seconds
        for(int j = i; j < currMacIndex - 1; j++){
          for(int k = 1; k < 11; k++){
            networkMacAddress[j][k] = networkMacAddress[j+1][k];
          }
          networkMacAddressTime[j] = networkMacAddressTime[j+1];
        }
        for(int k = 0; k < 11; k++){
          networkMacAddress[currMacIndex-1][k] = "";
        }
        networkMacAddressTime[currMacIndex-1] = NULL;
        currMacIndex--;
      }
    }
  }
  
  // --- Checks Local Clients for Timeout ---
  if(macTimer%5000 == 0){
    for(int i = 0; i < currClientIndex; i++){
      clientTime[i] = clientTime[i] + 5000; 
      if(clientTime[i] >= 50000){
        for(int j = i; j < currClientIndex - 1; j++){
          clientTime[j] = clientTime[j + 1]; 
        }
        clientTime[currClientIndex - 1] = NULL;
        currClientIndex--;
      }
    }
  }

  // --- Measures new GPS location --- // 
  // clientArray  has form /client_name:(client name)/client_id:(client id)/client_ip:(client ip)/client_loc:(client location)
  if(macTimer%10000 == 0){
    findLocation();
    for(int i = 0; i < currClientIndex; i++){
      int indexLoc = clientArray[i].indexOf("/client_loc:");
      String tempClientInfo = clientArray[i].substring(0, indexLoc);
      clientArray[i] = tempClientInfo + "/client_loc:" + nodeLocation;
    }
  }
  
  // --- Resets Emergency Location Array ---
  if(macTimer%200000 == 0){
    currEmergencyIndex = 0;
  }
}


/* ADDITIONAL IMPLEMENTED FUNCTIONS */

/* Interrupt Handler */
void SERCOM3_Handler()
{
  gpsSerial.IrqHandler();
}

/* Broadcast Protocol */
// broadcast has general form ping_mac:(source mac)/client:(client info 1)/client:(client info 2)/...
void broadcastPing(){
  String broadcast = "ping_mac:" + currMacAddress;
  for(int i = 0; i < currClientIndex; i++){
    broadcast = broadcast + "/client:" + clientArray[i];
  }
  // refreshes the local network clients
  for(int i = 1; i < currClientIndex+1; i++){
    networkMacAddress[0][i] = clientArray[i-1] + "/client_mac:" + currMacAddress;
  }
  for(int i = currClientIndex+1; i < 11; i++){
    networkMacAddress[0][i] = "";
  }
  for(int i = 0; i < currMessageSendIndex; i++){
    broadcast = broadcast + "/" + messageSendList[i];
    messageSendList[i] = "";
  }
  if(broadcastEmergency){
    broadcast = broadcast + "/emergency_broadcast:" + nodeLocation; // CHANGED CHANGED CHANGED CHANGED CHANGED
  }
  // Tokens of form: 1-2-3-4-5-....-N-0
  currMessageSendIndex = 0;
  currToken = currToken.substring(2) + "-" + currToken.substring(0,1); // moves token forward
  broadcast = broadcast + "/token:" + currToken + "/finish_protocol";
  Serial.println("PING broadcast: " + broadcast);
  Serial1.println(broadcast);
}


/* Find Current GPS Location */
void findLocation(){
    while(gpsSerial.available()){ // check for gps data
       if(gps.encode(gpsSerial.read())){ // encode gps data
         gps.f_get_position(&lat,&lon); // get latitude and longitude
       }
    }
    String latitude = String(lat,6);
    String longitude = String(lon,6);
    nodeLocation = latitude + ";" + longitude;
    Serial.println("nodeLocation: " + nodeLocation);
}

/* Prints WiFi Status */
void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

/* Reads Incoming Serial Messages */
void readSerialMessage(){
  int timer = 0;
  if(Serial1.available() > 0){ // continues reading serial message until /finish_protocol is detected
    while(incomingMessage.indexOf("/finish_protocol") == -1 && timer < 20000 && incomingMessage.indexOf("\n") == -1){
      while(Serial1.available() > 0){
        char c = Serial1.read();
        incomingMessage.concat(c);
        Serial.println(incomingMessage);
      }
      timer++;
    }
  }
}
