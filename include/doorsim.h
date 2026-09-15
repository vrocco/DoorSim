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
    String formatId;
    String formatDescription;
    unsigned int formatCandidateCount;
    unsigned int formatViableCount;
};

struct Credential
{
    unsigned long facilityCode;
    unsigned long cardNumber;
    char name[50];
};

const unsigned int MAX_WIEGAND_PARITY_RULES = 3;

struct WiegandParityRule
{
    unsigned int bit;
    bool even;
    uint64_t mask;
};

struct WiegandFormat
{
    char id[40];
    char description[48];
    unsigned int bitCount;
    unsigned int facilityCodeStart;
    unsigned int facilityCodeEnd;
    unsigned int cardNumberStart;
    unsigned int cardNumberEnd;
    unsigned int parityEvenBit;
    unsigned int parityEvenStart;
    unsigned int parityEvenEnd;
    unsigned int parityOddBit;
    unsigned int parityOddStart;
    unsigned int parityOddEnd;
    WiegandParityRule parityRules[MAX_WIEGAND_PARITY_RULES];
    unsigned int parityRuleCount;
};

void ISR_INT0();
void ISR_INT1();
void saveSettingsToPreferences();
void loadSettingsFromPreferences();
void saveCredentialsToPreferences();
void loadCredentialsFromPreferences();
void loadWiegandFormats();
bool wiegandFormatHasParity(const WiegandFormat *format);
String describeWiegandCandidate(const WiegandFormat *format);
String buildWiegandCandidateDetails(unsigned int bits);
void printWiegandCandidateDiagnostics(unsigned int bits);
const WiegandFormat *selectWiegandFormat(unsigned int bits,
                                         unsigned int *candidateCount,
                                         unsigned int *viableCount);
const Credential *checkCredential(unsigned long fc, unsigned long cn);
bool validateWiegandParity(const WiegandFormat *format);
void ledOnValid();
void speakerOnValid();
void lcdInvalidCredentials();
void speakerOnFailure();
void printCardData();
unsigned long decodeHIDFacilityCode(unsigned int start, unsigned int end);
unsigned long decodeHIDCardNumber(unsigned int start, unsigned int end);
bool processHIDCard();
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
