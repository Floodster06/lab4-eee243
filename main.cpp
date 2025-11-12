#include <Arduino.h>
#include <Pololu3piPlus32U4.h>
#include "code39.h"

using namespace Pololu3piPlus32U4;

// ================== Hardware ==================
Buzzer buzzer;
ButtonB buttonB;
OLED display;
Motors motors;
LineSensors lineSensors;
Encoders encoders;

// ================== Constants ==================
// General limits
const uint8_t MAX_CODES = 8; // total characters incl. delimiters
const uint8_t MAX_DATA_CHARS = 6; // characters between * and *

// “Line lost” (Off End) check on center sensors (calibrated values)
const uint16_t CENTER_WHITE_LIMIT = 100; // center sensors < this => white
// Outer “black” threshold (uncalibrated brightness varies; use a modest bar)
const uint16_t BLACK_EDGE_MIN = 300; // both outers > NOIR => treat as BLACK

// Start-delimiter normalization
const float WIDE_FACTOR = 1.8f; // threshold = WIDE_FACTOR * lengthNarrow

// Follower speeds (slow & steady during scanning)
const int16_t FWD_L_SLOW = 35;
const int16_t FWD_R_SLOW = 35;
const int16_t TURN_L_SLOW = 20;
const int16_t TURN_R_SLOW = 45;

// Inter-character gap guard
const uint8_t WHITE_DWELL_MS = 7; // dwell in white before next char
const uint8_t EDGE_DEBOUNCE_MS = 3; // confirm edge
const long MIN_TICKS = 5; // ignore microscopic encoder blips

// Error codes
enum ErrorType { NO_ERROR, ERR_BAD_CODE, ERR_TOO_LONG, ERR_OFF_END };

// ================== UI ==================
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

void readyScreen() {
  display.clear();
  display.gotoXY(7, 0);
  display.print(F("Ready"));
  display.gotoXY(3, 2);
  display.print(F("Place on line"));
  display.gotoXY(2, 7);
  display.print(F("Press B to read"));
}

// ================== Helpers ==================
inline void zeroEncoders() {
  encoders.getCountsLeft();
  encoders.getCountsRight();
}

inline long leftReset() {
  return encoders.getCountsAndResetLeft();
}

// quick center-3 white check => Off End
inline bool lostLineCenter(uint16_t s[5]) {
  return (s[1] < CENTER_WHITE_LIMIT) && (s[2] < CENTER_WHITE_LIMIT) && (
           s[3] < CENTER_WHITE_LIMIT);
}

// simple slow follower around the center line (1 vs 3)
inline void followSlow(uint16_t s[5]) {
  if (s[1] > s[3]) motors.setSpeeds(TURN_L_SLOW, TURN_R_SLOW);
  else if (s[1] < s[3]) motors.setSpeeds(TURN_R_SLOW, TURN_L_SLOW);
  else motors.setSpeeds(FWD_L_SLOW, FWD_R_SLOW);
}

// “both outers black?” using modest threshold (robust on lighter prints)
inline bool outerSensorsOnLine(uint16_t s[5]) {
  return (s[0] > BLACK_EDGE_MIN) && (s[4] > BLACK_EDGE_MIN);
}

// Wait until both outers are black; follow as we wait.
// Returns false if Off End occurs on the way.
bool waitForFirstBlack() {
  uint16_t s[5];
  while (true) {
    lineSensors.readCalibrated(s);
    if (lostLineCenter(s)) return false;
    followSlow(s);
    if (outerSensorsOnLine(s)) return true;
  }
}

// Block until color toggles (WHITE->BLACK or BLACK->WHITE) on outers.
// startColor: 0=BLACK, 1=WHITE.
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

// ================== Code39 mapping ==================
char FindChar(const char seq[9]) {
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

// ================== Calibration (robust, encoder-driven) ==================
void calibrateSensors() {
  display.clear();
  display.gotoXY(4, 4);
  display.print("Calibrating");
  delay(300);

  // Start fresh
  lineSensors.resetCalibration();

  auto stop = []() {
    motors.setSpeeds(0, 0);
    delay(50);
  };

  auto sampleWhile = [&](int16_t ls, int16_t rs, long targetTicks, bool spin) {
    zeroEncoders();
    motors.setSpeeds(ls, rs);
    while (true) {
      lineSensors.calibrate();
      delay(10);
      long la = labs(encoders.getCountsLeft());
      long ra = labs(encoders.getCountsRight());
      if ((spin && (la >= targetTicks && ra >= targetTicks)) ||
          (!spin && (la >= targetTicks || ra >= targetTicks))) {
        break;
      }
    }
    stop();
  };

  // 1) Load “white” history: crawl on white forward & back
  sampleWhile(+80, +80, 200, false);
  sampleWhile(-80, -80, 200, false);

  // 2) Sweep over line in both directions to hit darkest blacks
  const long TURN_TICKS = 380; // tune 320–450 if needed
  sampleWhile(-80, +80, TURN_TICKS, true); // left
  sampleWhile(+80, -80, TURN_TICKS, true); // right
  // optional top-ups
  sampleWhile(-80, +80, TURN_TICKS / 2, true);
  sampleWhile(+80, -80, TURN_TICKS / 2, true);

  stop();
  display.clear();
  display.gotoXY(5, 4);
  display.print("Calib OK!");
  delay(200);
}

// ================== Start-delimiter normalization ==================
bool measureNarrowFromStar(float &lengthNarrowOut) {
  // Ensure we're at first BLACK
  if (!waitForFirstBlack()) return false;

  // Determine starting color on outers (0=BLACK,1=WHITE)
  uint16_t s[5];
  lineSensors.readCalibrated(s);
  int color = outerSensorsOnLine(s) ? 0 : 1;

  // Reset encoder to measure the first segment length
  leftReset();

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
    long ticks = leftReset();
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

// ================== Scan one character (after star) ==================
// Returns: 0=OK, 1=Bad Code (3-wide rule), 2=Bad Code (no match)
int scanOne(char &letter, float threshold) {
  uint16_t s[5];
  // Current color on outers
  lineSensors.readCalibrated(s);
  int color = outerSensorsOnLine(s) ? 0 : 1;

  // We will collect 9 elements; encoder resets at each edge
  leftReset();

  char pattern[9];
  int wideCount = 0;

  for (int i = 0; i < 9; i++) {
    // Wait until color toggles (edge)
    if (!waitEdgeTransition(color)) return 0; // OffEnd handled by caller

    long ticks = leftReset();
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

  letter = FindChar(pattern);
  if (letter == '\0') return 2; // no match
  // 3-wide rule only for data (not for '*')
  if (letter != '*' && wideCount != 3) return 1;

  return 0; // OK
}

// ================== Read entire barcode ==================
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

// ================== Arduino stuff ==================
void setup() {
  display.init();
  display.setLayout21x8();
  display.clear();

  // prime encoders
  zeroEncoders();

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
