#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <utility/wifi_drv.h>
#include <LiquidCrystal.h>

#include "arduino_secrets.h"  // File containing secret WiFi credentials and other secrets

#if DEV
  #define CLIENT_CLASS WiFiClient  // Use WiFiClient in development mode
#else
  #define CLIENT_CLASS WiFiSSLClient  // Use WiFiSSLClient in production mode
#endif

CLIENT_CLASS *client = new CLIENT_CLASS();
int status = WL_IDLE_STATUS;

// Define pin mappings for various components
#define PN532_SCK 2
#define PN532_MOSI 3
#define PN532_SS 4
#define PN532_MISO 5
#define BUZZER A1
#define RED 25
#define GREEN 26
#define BLUE 27
#define LCD_RS 7
#define LCD_EN 8
#define LCD_D4 9
#define LCD_D5 10
#define LCD_D6 11
#define LCD_D7 12
#define BUTTON 0
#define NBR_MODE 10

int maxMode;
String modes[NBR_MODE] = {};
String responseFromServer;
volatile int currentMode = 0;

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

String real_modes[] = {"event", "freeze", "scan"};

void setup() {
  Serial.begin(9600);

  // Setup pin configurations
  setupPin();

  // Initialize NFC reader
  setupNfcReader();

  // Connect to WiFi network
  setupAndConnectWifi();

  // Connect to web application
  connectToWebApp();

  // Initialize modes for the current event
  initModes();

  // Attach interrupt to the button for changing mode
  attachInterrupt(digitalPinToInterrupt(BUTTON), changeMode, FALLING);
}

void loop() {
  // Read UID from NFC card
  int len = 0;
  char id[20];
  String uid;
  clientIsConnected(false);

  uid = readCardUID(currentMode);

  // If there is an error reading the card, do nothing
  if (strcmp("ERROR", uid.c_str()) == 0) {
  } else {
    // Send HTTP request with UID and current mode
    createAndSendHttpRequestUser(uid, String(currentMode));

    // Check if response from web app is valid
    if (isResponseFromWebAppOK()) {
      // Process data received from web app
      getDataFromWebAppUser();
    } else {
      // Handle bad response from server
      Serial.println("Response from server:");
      while (client->available()) {
        char c = client->read();
        Serial.print(c);
      }
      lcd.clear();
      lcd.print("Bad response");
      lcd.setCursor(0, 1);
      lcd.print("from server...");
      delay(1500);
      client->stop();
    }
  }

  // Reconnect to WiFi if disconnected
  isConnectedToWifi();

  // Check and reconnect to server if disconnected
  clientIsConnected(true);
}

/*
 * Send a basic HTTP request with no body to get the initial modes.
 */
void createAndSendHttpRequestInit(void) {
  responseFromServer = "";
  client->print(
    String("POST ") + INIT_URL + " HTTP/1.1\r\n" + "Host: " + HOST + "\r\n" + "Content-Type: application/json\r\n" + "X-Secret: " + TOKEN_POST + "\r\n" + "\r\n");
}

/*
 * Initialize the global modes and the number of modes
 * with the response received from the server. Retry until an event is found.
 */
void initModes(void) {
  // Used for timing after retrying init mode
  int i = 20;
  if (!client->connected()) return;

  // Send initialization request to server
  createAndSendHttpRequestInit();

  // Check if response is valid
  if (isResponseFromWebAppOK()) {
    // Print server response in development mode
    Serial.println("Response from server for INIT:");
    while (client->available()) {
      char c = client->read();
      responseFromServer += c;
      Serial.print(c);
    }
    getInitDataFromWebApp();
    lcd.clear();
    lcd.print("Init. mode");
    lcd.setCursor(0, 1);
    lcd.print("OK!");
  } else {
    // Handle no event found scenario
    Serial.println("Response from server for INIT in else:");
    while (client->available()) {
      char c = client->read();
      Serial.print(c);
    }
    lcd.clear();
    lcd.print("No Event...");
    delay(2000);

    // Retry initialization
    while (i-- > 0) {
      lcd.clear();
      lcd.print("Retry init. in ");
      lcd.setCursor(0, 1);
      lcd.print((String)i + " sec");
      delay(1000);
    }

    // Stop client to get proper response
    client->stop();
    connectToWebApp();
    initModes();  // Retry initialization
  }
}

/*
 * Get the response from the server to initialize mode.
 * Deserialize JSON response and update global variables.
 */
void getInitDataFromWebApp(void) {
  int capacity = 512;
  JsonArray modesFromApp;
  const char *tmp;
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, responseFromServer);

  if (error) {
    Serial.println("Error ma boiiiiii");
    Serial.println(error.c_str());
  } else {
    currentMode = doc["mode"];
  }
}

/*
 * Change the current mode by handling interrupt.
 */
void changeMode() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  // Handle mode change with debouncing
  if (interrupt_time - last_interrupt_time > 310) {
    if (currentMode == maxMode - 1) {
      currentMode = 0;
    } else {
      currentMode++;
    }
    lcd.clear();
    lcd.print("Scan a badge...");
    lcd.setCursor(0, 1);
    lcd.print("mode = " + real_modes[currentMode - 1]);
  }
  last_interrupt_time = interrupt_time;
}

