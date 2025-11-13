/*
 *Moves the Pololu 3pi+ robot across a barcode, translating it from code39 to a
 *string, and returning an error message if the barcode is faulty.
 *
 *Author: OCdt Flood & OCdt Lee
 *Version: 12-11-2025
 */


#include <Arduino.h>
#include <Pololu3piPlus32U4.h>
#include "code39.h"

using namespace Pololu3piPlus32U4;

//CONSTS AND STUFF
Buzzer buzzer;
ButtonB buttonB;
OLED display;
Motors motors;
LineSensors lineSensors;
Encoders encoders;


const uint8_t MAX_CODES = 8; //max amount of chars including delimiters
const uint8_t MAX_DATA_CHARS = 6; //max amount of chars excluding delimiters

//Off End check on center sensors (calibrated values)
const uint16_t CENTER_WHITE_LIMIT = 100; //if center sensors < this => white
//Outer “black” threshold (uncalibrated brightness varies; use a modest bar)
const uint16_t BLACK_EDGE_MIN = 300; //both outers > NOIR => treat as BLACK

//Start-delimiter normalization
const float WIDE_FACTOR = 1.8f; //threshold = WIDE_FACTOR * lengthNarrow

//Follower speeds (slow & steady during scanning)
const int16_t FWD_L_SLOW = 35;
const int16_t FWD_R_SLOW = 35;
const int16_t TURN_L_SLOW = 20;
const int16_t TURN_R_SLOW = 45;

//Inter-character gap stuff
const uint8_t WHITE_DWELL_MS = 7; //dwell in white before next char
const uint8_t EDGE_DEBOUNCE_MS = 3; //confirm edge
const long MIN_TICKS = 5; //ignore microscopic encoder blips

// Error codes
enum ErrorType { NO_ERROR, ERR_BAD_CODE, ERR_TOO_LONG, ERR_OFF_END };

//UI SECTION

/*
 *Displays intro screen (pre-calibration)
 */
void introScreen() {
  display.clear();
  display.gotoXY(3, 0);
  display.print("Michael Flood");
  display.gotoXY(5, 1);
  display.print("Jeong Lee");
  display.gotoXY(8, 4);
  display.print("Lab 4");
  display.gotoXY(0, 5);
  display.print("When Barcodes Attack!");
  display.gotoXY(1, 7);
  display.print("Press B to start");
}

/*
 *Displays screen after calibration
 */
void readyScreen() {
  display.clear();
  display.gotoXY(7, 0);
  display.print(F("Ready"));
  display.gotoXY(3, 2);
  display.print(F("Place on line"));
  display.gotoXY(2, 7);
  display.print(F("Press B to read"));
}

//HELPER FUNCTIONS

/*
 *Checks if center 3 sensors are detecting white for Off-End error
 *s: sensor readings array
 *returns: true if detecting white, false otherwise
 *
 */
bool lostLineCenter(uint16_t s[5]) {
  return (s[1] < CENTER_WHITE_LIMIT) && (s[2] < CENTER_WHITE_LIMIT) && (
           s[3] < CENTER_WHITE_LIMIT);
}

/*
 *Follow code for guide line
 *s: sensor readings array
 */
void followSlow(uint16_t s[5]) {
  if (s[1] > s[3]) motors.setSpeeds(TURN_L_SLOW, TURN_R_SLOW);
  else if (s[1] < s[3]) motors.setSpeeds(TURN_R_SLOW, TURN_L_SLOW);
  else motors.setSpeeds(FWD_L_SLOW, FWD_R_SLOW);
}

/*
 *Check if outer sensors are detecting black
 *s: sensor readings
returns: true if detecting black, false if not
 */
bool outerSensorsOnLine(uint16_t s[5]) {
  return (s[0] > BLACK_EDGE_MIN) && (s[4] > BLACK_EDGE_MIN);
}


/*
 *Moves robot until the start of the barcode
 *returns: true if found the barcode start, false if it gets lost
 */
bool waitForFirstBlack() {
  uint16_t s[5];
  while (true) {
    lineSensors.readCalibrated(s);
    if (lostLineCenter(s)) return false;
    followSlow(s);
    if (outerSensorsOnLine(s)) return true;
  }
}

/*
 *Keeps reading a bar until the color swaps from white <-> black (so we know we
 *reached the end of it)
 *startColor: color of the start of the bar; 0=black, 1=white
 *returns: true once color has swapped, false if off the guide line
 */
