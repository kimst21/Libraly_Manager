#include <WiFi.h>
#include <WebServer.h>
#define accessPointButtonPin 35    // Connect a button to this pin
WebServer serverAP(80);   // the Access Point Server

#include <EEPROM.h>
#define accessPointLed 14
#define eepromTextVariableSize 33  // the max size of the ssid, password etc.  32+null terminated

char ssid[eepromTextVariableSize] = "WIFI-NAME";
char pass[eepromTextVariableSize] = "PASSWORD";

boolean accessPointMode = true;    // is true every time the board is started as Access Point
boolean debug = false;
unsigned long lastUpdatedTime = 0;

int pushDownCounter = 0;
int lastConnectedStatus = 0;

void initAsAccessPoint() {
  WiFi.softAP("ESP32-Access Point");      // or WiFi.softAP("ESP_Network","Acces Point Password");
  if (debug) Serial.println("AccesPoint IP: " + WiFi.softAPIP().toString());
  Serial.println("Mode= Access Point");
  //WiFi.softAPConfig(local_ip, gateway, subnet);  // enable this line to change the default Access Point IP address
  delay(100);
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (lastConnectedStatus == 1) Serial.println("WiFi disconnected\n");
    lastConnectedStatus = 0;
    Serial.print(".");
    delay(500);
  } else {
    if (lastConnectedStatus == 0) {
      Serial.println("Mode= Client");
      Serial.print("\nWiFi connectd to :");
      Serial.println(ssid);
      Serial.print("\n\nIP address: ");
      Serial.println(WiFi.localIP());

    }
    lastConnectedStatus = 1;
  }

}

void setup() {
  Serial.begin(115200);
  delay(500);
  int st = getStatusFromEeprom();
  if (st == 2) accessPointMode = true;
  else if (st != 0) saveSettingsToEEPPROM(ssid, pass); // run the void saveSettingsToEEPPROM on the first running or every time you want to save the default settings to eeprom
  Serial.println("\n\naccessPointMode=" + String(accessPointMode));
  readSettingsFromEEPROM(ssid, pass);   // read the SSID and Passsword from the EEPROM
  Serial.println(ssid);
  Serial.println(pass);
  if (accessPointMode) {     // start as Access Point
    initAsAccessPoint();
    serverAP.on("/", handle_OnConnect);
    serverAP.onNotFound(handle_NotFound);
    serverAP.begin();
    saveStatusToEeprom(0);  // enable the Client mode for the the next board starting
  }
  else {            // start as client
    Serial.println("Mode= Client");
    WiFi.begin(ssid, pass);
    // Enter your client setup code here
  }
  pinMode(accessPointButtonPin, INPUT);
  pinMode(accessPointLed, OUTPUT);
}

void loop() {
  if (accessPointMode) {
    serverAP.handleClient();
    playAccessPointLed();       // blink the LED every time the board works as Access Point
  }
  else {
    checkWiFiConnection();
    // enter your client code here

    if (millis() - lastUpdatedTime > 5000) {
      lastUpdatedTime = millis();
    }
  }
  checkIfModeButtonPushed();
}

void checkIfModeButtonPushed() {
  while (digitalRead(accessPointButtonPin)) {
    pushDownCounter++;
    if (debug) Serial.println(pushDownCounter);
    delay(1000);
    if (pushDownCounter == 20) {  // after 2 seconds the board will be restarted
      if (!accessPointMode) saveStatusToEeprom(2);   // write the number 2 to the eeprom
      ESP.restart();
    }
  }
  pushDownCounter = 0;
}

unsigned long lastTime = 0;
void playAccessPointLed() {
  if (millis() - lastTime > 300) {
    lastTime = millis();
    digitalWrite(accessPointLed, !digitalRead(accessPointLed));
  }
}

void handle_OnConnect() {
  if (debug) Serial.println("Client connected:  args=" + String(serverAP.args()));
  if (serverAP.args() >= 2)  {
    handleGenericArgs();
    serverAP.send(200, "text/html", SendHTML(1));
  }
  else  serverAP.send(200, "text/html", SendHTML(0));
}

void handle_NotFound() {
  if (debug) Serial.println("handle_NotFound");
  serverAP.send(404, "text/plain", "Not found");
}

