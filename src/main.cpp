#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "AdafruitIO_WiFi.h"
#include <WiFiManager.h>
#include <LittleFS.h>

// ========== WIFI & ADAFRUIT IO ==========
// Wi-Fi and Adafruit IO credentials are entered through the setup portal
// and stored on the board, so nothing secret lives in the code.

#define PORTAL_NAME     "LevelDetector-Setup"
#define CONFIG_FILE     "/aio.txt"
#define RESET_PIN       D3          // FLASH button on NodeMCU
#define RESET_HOLD_MS   3000

char aioUser[41] = "";
char aioKey[65]  = "";
String wifiSsid;
String wifiPass;

// Created in setup() once the credentials are known
AdafruitIO_WiFi *io = nullptr;

// ========== ADAFRUIT IO FEEDS ==========

AdafruitIO_Feed *temperatureFeed = nullptr;
AdafruitIO_Feed *humidityFeed    = nullptr;
AdafruitIO_Feed *distanceFeed    = nullptr;

// ========== OLED ==========

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

#define OLED_SDA D2
#define OLED_SCL D1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ========== DHT11 ==========

#define DHTPIN D7
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ========== HC-SR04 ==========

#define TRIG_PIN D5
#define ECHO_PIN D6

// ========== VARIABLES ==========

float temperature = NAN;
float humidity = NAN;
float distance = NAN;

unsigned long lastRead = 0;
unsigned long lastUpload = 0;

const unsigned long READ_INTERVAL = 2000;
const unsigned long UPLOAD_INTERVAL = 30000;

// ========== COMPANY HEADER ==========

void drawHeader() {

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(22, 0);
  display.print("BILLION CARBON");

  display.drawLine(
    0, 11, 127, 11,
    SSD1306_WHITE
  );
}

// ========== ULTRASONIC ICON ==========

void drawHCSR04Icon(int x, int y) {

  display.drawRect(
    x, y, 36, 20,
    SSD1306_WHITE
  );

  display.drawCircle(
    x + 10, y + 10, 6,
    SSD1306_WHITE
  );

  display.drawCircle(
    x + 26, y + 10, 6,
    SSD1306_WHITE
  );

  display.fillCircle(
    x + 10, y + 10, 2,
    SSD1306_WHITE
  );

  display.fillCircle(
    x + 26, y + 10, 2,
    SSD1306_WHITE
  );
}

// ========== OBSTACLE ICON ==========

void drawObstacle(int x, int y) {

  display.fillTriangle(
    x, y,
    x + 8, y + 5,
    x, y + 10,
    SSD1306_WHITE
  );
}

// ========== DISTANCE MEASUREMENT ==========

float readDistance() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(
    ECHO_PIN,
    HIGH,
    30000UL
  );

  if (duration == 0) {
    return NAN;
  }

  float cm = duration * 0.0343 / 2.0;

  if (cm < 2 || cm > 400) {
    return NAN;
  }

  return cm;
}

// ========== READ ALL SENSORS ==========

void readSensors() {

  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  temperature = newTemp;
  humidity = newHumidity;

  distance = readDistance();

  Serial.println("----- SENSOR DATA -----");

  Serial.print("Temperature: ");
  Serial.println(temperature);

  Serial.print("Humidity: ");
  Serial.println(humidity);

  Serial.print("Distance: ");
  Serial.println(distance);
}

// ========== OLED DISPLAY ==========

void updateDisplay() {

  display.clearDisplay();

  drawHeader();

  // Temperature
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.print("Temp: ");

  if (!isnan(temperature)) {
    display.print(temperature, 1);
    display.print(" C");
  } else {
    display.print("--");
  }

  // Humidity
  display.setCursor(0, 26);
  display.print("Humidity: ");

  if (!isnan(humidity)) {
    display.print(humidity, 1);
    display.print("%");
  } else {
    display.print("--");
  }

  // Distance
  display.setCursor(0, 39);
  display.print("Distance:");

  display.setTextSize(2);
  display.setCursor(0, 49);

  if (!isnan(distance)) {
    display.print(distance, 0);
    display.print("cm");
  } else {
    display.print("--");
  }

  // Sensor icon
  drawHCSR04Icon(89, 39);

  // Moving obstacle
  if (!isnan(distance)) {

    int obstacleX = map(
      (long)distance,
      2, 400,
      89, 118
    );

    obstacleX = constrain(
      obstacleX,
      89, 118
    );

    drawObstacle(obstacleX, 29);
  }

  display.display();
}

// ========== STATUS MESSAGE ==========

void showMessage(const char *line1, const char *line2 = "", const char *line3 = "") {

  display.clearDisplay();
  drawHeader();

  display.setTextSize(1);

  display.setCursor(0, 16);
  display.print(line1);

  display.setCursor(0, 28);
  display.print(line2);

  display.setCursor(0, 40);
  display.print(line3);

  display.display();
}

// ========== SAVED CONFIG ==========

void loadConfig() {

  File f = LittleFS.open(CONFIG_FILE, "r");

  if (!f) {
    return;
  }

  String user = f.readStringUntil('\n');
  String key  = f.readStringUntil('\n');
  f.close();

  user.trim();
  key.trim();

  user.toCharArray(aioUser, sizeof(aioUser));
  key.toCharArray(aioKey, sizeof(aioKey));
}