bool waitEdgeTransition(int &startColor) {
  uint16_t s[5];
  while (true) {
    lineSensors.readCalibrated(s);
    if (lostLineCenter(s)) return false;
    followSlow(s);
    int nowColor = outerSensorsOnLine(s) ? 0 : 1;
    if (nowColor != startColor) {
      // debounce/confirm
      delay(EDGE_DEBOUNCE_MS);
      lineSensors.readCalibrated(s);
      int confirm = outerSensorsOnLine(s) ? 0 : 1;
      if (confirm != startColor) {
        startColor = confirm;
        return true;
      }
    }
  }
}

/*
 *Converts an array of W (wide) and N (narrow) to a character from code39
 *seq: array of W/N
 *returns: translated character from code39 |OR| '\0' if a character can't be
 *translated
 */
char findChar(const char seq[9]) {
  for (int r = 0; r < 44; ++r) {
    bool match = true;
    for (int j = 0; j < 9; ++j) {
      if (seq[j] != code39[r][j + 1]) {
        match = false;
        break;
      }
    }
    if (match) return code39[r][0];
  }
  return '\0'; // no match
}

/*
 *Calibrates the robot sensors so that the readCalibrated() function will work
 *properly.
 */
void calibrateSensors() {
  display.clear();
  display.gotoXY(4, 4);
  display.print("Calibrating");
  delay(300);
  lineSensors.calibrate();
  motors.setSpeeds(-100, 100);
  delay(185);
  motors.setSpeeds(0, 0);
  lineSensors.calibrate();
  delay(1000);
  motors.setSpeeds(100, -100);
  delay(82);
  motors.setSpeeds(0, 0);
  lineSensors.calibrate();
  delay(1000);
  motors.setSpeeds(100, -100);
  delay(61);
  motors.setSpeeds(0, 0);
  lineSensors.calibrate();
  delay(1000);
  motors.setSpeeds(100, -100);
  delay(72);
  motors.setSpeeds(0, 0);
  lineSensors.calibrate();
  delay(1000);
  motors.setSpeeds(100, -100);
  delay(71);
  motors.setSpeeds(0, 0);
  lineSensors.calibrate();
  delay(1000);
  motors.setSpeeds(-100, 100);
  delay(95);
  motors.setSpeeds(0, 0);
  display.clear();
  delay(200);
}

/*
 *Normalizes length of a narrow bar using the delimiter character
 *lengthNarrowOut: average length of narrow bars in the delimiter
 *returns: true if delimiter has been fully scanned, false if off guide line or
 * doesn't detect a color swap of bars
 */
bool measureNarrowFromStar(float &lengthNarrowOut) {
  // Ensure we're at first BLACK
  if (!waitForFirstBlack()) return false;

  // Determine starting color on outers (0=BLACK,1=WHITE)
  uint16_t s[5];
  lineSensors.readCalibrated(s);
  int color = outerSensorsOnLine(s) ? 0 : 1;

  // Reset encoder to measure the first segment length
  encoders.getCountsAndResetLeft();

  // Find '*' row once:
  int starRow = -1;
  for (int r = 0; r < 44; r++) {
    if (code39[r][0] == '*') {
      starRow = r;
      break;
    }
  }
  if (starRow < 0) {
    lengthNarrowOut = 10.f;
    return true;
  } // fallback

  float totalNarrow = 0.f;
  int narrowCnt = 0;

  for (int i = 0; i < 9; i++) {
    // wait for next color change
    if (!waitEdgeTransition(color)) return false;

    // width of just-finished segment
    long ticks = encoders.getCountsAndResetLeft();
    if (ticks < 0) ticks = -ticks;

    // Is this element narrow or wide in the star pattern?
    char expected = code39[starRow][i + 1]; // columns 1..9 hold N/W
    if (expected == 'N') {
      totalNarrow += (float) ticks;
      narrowCnt++;
    } else { buzzer.playNote(NOTE_A(5), 30, 10); } // wide = high note (Req 4b)
  }

  lengthNarrowOut = (narrowCnt > 0) ? (totalNarrow / (float) narrowCnt) : 10.f;
  return true;
}


/*
 *Scans one character (after 1st delimiter)
 *letter: translated char after resulting read
 *threshold: threshold length for wide elements
 *returns: 0 if all ok, 1 if bad code error (3-wide), 2 if bad code (no matching
 *char)
 */
