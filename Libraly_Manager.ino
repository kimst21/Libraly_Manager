// ========================= 필수 라이브러리 가져오기 =========================
#include <WiFi.h>                // ESP32의 Wi-Fi 기능을 위한 라이브러리
#include <WebServer.h>           // ESP32에서 웹 서버를 실행하기 위한 라이브러리

#define accessPointButtonPin 7  // 액세스 포인트 모드를 활성화하는 버튼이 연결될 핀
WebServer serverAP(80);          // 액세스 포인트 웹 서버 (포트 80에서 실행)

#include <EEPROM.h>              // EEPROM을 사용하여 Wi-Fi 정보를 저장하고 로드하는 라이브러리
#define accessPointLed 35        // 액세스 포인트 모드에서 점멸할 LED 핀
#define eepromTextVariableSize 33 // SSID, 비밀번호 등 문자열 최대 크기 (32 + null 문자 포함)

// 기본 Wi-Fi SSID 및 비밀번호 (초기값)
char ssid[eepromTextVariableSize] = "WIFI-NAME";
char pass[eepromTextVariableSize] = "PASSWORD";

// 액세스 포인트 모드 활성화 여부
boolean accessPointMode = true;   // 보드가 시작될 때 기본적으로 AP 모드로 설정
boolean debug = false;            // 디버깅 모드 (true로 설정하면 디버깅 메시지 출력)
unsigned long lastUpdatedTime = 0; // 마지막 업데이트 시간 기록

int pushDownCounter = 0;          // 버튼이 눌린 시간 카운터
int lastConnectedStatus = 0;       // 마지막 Wi-Fi 연결 상태

// ========================= 액세스 포인트 모드 초기화 =========================
void initAsAccessPoint() {
  WiFi.softAP("ESP32-Access Point");  // "ESP32-Access Point"라는 SSID로 액세스 포인트 생성
  if (debug) Serial.println("AccesPoint IP: " + WiFi.softAPIP().toString());
  Serial.println("Mode= Access Point");  // 콘솔 출력: 현재 모드가 AP 모드임을 알림
  delay(100);
}

// ========================= Wi-Fi 연결 상태 확인 =========================
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {  // Wi-Fi가 연결되지 않았을 경우
    if (lastConnectedStatus == 1) Serial.println("WiFi disconnected\n");
    lastConnectedStatus = 0;
    Serial.print(".");
    delay(500);
  } else {  // Wi-Fi가 연결된 경우
    if (lastConnectedStatus == 0) {
      Serial.println("Mode= Client");
      Serial.print("\nWiFi connected to: ");
      Serial.println(ssid);
      Serial.print("\n\nIP address: ");
      Serial.println(WiFi.localIP());
    }
    lastConnectedStatus = 1;
  }
}

// ========================= 초기 설정 (setup 함수) =========================
void setup() {
  Serial.begin(115200);  // 시리얼 통신 시작
  delay(500);

  int st = getStatusFromEeprom();  // EEPROM에서 저장된 모드 상태 읽기
  if (st == 2) accessPointMode = true;  // EEPROM 값이 2면 AP 모드로 시작
  else if (st != 0) saveSettingsToEEPPROM(ssid, pass);  // EEPROM이 비어 있으면 초기 SSID/비밀번호 저장

  Serial.println("\n\naccessPointMode=" + String(accessPointMode));

  readSettingsFromEEPROM(ssid, pass);   // EEPROM에서 저장된 Wi-Fi 설정을 읽어오기
  Serial.println(ssid);
  Serial.println(pass);

  if (accessPointMode) {  // 액세스 포인트 모드일 경우
    initAsAccessPoint();
    serverAP.on("/", handle_OnConnect);  // 루트 URL 처리
    serverAP.onNotFound(handle_NotFound);
    serverAP.begin();
    saveStatusToEeprom(0);  // 다음 부팅 시 클라이언트 모드로 시작하도록 EEPROM 업데이트
  } else {  // 클라이언트 모드일 경우
    Serial.println("Mode= Client");
    WiFi.begin(ssid, pass);  // 저장된 SSID와 비밀번호로 Wi-Fi 연결 시도
  }

  pinMode(accessPointButtonPin, INPUT);  // 버튼 핀 설정
  pinMode(accessPointLed, OUTPUT);       // LED 핀 설정
}

