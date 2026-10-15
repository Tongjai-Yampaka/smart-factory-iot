// Smart Factory Cirkit Designer ESP32-S3 - deploy separately for M01 and M02
// USE DEMO DATA ONLY. This lab uses setInsecure() for simulator compatibility,
// which DISABLES TLS certificate verification. Do not use this practice in production.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char* MACHINE_ID = "M01";  // change to M02 for second ESP32-S3
const char* WIFI_SSID = "CirkitWifi";
const char* CLOUD_URL = ""; // paste deployed Google Apps Script /exec URL
const char* DEVICE_TOKEN = ""; // same as Script Properties DEVICE_TOKEN
const int TEMP_PIN=4, VIB_PIN=5, BUTTON_PIN=6, GREEN_LED=15, RED_LED=16;
unsigned long previousRead=0, previousSend=0, lastButtonChange=0;
bool lastRawButton=HIGH, stableButton=HIGH;
unsigned long totalCount=0;
float tempC=20, vibration=0, energy=3;
bool warning=false;
unsigned int seqNo=0;

void readMachine(){
  int rawTemp=analogRead(TEMP_PIN), rawVibration=analogRead(VIB_PIN);
  tempC=20.0f+80.0f*rawTemp/4095.0f;
  vibration=10.0f*rawVibration/4095.0f;
  energy=3.0f+vibration*0.4f;
  warning=tempC>85.0f || vibration>7.0f;
  digitalWrite(GREEN_LED,warning?LOW:HIGH);
  digitalWrite(RED_LED,warning?HIGH:LOW);
}
void pollButton(unsigned long now){
  bool current=digitalRead(BUTTON_PIN);
  if(current!=lastRawButton){lastButtonChange=now;lastRawButton=current;}
  if(now-lastButtonChange>40 && current!=stableButton){
    stableButton=current;
    if(stableButton==LOW)totalCount++;
  }
}
void connectWiFi(){
  if(WiFi.status()==WL_CONNECTED)return;
  WiFi.begin(WIFI_SSID,"");
  Serial.print("Connecting CirkitWifi");
  for(int i=0;i<50 && WiFi.status()!=WL_CONNECTED;i++){delay(100);Serial.print('.');}
  Serial.println(WiFi.status()==WL_CONNECTED?" Connected":" WiFi not connected");
}
void sendTelemetry(){
  if(strlen(CLOUD_URL)==0 || strlen(DEVICE_TOKEN)==0){
    Serial.println("CLOUD NOT CONFIGURED: sensor/serial simulation only");return;
  }
  connectWiFi();if(WiFi.status()!=WL_CONNECTED){Serial.println("UPLOAD FAILED: WiFi offline");return;}
  // Message ID remains unique even if the ESP32 reboots (random ID + sequence).
  String mid=String(MACHINE_ID)+"-"+String(millis())+"-"+String(++seqNo)+"-"+String((unsigned int)random(0,65535));
  unsigned long defects=totalCount/20;
  unsigned long goods=totalCount-defects;
  String body="{\"message_id\":\""+mid+"\",\"machine_id\":\""+MACHINE_ID+"\",\"device_token\":\""+DEVICE_TOKEN+"\"";
  body+=",\"temperature\":"+String(tempC,1)+",\"humidity\":55.0,\"vibration\":"+String(vibration,2);
  body+=",\"energy_kw\":"+String(energy,2)+",\"machine_status\":\""+String(warning?"WARNING":"RUNNING")+"\"";
  body+=",\"total_count\":"+String(totalCount)+",\"good_count\":"+String(goods)+",\"defect_count\":"+String(defects)+"}";
  WiFiClientSecure client;
  client.setInsecure(); // SIMULATOR-ONLY, disables verification - NOT FOR REAL INDUSTRIAL IOT
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(12000);
  if(!http.begin(client,CLOUD_URL)){Serial.println("HTTP begin failed");return;}
  http.addHeader("Content-Type","application/json");
  int code=http.POST(body);
  String response=http.getString();
  Serial.printf("MACHINE=%s HTTP=%d TEMP=%.1f VIB=%.2f\n",MACHINE_ID,code,tempC,vibration);
  Serial.println("Response: "+response);
  if(code>=200 && code<300 && response.indexOf("\"stored\":true")>=0)Serial.println("SERVER CONFIRMED STORE (verify Google Sheet row)");
  else Serial.println("NOT CONFIRMED: inspect response and Google Sheets");
  http.end();
}
void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(BUTTON_PIN,INPUT_PULLUP);
  pinMode(GREEN_LED,OUTPUT);pinMode(RED_LED,OUTPUT);
  randomSeed(analogRead(TEMP_PIN));
  connectWiFi();
  readMachine();
}
void loop(){
  unsigned long now=millis();pollButton(now);
  if(now-previousRead>=500){previousRead=now;readMachine();Serial.printf("%s TEMP=%.1fC VIB=%.2fmm/s COUNT=%lu STATE=%s\n",MACHINE_ID,tempC,vibration,totalCount,warning?"WARNING":"RUNNING");}
  if(now-previousSend>=10000){previousSend=now;sendTelemetry();}
  delay(10);
}
