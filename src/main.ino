#include "config.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <MFRC522.h>
#include <SPI.h>
#include <WiFiMulti.h>


bool isProcessingCard = false;
String nuipValue;

WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

unsigned long lastPingTime = 0;           // last ping time
const unsigned long pingInterval = 30000; // interval to next ping

#define SS_PIN 21
#define RST_PIN 22
#define SIZE_BUFFER 18
#define MAX_SIZE_BLOCK 16
#define blue_led_pin 2

MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup()
{

  Serial.begin(115200);
  pinMode(blue_led_pin, OUTPUT);

  // Connect to WiFi
  connectToWiFi();

  // Connect to WebSocket server
  connectToWebSocket();

  // Initialize MFRC522 RFID reader
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Approach your RFID card to read...");
  webSocket.onEvent(webSocketEventHandler);
}

void loop()
{
  webSocket.loop();
  unsigned long currentTime = millis();
  if (currentTime - lastPingTime >= pingInterval)
  {
    sendPing();
    lastPingTime = currentTime;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi connection lost. Reconnecting...");
    connectToWiFi();
  }

  if (!mfrc522.PICC_IsNewCardPresent())
  {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }
  if (!isProcessingCard)
  {
    String rfidData = readingData();
    if (rfidData != "Fail")
    {
      sendRfidDataToServer(rfidData);
      delay(1000);
    }
  }
  if (isProcessingCard)
  {
    String hex_data = writingData(nuipValue);
    if (hex_data == "Fail")
    {
      return;
    }
    Serial.println("Data successfully written to the card.");
    Serial.println(hex_data);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void connectToWiFi()
{
  Serial.println("Connecting to WiFi");
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASS);
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected.");
  Serial.println(WiFi.localIP());
}

void connectToWebSocket()
{
  Serial.println("Connecting to WebSocket server");
  webSocket.beginSslWithCA("sevae-backend.onrender.com", 443, "/", ENDPOINT_CA_CERT);
}

void sendPing()
{
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["ping"] = "Ping";

  // Serialize the JSON object to a string
  String jsonStr;
  serializeJson(jsonDoc, jsonStr);

  if (webSocket.sendTXT(jsonStr))
  {
    Serial.println("Ping sent.");
  }
  else
  {
    Serial.println("Failed to send ping.");
  }
}

String readingData()
{
  String info = "";

  for (byte i = 0; i < 6; i++)
    key.keyByte[i] = 0xFF;

  byte buffer[SIZE_BUFFER] = {0};
  byte block = 1;
  byte size = SIZE_BUFFER;
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return "Fail";
  }

  status = mfrc522.MIFARE_Read(block, buffer, &size);

  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return "Fail";
  }

  for (uint8_t i = 0; i < MAX_SIZE_BLOCK; i++)
  {
    info += String(buffer[i], HEX);
  }

  return info;
}

void sendRfidDataToServer(String rfidData)
{
  // Create a JSON object
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["rfidData"] = rfidData;

  // Serialize the JSON object to a string
  String jsonStr;
  serializeJson(jsonDoc, jsonStr);

  // Send the JSON string to the WebSocket server
  webSocket.sendTXT(jsonStr);

  Serial.println("RFID data sent to server: " + rfidData);
}

void onMessage(String message)
{
  Serial.print("Mensaje recibido: ");
  Serial.println(message);

  // Mensaje recibido desde el servidor
  String receivedMessage = message;

  if (receivedMessage.indexOf("nuip_student") != -1)
  {
    int nuipPos = receivedMessage.indexOf("nuip_student") + 14;
    nuipValue = receivedMessage.substring(nuipPos);
    nuipValue.trim();
    isProcessingCard = true;
  }
}

String writingData(String nuip)
{
  for (byte i = 0; i < 6; i++)
    key.keyByte[i] = 0xFF;

  byte buffer[SIZE_BUFFER] = {0};
  byte block = 1;
  byte size = SIZE_BUFFER;
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return "Fail";
  }

  for (uint8_t i = 0; i < nuip.length(); i++)
  {
    buffer[i] = nuip[i];
  }

  status = mfrc522.MIFARE_Write(block, buffer, MAX_SIZE_BLOCK);

  if (status != MFRC522::STATUS_OK)
  {
    Serial.print(F("Writing failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return "Fail";
  }
  else
  {
    digitalWrite(blue_led_pin, HIGH);
    delay(1000);
    digitalWrite(blue_led_pin, LOW);
  }
  isProcessingCard = false;

  return nuip;
}

void webSocketEventHandler(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[WSc] Disconnected\n");
    break;
  case WStype_CONNECTED:
    Serial.printf("[WSc] Connected to url: %s\n", payload);

    break;

  case WStype_TEXT:
    Serial.printf("[WSc] get text: %s\n", payload);
    onMessage(String((char *)payload));
    break;

  case WStype_BIN:
    Serial.printf("[WSc] get binary length: %u\n", length);
    break;

  case WStype_ERROR:
    Serial.printf("[WSc] get error length: %u\n", length);
    break;

  default:
    Serial.printf("[WSc] Unknown transmission: %i. Probably can be ignored.", type);
  }
}