#include <Arduino.h>
#include <Pololu3piPlus32U4.h>
#include "code39.h"

using namespace Pololu3piPlus32U4;

// ---------- Hardware objects ----------
Buzzer buzzer;
ButtonB buttonB;
LineSensors lineSensors;
Motors motors;
Encoders encoders;
OLED display;

// ---------- Lab constants ----------
const uint8_t  MAX_CODES        = 8;  // total characters incl. delimiters
const uint8_t  MAX_DATA_CHARS   = 6;  // chars between * and *
const uint16_t OUTER_BLACK_TH   = 600; // tune: outer sensor "black" threshold
const uint16_t CENTER_LINE_TH   = 200; // tune: centre sensors "on line" threshold
const uint16_t OFF_END_LIMIT    = 80;  // iterations off line before "Off End"
const int16_t  BASE_SPEED       = 60;  // forward speed (tune)

// ---------- Error codes ----------
enum ErrorType
{
  NO_ERROR,
  ERR_BAD_CODE,
  ERR_TOO_LONG,
  ERR_OFF_END
};

// ---------- Function prototypes ----------
void introScreen();
void readyScreen();
void waitForButtonB();
void calibrateSensors();
void lineFollowStep(uint16_t sensorValues[5]);
bool outerIsBlack(const uint16_t sensorValues[5]);
bool isOffEnd(const uint16_t sensorValues[5]);

char patternToChar(const char pattern[9]);
ErrorType readBarcode(char decoded[MAX_DATA_CHARS + 1]);

// =========================================================
//  UI helpers
// =========================================================

void introScreen()
{
  display.clear();
  display.gotoXY(0, 0);
  display.print(F("EEE243 Lab 4"));
  display.gotoXY(0, 1);
  display.print(F("Barcode Reader"));
  display.gotoXY(0, 3);
  display.print(F("Press B..."));
}

void readyScreen()
{
  display.clear();
  display.gotoXY(0, 0);
  display.print(F("Ready"));
  display.gotoXY(0, 1);
  display.print(F("Place on line"));
  display.gotoXY(0, 2);
  display.print(F("Press B to go"));
}

void waitForButtonB()
{
  buttonB.waitForButton();
}

// =========================================================
//  Sensors & motion
// =========================================================

// True if ALL sensors see white (robot is off the line)
bool checkLost(uint16_t sensorReadings[5])
{
  int sensorLost = 0;
  for (int x = 0; x < 5; x++)
  {
    if (sensorReadings[x] == 0)
    {
      sensorLost++;
    }
  }
  return (sensorLost == 5);
}


// Calibrates line sensors using Lab 3 logic (Lee & Flood)
void calibrateSensors()
{
  // Delay so we let go of the robot before calibrating
  delay(1000);
  display.clear();

  display.gotoXY(1, 3);
  display.print("Calibrating Sensors");

  // Turn left to get onto white floor
  Motors::setSpeeds(-100, 100);
  delay(185);

  // Move forward on white tiles (lightest colour)
  Motors::setSpeeds(100, 100);
  delay(250);
  Motors::setSpeeds(0, 0);
  lineSensors.calibrate();

  // Go back to starting position
  Motors::setSpeeds(-100, -100);
  delay(250);
  Motors::setSpeeds(0, 0);

  // Spin in place to calibrate on dark line
  Motors::setSpeeds(-100, 100);
  for (int x = 0; x < 15; x++)
  {
    lineSensors.calibrate();
    delay(46);   // tuned in lab 3
  }

  // Extra delay to re-centre
  delay(26);
  Motors::setSpeeds(0, 0);

  display.clear();
}


// Line following using Lab 3 logic: compare sensor 1 vs 3
void lineFollowStep(uint16_t sensorReadings[5])
{
  // sensorReadings[] MUST already be filled by readCalibrated()
  // Turn left if too far right
  if (sensorReadings[1] > sensorReadings[3])
  {
    Motors::setSpeeds(25, 70);
  }
  // Turn right if too far left
  else if (sensorReadings[1] < sensorReadings[3])
  {
    Motors::setSpeeds(70, 25);
  }
  // Go straight if centered
  else
  {
    Motors::setSpeeds(50, 50);
  }
}


// outer sensors see black?
bool outerIsBlack(uint16_t sensorValues[5])
{
  // Treat outer sensors as "on black" if they are fully dark
  return (sensorValues[0] == 1000) || (sensorValues[4] == 1000);
}

//TODO: redudnant, replace with checkLost

// detect "Off End": we lost the centre line for a while
// "Off End" when we are completely off the line (all sensors white)
bool isOffEnd(uint16_t sensorValues[5])
{
  return checkLost(sensorValues);
}


// =========================================================
//  Code39 helpers
// =========================================================

// convert N/W pattern (length 9) to Code39 char using code39.h
char patternToChar(const char pattern[9])
{
  for (int row = 0; row < 44; row++)
  {
    bool match = true;
    for (int i = 0; i < 9; i++)
    {
      if (code39[row][i + 1] != pattern[i])
      {
        match = false;
        break;
      }
    }
    if (match)
    {
      return code39[row][0];
    }
  }
  return 0; // no match
}

