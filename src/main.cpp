#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <ArduinoJson.h>   // Installer la bibliothèque ArduinoJson
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

/********************************************************************
 * Configuration de l'écran OLED
 ********************************************************************/
#define SCREEN_WIDTH 128 // Largeur de l'écran OLED en pixels
#define SCREEN_HEIGHT 32 // Hauteur de l'écran OLED en pixels
#define OLED_RESET -1    // Pin de réinitialisation de l'écran OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/********************************************************************
 * Configuration NTP (pour générer le SKU basé sur l'heure)
 ********************************************************************/
const char* NTP_SERVER = "pool.ntp.org"; // Serveur NTP
const long GMT_OFFSET_SEC = -4 * 3600;  // Décalage GMT en secondes
const int DAYLIGHT_OFFSET_SEC = 0;      // Décalage pour l'heure d'été

/********************************************************************
 * Variables globales
 ********************************************************************/
String skuFull; // Contient le SKU complet (date et heure formatée)

// Pins pour le lecteur RFID et les boutons
#define LED1_PIN 2 // Power LED
#define LED2_PIN 4 // RFID ready LED
#define LED3_PIN 15 // Operation LED

#define BUZZER 12

#define RST_PIN  0
#define SS_PIN   5
#define BUTTON_PIN2 34 // Bouton pour confirmer l'action

// Définition des pins
//#define POT_PIN 32       // Pin analogique pour le potentiomètre

// Pages mémoire pour le RFID
#define START_PAGE 4
#define END_PAGE 15 // Pages classiques pour Ultralight

// Objet pour le lecteur RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);
byte dataBlock[3][4] = {
    {0x01, 0x03, 0xA0, 0x0C}, // page 4
    {0x34, 0x03, 0x16, 0xD1},  // page 5
    {0x01, 0x12, 0x54, 0x02}  // page 5
};

// États des LEDs et des boutons
bool ledState = false; // false = LED1 ON, LED2 OFF; true = LED1 OFF, LED2 ON
bool buttonPressed = false; // Indique si un bouton a été pressé
int currentState = 0; // 0 = Trie, 1 = Stockage, 2 = Vente

int lastPotValue = -1; // Valeur précédente du potentiomètre

/********************************************************************
 * Déclarations des fonctions
 ********************************************************************/
void writeStringToUltralight(const char* text, int page);
String readStringFromUltralight(byte startPage, byte length);
void updateSKU(); // Met à jour le SKU avec la date/heure actuelle
void OLEDiplay(const String& msg, int16_t size); // Affiche un message sur l'écran OLED
void updateState(int state);
void dumpTagInfo();
void clearUltralightTag(); // Prototype for clearUltralightTag
String extractTagSku(const String& json);

/********************************************************************
 * Fonction setup()
 * Initialisation des composants et des configurations
 ********************************************************************/
void setup() {
    

    Serial.begin(921600); // Initialisation de la communication série
    SPI.begin();          // Initialisation du bus SPI
    mfrc522.PCD_Init();   // Initialisation du lecteur RFID
    delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
    mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details


    pinMode(LED1_PIN, OUTPUT); // Configuration des LEDs
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    digitalWrite(LED1_PIN, HIGH);
    digitalWrite(BUZZER, HIGH);
    // Configuration des boutons
    pinMode(BUTTON_PIN2, INPUT_PULLUP); 

    // Initialisation de l'écran OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC)) {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0); 
        display.setTextSize(1);
        display.println(F("Echec de l'allocation OLED"));
        display.display();
        for (;;); // Boucle infinie en cas d'erreur
    }
    else {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0); 
        display.setTextSize(1);
        display.println(F("OLED alloue !"));
        display.display();
        delay(1000);
       // Serial.println(F("OLED alloue !"));
    }
}

/********************************************************************
 * Fonction loop()
 * Boucle principale pour gérer les modes lecture/écriture
 ********************************************************************/

