#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "nvs_flash.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include <LittleFS.h>

#include "doorsim.h"

AsyncWebServer server(80);

const char *settingsFile = "/settings.json";
const char *credentialsFile = "/credentials.json";
const char *wiegandFormatsFile = "/wiegand_formats.json";

const char defaultWiegandFormatsJson[] = R"json({
  "wiegandFormats": [
    {
      "description": "DoorSim 26-bit legacy mapping",
      "bitCount": 26,
      "facilityCodeStart": 2,
      "facilityCodeEnd": 9,
      "cardNumberStart": 10,
      "cardNumberEnd": 25
    },
    {
      "description": "DoorSim 27-bit legacy mapping",
      "bitCount": 27,
      "facilityCodeStart": 2,
      "facilityCodeEnd": 13,
      "cardNumberStart": 14,
      "cardNumberEnd": 27
    },
    {
      "description": "DoorSim 29-bit legacy mapping",
      "bitCount": 29,
      "facilityCodeStart": 2,
      "facilityCodeEnd": 13,
      "cardNumberStart": 14,
      "cardNumberEnd": 29
    },
    {
      "description": "DoorSim 30-bit legacy mapping",
      "bitCount": 30,
      "facilityCodeStart": 2,
      "facilityCodeEnd": 13,
      "cardNumberStart": 14,
      "cardNumberEnd": 29
    },
    {
      "description": "DoorSim 31-bit legacy mapping",
      "bitCount": 31,
      "facilityCodeStart": 2,
      "facilityCodeEnd": 5,
      "cardNumberStart": 6,
      "cardNumberEnd": 28
    },
    {
      "description": "DoorSim 32-bit legacy mapping",
      "bitCount": 32,
      "facilityCodeStart": 6,
      "facilityCodeEnd": 16,
      "cardNumberStart": 18,
      "cardNumberEnd": 32
    },
    {
      "description": "DoorSim 33-bit legacy mapping",
      "bitCount": 33,
      "facilityCodeStart": 2,
      "facilityCodeEnd": 8,
      "cardNumberStart": 9,
      "cardNumberEnd": 32
    },
    {
      "description": "DoorSim 34-bit legacy mapping",
      "bitCount": 34,
      "facilityCodeStart": 2,
      "facilityCodeEnd": 17,
      "cardNumberStart": 18,
      "cardNumberEnd": 33
    },
    {
      "description": "DoorSim 35-bit legacy mapping",
      "bitCount": 35,
      "facilityCodeStart": 3,
      "facilityCodeEnd": 14,
      "cardNumberStart": 15,
      "cardNumberEnd": 34
    },
    {
      "description": "DoorSim 36-bit legacy mapping",
      "bitCount": 36,
      "facilityCodeStart": 22,
      "facilityCodeEnd": 33,
      "cardNumberStart": 2,
      "cardNumberEnd": 17
    }
  ]
})json";

#define I2C_SDA 21
#define I2C_SCL 22
// Set the LCD I2C address
LiquidCrystal_I2C lcd(0x20, 20, 4);

// general device settings
bool isCapturing = true;
String MODE = "CTF";

// card reader config and variables

// max number of bits
#define MAX_BITS 100
// Frame is complete after this much real elapsed silence between Wiegand pulses.
#define WIEGAND_FRAME_GAP_MS 25

// stores all of the data bits
volatile unsigned char databits[MAX_BITS];
volatile unsigned int bitCount = 0;

// stores the last written card's data bits
unsigned char lastWrittenDatabits[MAX_BITS];
unsigned int lastWrittenBitCount = 0;

// goes low when data is currently being captured
volatile unsigned char flagDone;

// Frame timing is tracked from loop(), not from inside the interrupt handlers.
unsigned int lastObservedBitCount = 0;
unsigned long lastWiegandBitChangeMs = 0;

// Display screen timer
unsigned long displayTimeout = 30000; // 30 seconds
unsigned long lastCardTime = 0;
bool displayingCard = false;

// Wifi Settings
bool ap_mode = true;
// AP Settings
// ssid_hidden = broadcast ssid = 0, hidden = 1
// ap_passphrase = NULL for open, min 8 chars, max 63
String ap_ssid = "doorsim";
String ap_passphrase;
int ap_channel = 1;
int ssid_hidden;

// Speaker and LED Settings
int spkOnInvalid = 1;
int spkOnValid = 1;
int ledValid = 1;