// =========================================================
//  Read entire barcode
// =========================================================
//
// Strategy:
//  - Follow centre line at low speed.
//  - Use outer sensors to detect black/white transitions.
//  - For each character:
//      * Measure widths of 9 elements via encoders.
//      * Use min+max widths to set N/W threshold.
//      * Ensure exactly 3 wide elements.
//      * Map pattern -> char using code39.h.
//      * Handle *, length limits, and errors.
//  - Detect Off End if centre line disappears too long.
//
ErrorType readBarcode(char decoded[MAX_DATA_CHARS + 1])
{
  decoded[0] = '\0';
  uint8_t dataLen        = 0;   // # data characters (no delimiters)
  uint8_t codeCount      = 0;   // # character codes including delimiters
  bool    gotStartStar   = false;

  uint16_t sensors[5];

  // 1) Drive forward until first black element on outer sensors
  while (true)
  {
    lineSensors.readCalibrated(sensors);
    lineFollowStep(sensors);

    if (isOffEnd(sensors))
    {
      motors.setSpeeds(0, 0);
      return ERR_OFF_END;
    }

    if (outerIsBlack(sensors))
    {
      // start of first character
      buzzer.playNote(NOTE_C(4), 100, 10); // low note
      break;
    }
  }

  // 2) Main loop: read characters one by one
  while (true)
  {
    long widths[9];

    // current colour of element (black or white on outer sensors)
    lineSensors.readCalibrated(sensors);
    lineFollowStep(sensors);
    bool currentIsBlack = outerIsBlack(sensors);

    long lastEnc = encoders.getCountsLeft();

    // read 9 elements of this character
    for (uint8_t e = 0; e < 9; e++)
    {
      while (true)
      {
        lineSensors.readCalibrated(sensors);
        lineFollowStep(sensors);

        if (isOffEnd(sensors))
        {
          motors.setSpeeds(0, 0);
          return ERR_OFF_END;
        }

        bool newIsBlack = outerIsBlack(sensors);
        if (newIsBlack != currentIsBlack)
        {
          long nowEnc = encoders.getCountsLeft();
          long width  = nowEnc - lastEnc;
          if (width < 0) width = -width;

          widths[e] = width;
          lastEnc   = nowEnc;
          currentIsBlack = newIsBlack;
          break;
        }
      }
    }

    // convert widths[] -> N/W pattern using min/max threshold
    long minW = widths[0], maxW = widths[0], sum = 0;
    for (uint8_t i = 0; i < 9; i++)
    {
      if (widths[i] < minW) minW = widths[i];
      if (widths[i] > maxW) maxW = widths[i];
      sum += widths[i];
    }

    long threshold = (minW + maxW) / 2;
    char pattern[9];
    uint8_t wideCount = 0;

    for (uint8_t i = 0; i < 9; i++)
    {
      if (widths[i] > threshold)
      {
        pattern[i] = 'W';
        wideCount++;

        // high note for wide element
        buzzer.playNote(NOTE_A(5), 30, 10);
      }
      else
      {
        pattern[i] = 'N';
      }
    }

    // must have exactly 3 wide elements
    if (wideCount != 3)
    {
      motors.setSpeeds(0, 0);
      return ERR_BAD_CODE;
    }

    // map pattern -> character
    char c = patternToChar(pattern);
    if (c == 0)
    {
      motors.setSpeeds(0, 0);
      return ERR_BAD_CODE;
    }

    codeCount++;
    if (codeCount > MAX_CODES)
    {
      motors.setSpeeds(0, 0);
      return ERR_TOO_LONG;
    }

    // handle delimiter vs data
    if (c == '*')
    {
      if (!gotStartStar)
      {
        gotStartStar = true;   // first delimiter
      }
      else
      {
        // second delimiter: success
        motors.setSpeeds(0, 0);
        return NO_ERROR;
      }
    }
    else
    {
      // data character
      if (!gotStartStar)
      {
        motors.setSpeeds(0, 0);
        return ERR_BAD_CODE;   // data before first *
      }

      if (dataLen < MAX_DATA_CHARS)
      {
        decoded[dataLen++] = c;
        decoded[dataLen]   = '\0';
      }
      else
      {
        motors.setSpeeds(0, 0);
        return ERR_TOO_LONG;
      }
    }

    // If we haven't finished yet, move through the inter-character space
    if (codeCount >= MAX_CODES)
    {
      motors.setSpeeds(0, 0);
      return ERR_TOO_LONG;
    }

    // drive until next outer black -> start of next character
    while (true)
    {
      lineSensors.readCalibrated(sensors);
      lineFollowStep(sensors);

      if (isOffEnd(sensors))
      {
        motors.setSpeeds(0, 0);
        return ERR_OFF_END;
      }

      if (outerIsBlack(sensors))
      {
        buzzer.playNote(NOTE_C(4), 100, 10); // low note for new char
        break;
      }
    }
  }
}

// =========================================================
//  Arduino entry points
// =========================================================

void setup()
{
  display.init();
  display.setLayout21x8();   // text mode for OLED/LCD wrapper
  display.clear();

  // No initFiveSensors() needed on 3pi+ 32U4
  // lineSensors.initFiveSensors();

  encoders.getCountsLeft();
  encoders.getCountsRight();

  introScreen();
}


void loop()
{
  // Intro and first press
  introScreen();
  waitForButtonB();

  calibrateSensors();

  readyScreen();
  waitForButtonB();

  char decoded[MAX_DATA_CHARS + 1];
  ErrorType err = readBarcode(decoded);

  // show result
  display.clear();
  display.gotoXY(0, 0);
  display.print(decoded);   // string between delimiters (may be empty)

  display.gotoXY(0, 1);
  switch (err)
  {
    case NO_ERROR:     display.print(F("OK"));        break;
    case ERR_BAD_CODE: display.print(F("Bad Code"));  break;
    case ERR_TOO_LONG: display.print(F("Too Long"));  break;
    case ERR_OFF_END:  display.print(F("Off End"));   break;
  }

  motors.setSpeeds(0, 0);

  // stay here so result remains visible
  while (true) {}
}