int scanOne(char &letter, float threshold) {
  uint16_t s[5];
  // Current color on outers
  lineSensors.readCalibrated(s);
  int color = outerSensorsOnLine(s) ? 0 : 1;

  // We will collect 9 elements; encoder resets at each edge
  encoders.getCountsAndResetLeft();

  char pattern[9];
  int wideCount = 0;

  for (int i = 0; i < 9; i++) {
    // Wait until color toggles (edge)
    if (!waitEdgeTransition(color)) return 0; // OffEnd handled by caller

    long ticks = encoders.getCountsAndResetLeft();
    if (ticks < 0) ticks = -ticks;
    if (ticks < MIN_TICKS) {
      i--;
      continue;
    } // ignore flicker

    if ((float) ticks > threshold) {
      pattern[i] = 'W';
      wideCount++;
      buzzer.playNote(NOTE_A(5), 30, 10);
    } else {
      pattern[i] = 'N';
    }
  }

  letter = findChar(pattern);
  if (letter == '\0') return 2; // no match
  // 3-wide rule only for data (not for '*')
  if (letter != '*' && wideCount != 3) return 1;

  return 0; // OK
}

/*
 *Actually reads the entire barcode using above functions
 *decoded: array with the decoded string
 *returns: Enum ErrorType depending on if it encounters an error or not
 */
ErrorType readBarcode(char decoded[MAX_DATA_CHARS + 1]) {
  decoded[0] = '\0';
  uint8_t dataLen = 0;
  uint8_t codeCount = 0; // includes delimiters

  // 0) Normalize using the first '*'
  float narrowRefLen = 10.f;
  if (!measureNarrowFromStar(narrowRefLen)) {
    motors.setSpeeds(0, 0);
    return ERR_OFF_END;
  }
  float wideCutoff = WIDE_FACTOR * narrowRefLen;

  // Dwell helper: ensure white gap, then wait for next black
  auto waitWhiteThenBlack = [&]()-> bool {
    uint16_t s[5];
    // ensure white
    while (true) {
      lineSensors.readCalibrated(s);
      if (lostLineCenter(s)) return false;
      followSlow(s);
      if (!outerSensorsOnLine(s)) break;
    }
    delay(WHITE_DWELL_MS);
    // wait until next black
    return waitForFirstBlack();
  };

  // 1) Now scan characters until ending '*'
  while (true) {
    // Safety: Too long? (max 6 data + 1 end delimiter after the first star)
    if (codeCount >= 7) {
      motors.setSpeeds(0, 0);
      return ERR_TOO_LONG;
    }

    // Be sure we start each char cleanly
    if (!waitWhiteThenBlack()) {
      motors.setSpeeds(0, 0);
      return ERR_OFF_END;
    }

    // Low note = new character (Req 4a)
    buzzer.playNote(NOTE_C(4), 100, 10);

    char letter = '\0';
    int err = scanOne(letter, wideCutoff);
    if (err == 1 || err == 2) {
      motors.setSpeeds(0, 0);
      return ERR_BAD_CODE;
    }

    // End delimiter?
    if (letter == '*') {
      motors.setSpeeds(0, 0);
      return NO_ERROR;
    }

    // Append data
    if (dataLen < MAX_DATA_CHARS) {
      decoded[dataLen++] = letter;
      decoded[dataLen] = '\0';
    } else {
      motors.setSpeeds(0, 0);
      return ERR_TOO_LONG;
    }

    codeCount++;
  }
}

//ARDUINO STUFF
void setup() {
  display.init();
  display.setLayout21x8();
  display.clear();

  //reset encoders
  encoders.getCountsLeft();
  encoders.getCountsRight();

  introScreen();
}

void loop() {
  introScreen();
  buttonB.waitForButton();

  calibrateSensors();

  readyScreen();
  buttonB.waitForButton();

  char decoded[MAX_DATA_CHARS + 1];
  ErrorType err = readBarcode(decoded);

  // Show result
  display.clear();
  display.gotoXY(0, 0);
  display.print(decoded); // may be empty (e.g., "**")
  display.gotoXY(0, 1);
  switch (err) {
    case NO_ERROR: display.print(F("OK"));
      break;
    case ERR_BAD_CODE: display.print(F("Bad Code"));
      break;
    case ERR_TOO_LONG: display.print(F("Too Long"));
      break;
    case ERR_OFF_END: display.print(F("Off End"));
      break;
  }

  motors.setSpeeds(0, 0);
  while (true) {
  }
}
