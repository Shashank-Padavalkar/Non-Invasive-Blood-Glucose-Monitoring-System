#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

#define I2C_SLAVE_ADDRESS 0x04

const char* ssid = "WIFI_SSID"; // Replace with your Wi-Fi SSID
const char* password = "WIFI_PASSWORD"; // Replace with your Wi-Fi password

const char* firebaseHost = "FIREBASE_HOST_URL"; // Replace with Firebase host URL
const char* firebaseApiKey = "FIREBASE_API_KEY"; // Replace with Firebase API key

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // India Standard Time (GMT+5:30)
const int daylightOffset_sec = 0;

unsigned long previousMillis = 0;
const unsigned long interval = 1000;

// Variables to store received data
int irValue = -1;
String diabetesHistory = "Unknown"; // Stores "Yes" or "No"

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(receiveData);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to Wi-Fi...");
  }
  Serial.println("Connected to Wi-Fi");

  // Initialize time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time synchronized");

  // Fetch diabetes history from Firebase
  fetchDiabetesHistory();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval && irValue != -1) {
    previousMillis = currentMillis;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    String dateTime = getFormattedDateTime(timeinfo);

    // Determine glucose level calculation method
    int glucoseLevel;
    if (diabetesHistory == "No") {
      glucoseLevel = int((2.0177 * irValue) - 2452.3);
    } else if (diabetesHistory == "Yes") {
      glucoseLevel = lookupGlucose(irValue);
    } else {
      Serial.println("Error: diabetesHistory unknown, using fallback equation.");
      glucoseLevel = int((2.0177 * irValue) - 2452.3);
      glucoseLevel = int(0.9*glucoseLevel);
    }

    Serial.printf("ADC Value: %d | Glucose Level: %d mg/dL\n", irValue, glucoseLevel);

    // Send data to Firebase
    if (sendDataToFirebase(dateTime, irValue, glucoseLevel)) {
      Serial.println("Data sent to Firebase");
      irValue = -1; // Reset value after sending
    } else {
      Serial.println("Failed to send data to Firebase");
    }
  }
}

String getFormattedDateTime(struct tm timeinfo) {
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d | %H:%M:%S", &timeinfo);
  return String(buffer);
}

void receiveData(int byteCount) {
  if (byteCount >= 2) {
    byte highByte = Wire.read();
    byte lowByte = Wire.read();
    irValue = ((highByte << 8) | lowByte) & 0x0FFF;
    Serial.printf("Processed I2C Value: %d\n", irValue);
  } else {
    Serial.println("Not enough data received");
  }
}

// Fetches diabetesHistory from Firebase
void fetchDiabetesHistory() {
  HTTPClient http;
  String url = String(firebaseHost) + "/profile/diabetesHistory.json?auth=" + firebaseApiKey;

  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.print("Fetched diabetesHistory: ");
    Serial.println(response);

    response.trim();
    if (response == "\"Yes\"") {
      diabetesHistory = "Yes";
    } else if (response == "\"No\"") {
      diabetesHistory = "No";
    } else {
      diabetesHistory = "Unknown";
    }
  } else {
    Serial.print("Error fetching diabetesHistory: ");
    Serial.println(http.errorToString(httpResponseCode));
    diabetesHistory = "Unknown";
  }

  http.end();
}

// Lookup table for glucose levels
int lookupGlucose(int adcValue) {
  switch (adcValue) {
    case 1260: return 166;
    case 1261: return 192;
    case 1262: return 219;
    case 1263: return 246;
    case 1264: return 272;
    case 1265: return 299;
    case 1266: return 326;
    case 1267: return 157;
    case 1268: return 150;
    case 1269: return 152;
    case 1270: return 155;
    case 1271: return 157;
    case 1272: return 160;
    case 1273: return 164;
    case 1274: return 168;
    case 1275: return 197;
    case 1276: return 226;
    case 1277: return 256;
    case 1278: return 286;
    case 1279: return 316;
    case 1280: return 346;
    case 1281: return 343;
    case 1282: return 340;
    case 1283: return 337;
    case 1284: return 334;
    case 1285: return 331;
    case 1286: return 328;
    case 1287: return 325;
    case 1288: return 322;
    case 1289: return 319;
    case 1290: return 316;
    case 1291: return 313;
    case 1292: return 310;
    default: return -1; // Return -1 if ADC value is not found
  }
}

// Sends glucose data to Firebase
bool sendDataToFirebase(String dateTime, int irValue, int glucoseLevel) {
  HTTPClient http;
  String url = String(firebaseHost) + "/glucose_readings.json?auth=" + firebaseApiKey;

  String jsonPayload = "{";
  jsonPayload += "\"" + dateTime + "\": {";
  jsonPayload += "\"ADC_Value\": " + String(irValue) + ", ";
  jsonPayload += "\"Glucose_Level\": " + String(glucoseLevel);
  jsonPayload += "}}";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PATCH(jsonPayload);

  Serial.printf("HTTP Response Code: %d\n", httpResponseCode);
  if (httpResponseCode > 0) {
    Serial.println("Server Response: " + http.getString());
  } else {
    Serial.printf("Error sending data: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  return (httpResponseCode > 0);
}