// Custom Display Message
String customMessage;
String welcomeMessage = "default";

// decoded facility code and card code
unsigned long facilityCode = 0;
unsigned long cardNumber = 0;

// hex data string
String hexCardData;

// raw data string
String rawCardData;
String status;
String details;

// Define reader input pins
// card reader DATA0
#define DATA0 19
// card reader DATA1
#define DATA1 18

// define reader output pins
//  LED Output for a GND tie back
#define LED 32
// Speaker Output for a GND tie back
#define SPK 33

// define relay modules
#define RELAY1 25
#define RELAY2 26

const int MAX_CREDENTIALS = 100;
Credential credentials[MAX_CREDENTIALS];
int validCount = 0;

const int MAX_WIEGAND_FORMATS = 32;
WiegandFormat wiegandFormats[MAX_WIEGAND_FORMATS];
int wiegandFormatCount = 0;

// maximum number of stored cards
const int MAX_CARDS = 100;
CardData cardDataArray[MAX_CARDS];
int cardDataIndex = 0;

// Interrupts for card reader
void ISR_INT0()
{
  if (bitCount < MAX_BITS)
  {
    databits[bitCount] = 0;
    bitCount++;
  }

  flagDone = 0;
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  if (bitCount < MAX_BITS)
  {
    databits[bitCount] = 1;
    bitCount++;
  }

  flagDone = 0;
}

void saveSettingsToPreferences()
{
  Serial.println("Saving settings to Preferences...");

  File file = LittleFS.open(settingsFile, "w");
  if (!file)
  {
    Serial.println("Failed to open settings file for writing.");
    return;
  }

  // Write settings to JSON
  JsonDocument doc;
  doc["MODE"] = MODE;
  doc["displayTimeout"] = displayTimeout;
  doc["ap_mode"] = ap_mode;
  doc["ap_ssid"] = ap_ssid;
  doc["ap_passphrase"] = ap_passphrase;
  doc["ap_channel"] = ap_channel;
  doc["ssid_hidden"] = ssid_hidden;
  doc["spkOnInvalid"] = spkOnInvalid;
  doc["spkOnValid"] = spkOnValid;
  doc["ledValid"] = ledValid;
  doc["customMessage"] = customMessage;
  doc["welcomeMessage"] = welcomeMessage;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write settings to file.");
  }
  else
  {
    Serial.println("customMessage: " + customMessage);
    Serial.println("Settings saved successfully.");
  }
  file.close();
  Serial.println("Settings saved!");
}

void loadSettingsFromPreferences()
{
  if (!LittleFS.exists(settingsFile))
  {
    Serial.println("Settings file does not exist. Creating with defaults...");
    saveSettingsToPreferences();
    return;
  }

  File file = LittleFS.open(settingsFile, "r");
  if (!file)
  {
    Serial.println("Failed to open settings file for reading.");
    return;
  }

  // Parse JSON from file
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  Serial.println("File Content:");
  file.seek(0);
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
  Serial.println();

  if (error)
  {
    Serial.print("Failed to parse settings file: ");
    Serial.println(error.c_str());
    return;
  }

  // Load settings
  MODE = doc["MODE"] | "CTF";
  displayTimeout = doc["displayTimeout"] | 30000;
  ap_mode = doc["ap_mode"] | true;
  ap_ssid = doc["ap_ssid"] | "doorsim";
  ap_passphrase = doc["ap_passphrase"] | "";
  ap_channel = doc["ap_channel"] | 1;
  ssid_hidden = doc["ssid_hidden"] | 0;
  spkOnInvalid = doc["spkOnInvalid"] | 1;
  spkOnValid = doc["spkOnValid"] | 1;
  ledValid = doc["ledValid"] | 1;
  welcomeMessage = doc["welcomeMessage"] | "default";
  if (welcomeMessage != "default")
  {
    customMessage = doc["customMessage"] | "";
  }
  Serial.println("Settings loaded successfully:");
}

