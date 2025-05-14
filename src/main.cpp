#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <WiFi.h>
#include <ArduinoJson.h>   // install ArduinoJson library

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels


#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,&Wire, OLED_RESET);
// NTP / fuseau
const char*  NTP_SERVER          = "pool.ntp.org";
const long   GMT_OFFSET_SEC      = -4 * 3600;
const int    DAYLIGHT_OFFSET_SEC = 0;

// Variables globales pour le SKU
String skuFull;

/*const char* ssid    = "SM-G781W6479";
const char* password = "kmwq3644";*/
const char* ssid = "FIZZ04289"; 
const char* password = "KHEOPS1001"; 
/*const char* ssid = "Helix7291";   // Change this
const char* password = "chezfrancois1";  // Change this*/
const char* serverUrl = "https://script.google.com/macros/s/AKfycby-713toRe1KlkaOKbxXv-gDu_1bg96uprV3CGGm5sRt7uFnmNLCc4ed_cEFekJiIJv/exec";  // Change IP

#define RST_PIN  0
#define SS_PIN   5
#define BUTTON_PIN 4 // Define the button pin
#define BUTTON_PIN2 34 // Define the button pin
#define LED_READ 2   // LED for read mode
#define LED_WRITE 15 // LED for write mode

#define START_PAGE 4
#define END_PAGE  15   // classic Ultralight

MFRC522 mfrc522(SS_PIN, RST_PIN);

bool ledState = false; // false = LED1 ON, LED2 OFF; true = LED1 OFF, LED2 ON
bool buttonPressed = false; // Flag to indicate button press

void writeStringToUltralight(const char* text, int page);
String readStringFromUltralight(byte startPage, byte length);
void sendToServer(String uid, String name,String location);
//void readFromServer(const String& uid); // Function prototype declaration
void updateSKU(); // Function prototype declaration
void writeFixedStrings(); // Function prototype declaration
void initWiFi();  // Function prototype declaration
void initTime();  // Function prototype declaration
void OLEDiplay(const String& msg,int16_t size);  // Function prototype declaration



void setup() {
    Serial.begin(921600);
    SPI.begin();    
    mfrc522.PCD_Init();    

    pinMode(BUTTON_PIN, INPUT_PULLUP); // Set button pin as input with pull-up resistor
    pinMode(BUTTON_PIN2, INPUT_PULLUP); // Set button pin as input with pull-up resistor
    pinMode(LED_READ, OUTPUT);         // Set LED for read mode
    pinMode(LED_WRITE, OUTPUT);        // Set LED for write mode


    digitalWrite(LED_READ, HIGH);     // Turn off LED for read mode
    digitalWrite(LED_WRITE, LOW);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC)) {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);        // Draw white text
        display.setCursor(0,0); 
        display.setTextSize(1);             // Draw 2X-scale text
        display.println(F("SSD1306 allocation failed"));
        display.display();
        for(;;); // Don't proceed, loop forever
        }
        initWiFi();
        initTime();
}

