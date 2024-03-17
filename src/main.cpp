/*
 * The dongle subscribes to two MQTT topics and displays the values alternating.
 * A publisher on button push is also implemented
 * 
 *
 * Author: Bjoern Heller <tec att sixtopia.net>
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include "TFT_eSPI.h"
#include <SPI.h>
#include <FastLED.h>
#include <OneButton.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// MQTT Broker settings
const char* mqtt_server = "";
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_topic1 = "/T9602-1/temp"; // First topic
const char* mqtt_topic2 = "/T9602/temp"; // Second topic

// Messages storage
String message1 = "Waiting...";
String message2 = "Waiting...";

// Timing
unsigned long lastSwitchTime = 0;
const long switchInterval = 2000; // Switch every 1000 milliseconds (1 second)
bool displayTopic1 = true; // Start with topic 1

// TTGO T-Dongle S3 Pin Configuration
#define BTN_PIN     0

#define LED_DI_PIN     40
#define LED_CI_PIN     39

#define TFT_CS_PIN     4
#define TFT_SDA_PIN    3
#define TFT_SCL_PIN    5
#define TFT_DC_PIN     2
#define TFT_RES_PIN    1
#define TFT_LEDA_PIN   38

#define SD_MMC_D0_PIN  14
#define SD_MMC_D1_PIN  17
#define SD_MMC_D2_PIN  21
#define SD_MMC_D3_PIN  18
#define SD_MMC_CLK_PIN 12
#define SD_MMC_CMD_PIN 16

#define MAX_GEN_COUNT 500

TFT_eSPI tft = TFT_eSPI();
CRGB leds;
OneButton button(BTN_PIN, true);
uint8_t btn_press = 0;

WiFiClient espClient;
PubSubClient client(espClient);

int fanSpeed = 10; // Initial value
String message = "";

void led_task(void *param) {
  while (1) {
    static uint8_t hue = 0;
    leds = CHSV(hue++, 0XFF, 100);
    FastLED.show();
    delay(50);
  }
}

void setup_wifi() {
  delay(10);
  // Connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void displayMessage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(false);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);

  if (displayTopic1) {
    tft.println("Temp INSIDE:");
    tft.setTextSize(4);
    tft.println(message1);
  } else {
    tft.println("Temp OUTSIDE:");
    tft.setTextSize(4);
    tft.println(message2);
  }
    tft.setCursor(0, 45);
    tft.setTextColor(TFT_VIOLET);
    tft.setTextSize(2);
    tft.println("Fan01 Speed:");
    tft.println(fanSpeed);
}

void callback(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';

  if (strcmp(topic, mqtt_topic1) == 0) {
    message1 = String(msg);
  } else if (strcmp(topic, mqtt_topic2) == 0) {
    message2 = String(msg);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("t-dongle-s3", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Subscribe to the topics
      client.subscribe(mqtt_topic1);
      client.subscribe(mqtt_topic2);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TFT_LEDA_PIN, OUTPUT);
  // Initialise TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_SKYBLUE);
  digitalWrite(TFT_LEDA_PIN, 0);
  FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  xTaskCreatePinnedToCore(led_task, "led_task", 1024, NULL, 1, NULL, 0);
  button.attachClick([] {
    btn_press = 1 ^ btn_press;
    fanSpeed = (fanSpeed == 10) ? 35 : 10; // Toggle between 10 and 30
    String message = String(fanSpeed);
    client.publish("fan01", message.c_str());
    Serial.println("Fan speed set to: " + message);
    tft.setCursor(0, 45);
    tft.setTextColor(TFT_VIOLET);
    tft.setTextSize(2);
    tft.println("Fan01 Speed:");
    tft.println(message);
  });
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastSwitchTime >= switchInterval) {
    lastSwitchTime = currentMillis;
    displayTopic1 = !displayTopic1; // Switch between topic 1 and topic 2
    displayMessage(); // Update display
  }
   button.tick();
}