void saveCredentialsToPreferences()
{
  File file = LittleFS.open(credentialsFile, "w");
  if (!file)
  {
    Serial.println("Failed to open credentials file for writing.");
    return;
  }

  // Write settings to JSON
  JsonDocument doc;
  doc["validCount"] = validCount;
  JsonArray credentialsArray = doc.createNestedArray("credentials");
  for (int i = 0; i < validCount; i++)
  {
    JsonObject credential = credentialsArray.createNestedObject();
    credential["facilityCode"] = credentials[i].facilityCode;
    credential["cardNumber"] = credentials[i].cardNumber;
    credential["name"] = credentials[i].name;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write credentials to file.");
  }
  else
  {
    Serial.println("credentials saved successfully.");
  }
  file.close();

  for (int i = 0; i < validCount; i++)
  {
    Serial.print("Credential ");
    Serial.print(i);
    Serial.print(": FC=");
    Serial.print(credentials[i].facilityCode);
    Serial.print(", CN=");
    Serial.print(credentials[i].cardNumber);
    Serial.print(", Name=");
    Serial.println(credentials[i].name);
  }
  Serial.print("Valid Count: ");
  Serial.println(validCount);
}

void loadCredentialsFromPreferences()
{
  Serial.println("Loading credentials from Preferences...");

  if (!LittleFS.exists(credentialsFile))
  {
    Serial.println("credentials file does not exist. Creating with defaults...");
    saveCredentialsToPreferences();
    return;
  }

  File file = LittleFS.open(credentialsFile, "r");
  if (!file)
  {
    Serial.println("Failed to open credentials file for reading.");
    return;
  }

  // Parse JSON from file
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  Serial.println("File Content:");
  file.seek(0);
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
  Serial.println();

  if (error)
  {
    Serial.print("Failed to parse settings file: ");
    Serial.println(error.c_str());
    return;
  }

  // Load settings
  validCount = doc["validCount"] | 0;
  if (validCount > 0)
  {
    JsonArray credentialsArray = doc["credentials"].as<JsonArray>();
    for (int i = 0; i < validCount; i++)
    {
      Serial.println("Loading credential " + String(i));
      JsonObject credential = credentialsArray[i].as<JsonObject>();
      credentials[i].facilityCode = credential["facilityCode"] | 0;
      credentials[i].cardNumber = credential["cardNumber"] | 0;
      String name = credential["name"] | "";
      strncpy(credentials[i].name, name.c_str(), sizeof(credentials[i].name) - 1);
      credentials[i].name[sizeof(credentials[i].name) - 1] = '\0';
    }
  }
  else
  {
    Serial.println("No valid credentials found.");
  }
  Serial.println("Credentials loaded from Preferences:");
  for (int i = 0; i < validCount; i++)
  {
    Serial.print("Credential ");
    Serial.print(i);
    Serial.print(": FC=");
    Serial.print(credentials[i].facilityCode);
    Serial.print(", CN=");
    Serial.print(credentials[i].cardNumber);
    Serial.print(", Name=");
    Serial.println(credentials[i].name);
  }
  Serial.print("Valid Count: ");
  Serial.println(validCount);
}

// Check if credential is valid
const Credential *checkCredential(unsigned long fc, unsigned long cn)
{
  for (unsigned int i = 0; i < validCount; i++)
  {
    if (credentials[i].facilityCode == fc && credentials[i].cardNumber == cn)
    {
      // Found a matching credential, return a pointer to it
      return &credentials[i];
    }
  }
  // No matching credential found, return nullptr
  return nullptr;
}

void ledOnValid()
{
  switch (ledValid)
  {
  case 0:
    break;

  case 1:
    // Flashing LED
    digitalWrite(LED, LOW);
    delay(250);
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(250);
    digitalWrite(LED, HIGH);
    break;

  case 2:
    digitalWrite(LED, LOW);
    delay(2000);
    digitalWrite(LED, HIGH);
    break;
  }
}

// Functions to handle valid credentials
void speakerOnValid()
{
  switch (spkOnValid)
  {
  case 0:
    break;

  case 1:
    // Nice Beeps LED
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    break;

  case 2:
    // Long Beeps
    digitalWrite(SPK, LOW);
    delay(2000);
    digitalWrite(SPK, HIGH);
    break;
  }
}

// Functions to handle invalid credentials
void lcdInvalidCredentials()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card Read: ");
  lcd.setCursor(11, 0);
  lcd.print("INVALID");
  lcd.setCursor(0, 2);
  lcd.print(" THIS INCIDENT WILL");
  lcd.setCursor(0, 3);
  lcd.print("    BE REPORTED    ");
}

void speakerOnFailure()
{
  switch (spkOnInvalid)
  {
  case 0:
    break;

  case 1:
    // Sad Beeps
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    break;
  }
}