void loop() {
   
    if (digitalRead(BUTTON_PIN) == LOW) {
        delay(300); // Debounce delay
        ledState = !ledState; // Toggle LED state
                // Update LEDs based on the state
        if (ledState) {
            digitalWrite(LED_READ, LOW);  // Turn LED1 OFF
            digitalWrite(LED_WRITE, HIGH); // Turn LED2 ON
            display.clearDisplay();
            display.setCursor(0,0);
            OLEDiplay(F("Writing mode:"),1) ;
        } else {
            digitalWrite(LED_READ, HIGH); // Turn LED1 ON
            digitalWrite(LED_WRITE, LOW);  // Turn LED2 OFF
            display.clearDisplay();
            display.setCursor(0,0);
            OLEDiplay(F("Reading mode:"),1) ;
        }
    } 

    if (digitalRead(BUTTON_PIN2) == LOW) {
        delay(300); // Debounce delay
     // Wait for a new card
        if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
            return;
        }

        Serial.print(F("Card UID: "));
        String cardUID = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
            Serial.print(mfrc522.uid.uidByte[i], HEX);
            cardUID += String(mfrc522.uid.uidByte[i], HEX);
        }
        Serial.println();
    
        if(ledState) { // Writing mode
            display.clearDisplay();
            display.setCursor(0,0); 
            OLEDiplay("Writing new SKU...",1) ;
            updateSKU();          // remplit skuFull
            skuFull.trim(); // Remove leading and trailing whitespace
            writeStringToUltralight(skuFull.c_str(),4);
            //writeFixedStrings();
            OLEDiplay("DONE",3) ;
            delay(2000);
        }
        else if (!ledState) { // Reading mode
          //  Serial.println(F("Press the button to read the stored name..."));
            Serial.println(F("\nReading stored name..."));
            OLEDiplay("Reading stored name..",1) ;
            String storedName = readStringFromUltralight(4, 22);
            if (storedName.length() == 0) {
                Serial.println(F("No name stored!"));
                display.clearDisplay();
                OLEDiplay(F("No name stored!"),1) ;
                return;
            }

                        // now slice it back into two parts
            String storedNameSKU = storedName.substring(0,15);  // chars [0..14]
            String storedNameLoc = storedName.substring(15);    // chars [15..21]
            // OLED display STORED NAME     
            display.clearDisplay();
            display.setCursor(0,0); 
            OLEDiplay(F("Stored Name:"),1) ;
            OLEDiplay(storedName,1) ;
            Serial.println(storedName);
           // readFromServer(cardUID);
            // Send data to server
            sendToServer(cardUID, storedNameSKU,"1A1A1-2" );
           // display.clearDisplay();
            OLEDiplay(F("Data sent to server!"),1) ;
            Serial.println("Data sent to server!");
            delay(1000); // Wait for 5 seconds before next operation
        }
    }

}

/*void writeFixedStrings() {
    // 1) First string
    const char* s1 = "12345678_064152";
    writeStringToUltralight(s1, START_PAGE);
  
    // 2) Second string
    const char* s2 = "1A1A1-2";
    int secondStart = START_PAGE + 4;    
    writeStringToUltralight(s2, secondStart);
  }*/

void writeStringToUltralight(const char* text, int page) {
    byte buffer[4];
    int len = strlen(text);
    int written = 0;
  
    while (written < len && page <= END_PAGE) {
      memset(buffer, 0, 4);
      for (int j = 0; j < 4 && written < len; j++) {
        buffer[j] = text[written++];
      }
      auto status = mfrc522.MIFARE_Ultralight_Write(page, buffer, 4);
      if (status != MFRC522::STATUS_OK) {
        Serial.print("Write failed at page "); Serial.println(page);
        return;
      }
      page++;
    }
    Serial.println("Write done at pages.");
  }

/*void writeStringToUltralight(const char *text) {
    byte buffer[4];
    byte page = 4;  // Start writing from page 4
    int textLength = strlen(text);

    for (byte i = 0; i < 20; i += 4, page++) {
        memset(buffer, 0, 4); // Clear buffer
        for (byte j = 0; j < 4; j++) {
            if (i + j < textLength) {
                buffer[j] = text[i + j];
            } else {
                buffer[j] = 0x00; // Add NULL termination
            }
        }
        MFRC522::StatusCode status = mfrc522.MIFARE_Ultralight_Write(page, buffer, 4);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("Write failed at page ")); Serial.print(page);
            Serial.print(F(": ")); Serial.println(mfrc522.GetStatusCodeName(status));
            return;
        }
    }
    Serial.println(F("Write completed!"));
    delay(5000); // Wait for 5 seconds before next operation
}*/

/*String readStringFromUltralight(byte startPage, byte length) {
    String result = "";
    byte buffer[20];
    byte size = sizeof(buffer);

    for (byte page = startPage; page < startPage + (length / 4); page++) {
        MFRC522::StatusCode status = mfrc522.MIFARE_Read(page, buffer, &size);
        if (status == MFRC522::STATUS_OK) {
            for (byte i = 0; i < 4; i++) {
                if (buffer[i] == 0x00) break; // Stop reading 
                result += (char)buffer[i];
            }
        } else {
            Serial.print(F("Read failed at page ")); Serial.print(page);
            Serial.print(F(": ")); Serial.println(mfrc522.GetStatusCodeName(status));
            return "";
        }
    }
    result.trim();
    Serial.println(F("Read completed!"));
    delay(5000);
    return result;
}*/

