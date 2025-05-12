#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- OLED config ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Sensor config ---
#define DHTPIN 23
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- WiFi Credentials ---
const char* ssid = "YES_5G_FOR_ALL_2.4GHz_4519";
const char* password = "3AC56A59";

// --- Firebase Auth ---
const char* authHost = "identitytoolkit.googleapis.com";
const char* apiKey = "AIzaSyA7kb8ZY-0DusBBFFfWStJxHRG7tC2Wswc";
const char* userEmail = "mwael3785@gmail.com";
const char* userPassword = "Mwael2006_";
const char* dbHost = "esp32-dht11-dc430-default-rtdb.asia-southeast1.firebasedatabase.app";
String idToken;

// --- Timestamp Formatter ---
String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char buf[20];
  sprintf(buf, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  return String(buf);
}

// --- Upload to Firebase ---
void writeToFirebase(float temperature, float humidity) {
  WiFiClientSecure dbClient;
  dbClient.setInsecure();

  String timestamp = getTimestamp();
  String path = "/sensor_log/" + timestamp + ".json?auth=" + idToken;
  String data = "{\"temp\": " + String(temperature) + ", \"humidity\": " + String(humidity) + "}";

  Serial.println("\n📤 Uploading to Firebase...");
  Serial.println("Connecting to: " + String(dbHost));

  if (!dbClient.connect(dbHost, 443)) {
    Serial.println("❌ Firebase DB connection failed.");
    return;
  }

  dbClient.println("PUT " + path + " HTTP/1.1");
  dbClient.println("Host: " + String(dbHost));
  dbClient.println("Content-Type: application/json");
  dbClient.println("Content-Length: " + String(data.length()));
  dbClient.println();
  dbClient.println(data);

  unsigned long timeout = millis() + 3000;
  while (dbClient.connected() && millis() < timeout) {
    while (dbClient.available()) {
      Serial.write(dbClient.read());
      yield();
    }
    yield();
  }

  dbClient.stop();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA, SCL
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED init failed");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();
  delay(1000);

  Serial.println("📶 Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    yield();
  }
  Serial.println("\n✅ WiFi connected.");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 10000) {
    delay(500);
    Serial.print("*");
    yield();
  }
  Serial.println("\n⏰ Time synced.");

  WiFiClientSecure client;
  client.setInsecure();

  Serial.println("🔐 Authenticating with Firebase...");
  if (!client.connect(authHost, 443)) {
    Serial.println("❌ Auth connection failed.");
    return;
  }

  String url = "/v1/accounts:signInWithPassword?key=" + String(apiKey);
  String payload = "{\"email\":\"" + String(userEmail) + "\",\"password\":\"" + String(userPassword) + "\",\"returnSecureToken\":true}";

  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + String(authHost));
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println();
  client.println(payload);

  String response = "";
  unsigned long timeout = millis() + 5000;
  while (client.connected() && millis() < timeout) {
    while (client.available()) {
      char c = client.read();
      response += c;
      yield();
    }
    yield();
  }

  int jsonStart = response.indexOf('{');
  int jsonEnd = response.lastIndexOf('}');
  if (jsonStart > 0 && jsonEnd > jsonStart) {
    String jsonBody = response.substring(jsonStart, jsonEnd + 1);
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, jsonBody);
    if (!err) {
      idToken = doc["idToken"].as<String>();
      Serial.println("✅ Firebase Auth success.");
    } else {
      Serial.println("❌ Failed to parse Firebase auth response.");
    }
  } else {
    Serial.println("❌ Invalid Firebase auth response.");
  }
}

unsigned long lastUpload = 0;
const unsigned long uploadInterval = 10000;

void loop() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("❌ Sensor read failed.");
    delay(1000);
    yield();
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf(" Temp: %.1f C\n", temp);
  display.printf(" Humidity: %.1f %%\n", hum);

  if (millis() - lastUpload >= uploadInterval) {
    display.setCursor(0, 32);
    display.println(" Uploading...");
    display.display();
    yield();

    writeToFirebase(temp, hum);

    display.setCursor(0, 48);
    display.println(" Uploaded!");
    lastUpload = millis();
  }

  display.display();
  yield();
  delay(1000);  
}