bool validateWiegand26Parity()
{
  if (bitCount != 26)
  {
    return false;
  }

  unsigned int firstHalfOnes = databits[0];
  for (unsigned int i = 1; i <= 12; i++)
  {
    firstHalfOnes += databits[i];
  }

  unsigned int secondHalfOnes = databits[25];
  for (unsigned int i = 13; i <= 24; i++)
  {
    secondHalfOnes += databits[i];
  }

  // Standard Wiegand-26: leading parity is even, trailing parity is odd.
  return ((firstHalfOnes % 2) == 0) && ((secondHalfOnes % 2) == 1);
}

void printCardData()
{
  if (MODE == "CTF")
  {
    const Credential *result = checkCredential(facilityCode, cardNumber);
    if (result != nullptr)
    {
      // Valid credential found
      Serial.println("Valid credential found:");
      Serial.println("FC: " + String(result->facilityCode) + ", CN: " + String(result->cardNumber) + ", Name: " + result->name);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card Read: ");
      lcd.setCursor(11, 0);
      lcd.print("VALID");
      lcd.setCursor(0, 1);
      lcd.print("FC: " + String(result->facilityCode));
      lcd.setCursor(9, 1);
      lcd.print("CN:" + String(result->cardNumber));
      lcd.setCursor(0, 3);
      lcd.print("Name: " + String(result->name));
      ledOnValid();
      speakerOnValid();

      // Update card data status and details
      status = "Authorized";
      details = result->name;
    }
    else
    {
      // No valid credential found
      Serial.println("Error: No valid credential found.");
      lcdInvalidCredentials();
      speakerOnFailure();

      // Update card data status and details
      status = "Unauthorized";
      details = "FC: " + String(facilityCode) + ", CN: " + String(cardNumber);
    }
  }
  else
  {
    // ranges for "valid" bitCount are a bit larger for debugging
    if (bitCount > 20 && bitCount < 120)
    {
      // ignore data caused by noise
      Serial.print("[*] Bit length: ");
      Serial.println(bitCount);
      Serial.print("[*] Facility code: ");
      Serial.println(facilityCode);
      Serial.print("[*] Card number: ");
      Serial.println(cardNumber);
      Serial.print("[*] Hex: ");
      Serial.println(hexCardData);
      Serial.print("[*] Raw: ");
      Serial.println(rawCardData);

      if (bitCount == 26)
      {
        Serial.print("[*] Wiegand-26 parity: ");
        Serial.println(validateWiegand26Parity() ? "OK" : "BAD");
      }

      // LCD Printing
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card Read: ");
      lcd.setCursor(11, 0);
      lcd.print(bitCount);
      lcd.print("bits");
      lcd.setCursor(0, 1);
      lcd.print("FC: ");
      lcd.print(facilityCode);
      lcd.setCursor(9, 1);
      lcd.print(" CN: ");
      lcd.print(cardNumber);
      if (bitCount == 26)
      {
        lcd.setCursor(0, 2);
        lcd.print("Parity: ");
        lcd.print(validateWiegand26Parity() ? "OK" : "BAD");
      }
      lcd.setCursor(0, 3);
      lcd.print("Hex: ");
      hexCardData.toUpperCase();
      lcd.print(hexCardData);

      // Update card data status and details
      status = "Read";
      details = "Hex: " + hexCardData;
    }
  }

  // Store card data
  if (cardDataIndex < MAX_CARDS)
  {
    cardDataArray[cardDataIndex].bitCount = bitCount;
    cardDataArray[cardDataIndex].facilityCode = facilityCode;
    cardDataArray[cardDataIndex].cardNumber = cardNumber;
    cardDataArray[cardDataIndex].hexCardData = hexCardData;
    cardDataArray[cardDataIndex].rawCardData = rawCardData;
    cardDataArray[cardDataIndex].status = status;
    cardDataArray[cardDataIndex].details = details;
    cardDataIndex++;
  }

  // Start the display timer
  lastCardTime = millis();
  displayingCard = true;
}

