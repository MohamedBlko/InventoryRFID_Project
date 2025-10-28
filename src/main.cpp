// AUteur Mohamed marou Belko

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


// URL du serveur pour envoyer les données
const char* serverUrl = "https://script.google.com/macros/s/AKfycbyXPDymDrVKF9IzpyUfbctDl8EdX7xHcnvYmFywMmLFSXdc_2oBvTn38dEBQIFGxILT/exec";

// Pins pour le lecteur RFID et les boutons
#define RST_PIN  0
#define SS_PIN   5
//#define BUTTON_PIN 4 // Bouton pour basculer entre lecture/écriture
#define BUTTON_PIN2 34 // Bouton pour confirmer l'action
//#define LED_READ 2   // LED pour le mode lecture
//#define LED_WRITE 15 // LED pour le mode écriture

// Définition des pins
#define POT_PIN 32       // Pin analogique pour le potentiomètre
//#define LED_TRIE 2       // LED pour l'état "Trie"
//#define LED_STOCKAGE 15  // LED pour l'état "Stockage"
//#define LED_VENTE 4      // LED pour l'état "Vente"

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
void sendToServer(String uid, String name);
void updateSKU(); // Met à jour le SKU avec la date/heure actuelle
void initWiFi();  // Initialise la connexion Wi-Fi
void initTime();  // Synchronise l'heure via NTP
void OLEDiplay(const String& msg, int16_t size); // Affiche un message sur l'écran OLED
void updateState(int state);
void fetchAndDisplayLocation(const String& storedName); //
void dumpTagInfo();
void onPortalStart(WiFiManager *wm); // Prototype for WiFiManager AP callback
void onSaveConfig(); // Prototype for WiFiManager save config callback
void clearUltralightTag(); // Prototype for clearUltralightTag
String getLocation(const String& nameKey); // Prototype for getLocation


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
	Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

    // Configuration des boutons et LEDs
   // pinMode(BUTTON_PIN, INPUT_PULLUP); 
    pinMode(BUTTON_PIN2, INPUT_PULLUP); 
    //pinMode(LED_READ, OUTPUT);         
    //pinMode(LED_WRITE, OUTPUT);        

    // État initial des LEDs
   // digitalWrite(LED_READ, HIGH);  // Éteindre la LED de lecture
    //digitalWrite(LED_WRITE, LOW); // Allumer la LED d'écriture

    // Configuration du mode
    /*pinMode(LED_TRIE, OUTPUT);
    pinMode(LED_STOCKAGE, OUTPUT);
    pinMode(LED_VENTE, OUTPUT);*/

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

    // Initialisation Wi-Fi et synchronisation NTP
    initWiFi();
    initTime();
}

/********************************************************************
 * Fonction loop()
 * Boucle principale pour gérer les modes lecture/écriture
 ********************************************************************/