void saveConfig() {

  File f = LittleFS.open(CONFIG_FILE, "w");

  if (!f) {
    Serial.println("Failed to save config");
    return;
  }

  f.println(aioUser);
  f.println(aioKey);
  f.close();
}

// ========== WIFI SETUP PORTAL ==========

void onPortalStart(WiFiManager *wm) {

  showMessage("Join Wi-Fi:", PORTAL_NAME, "then open 192.168.4.1");
}

// Holding the reset button at boot clears the saved Wi-Fi and Adafruit IO details
bool resetRequested() {

  if (digitalRead(RESET_PIN) != LOW) {
    return false;
  }

  showMessage("Hold to reset", "Wi-Fi settings...");

  unsigned long start = millis();

  while (digitalRead(RESET_PIN) == LOW) {

    if (millis() - start >= RESET_HOLD_MS) {
      return true;
    }

    delay(50);
  }

  return false;
}

void connectNetwork() {

  WiFiManager wm;

  WiFiManagerParameter userParam(
    "aio_user", "Adafruit IO username",
    aioUser, sizeof(aioUser) - 1
  );

  WiFiManagerParameter keyParam(
    "aio_key", "Adafruit IO key",
    aioKey, sizeof(aioKey) - 1
  );

  wm.addParameter(&userParam);
  wm.addParameter(&keyParam);

  wm.setAPCallback(onPortalStart);
  wm.setConfigPortalTimeout(300);
  wm.setConnectTimeout(20);

  if (resetRequested()) {

    wm.resetSettings();
    LittleFS.remove(CONFIG_FILE);

    aioUser[0] = '\0';
    aioKey[0]  = '\0';
  }

  showMessage("Connecting Wi-Fi...");

  bool connected;

  if (aioUser[0] == '\0' || aioKey[0] == '\0') {
    connected = wm.startConfigPortal(PORTAL_NAME);
  } else {
    connected = wm.autoConnect(PORTAL_NAME);
  }

  if (!connected) {

    showMessage("Wi-Fi setup failed", "Restarting...");
    delay(3000);
    ESP.restart();
  }

  // Keep the Adafruit IO details if they were entered or changed in the portal
  String user = userParam.getValue();
  String key  = keyParam.getValue();

  user.trim();
  key.trim();

  if (user != aioUser || key != aioKey) {

    user.toCharArray(aioUser, sizeof(aioUser));
    key.toCharArray(aioKey, sizeof(aioKey));

    saveConfig();
  }

  if (aioUser[0] == '\0' || aioKey[0] == '\0') {

    showMessage("Adafruit IO details", "missing, restarting");
    delay(3000);
    ESP.restart();
  }

  wifiSsid = WiFi.SSID();
  wifiPass = WiFi.psk();
}

// ========== UPLOAD DATA ==========

void uploadData() {

  if (io == nullptr || io->status() != AIO_CONNECTED) {

    Serial.println("Adafruit IO not connected");
    return;
  }

  Serial.println("Uploading data...");

  if (!isnan(temperature)) {
    temperatureFeed->save(temperature);
  }

  if (!isnan(humidity)) {
    humidityFeed->save(humidity);
  }

  if (!isnan(distance)) {
    distanceFeed->save(distance);
  }

  Serial.println("Upload requests sent");
}

// ========== SETUP ==========

void setup() {

  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RESET_PIN, INPUT_PULLUP);

  LittleFS.begin();
  loadConfig();

  digitalWrite(TRIG_PIN, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);

  dht.begin();

  if (!display.begin(
    SSD1306_SWITCHCAPVCC,
    OLED_ADDR
  )) {

    Serial.println("OLED failed");

    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  drawHeader();

  display.setCursor(0, 25);
  display.print("Starting system...");

  display.display();

  // Wi-Fi (saved, or entered through the setup portal)
  connectNetwork();

  // Connect to Adafruit IO
  io = new AdafruitIO_WiFi(
    aioUser,
    aioKey,
    wifiSsid.c_str(),
    wifiPass.c_str()
  );

  temperatureFeed = io->feed("temperature");
  humidityFeed    = io->feed("humidity");
  distanceFeed    = io->feed("distance");

  Serial.println("Connecting to Adafruit IO");
  showMessage("Connecting to", "Adafruit IO...");

  io->connect();

  unsigned long connectStart = millis();

  while (io->status() < AIO_CONNECTED && millis() - connectStart < 20000UL) {
    delay(500);
  }

  Serial.println(io->statusText());

  // Initial sensor reading
  readSensors();
  updateDisplay();

  lastRead = millis();
  lastUpload = millis();
}

// ========== MAIN LOOP ==========

void loop() {

  // Maintain Adafruit IO connection
  io->run();

  unsigned long currentMillis = millis();

  // Read sensors every 2 seconds
  if (currentMillis - lastRead >= READ_INTERVAL) {

    lastRead = currentMillis;

    readSensors();
    updateDisplay();
  }

  // Upload every 30 seconds
  if (currentMillis - lastUpload >= UPLOAD_INTERVAL) {

    lastUpload = currentMillis;

    uploadData();
  }
}