void loop() {
    display.clearDisplay();
    display.setCursor(0, 0); 
    OLEDiplay("Lecture actif", 1);
    delay(300); // Anti-rebond
    // Attente d'une nouvelle carte RFID
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
            digitalWrite(LED3_PIN, LOW);
    }
    digitalWrite(LED2_PIN, HIGH);
    digitalWrite(BUZZER,HIGH);
    delay(1000);
    digitalWrite(BUZZER,LOW);
    String storedName = readStringFromUltralight(7,35);
    Serial.println(storedName);
    if (storedName.length() == 0 ) {
        //  Serial.println(F("No SKU stored!"));
        display.clearDisplay();
        OLEDiplay(F("Aucun SKU !"), 1);
    }
    else {
        display.clearDisplay();
        display.setCursor(0, 0); 
        OLEDiplay(F("SKU:"), 1);
        OLEDiplay(storedName, 1);
        } 
     if (digitalRead(BUTTON_PIN2) == LOW)
     {
        // stay in this until BUTTON_PIN2 goes HIGH
        while (digitalRead(BUTTON_PIN2) == LOW) {}
     }
}

/********************************************************************
 * Fonctions utilitaires
 ********************************************************************/

// Écriture d'une chaîne de caractères dans la mémoire RFID
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
          //  Serial.print("Échec d'écriture à la page "); Serial.println(page);
            return;
        }
        page++;
    }
 //   Serial.println("Écriture terminée.");
}

String extractTagSku(const String& json) {
    int idIndex = json.indexOf("\"id\":\"");
    if (idIndex == -1) return "";
    int start = idIndex + 6; // length of '"id":"'
    int end = json.indexOf("\"", start);
    if (end == -1) return "";
    return json.substring(start, end);
}

String readStringFromUltralight(byte startPage, byte length) {
    String result = "";
    uint8_t pages = ((length + 3) / 4) + 1;
    byte rawBuf[18];
    byte size = sizeof(rawBuf);

    for (uint8_t i = 0; i < pages; i++) {
        byte page = startPage + i;
        MFRC522::StatusCode status = mfrc522.MIFARE_Read(page, rawBuf, &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("Read failed at page "));
            Serial.print(page);
            Serial.print(F(": "));
            return "";
        }
        for (uint8_t j = 0; j < 4 && result.length() < length; j++) {
            if (i == 0 && j == 0) j += 2; // Ignore les 2 premiers octets
            byte b = rawBuf[j];
            if (b == 0x00) break;
            result += (char)b;
        }
    }

    // Use string search to extract the id
    String id = extractTagSku(result);
    if (id.length() == 0) {
        Serial.println("Could not extract 'id' from tag data!");
    }
    return id;
}

// Mise à jour du SKU avec la date/heure actuelle
void updateSKU() { 
    //skuFull = "20250922_090638";
    skuFull = "20250923_105710";
}

// Affichage d'un message sur l'écran OLED
void OLEDiplay(const String& msg, int16_t size) {
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(size);
    display.println(msg);
    display.display();
}

// Mettre à jour les LEDs et l'écran OLED en fonction de l'état
void updateState(int state) {
    String message;
    display.clearDisplay();
    display.setCursor(0, 12);
    if(currentState == lastPotValue) {
        return; // Pas de changement d'état
    }
    else {
        currentState = lastPotValue;
        switch (state) {
            case 0: // Trie
                message="Sorting";
                break;
            case 1: // Stockage
                message="Storage";
                break;
            case 2: // Vente
                message="Sale";
                break;
            case 3: // Lecture
                message="Erase";
                break;
            case 4: // Autre état
                message="Reset Wifi";
                break;
        }
        OLEDiplay(message,2);
    }
}

// Nouvelle fonction pour dumper les infos du tag
void dumpTagInfo() {
    // Vérifie si une nouvelle carte est présente
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }
    // Sélectionne la carte
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }
    // Affiche les infos de la carte
    mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
    mfrc522.PICC_HaltA();
}



/*void clearUltralightTag() {
    byte empty[4] = {0, 0, 0, 0};
    uint16_t idx=0;
    for (byte page = 4; page <= 6; page++) {
        MFRC522::StatusCode status = mfrc522.MIFARE_Ultralight_Write(page, dataBlock[idx], 4);
        idx++;
        if (status != MFRC522::STATUS_OK) {
            Serial.print("Failed to set headers");
            Serial.println(page);
        }
    }
    for (byte page = 7; page <= 15; page++) {
        MFRC522::StatusCode status = mfrc522.MIFARE_Ultralight_Write(page, empty, 4);
        if (status != MFRC522::STATUS_OK) {
            Serial.print("Failed to clear page ");
           // Serial.println(page);
        }
    }
  //  Serial.println("Tag cleared!");
}*/
