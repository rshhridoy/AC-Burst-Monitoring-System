#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <ThingSpeak.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "Gunslinger";       // Replace with your WiFi SSID
const char* password = "12@#hridoy#@12"; // Replace with your WiFi password

// ThingSpeak API Key
const char* server = "api.thingspeak.com";
const char* apiKey = "FF44606RPFOMXC5W"; // Replace with your ThingSpeak API Key

// Pin definitions
#define DHTPIN D2                 // DHT22 sensor connected to D2
#define DHTTYPE DHT22             // Define sensor type DHT22
#define MQ2_PIN A0                // MQ-2 connected to A0 (analog input)
#define FIRE_SENSOR D3            // Fire sensor connected to D3 (GPIO 0)
#define BUZZER_PIN D5             // Buzzer connected to D5 (GPIO 14)
#define RED_LED_PIN D6            // Red LED connected to D6 (GPIO 12)
#define GREEN_LED_PIN D7          // Green LED connected to D7 (GPIO 13)

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD at address 0x27, 16 chars and 2 rows

const float TEMP_THRESHOLD = 50.0;   // Temperature threshold (in Celsius)
const int GAS_THRESHOLD = 250;      // Analog threshold for gas detection
const int FIRE_DETECTED = LOW;      // Fire sensor triggers when LOW

WiFiClient client;

void setup() {
  Serial.begin(9600);

  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Initialize DHT sensor
  dht.begin();

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart AC Monitor");
  delay(2000);
  lcd.clear();

  // Initialize WiFi
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // WiFi connected
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(2000);
  lcd.clear();
  Serial.println("\nConnected to WiFi");


  // Initialize pins
  pinMode(MQ2_PIN, INPUT);
  pinMode(FIRE_SENSOR, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  
  digitalWrite(BUZZER_PIN, LOW);      // Start with buzzer off
  digitalWrite(RED_LED_PIN, LOW);     // Start with red LED off
  digitalWrite(GREEN_LED_PIN, HIGH);  // Start with green LED on to indicate normal state
}

unsigned long previousMillis = 0;
const long interval = 3000; // Switch pages every 3 seconds
int currentPage = 0;

void loop() {
  // ---------- DHT22 Sensor Reading ----------
  float humidity = dht.readHumidity();
  float temperatureDHT = dht.readTemperature();

  // Read Gas Levels from MQ-2
  int gasLevel = analogRead(MQ2_PIN);

  // Read Fire Sensor State
  int fireState = digitalRead(FIRE_SENSOR);


  // Display readings on Serial Monitor
  Serial.print("Temperature: ");
  Serial.print(temperatureDHT);
  Serial.print(" °C, Humidity: ");
  Serial.print(humidity);
  Serial.print("%, Gas Level: ");
  Serial.print(gasLevel);
  Serial.print(", Fire Detected: ");
  Serial.println(fireState == FIRE_DETECTED ? "YES" : "NO");

  // Timer logic for page switching
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // Reset the timer
    currentPage = (currentPage + 1) % 2; // Toggle between pages (0 and 1)
  }

  // Display data on LCD based on the current page
  lcd.clear();
  if (currentPage == 0) {
    // Page 1: Temperature and Humidity
    lcd.setCursor(0, 0);
    if (isnan(humidity) || isnan(temperatureDHT)) {
      lcd.print("DHT22 Error");
    } else {
      lcd.print("Temp: ");
      lcd.print(temperatureDHT);
      lcd.print("C");
      lcd.setCursor(0, 1);
      lcd.print("Hum: ");
      lcd.print(humidity);
      lcd.print("%");
    }
  } else if (currentPage == 1) {
    // Page 2: Gas Level and Fire Detection
    lcd.setCursor(0, 0);
    lcd.print("Gas: ");
    lcd.print(gasLevel);
    lcd.setCursor(0, 1);
    lcd.print("Fire: ");
    lcd.print(fireState == FIRE_DETECTED ? "YES" : "NO");
  }

  
  // Check conditions and trigger alarms
  if (temperatureDHT > TEMP_THRESHOLD || gasLevel > GAS_THRESHOLD || fireState == FIRE_DETECTED) {
    triggerAlarm();  // Activate red LED and buzzer in alert state
    lcd.setCursor(0, 0);
    lcd.print("    ALERT!     ");
    lcd.setCursor(0, 1);
    lcd.print("    DANGER     ");
  } else {
    resetAlarm();    // Deactivate red LED and buzzer, turn on green LED for normal state
  }

  // Send data to ThingSpeak
  sendToThingSpeak(temperatureDHT, humidity, gasLevel, fireState == FIRE_DETECTED ? 1 : 0);

  delay(2000); // Update every 2 seconds
}

void triggerAlarm() {
  digitalWrite(BUZZER_PIN, HIGH);   // Turn on buzzer
  digitalWrite(RED_LED_PIN, HIGH);  // Turn on red LED
  digitalWrite(GREEN_LED_PIN, LOW); // Turn off green LED
  Serial.println("ALERT! Potential AC burst detected!");
}

void resetAlarm() {
  digitalWrite(BUZZER_PIN, LOW);    // Turn off buzzer
  digitalWrite(RED_LED_PIN, LOW);   // Turn off red LED
  digitalWrite(GREEN_LED_PIN, HIGH); // Turn on green LED
}

void sendToThingSpeak(float temp, float hum, int gas, int fire) {
  if (client.connect(server, 80)) {
    String url = "/update?api_key=" + String(apiKey) +
                 "&field1=" + String(temp) +
                 "&field2=" + String(hum) +
                 "&field3=" + String(gas) +
                 "&field4=" + String(fire);
    Serial.print("Sending to ThingSpeak: ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + server + "\r\n" +
                 "Connection: close\r\n\r\n");
    client.stop();
  } else {
    Serial.println("Connection to ThingSpeak failed");
  }
}
