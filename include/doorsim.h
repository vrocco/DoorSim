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
    String formatName;
    bool hasFormat;
    int parityStatus;
};

struct Credential
{
    unsigned long facilityCode;
    unsigned long cardNumber;
    char name[50];
};

void ISR_INT0();
void ISR_INT1();
void saveSettingsToPreferences();
void loadSettingsFromPreferences();
void saveCredentialsToPreferences();
void loadCredentialsFromPreferences();
const Credential *checkCredential(unsigned long fc, unsigned long cn);
void ledOnValid();
void speakerOnValid();
void lcdInvalidCredentials();
void speakerOnFailure();
void printCardData();
unsigned long decodeWiegandField(unsigned int start, unsigned int end);
bool checkParityRange(unsigned int parityBit, unsigned int start, unsigned int end, bool expectOdd);
String rawBitsToHex();
void processWiegandCard();
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
