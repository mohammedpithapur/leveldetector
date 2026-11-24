#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "AdafruitIO_WiFi.h"

// ========== WIFI & ADAFRUIT IO ==========

#include "secrets.h"

AdafruitIO_WiFi io(
  IO_USERNAME,
  IO_KEY,
  WIFI_SSID,
  WIFI_PASS
);

// ========== ADAFRUIT IO FEEDS ==========

AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed    = io.feed("humidity");
AdafruitIO_Feed *distanceFeed    = io.feed("distance");

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

// ========== UPLOAD DATA ==========

void uploadData() {

  if (io.status() != AIO_CONNECTED) {

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

  // Connect to Adafruit IO
  Serial.println("Connecting to Adafruit IO");

  io.connect();

  // Initial sensor reading
  readSensors();
  updateDisplay();

  lastRead = millis();
  lastUpload = millis();
}

// ========== MAIN LOOP ==========

void loop() {

  // Maintain Adafruit IO connection
  io.run();

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