void loop() {
    // Lire la valeur du potentiomètre
    int potValue = analogRead(POT_PIN)/1023;
    
    // Afficher uniquement si la variation est significative (ici 20)
    if (abs(potValue - lastPotValue) > 0) {
        Serial.print("Valeur potentiometre : ");
        Serial.println(potValue);
        lastPotValue = potValue;
    }

  /*  // Déterminer l'état en fonction de la valeur du potentiomètre
    if (potValue==0) {
        currentState = 0; // Trie
    } else if (potValue < 2730) {
        currentState = 1; // Stockage
    } else {
        currentState = 2; // Vente
    }*/

    // Mettre à jour les LEDs et l'affichage OLED
    updateState(lastPotValue);
    dumpTagInfo();

    // Gestion du bouton pour confirmer l'action
    if (digitalRead(BUTTON_PIN2) == HIGH) {
        delay(300); // Anti-rebond

        if(lastPotValue == 4) { 
            Serial.println(F("\nReseting Wifi..."));
            display.clearDisplay();
            display.setCursor(0, 0); 
            Serial.println(F("Wifi deleted"));
            OLEDiplay(F("Wifi deleted"), 1);
            delay(5000);
              //WiFiManager
            WiFiManager wm;
            wm.resetSettings();
            initWiFi(); // Réinitialisation de la connexion Wi-Fi
        }

        // Attente d'une nouvelle carte RFID
        if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
            return;
        }

        // Lecture de l'UID de la carte
        Serial.print(F("UID de la carte: "));
        String cardUID = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
            Serial.print(mfrc522.uid.uidByte[i], HEX);
            cardUID += String(mfrc522.uid.uidByte[i], HEX);
        }
        Serial.println();

        // Station de trie
        if (lastPotValue == 0) { 
            display.clearDisplay();
            display.setCursor(0, 0); 
            OLEDiplay("Ecriture du SKU...", 1);
            Serial.println(F("SKU generation..."));
            updateSKU(); // Met à jour le SKU
            skuFull.trim(); // Supprime les espaces inutiles
            Serial.println(skuFull.c_str());
            //writeStringToUltralight("en20250517_163642",7);
            writeStringToUltralight(("en"+ skuFull).c_str(),7);
             // Envoi des données au serveur
            sendToServer(cardUID, skuFull);
            //fetchAndDisplayLocation(storedNameSKU);
            OLEDiplay(F("Donnees envoyees !"), 1);
            Serial.println(F("SKU registered!"));
            Serial.println(F("End of processus!"));
            Serial.println ("SKU: "+ skuFull);
            display.clearDisplay();
            display.setCursor(0, 12);
            OLEDiplay("End of processus", 1);
            delay(2000);
        }
        // Station de stockage
        else if(lastPotValue == 1)  { 
            Serial.println(F("\nReading tag..."));
            OLEDiplay("Lecture du nom...", 1);
            String storedName = readStringFromUltralight(7, 15);
            String loc = getLocation(storedName);
            Serial.print(F("SKU read:"));
            Serial.println(storedName);
            Serial.print("Location for ");
            Serial.print(storedName);
            Serial.print(": ");
            Serial.println(loc);
            delay(10000);        
            if (storedName.length() == 0 ) {
                Serial.println(F("No SKU stored!"));
                display.clearDisplay();
                OLEDiplay(F("Aucun nom stocke !"), 1);
                return; 
            }
            else if (loc.length() == 0) {
                Serial.println(F("No location found!"));
                display.clearDisplay();
                OLEDiplay(F("Aucun emplacement trouve !"), 1);
                return; 
            }
           // String storedNameSKU = storedName.substring(0, 15);    
          //  Serial.println(storedNameSKU);
            // Affichage OLED
            display.clearDisplay();
            display.setCursor(0, 0); 
            OLEDiplay(F("SKU:"), 1);
            OLEDiplay(storedName, 1);
           // Serial.println(storedName);
            delay(2000);
        }
        else if(lastPotValue == 2) { 
            Serial.println(F("\nVente..."));
        }
        else if(lastPotValue == 3) {
            Serial.println(F("\nErase tag ... !"));
            clearUltralightTag() ;
            display.clearDisplay();
            display.setCursor(0, 0); 
            OLEDiplay(F("Tag cleared !"), 1);
            delay(1000);
        }
    }
}

/********************************************************************
 * Fonctions utilitaires
 ********************************************************************/

// Écriture d'une chaîne de caractères dans la mémoire RFID
/*void writeStringToUltralight(const char* text, int page) {
    byte buffer[4];
    int len = strlen(text);
    int written = 0;

    while (written < len && page <= END_PAGE) {
        memset(buffer, 0, 4);
        for (int j = 0; j < 4 && written < len; j++) {
            buffer[j] = text[written++];
        }
        for (int row = 0; row < 2; row++) {
                auto status = mfrc522.MIFARE_Ultralight_Write(page,dataBlock[row], 4);
            if (status != MFRC522::STATUS_OK) {
                Serial.print("Échec d'écriture à la page "); Serial.println(page);
                return;
            }
            page++;
        }
    }
    Serial.println("Écriture terminée.");
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
            Serial.print("Échec d'écriture à la page "); Serial.println(page);
            return;
        }
        page++;
    }
    Serial.println("Écriture terminée.");
}
// Lecture d'une chaîne de caractères depuis la mémoire RFID
String readStringFromUltralight(byte startPage, byte length) {
    String result = "";
    uint8_t pages = ((length + 3) / 4)+1; // Nombre de pages à lire
    Serial.println(pages);
    byte rawBuf[18];
    byte size = sizeof(rawBuf);

    for (uint8_t i = 0; i < pages; i++) {
        byte page = startPage + i;
        MFRC522::StatusCode status = mfrc522.MIFARE_Read(page, rawBuf, &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("Échec de lecture à la page "));
            Serial.print(page);
            Serial.print(F(": "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            return "";
        }
        for (uint8_t j = 0; j < 4 && result.length() < length; j++) {
            if (i==0 && j==0) j+=2; // Ignore les 2 premiers octets
           // else if (i==pages-1 && j==2) break; // Ignore les 2 derniers octets
            byte b = rawBuf[j];
            Serial.println(b, HEX);
            Serial.println(j);
            if (b == 0x00) break;
            result += (char)b;
        }
    }
    return result;
}

// Envoi des données au serveur
void sendToServer(String uid, String name) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        String postData = "uid=" + uid + "&name=" + name ;
        int httpResponseCode = http.POST(postData);

        if (httpResponseCode > 0) {
            Serial.println("Réponse du serveur: " + http.getString());
        } else {
            Serial.println("Erreur lors de l'envoi des données.");
        }
        http.end();
    } else {
        Serial.println("Wi-Fi déconnecté !");
    }
}

// Initialisation de la connexion Wi-Fi
/*void initWiFi2() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0); 
    display.setTextSize(1);
    display.printf("Wi-Fi: connexion a %s …\n", ssid);
    display.display();
    delay(1000);

   // WiFi.begin(ssid, password);
    uint16_t idx = 0;
    display.clearDisplay();
    while (WiFi.status() != WL_CONNECTED) {
        display.setCursor(idx, 0); 
        display.print('.');
        display.display();
        idx += 3;
        delay(250);
    }
    display.clearDisplay();
    display.println("Wi-Fi connecte !");
    display.display();
    delay(1000);
}*/

