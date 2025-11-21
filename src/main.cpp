#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/********************************************************************
 * OLED CONFIGURATION
 ********************************************************************/
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/********************************************************************
 * RFID CONFIGURATION
 ********************************************************************/
// Pages mémoire pour le RFID
#define START_PAGE 4
#define END_PAGE 15 // Pages classiques pour Ultralight

#define RST_PIN  15
#define SS_PIN   5
MFRC522 mfrc522(SS_PIN, RST_PIN);

/********************************************************************
 * LEDS, BUZZER AND BUTTON
 ********************************************************************/
#define LED_POWER 2   // Always ON when ESP32 is running
#define LED_CONN  27  // Solid ON = PC connected, slow blink = disconnected
#define LED_OP    12  // couplé au buzzer
#define BUZZER    14  // coupllé au LED_OP
#define BUTTON_PIN 34 // Must press to restart reading

/********************************************************************
 * SERIAL LINK (PING heartbeat)
 ********************************************************************/
unsigned long lastPingTime = 0;
bool serialConnected = false;
String skuFull; // Contient le SKU complet (date et heure formatée)

/********************************************************************
 * FUNCTIONS
 ********************************************************************/
void OLEDiplay(const String &msg, int size);
String extractTagSku(const String &json);
String readStringFromUltralight(byte startPage, byte length);

/********************************************************************
 * SETUP
 ********************************************************************/
void setup() {
    Serial.begin(921600);

    pinMode(LED_POWER, OUTPUT);
    pinMode(LED_CONN, OUTPUT);
    pinMode(LED_OP, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // LED_POWER always ON
    digitalWrite(LED_POWER, HIGH);
    digitalWrite(BUZZER, HIGH);

    SPI.begin();          // Initialisation du bus SPI
    mfrc522.PCD_Init();   // Initialisation du lecteur RFID
    delay(100);				// Optional delay. Some board do need more time after init to be ready, see Readme
    mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
    
    if (!display.begin(SSD1306_SWITCHCAPVCC)) {
        OLEDiplay("Echec OLED!", 1);
        for (;;) {}
    }else {
        OLEDiplay("OLED alloue !",1);
        delay(800);
    }
}

/********************************************************************
 * LOOP
 ********************************************************************/
void loop() {

    /*************************************************************
     * 1. RFID SCANNING
     *************************************************************/
    digitalWrite(BUZZER, LOW);
    OLEDiplay("Lecture Actif !", 1);
    if (!mfrc522.PICC_IsNewCardPresent() ||
        !mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    // Beep once when tag is detected
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED_OP, HIGH);
    delay(200);
    digitalWrite(LED_OP, LOW);
    digitalWrite(BUZZER, LOW);

    String storedName = readStringFromUltralight(7, 35);
    Serial.println(storedName);

    if (storedName.length() == 0) {
        OLEDiplay("Aucun SKU !", 1);
    } else {
         Serial.println(storedName);
        OLEDiplay("SKU:"+ storedName, 1);
    }

    /*************************************************************
     * 3. WAIT FOR BUTTON PRESS TO CONTINUE
     *************************************************************/

    // Wait for press
    while (digitalRead(BUTTON_PIN) == LOW) {
       // stay in this until BUTTON_PIN2 goes HIGH
        while (digitalRead (BUTTON_PIN) == LOW) {}
    }
    // Debounce + wait for release
    while (digitalRead(BUTTON_PIN) == LOW) {}
    delay(200);
}

/********************************************************************
 * FUNCTIONS 
 ********************************************************************/
void OLEDiplay(const String& msg, int size) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(size);

    int16_t w = display.width();
    int16_t h = display.height();
    int16_t tw = msg.length() * 5 * size;
    int16_t th =  size/2;

    int16_t x = (w - tw) / 2;
    int16_t y = (h - th) / 2;

    display.setCursor(x, y);
    display.println(msg);
    display.display();
}

String extractTagSku(const String& json) {
    int idIndex = json.indexOf("\"id\":\"");
    if (idIndex == -1) return "";
    int start = idIndex + 6; // length of '"id":"'
    int end = json.indexOf("\"", start);
    if (end == -1) return "";
    String sku = json.substring(start, end);
    // Remove any trailing null bytes or whitespace
    sku.trim();
    return sku;
}

String readStringFromUltralight(byte startPage, byte length) {
    String result = "";
    byte rawBuf[18];
    byte size = sizeof(rawBuf);
    byte pages = (length + 3) / 4;

    for (byte i = 0; i < pages; i++) {
        byte page = startPage + i;

        if (mfrc522.MIFARE_Read(page, rawBuf, &size) != MFRC522::STATUS_OK)
            return "";

        for (int j = 0; j < 4 && result.length() < length; j++) {
            byte b = rawBuf[j];
            if (b == 0x00) break;
            result += (char)b;
        }
    }

    return extractTagSku(result);
}