/*
 * Check if WiFi connection is still active.
 * Reconnect if not connected, with a retry interval of 5 seconds.
 */
void isConnectedToWifi(void) {
  char ssid[] = SECRET_SSID;
  char pass[] = SECRET_PASS;

  if (WiFi.status() == WL_CONNECTED) return;

  // Display WiFi connection lost message
  detachInterrupt(BUTTON);
  lcd.clear();
  lcd.print("Wifi connect.");
  lcd.setCursor(0, 1);
  lcd.print("lost...");
  delay(1000);
  lcd.clear();
  lcd.print("Reconnect. to");
  lcd.setCursor(0, 1);
  lcd.print("wifi");

  // Attempt to reconnect to WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }

  // Display WiFi reconnection success message
  lcd.clear();
  lcd.print("Wifi connect.");
  lcd.setCursor(0, 1);
  lcd.print("OK!");
  delay(1500);
  lcd.clear();
  attachInterrupt(digitalPinToInterrupt(BUTTON), changeMode, FALLING);
}

/*
 * Check if the client is connected to the server.
 * Reconnect if necessary, with a retry interval of 5 seconds.
 * Disable button interrupt during reconnect.
 *
 * @param bool reconnection : true if it's a reconnection, false otherwise.
 */
void clientIsConnected(bool reconnection) {
  // Disable button interrupt during reconnect
  detachInterrupt(BUTTON);

  // Handle server connection lost scenario
  if (reconnection && !client->connected()) {
    lcd.clear();
    lcd.print("Server connect.");
    lcd.setCursor(0, 1);
    lcd.print("lost...");
    delay(1000);
    client->stop();
  }

  // Attempt to reconnect to server
  while (!client->connected()) {
    connectToWebApp();
    if (!client->connected()) {
      lcd.print("Retry connect.");
      lcd.setCursor(0, 1);
      lcd.print("to server...");
      delay(5000);
    }
  }

  // Re-enable button interrupt after successful connection
  attachInterrupt(digitalPinToInterrupt(BUTTON), changeMode, FALLING);
}

/*
 * Setup pin configurations for LCD, LEDs, and buttons.
 */
void setupPin(void) {
  // Initialize LCD
  lcd.begin(16, 2);

  // Initialize pins for other components
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  // Initialize built-in LED pins
  WiFiDrv::pinMode(GREEN, OUTPUT);
  WiFiDrv::pinMode(RED, OUTPUT);
  WiFiDrv::pinMode(BLUE, OUTPUT);
}

/*
 * Retrieve JSON data from server and display scrolling message on LCD.
 * Also control LEDs and buzzer based on server response.
 */
void getDataFromWebAppUser(void) {
  int capacity = 256;
  uint8_t red, green, blue;
  const char *msg;
  JsonArray led;
  DynamicJsonDocument doc(capacity);

  // Deserialize JSON response from server
  DeserializationError error = deserializeJson(doc, *client);

  if (error) {
    // Handle JSON deserialization error
  } else {
    // Extract LED colors and buzzer status from JSON
    led = doc["led"];
    red = led[0];
    green = led[1];
    blue = led[2];
    turnOnLed(red, green, blue);

    bool buzzer = doc["buzzer"];
    if (buzzer) {
      playSuccessBuzzer();
    } else {
      playFailureBuzzer();
    }

    // Display message on LCD with scrolling effect
    delay(200);
    turnOnLed(red, green, blue);
    delay(300);
    turnOnLed(0, 0, 255);
    msg = doc["msg"];
    lcd.clear();
    scrollingMessage(msg);
    delay(500);
  }
}

/*
 * Utility function to display a message with scrolling view on LCD.
 */
void scrollingMessage(const char *msg) {
  uint8_t messageLength;
  uint8_t lcdLength = 15;
  uint8_t totalScroll;

  // Print message on LCD
  lcd.print(msg);
  delay(300);

  // Calculate total scrollable length
  messageLength = strlen(msg);
  totalScroll = messageLength - lcdLength;
  if (totalScroll <= 0) {
    totalScroll = 0;
  }

  // Perform scrolling animation on LCD
  detachInterrupt(BUTTON);
  for (int i = totalScroll; i >= 0; i--) {
    lcd.scrollDisplayLeft();
    delay(250);
  }
  attachInterrupt(digitalPinToInterrupt(BUTTON), changeMode, FALLING);
}

/*
 * Check if the response from the webApp is valid to continue processing.
 * @return bool : true if the response is correct, false otherwise.
 */
bool isResponseFromWebAppOK() {
  char status[64] = { 0 };
  client->readBytesUntil('\r', status, sizeof(status));

  // Check if HTTP status is "201 Created"
  if (strcmp(status + 9, "201 Created") != 0) {
    return false;
  }

  // Check end of headers in response
  char endOfHeaders[] = "\r\n\r\n";
  if (!client->find(endOfHeaders)) {
    return false;
  }

  return true;
}

/*
 * Send HTTP POST request to the webApp with JSON data.
 * @param String uid : UID of the scanned card.
 * @param String mode : Current mode for event.
 */