void initWiFi() {
    display.setCursor(0, 0); 
    display.setTextSize(1);
    display.printf("Wi-Fi: connexion");
    display.display();
    delay(1000);

    WiFiManager wm;

    // Register callbacks
    wm.setAPCallback(onPortalStart);      // Called when portal starts
    wm.setSaveConfigCallback(onSaveConfig); // Called when WiFi is connected

    wm.setConfigPortalTimeout(90);
    if(!wm.autoConnect("StationTrie")) {
        display.clearDisplay();
        if (WiFi.status() != WL_CONNECTED) {
            OLEDiplay("Connection failed!", 1);
        }
        delay(2000);
        ESP.restart();
    } 
    Serial.println("connected...yeey :)");
    display.clearDisplay();
    display.println("Wi-Fi connected!");
    display.display();
    delay(2000);
}

// Synchronisation de l'heure via NTP
void initTime() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("NTP: synchronisation");
    display.display();
    struct tm tm;
    while (!getLocalTime(&tm)) {
        for (int i = 0; i < 3; i++) {
            display.print(".");
            display.display();
            delay(500);
        }
    }
    display.clearDisplay();
    display.println("Pret !");
    display.display();
    delay(1000);
}

// Mise à jour du SKU avec la date/heure actuelle
void updateSKU() { 
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        display.print("Erreur time");
        display.display();
        return;
    }
    char buf[20];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &timeinfo);
    skuFull = String(buf);
}

// Affichage d'un message sur l'écran OLED
void OLEDiplay(const String& msg, int16_t size) {
    display.setTextColor(SSD1306_WHITE);
    //display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
    display.setTextSize(size);
    display.println(msg);
    display.display();
}

// Mettre à jour les LEDs et l'écran OLED en fonction de l'état
void updateState(int state) {
    String message;
    display.clearDisplay();
    display.setCursor(0, 12);

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
// Fonction pour récupérer et afficher la localisation
// (à utiliser dans la boucle principale ou une nouvelle fonction)
void fetchAndDisplayLocation(const String& storedName) {
  if (WiFi.status() != WL_CONNECTED) {
    OLEDiplay("WiFi not connected", 1);
    return;
  }

  // 1) Construction de l'URL
  String url = String(serverUrl) + "?name=" + storedName;
  
  // 2) Requête GET
  HTTPClient http;
  http.begin(url);
  int statusCode = http.GET();

  if (statusCode == HTTP_CODE_OK) {
    String payload = http.getString();  // ex: {"location":"1A3B2-4"}
    DynamicJsonDocument doc(200);
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
      OLEDiplay("JSON parse error", 1);
    } else if (doc.containsKey("location")) {
      String loc = doc["location"].as<String>();
      OLEDiplay("Loc: " + loc, 2);   // display location
    } else if (doc.containsKey("error")) {
      String errMsg = doc["error"].as<String>();
      OLEDiplay("Error: " + errMsg, 1);
    }
  } else {
    OLEDiplay("HTTP error: " + String(statusCode), 1);
  }

  http.end();
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

// Callback: called when the config portal starts
void onPortalStart(WiFiManager *wm) {
    display.clearDisplay();
    display.setCursor(0, 0);
    OLEDiplay("Portal started!", 1);
}

// Callback: called when WiFi is connected and configuration is saved
void onSaveConfig() {
    display.clearDisplay();
    display.setCursor(0, 0);
    OLEDiplay("WiFi connected!", 1);
}

void clearUltralightTag() {
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
            Serial.println(page);
        }
    }
    Serial.println("Tag cleared!");
}

String urlencode(const String& str) {
  String ret;
  char c;
  for (size_t i = 0; i < str.length(); i++) {
    c = str[i];
    if (isalnum(c)) ret += c;
    else if (c == ' ') ret += '+';
    else {
      ret += '%';
      ret += String((uint8_t)c, HEX);
    }
  }
  return ret;
}

String getLocation(const String& nameKey) {
  HTTPClient http;
  String url = String(serverUrl) + "?name=" + urlencode(nameKey);

  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int httpCode = http.GET();

  Serial.printf("GET %s → HTTP %d\n", url.c_str(), httpCode);

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("RAW payload: " + payload);
    http.end();

    DynamicJsonDocument doc(256);
    auto err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("JSON parse failed: ");
      Serial.println(err.c_str());
      return "JSON error";
    }
    if (doc.containsKey("location")) {
      return doc["location"].as<String>();
    } else {
      return "no ‘location’ key";
    }
  }

  http.end();
  return String("HTTP error ") + httpCode;
}