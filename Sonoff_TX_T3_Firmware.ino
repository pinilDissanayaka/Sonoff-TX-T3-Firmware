#include <WiFi.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <Firebase_ESP_Client.h>  // https://github.com/mobizt/Firebase-ESP-Client
#include <ESPAsyncWebServer.h>

// -------------------- GPIO CONFIGURATION --------------------
#define RELAY1 12
#define RELAY2 5
#define RELAY3 4
#define BUTTON1 0
#define BUTTON2 9
#define BUTTON3 10
#define LED_WIFI 13

// -------------------- FIREBASE CONFIGURATION --------------------
#define API_KEY "#"
#define DATABASE_URL "#"  // Realtime DB URL

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;
bool firebaseOnline = false;

unsigned long lastFirebaseSync = 0;
unsigned long button1PressTime = 0;

// -------------------- RELAY STATES --------------------
bool relay1State = false;
bool relay2State = false;
bool relay3State = false;

// -------------------- FUNCTION DECLARATIONS --------------------
void blinkWiFiLED();
void connectWiFi();
void startFirebase();

// -------------------- INTERRUPT HANDLERS --------------------
void IRAM_ATTR handleButton2() {
  relay2State = !relay2State;
  digitalWrite(RELAY2, relay2State ? HIGH : LOW);
  if (firebaseOnline)
    Firebase.RTDB.setBool(&fbdo, "/relays/relay2", relay2State);
}

void IRAM_ATTR handleButton3() {
  relay3State = !relay3State;
  digitalWrite(RELAY3, relay3State ? HIGH : LOW);
  if (firebaseOnline)
    Firebase.RTDB.setBool(&fbdo, "/relays/relay3", relay3State);
}

// -------------------- BLINK WIFI LED IF DISCONNECTED --------------------
void blinkWiFiLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink >= 500) {
    ledState = !ledState;
    digitalWrite(LED_WIFI, ledState);
    lastBlink = millis();
  }
}

// -------------------- CONNECT WIFI WITH WIFIMANAGER --------------------
void connectWiFi() {
  WiFiManager wm;
  bool res = wm.autoConnect("");  // Opens AP if no saved Wi-Fi
  if (!res) {
    Serial.println("⚠️ Failed to connect. Rebooting...");
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("✅ Wi-Fi Connected!");
    digitalWrite(LED_WIFI, HIGH);
  }
}

// -------------------- FIREBASE INITIALIZATION --------------------
void startFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
    Serial.println("✅ Firebase signUp successful!");
  } else {
    Serial.printf("❌ Firebase signUp failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);

  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);

  attachInterrupt(BUTTON2, handleButton2, FALLING);
  attachInterrupt(BUTTON3, handleButton3, FALLING);

  connectWiFi();
  startFirebase();
}

// -------------------- LOOP --------------------
void loop() {
  // Blink Wi-Fi LED if not connected
  if (WiFi.status() != WL_CONNECTED) {
    blinkWiFiLED();
    firebaseOnline = false;
  } else {
    digitalWrite(LED_WIFI, HIGH);
    firebaseOnline = true;
  }

  // -------- Button 1 Logic (Relay toggle or Wi-Fi Reset) --------
  if (digitalRead(BUTTON1) == LOW) {
    if (button1PressTime == 0)
      button1PressTime = millis();

    // Blink LED fast while holding to show reset pending
    if (millis() - button1PressTime > 4000) {
      digitalWrite(LED_WIFI, LOW);
      delay(100);
      digitalWrite(LED_WIFI, HIGH);
      delay(100);
    }
  } else {
    if (button1PressTime > 0) {
      unsigned long pressDuration = millis() - button1PressTime;
      button1PressTime = 0;

      // ---- Force Wi-Fi Reconfiguration ----
      if (pressDuration >= 5000) {
        Serial.println("⚙️ Forcing Wi-Fi reconfiguration...");
        digitalWrite(LED_WIFI, LOW);
        WiFi.disconnect(true, true);  // Erase credentials
        delay(1000);
        ESP.restart();                // Restart to open config AP
      }
      // ---- Normal Short Press (Toggle Relay 1) ----
      else {
        relay1State = !relay1State;
        digitalWrite(RELAY1, relay1State ? HIGH : LOW);
        if (firebaseOnline)
          Firebase.RTDB.setBool(&fbdo, "/relays/relay1", relay1State);
      }
    }
  }

  // -------- Firebase Sync (online only) --------
  if (firebaseOnline && Firebase.ready() && signupOK && millis() - lastFirebaseSync > 1000) {
    lastFirebaseSync = millis();

    if (Firebase.RTDB.getBool(&fbdo, "/relays/relay1"))
      digitalWrite(RELAY1, fbdo.boolData() ? HIGH : LOW);

    if (Firebase.RTDB.getBool(&fbdo, "/relays/relay2"))
      digitalWrite(RELAY2, fbdo.boolData() ? HIGH : LOW);

    if (Firebase.RTDB.getBool(&fbdo, "/relays/relay3"))
      digitalWrite(RELAY3, fbdo.boolData() ? HIGH : LOW);
  }
}