void createAndSendHttpRequestUser(String uid, String mode) {
  String postData = "{\"id\":\"" + uid + "\",\"mode\":\"" + mode + "\"}";
  client->print(
    String("POST ") + SCAN_URL + " HTTP/1.1\r\n" + "Host: " + HOST + "\r\n" + "Content-Type: application/json\r\n" + "Content-Length: " + postData.length() + "\r\n" + "X-Secret: " + TOKEN_POST + "\r\n" + "\r\n" + postData);
}

/*
 * Attempt to connect to the server.
 */
void connectToWebApp() {
  lcd.clear();
  lcd.print("Try connect.");
  lcd.setCursor(0, 1);
  lcd.print("to server...");

  bool connected;
  if (DEV) {
    connected = client->connect(IPADDRESS_SERVER, PORT);
  } else {
    connected = client->connectSSL(IPADDRESS_SERVER, PORT);
  }

  // Handle connection result
  if (connected) {
    lcd.clear();
    lcd.print("Connect. server");
    lcd.setCursor(0, 1);
    lcd.print("OK!");
    turnOnLed(0, 0, 255);
  } else {
    lcd.clear();
    lcd.print("Connect. server");
    lcd.setCursor(0, 1);
    lcd.print("KO!");
  }
  delay(1500);
  lcd.clear();
}

/*
 * Setup and connect to WiFi network using credentials defined in arduino_secrets.h.
 * Retry every 5 seconds until connection is established.
 */
void setupAndConnectWifi(void) {
  char ssid[] = SECRET_SSID;
  char pass[] = SECRET_PASS;
  String fv = WiFi.firmwareVersion();

  // Display firmware upgrade message if necessary
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    lcd.clear();
    lcd.print("Please upgrade");
    lcd.setCursor(0, 1);
    lcd.print("the firmware");
  }

  lcd.clear();
  lcd.print("Try connect.");
  lcd.setCursor(0, 1);
  lcd.print("to wifi...");

  // Attempt to connect to WiFi network
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(100);
  }

  // Display WiFi connection success message
  lcd.clear();
  lcd.print("Wifi connect.");
  lcd.setCursor(0, 1);
  lcd.print("OK!");
  delay(1500);
  lcd.clear();
}

/*
 * Setup NFC Reader. Force restart Arduino if reader is not connected after 10 tries.
 */
void setupNfcReader(void) {
  nfc.begin();
  uint32_t versiondata = 0;
  int tries = 0;

  // Attempt to get NFC firmware version
  while (!versiondata && tries++ < 10) {
    delay(500);
    versiondata = nfc.getFirmwareVersion();
  }

  // Display NFC reader status
  while (!versiondata) {
    lcd.print("NFC Reader");
    lcd.setCursor(0, 1);
    lcd.print("KO!");
    while (1)
      ;
  }

  // Set max retries for reading from NFC card
  nfc.setPassiveActivationRetries(0xFF);

  // Configure NFC reader for RFID tags
  nfc.SAMConfig();
  lcd.print("NFC reader");
  lcd.setCursor(0, 1);
  lcd.print("OK!");
  delay(1500);
  lcd.clear();
}

/*
 * Turn on built-in LED with RGB color code.
 * @param uint8_t red : red color intensity.
 * @param uint8_t green : green color intensity.
 * @param uint8_t blue : blue color intensity.
 */
void turnOnLed(uint8_t red, uint8_t green, uint8_t blue) {
  WiFiDrv::analogWrite(RED, red);
  WiFiDrv::analogWrite(GREEN, green);
  WiFiDrv::analogWrite(BLUE, blue);
}

/*
 * Play success sound on piezo buzzer.
 */
void playSuccessBuzzer(void) {
  tone(BUZZER, 987);
  delay(100);
  tone(BUZZER, 1318);
  delay(300);
  noTone(BUZZER);
}

/*
 * Play failure sound on piezo buzzer.
 */
void playFailureBuzzer(void) {
  tone(BUZZER, 100);
  delay(200);
  tone(BUZZER, 100);
  delay(200);
  noTone(BUZZER);
}

/*
 * Read card and return UID as a String.
 * @param int currentMode : current mode index.
 * @return String : UID of the card or "ERROR" if reading fails.
 */
String readCardUID(int currentMode) {
  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store UID
  uint8_t uidLength;  // Length of the UID
  int len = 0;
  char id[20];

  // Display scanning message on LCD
  lcd.clear();
  lcd.print("Scan a badge...");
  lcd.setCursor(0, 1);
  lcd.print("mode = " + real_modes[currentMode - 1]);
  delay(1000);

  // Attempt to read UID from NFC card
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

  // Handle UID read success or failure
  if (success) {
    for (uint8_t i = 0; i < uidLength - 1; i++) {
      len += sprintf(id + len, "%X:", uid[i]);
    }
    sprintf(id + len, "%X", uid[uidLength - 1]);
    return String(id);
  }

  // Blink LED to indicate error
  turnOnLed(0, 0, 0);
  delay(200);
  turnOnLed(0, 0, 255);
  return "ERROR";
}