// Process hid cards
void loadWiegandFormats()
{
  wiegandFormatCount = 0;

  if (!LittleFS.exists(wiegandFormatsFile))
  {
    Serial.println("Wiegand format file does not exist. Creating defaults...");
    File defaultFile = LittleFS.open(wiegandFormatsFile, "w");
    if (!defaultFile)
    {
      Serial.println("Failed to create default Wiegand format file.");
      return;
    }

    defaultFile.print(defaultWiegandFormatsJson);
    defaultFile.close();
  }

  File file = LittleFS.open(wiegandFormatsFile, "r");
  if (!file)
  {
    Serial.println("Failed to open Wiegand format file.");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.print("Failed to parse Wiegand formats: ");
    Serial.println(error.c_str());
    return;
  }

  JsonArray formats = doc["wiegandFormats"].as<JsonArray>();
  for (JsonObject item : formats)
  {
    if (wiegandFormatCount >= MAX_WIEGAND_FORMATS)
    {
      Serial.println("Maximum Wiegand format count reached.");
      break;
    }

    WiegandFormat format = {};
    String description = item["description"] | "";
    format.bitCount = item["bitCount"] | 0;
    format.facilityCodeStart = item["facilityCodeStart"] | 0;
    format.facilityCodeEnd = item["facilityCodeEnd"] | 0;
    format.cardNumberStart = item["cardNumberStart"] | 0;
    format.cardNumberEnd = item["cardNumberEnd"] | 0;

    bool valid =
        format.bitCount > 0 &&
        format.bitCount <= MAX_BITS &&
        format.facilityCodeStart > 0 &&
        format.facilityCodeEnd >= format.facilityCodeStart &&
        format.facilityCodeEnd <= format.bitCount &&
        format.cardNumberStart > 0 &&
        format.cardNumberEnd >= format.cardNumberStart &&
        format.cardNumberEnd <= format.bitCount;

    if (!valid)
    {
      Serial.print("Skipping invalid Wiegand format: ");
      Serial.println(description);
      continue;
    }

    strncpy(format.description, description.c_str(), sizeof(format.description) - 1);
    format.description[sizeof(format.description) - 1] = '\0';
    wiegandFormats[wiegandFormatCount] = format;
    wiegandFormatCount++;
  }

  Serial.print("Loaded Wiegand formats: ");
  Serial.println(wiegandFormatCount);
}

const WiegandFormat *findWiegandFormat(unsigned int bits)
{
  for (int i = 0; i < wiegandFormatCount; i++)
  {
    if (wiegandFormats[i].bitCount == bits)
    {
      return &wiegandFormats[i];
    }
  }

  return nullptr;
}

unsigned long decodeHIDFacilityCode(unsigned int start, unsigned int end)
{
  unsigned long HIDFacilityCode = 0;
  for (unsigned int i = start; i < end; i++)
  {
    HIDFacilityCode = (HIDFacilityCode << 1) | databits[i];
  }
  return HIDFacilityCode;
}

unsigned long decodeHIDCardNumber(unsigned int start, unsigned int end)
{
  unsigned long HIDCardNumber = 0;
  for (unsigned int i = start; i < end; i++)
  {
    HIDCardNumber = (HIDCardNumber << 1) | databits[i];
  }
  return HIDCardNumber;
}

String rawBitsToHex()
{
  static const char hexDigits[] = "0123456789ABCDEF";
  String out = "";

  if (bitCount == 0)
  {
    return out;
  }

  // Pad on the left so the received bitstream aligns to complete hex nibbles.
  // This preserves the numeric value while keeping every received bit intact.
  unsigned int padBits = (4 - (bitCount % 4)) % 4;
  unsigned char nibble = 0;
  unsigned int nibbleBits = 0;

  for (unsigned int i = 0; i < padBits + bitCount; i++)
  {
    unsigned char bit = 0;
    if (i >= padBits)
    {
      bit = databits[i - padBits] ? 1 : 0;
    }

    nibble = (nibble << 1) | bit;
    nibbleBits++;

    if (nibbleBits == 4)
    {
      out += hexDigits[nibble];
      nibble = 0;
      nibbleBits = 0;
    }
  }

  return out;
}

void processHIDCard()
{
  const WiegandFormat *format = findWiegandFormat(bitCount);
  if (format == nullptr)
  {
    Serial.println("[-] Unsupported bitCount for Wiegand card");
    return;
  }

  Serial.print("[*] Bit length: ");
  Serial.println(bitCount);
  Serial.print("[*] Wiegand format: ");
  Serial.println(format->description);

  // Format files use human-readable, 1-based inclusive bit positions.
  // The existing decode helpers use 0-based start / exclusive end indices.
  facilityCode =
      decodeHIDFacilityCode(format->facilityCodeStart - 1, format->facilityCodeEnd);
  cardNumber =
      decodeHIDCardNumber(format->cardNumberStart - 1, format->cardNumberEnd);
}

