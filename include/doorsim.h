#ifndef DOORSIM_H
#define DOORSIM_H

#include <Arduino.h>

// Structs
struct CardData
{
    unsigned int bitCount;
    unsigned long facilityCode;
    unsigned long cardNumber;
    String hexCardData;
    String rawCardData;
    String status;
    String details;
};

struct Credential
{
    unsigned long facilityCode;
    unsigned long cardNumber;
    char name[50];
};

struct WiegandFormat
{
    char description[48];
    unsigned int bitCount;
    unsigned int facilityCodeStart;
    unsigned int facilityCodeEnd;
    unsigned int cardNumberStart;
    unsigned int cardNumberEnd;
};

void ISR_INT0();
void ISR_INT1();
void saveSettingsToPreferences();
void loadSettingsFromPreferences();
void saveCredentialsToPreferences();
void loadCredentialsFromPreferences();
void loadWiegandFormats();
const WiegandFormat *findWiegandFormat(unsigned int bits);
const Credential *checkCredential(unsigned long fc, unsigned long cn);
void ledOnValid();
void speakerOnValid();
void lcdInvalidCredentials();
void speakerOnFailure();
void printCardData();
unsigned long decodeHIDFacilityCode(unsigned int start, unsigned int end);
unsigned long decodeHIDCardNumber(unsigned int start, unsigned int end);
void processHIDCard();
void processCardData();
void clearDatabits();
void cleanupCardData();
bool allBitsAreOnes();
String centerText(const String &text, int width);
void printWelcomeMessage();
void updateDisplay();
void printAllCardData();
void setupWifi();
void webServer();

#endif // DOORSIM_H