String readStringFromUltralight(byte startPage, byte length) {
    String result = "";

    // How many 4-byte pages to read (rounded up)
    uint8_t pages = (length + 3) / 4;  

    // RF522 MIFARE_Read wants a buffer up to 18 bytes
    byte rawBuf[18];
    byte size = sizeof(rawBuf);

    for (uint8_t i = 0; i < pages; i++) {
        byte page = startPage + i;
        
        // Read the 16-byte block starting at 'page'
        MFRC522::StatusCode status = mfrc522.MIFARE_Read(page, rawBuf, &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("Read failed at page "));
            Serial.print(page);
            Serial.print(F(": "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            return "";
        }

        // Pull off only the first 4 bytes—Ultralight pages are 4 bytes each
        for (uint8_t j = 0; j < 4 && result.length() < length; j++) {
            byte b = rawBuf[j];
            if (b == 0x00) break;
            result += (char)b;
        }
    }

    return result;
}


/*void readFromServer(const String& uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected!");
    return;
  }

  HTTPClient http;
  String url = String(serverUrl) + "?uid=" + uid;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // allow up to 5 redirects

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("GET failed, code: %d\n", httpCode);
    http.end();
    return;
  }
  
  // Parse JSON response
  String payload = http.getString();
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
  } else if (doc.containsKey("error")) {
    Serial.print("Server error: ");
    Serial.println(doc["error"].as<const char*>());
  } else {
    String name     = doc["name"].as<String>();
    String location = doc["location"].as<String>();
    Serial.printf("UID: %s, Name: %s, Location: %s\n",
                  uid.c_str(), name.c_str(), location.c_str());
  }
  http.end();
}*/

void sendToServer(String uid, String name, String location) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String postData = "uid=" + uid + "&name=" + name + "&location=" + location;
        int httpResponseCode = http.POST(postData);

        if (httpResponseCode > 0) {
            Serial.println("Server Response: " + http.getString());
        } else {
            Serial.println("Error on sending data.");
        }

        http.end();
    } else {
        Serial.println("WiFi Disconnected!");
    }
}
// Initialise la connexion Wi‑Fi
void initWiFi() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,0); 
    display.setTextSize(1);             // Draw 2X-scale text
    display.printf("Wi-Fi:connection to %s …\n", ssid);
    display.display();
    delay(000);
  
    WiFi.begin(ssid, password);
    uint16_t idx=0;
    display.clearDisplay();
    while (WiFi.status() != WL_CONNECTED) {
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(idx,0); 
      display.setTextSize(1);             // Draw 2X-scale text
      display.print('.');
      display.display();
      idx=idx+3;
      delay(250);
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,0); 
    display.setTextSize(1);             // Draw 2X-scale text
    display.println(" Wi-Fi connected !");
    display.display();
    delay(1000); 
  }
  // Initialise et synchronise l’heure via NTP
void initTime() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,0); 
    display.setTextSize(1);             // Draw 2X-scale text
    display.print("NTP: synchronization…");
    display.display();
    struct tm tm;
    while (!getLocalTime(&tm)) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(0,0); 
      display.setTextSize(1);             // Draw 2X-scale text
      display.print('.');
      display.display();
      delay(250);
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,0); 
    display.setTextSize(1);             // Draw 2X-scale text
    display.println(" Ready!");
    display.display();
    delay(1000);
  }
  // Met à jour skuFull, skuDate et skuTime avec la date/heure courantes
void updateSKU() { 
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(0,0); 
      display.setTextSize(1);             // Draw 2X-scale text
      display.print("Erreur time");
      display.display();
      return;
    }
  
    // Formattage YYYYMMDD_HHMMSS
    char buf[20];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &timeinfo);
    skuFull = String(buf);
  
  }

void OLEDiplay(const String& msg,int16_t size) {
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setTextSize(size); // Draw sizeX-scale text
  display.println(msg); // Affiche le message complet
  display.display();
}