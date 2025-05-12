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
/*const char* ssid = "";   // Change this
const char* password = "KHEOPS1001";  // Change this*/
const char* ssid = "Helix7291";   // Change this
const char* password = "chezfrancois1";  // Change this
const char* serverUrl = "https://script.google.com/macros/s/AKfycbyQXUx8hWHuQ3lZQBhwduRYHODfBbuFGnZqV3BCW5Il_-PZithH41iQaYMGTKjgx1ps/exec";  // Change IP

#define RST_PIN  0
#define SS_PIN   5
#define BUTTON_PIN 4 // Define the button pin
#define BUTTON_PIN2 34 // Define the button pin
#define LED_READ 2   // LED for read mode
#define LED_WRITE 15 // LED for write mode

MFRC522 mfrc522(SS_PIN, RST_PIN);

bool ledState = false; // false = LED1 ON, LED2 OFF; true = LED1 OFF, LED2 ON
bool buttonPressed = false; // Flag to indicate button press

void writeStringToUltralight(const char *text);
String readStringFromUltralight(byte startPage, byte length);
void sendToServer(String uid, String name);
void updateSKU(); // Function prototype declaration
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
        updateSKU();          // remplit skuFull
        skuFull.trim(); // Remove leading and trailing whitespace

        //Serial.print(F("\n: ")); Serial.println(skuFull);
        writeStringToUltralight(skuFull.c_str());
        }
        else if (!ledState) { // Reading mode
          //  Serial.println(F("Press the button to read the stored name..."));
            Serial.println(F("\nReading stored name..."));
            OLEDiplay("Reading stored name...",1) ;
            String storedName = readStringFromUltralight(4, 20);
            if (storedName.length() == 0) {
                Serial.println(F("No name stored!"));
                display.clearDisplay();
                OLEDiplay(F("No name stored!"),1) ;
                return;
            }
            // OLED display STORED NAME     
            display.clearDisplay();
            display.setCursor(0,0); 
            OLEDiplay(F("Stored Name:"),1) ;
            OLEDiplay(storedName,1) ;
            Serial.println(storedName);

            // Send data to server
            sendToServer(cardUID, storedName);
           // display.clearDisplay();
            OLEDiplay(F("Data sent to server!"),1) ;
            Serial.println("Data sent to server!");
            delay(1000); // Wait for 5 seconds before next operation
        }
    }

}



void writeStringToUltralight(const char *text) {
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
}

String readStringFromUltralight(byte startPage, byte length) {
    String result = "";
    byte buffer[18];
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
}

void sendToServer(String uid, String name) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String postData = "uid=" + uid + "&name=" + name;
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