// ========================= 메인 루프 (loop 함수) =========================
void loop() {
  if (accessPointMode) {
    serverAP.handleClient();   // AP 모드에서는 클라이언트 요청 처리
    playAccessPointLed();      // AP 모드에서 LED 점멸 실행
  } else {
    checkWiFiConnection();     // Wi-Fi 상태 확인

    if (millis() - lastUpdatedTime > 5000) {
      lastUpdatedTime = millis();
    }
  }

  checkIfModeButtonPushed();   // 모드 변경 버튼이 눌렸는지 확인
}

// ========================= 모드 변경 버튼 체크 =========================
void checkIfModeButtonPushed() {
  while (digitalRead(accessPointButtonPin)) {  // 버튼이 눌려 있는 경우
    pushDownCounter++;
    if (debug) Serial.println(pushDownCounter);
    delay(1000);
    if (pushDownCounter == 20) {  // 2초 동안 버튼을 누르면 보드를 재시작하고 AP 모드 활성화
      if (!accessPointMode) saveStatusToEeprom(2);
      ESP.restart();
    }
  }
  pushDownCounter = 0;
}

// ========================= AP 모드 LED 점멸 기능 =========================
unsigned long lastTime = 0;
void playAccessPointLed() {
  if (millis() - lastTime > 300) {
    lastTime = millis();
    digitalWrite(accessPointLed, !digitalRead(accessPointLed));  // LED 깜빡이기
  }
}

// ========================= 웹 서버 핸들러 =========================
void handle_OnConnect() {
  if (debug) Serial.println("Client connected:  args=" + String(serverAP.args()));
  if (serverAP.args() >= 2)  {
    handleGenericArgs();
    serverAP.send(200, "text/html", SendHTML(1));
  } else {
    serverAP.send(200, "text/html", SendHTML(0));
  }
}

void handle_NotFound() {
  if (debug) Serial.println("handle_NotFound");
  serverAP.send(404, "text/plain", "Not found");
}

void handleGenericArgs() {
  for (int i = 0; i < serverAP.args(); i++) {
    if (serverAP.argName(i) == "ssid") {
      memset(ssid, '\0', sizeof(ssid));
      strcpy(ssid, serverAP.arg(i).c_str());
    } else if (serverAP.argName(i) == "pass") {
      memset(pass, '\0', sizeof(pass));
      strcpy(pass, serverAP.arg(i).c_str());
    }
  }
  saveSettingsToEEPPROM(ssid, pass);
}

// ========================= HTML 페이지 생성 =========================
String SendHTML(uint8_t st) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>ESP WiFi Manager</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>ESP WiFi Manager Using EEPROM</h1>\n";
  if (st == 1) ptr += "<h3>WiFi settings saved successfully!</h3>\n";
  else ptr += "<h3>Enter the WiFi settings</h3>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

// ========================= EEPROM 핸들러 =========================
void saveSettingsToEEPPROM(char* ssid_, char* pass_) {
  writeEEPROM(1 * eepromTextVariableSize, eepromTextVariableSize, ssid_);
  writeEEPROM(2 * eepromTextVariableSize, eepromTextVariableSize, pass_);
}

void readSettingsFromEEPROM(char* ssid_, char* pass_) {
  readEEPROM(1 * eepromTextVariableSize, eepromTextVariableSize, ssid_);
  readEEPROM(2 * eepromTextVariableSize, eepromTextVariableSize, pass_);
}

void writeEEPROM(int startAdr, int length, char* writeString) {
  EEPROM.begin(200);
  for (int i = 0; i < length; i++) EEPROM.write(startAdr + i, writeString[i]);
  EEPROM.commit();
  EEPROM.end();
}

void readEEPROM(int startAdr, int maxLength, char* dest) {
  EEPROM.begin(200);
  for (int i = 0; i < maxLength; i++) dest[i] = char(EEPROM.read(startAdr + i));
  EEPROM.end();
}

void saveStatusToEeprom(byte value) {
  EEPROM.begin(200);
  EEPROM.write(0, value);
  EEPROM.commit();
  EEPROM.end();
}

byte getStatusFromEeprom() {
  EEPROM.begin(200);
  byte value = EEPROM.read(0);
  EEPROM.end();
  return value;
}