bool isSupportedWiegandBitCount(unsigned int bits)
{
  return findWiegandFormat(bits) != nullptr;
}

void processCardData()
{
  Serial.println("Processing card data...");
  // clear the databits array
  rawCardData = "";
  for (unsigned int i = 0; i < bitCount; i++)
  {
    rawCardData += String(databits[i]);
  }

  Serial.print("[*] Raw: ");
  Serial.println(rawCardData);
  Serial.print("[*] bitCount: ");
  Serial.println(bitCount);

  if (isSupportedWiegandBitCount(bitCount))
  {
    processHIDCard();
  }

  // Treat the captured Wiegand bitstream as authoritative for the hex value.
  // The existing facility/card-number decoder remains unchanged for comparison.
  hexCardData = rawBitsToHex();
}

void clearDatabits()
{
  Serial.println("Clearing databits...");
  // clear the databits array
  for (unsigned char i = 0; i < MAX_BITS; i++)
  {
    databits[i] = 0;
  }
}

// reset variables and prepare for the next card read
void cleanupCardData()
{
  rawCardData = "";
  hexCardData = "";
  bitCount = 0;
  facilityCode = 0;
  cardNumber = 0;
  status = "";
  details = "";
}

bool allBitsAreOnes()
{
  if (bitCount == 0)
  {
    return false;
  }

  for (unsigned int i = 0; i < bitCount; i++)
  {
    if (databits[i] != 1)
    {
      return false;
    }
  }

  return true;
}

String centerText(const String &text, int width)
{
  int len = text.length();
  if (len >= width)
  {
    return text;
  }
  int padding = (width - len) / 2;
  String spaces = "";
  for (int i = 0; i < padding; i++)
  {
    spaces += " ";
  }
  return spaces + text;
}

void displaySetupMassage(const char *message)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(centerText("Setup", 20));
  lcd.setCursor(0, 2);
  lcd.print(centerText(message, 20));
}

void printWelcomeMessage()
{
  if (MODE == "CTF")
  {
    if (customMessage != NULL)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(centerText(String(customMessage), 20));
      lcd.setCursor(0, 2);
      lcd.print(centerText("Present Card", 20));
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(centerText("CTF Mode", 20));
      lcd.setCursor(0, 2);
      lcd.print(centerText("Present Card", 20));
    }
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(centerText("Door Sim - Ready", 20));
    lcd.setCursor(0, 2);
    lcd.print(centerText("Present Card", 20));
  }
}

void updateDisplay()
{
  if (displayingCard && (millis() - lastCardTime >= displayTimeout))
  {
    printWelcomeMessage();
    displayingCard = false;
  }
}

void printAllCardData()
{
  Serial.println("Previously read card data:");
  for (int i = 0; i < cardDataIndex; i++)
  {
    Serial.print(i + 1);
    Serial.print(": Bit length: ");
    Serial.print(cardDataArray[i].bitCount);
    Serial.print(", Facility code: ");
    Serial.print(cardDataArray[i].facilityCode);
    Serial.print(", Card number: ");
    Serial.print(cardDataArray[i].cardNumber);
    Serial.print(", Hex: ");
    Serial.print(cardDataArray[i].hexCardData);
    Serial.print(", Raw: ");
    Serial.println(cardDataArray[i].rawCardData);
  }
}

void setupWifi()
{
  WiFi.softAP(ap_ssid, ap_passphrase, ap_channel, ssid_hidden);
}