void handleGenericArgs() { //Handler
  for (int i = 0; i < serverAP.args(); i++) {
    if (debug) Serial.println("*** arg(" + String(i) + ") =" + serverAP.argName(i));
    if (serverAP.argName(i) == "ssid") {
      if (debug)  Serial.print("sizeof(ssid)="); Serial.println(sizeof(ssid));
      memset(ssid, '\0', sizeof(ssid));
      strcpy(ssid, serverAP.arg(i).c_str());
    }
    else   if (serverAP.argName(i) == "pass") {
      if (debug) Serial.print("sizeof(pass)="); Serial.println(sizeof(pass));
      memset(pass, '\0', sizeof(pass));
      strcpy(pass, serverAP.arg(i).c_str());
    }
  }
  if (debug) Serial.println("*** New settings have received");
  if (debug) Serial.print("*** ssid="); Serial.println(ssid);
  if (debug) Serial.print("*** password="); Serial.println(pass);
  saveSettingsToEEPPROM(ssid, pass);
}

String SendHTML(uint8_t st) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>ESP WiFi Manager</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 30px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += "label{display:inline-block;width: 160px;text-align: right;}\n";
  ptr += "form{margin: 0 auto;width: 360px;padding: 1em;border: 1px solid #CCC;border-radius: 1em; background-color: #6e34db;}\n";
  ptr += "input {margin: 0.5em;}\n";
  if (st == 1) ptr += "h3{color: green;}\n";
  ptr += "</style>\n";
  ptr += "<meta charset=\"UTF-8\">\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>ESP WiFi Manager Using EEPROM</h1>\n";
  if (st == 1)ptr += "<h3>WiFi settings has saved successfully!</h3>\n";
  else if (st == 2)ptr += "<h3>WIFI Credentials has saved successfully!</h3>\n";
  else ptr += "<h3>Enter the WiFi settings</h3>\n";
  ptr += "<form>";
  ptr += "<div><label for=\"label_1\">WiFi SSID</label><input id=\"ssid_id\" required type=\"text\" name=\"ssid\" value=\"";
  ptr += ssid;
  ptr += "\" maxlength=\"32\"></div>\n";
  ptr += "<div><label for=\"label_2\">WiFi Password</label><input id=\"pass_id\" type=\"text\" name=\"pass\" value=\"";
  ptr += pass;
  ptr += "\" maxlength=\"32\"></div>\n";
  ptr += "<div><input type=\"submit\" value=\"Submit\"accesskey=\"s\"></div></form>";
  ptr += "<h5></h5>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
#define eepromBufferSize 200     // have to be >  eepromTextVariableSize * (eepromVariables+1)   (33 * (5+1))

void saveSettingsToEEPPROM(char* ssid_, char* pass_) {
  if (debug) Serial.println("\n============ saveSettingsToEEPPROM");
  writeEEPROM(1 * eepromTextVariableSize , eepromTextVariableSize , ssid_);
  writeEEPROM(2 * eepromTextVariableSize , eepromTextVariableSize ,  pass_);
}

void readSettingsFromEEPROM(char* ssid_, char* pass_) {
  readEEPROM( 1 * eepromTextVariableSize , eepromTextVariableSize , ssid_);
  readEEPROM( (2 * eepromTextVariableSize) , eepromTextVariableSize , pass_);

  if (debug) Serial.println("\n============ readSettingsFromEEPROM");
  if (debug) Serial.print("\n============ ssid="); if (debug) Serial.println(ssid_);
  if (debug) Serial.print("============ password="); if (debug) Serial.println(pass_);
}

void writeEEPROM(int startAdr, int length, char* writeString) {
  EEPROM.begin(eepromBufferSize);
  yield();
  for (int i = 0; i < length; i++) EEPROM.write(startAdr + i, writeString[i]);
  EEPROM.commit();
  EEPROM.end();
}

void readEEPROM(int startAdr, int maxLength, char* dest) {
  EEPROM.begin(eepromBufferSize);
  delay(10);
  for (int i = 0; i < maxLength; i++) dest[i] = char(EEPROM.read(startAdr + i));
  dest[maxLength - 1] = 0;
  EEPROM.end();
}

void saveStatusToEeprom(byte value) {
  EEPROM.begin(eepromBufferSize);
  EEPROM.write(0, value);
  EEPROM.commit();
  EEPROM.end();
}

byte getStatusFromEeprom() {
  EEPROM.begin(eepromBufferSize);
  byte value = 0;
  value = EEPROM.read (0);
  EEPROM.end();
  return value;
}