void webServer()
{

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
              //request->send(200, "text/html", FPSTR(index_html)); 
              request->send(LittleFS, "/index.html", String()); });

  server.on("/getCards", HTTP_GET, [](AsyncWebServerRequest *request)
            {      
      JsonDocument doc;
      JsonArray cards = doc.to<JsonArray>();
      for (int i = 0; i < cardDataIndex; i++) {          
          JsonObject card = cards.add<JsonObject>();
          card["bitCount"] = cardDataArray[i].bitCount;
          card["facilityCode"] = cardDataArray[i].facilityCode;
          card["cardNumber"] = cardDataArray[i].cardNumber;
          card["hexCardData"] = cardDataArray[i].hexCardData;
          card["rawCardData"] = cardDataArray[i].rawCardData;
          card["status"] = cardDataArray[i].status;
          card["details"] = cardDataArray[i].details;
      }
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

  server.on("/getUsers", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      JsonDocument doc;
      JsonArray users = doc.to<JsonArray>();
      for (int i = 0; i < validCount; i++) {          
          JsonObject user = users.add<JsonObject>();
          user["facilityCode"] = credentials[i].facilityCode;
          user["cardNumber"] = credentials[i].cardNumber;
          user["name"] = credentials[i].name;
      }
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

  server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request)
            {      
      JsonDocument doc;
      doc["mode"] = MODE;
      doc["displayTimeout"] = displayTimeout;
      doc["apSsid"] = ap_ssid;
      doc["apPassphrase"] = ap_passphrase;
      doc["ssidHidden"] = ssid_hidden;
      doc["apChannel"] = ap_channel;
      doc["welcomeMessage"] = customMessage.length() > 0 ? "custom" : "default";
      doc["customMessage"] = customMessage;
      doc["ledValid"] = ledValid;
      doc["spkOnValid"] = spkOnValid;
      doc["spkOnInvalid"] = spkOnInvalid;
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/saveSettings", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                         {
      JsonObject jsonObj = json.as<JsonObject>();

      // Parse the JSON and update settings
      MODE = jsonObj["mode"] | "CTF";
      displayTimeout = jsonObj["displayTimeout"] | 30000;
      ap_ssid = jsonObj["apSsid"] | "doorsim";
      ap_passphrase = jsonObj["apPassphrase"] | "";
      ap_channel = jsonObj["apChannel"] | 1;
      ssid_hidden = jsonObj["ssidHidden"] | 0;
      welcomeMessage = jsonObj["welcomeMessage"] | "default";
      customMessage = jsonObj["customMessage"] | "";
      spkOnInvalid = jsonObj["spkOnInvalid"] | 1;
      spkOnValid = jsonObj["spkOnValid"] | 1;
      ledValid = jsonObj["ledValid"] | 1;

      saveSettingsToPreferences();
      //setupWifi();
      request->send(200, "application/json", "{\"status\":\"success\"}"); });
  server.addHandler(handler);

  server.on("/addCard", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (validCount < MAX_CREDENTIALS) {
      if (request->hasParam("facilityCode") && request->hasParam("cardNumber") && request->hasParam("name")) {
        String facilityCodeStr = request->getParam("facilityCode")->value();
        String cardNumberStr = request->getParam("cardNumber")->value();
        String name = request->getParam("name")->value();

        credentials[validCount].facilityCode = facilityCodeStr.toInt();
        credentials[validCount].cardNumber = cardNumberStr.toInt();
        strncpy(credentials[validCount].name, name.c_str(), sizeof(credentials[validCount].name) - 1);
        credentials[validCount].name[sizeof(credentials[validCount].name) - 1] = '\0';
        validCount++;
        saveCredentialsToPreferences();
        request->send(200, "text/plain", "Card added successfully");
      } else {
        request->send(400, "text/plain", "Missing parameters");
      }
    } else {
      request->send(500, "text/plain", "Max number of credentials reached");
    } });

  server.on("/deleteCard", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("index")) {
      int index = request->getParam("index")->value().toInt();
      if (index >= 0 && index < validCount) {
        for (int i = index; i < validCount - 1; i++) {
          credentials[i] = credentials[i + 1];
        }
        validCount--;
        saveCredentialsToPreferences();
        request->send(200, "text/plain", "Card deleted successfully");
      } else {
        request->send(400, "text/plain", "Invalid index");
      }
    } else {
      request->send(400, "text/plain", "Missing index parameter");
    } });

  server.on("/exportData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    JsonArray users = doc.to<JsonArray>();
    JsonObject user = users.add<JsonObject>();
    for (int i = 0; i < validCount; i++) {
        JsonObject user = users.add<JsonObject>();
        user["facilityCode"] = credentials[i].facilityCode;
        user["cardNumber"] = credentials[i].cardNumber;
        user["name"] = credentials[i].name;
    }
    JsonArray cards = doc["cards"].to<JsonArray>();    
    for (int i = 0; i < cardDataIndex; i++) {
        JsonObject card = cards.add<JsonObject>();
        card["bitCount"] = cardDataArray[i].bitCount;
        card["facilityCode"] = cardDataArray[i].facilityCode;
        card["cardNumber"] = cardDataArray[i].cardNumber;
        card["hexCardData"] = cardDataArray[i].hexCardData;
        card["rawCardData"] = cardDataArray[i].rawCardData;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  // Route to load style.css file, and script.js file
  server.serveStatic("/", LittleFS, "/");

  server.begin();
}

void setup()
{
  pinMode(DATA0, INPUT);
  pinMode(DATA1, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(SPK, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  // turn off led
  digitalWrite(LED, HIGH);
  // turn off buzzers
  digitalWrite(SPK, HIGH);
  // turn on relay, lock the door!
  digitalWrite(RELAY1, HIGH);

  Serial.begin(115200);
  delay(100);
  Serial.println("Starting DoorSim...");

  Serial.println("LCD Initialized");
  lcd.init(I2C_SDA, I2C_SCL);
  lcd.backlight();
  displaySetupMassage("Initializing...");

  attachInterrupt(DATA0, ISR_INT0, FALLING);
  attachInterrupt(DATA1, ISR_INT1, FALLING);

  flagDone = 1;
  lastObservedBitCount = bitCount;
  lastWiegandBitChangeMs = millis();
  for (unsigned char i = 0; i < MAX_BITS; i++)
  {
    lastWrittenDatabits[i] = 0;
  }

  displaySetupMassage("Mounting LittleFS...");

  Serial.println("Checking for LittleFS...");
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  loadWiegandFormats();
  loadSettingsFromPreferences();
  loadCredentialsFromPreferences();

  displaySetupMassage("Setup WiFi...");
  Serial.println("Setup Wifi...");
  setupWifi();
  Serial.println("Wifi Setup Complete");

  displaySetupMassage("Starting Web Server...");

  Serial.println("Starting web server...");
  webServer();

  printWelcomeMessage();

  Serial.println("DoorSim Ready!");
}

void loop() {
  updateDisplay();

  // Observe capture progress from the main loop. Each newly observed bit
  // restarts a real elapsed-time silence window without doing clock work in
  // the interrupt handlers.
  unsigned int observedBitCount = bitCount;
  if (observedBitCount != lastObservedBitCount) {
    lastObservedBitCount = observedBitCount;
    lastWiegandBitChangeMs = millis();
  }

  // A frame is complete only after the bit count has remained unchanged for
  // the configured silence period. Re-check atomically before setting
  // flagDone so a pulse arriving at the boundary cannot finish the frame early.
  if (!flagDone && observedBitCount > 0 &&
      (millis() - lastWiegandBitChangeMs >= WIEGAND_FRAME_GAP_MS)) {
    bool frameCompleted = false;
    noInterrupts();
    if (!flagDone && bitCount == lastObservedBitCount) {
      flagDone = 1;
      frameCompleted = true;
    }
    interrupts();

    if (frameCompleted && flagDone) {
      Serial.println("Weigand transmission complete.");
    }
  }

  // Check if the card reader has finished reading data
  if (bitCount > 0 && flagDone) {
    // Indicate that a card is being displayed
    displayingCard = true;

    // Ensure the data is valid (not all bits are 1s)
    if (!allBitsAreOnes()) { 
      // Process the card data     
      processCardData();

      if (isSupportedWiegandBitCount(bitCount)) {
        // Display decoded card data on LCD and Serial
        printCardData();
        // Print all stored card data to Serial
        printAllCardData();
      }
      else if (bitCount >= 26 && bitCount <= 96) {
        Serial.print("[-] Unsupported Wiegand bit length: ");
        Serial.println(bitCount);
        Serial.print("[*] Hex: ");
        Serial.println(hexCardData);
        Serial.print("[*] Raw: ");
        Serial.println(rawCardData);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Unsupported Wiegand");
        lcd.setCursor(0, 1);
        lcd.print("Bits: ");
        lcd.print(bitCount);
        lcd.print(" Hex:");
        lcd.setCursor(0, 2);
        lcd.print(hexCardData.substring(0, 20));
        if (hexCardData.length() > 20) {
          lcd.setCursor(0, 3);
          lcd.print(hexCardData.substring(20));
        }

        lastCardTime = millis();
        displayingCard = true;
      }
    }

    // Reset the card reader data for the next read
    cleanupCardData();
    clearDatabits();
  }
}