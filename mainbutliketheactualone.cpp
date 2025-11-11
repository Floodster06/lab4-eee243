#include <Arduino.h>
#include <Pololu3piPlus32U4Buttons.h>
#include <Pololu3piPlus32U4.h>
#include "code39.h"

using namespace Pololu3piPlus32U4;
ButtonB buttonB;
OLED display;
LineSensors lineSensors;
bool start = false;
bool lost = false;

enum  Errors{
    ALL_GOOD,
    BAD_CODE,
    TOO_LONG,
    OFF_END
};

/*
 *Calibrates the robot sensors so that the readCalibrated() function will work
 *properly.
 */
void calibration() {
    //Delay is so that it starts after we stop touching the robot, making sure
    //that us touching it doesn't fuck up its positioning
    delay(1000);
    display.clear();
    //Prints required message
    display.gotoXY(1, 3);
    display.print("Calibrating Sensors");
    //Turn left to calibrate on white floor
    Motors::setSpeeds(-100, 100);
    delay(185);
    //Move onto white tiles to calibrate the lightest color detected
    Motors::setSpeeds(100, 100);
    delay(250);
    Motors::setSpeeds(0, 0);
    lineSensors.calibrate();
    //Go back to starting position
    Motors::setSpeeds(-100, -100);
    delay(250);
    Motors::setSpeeds(0, 0);
    //Turn in place a bunch to calibrate side sensors to dark part of line
    Motors::setSpeeds(-100, 100);
    for (int x = 0; x < 15; x++) {
        lineSensors.calibrate();
        //Delay value here is arbitrary
        delay(46);
    }
    //Extra delay to ensure robot is properly lined up. Number is specifically
    //tuned.
    delay(26);
    Motors::setSpeeds(0, 0);
    display.clear();
}

/*
 *Function used in testing to print out all sensor readings neatly
 *
 *sensorReadings: array of the sensor readings
 */
void printSensor(uint16_t sensorReadings[5]) {
    display.clear();
    for (int x = 0; x < 5; x++) {
        display.print(sensorReadings[x]);
        display.gotoXY(0, x + 1);
    }
}

/*
 *Checks if the robot is lost. Being lost means that the black line isn't sensed
 *
 *sensorReadings: integer array of current readings of the robots' sensors
 *returns: boolean stating if the robot is lost (true) or not (false)
 */
bool checkLost(uint16_t sensorReadings[5]) {
    int sensorLost = 0;
    for (int x = 0; x < 5; x++) {
        if (!(sensorReadings[x])) {
            sensorLost++;
        }
    }
    if (sensorLost == 5) {
        return true;
    }
    return false;
}

/*
 *Makes the robot follow a black line on the floor.
 */
void follow(uint16_t sensorReadings[5]) {

    //Turn left if too far right
    if (sensorReadings[1] > sensorReadings[3]) {
        //used to be 25:70
        Motors::setSpeeds(25, 35);
        //Turn right if too far left
    } else if (sensorReadings[1] < sensorReadings[3]) {
        //used to be 70:25
        Motors::setSpeeds(35, 25);
        //Go straight if no issues
    } else {
        //used to be 50:50
        Motors::setSpeeds(25, 25);
    }

}

/*
 *Pause all further code, until buttonB has been pressed. Once
 *pressed, return true
 *
 *returns: true upon button press
 */
bool waitBPress() {
    bool pressed = false;
    do {
        if (buttonB.getSingleDebouncedRelease()) {
            pressed = true;
            delay(10);
        }
    } while (pressed == 0);
    return pressed;
}


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

//TODO: move all struct stuff to the top of these functions
typedef struct BarCharacter {
    int bars[9];
    struct BarCharacter *next;
} BarCharacter;

/*
 *Create new linked list node for barcode characters
 *
 *bar[9]:
 *returns:memory address of the newly created node
 */
BarCharacter *newBarCharacter(int bar[9]) {
    //Allocate memory for the struct
    BarCharacter *pointer = (BarCharacter *) malloc(sizeof(BarCharacter));
    if (pointer == nullptr) {
        exit(1);
    }
    //If successfully allocated, assign values to the array and next pointer
    for (int x = 0; x < 9; x++) {
        pointer->bars[x] = bar[x];
    }
    pointer->next = nullptr;
    //Return the location of the newly created struct
    return pointer;
}

/*
 *Insert new node at the end of the linked list for barcode characters
 *
 ***head:
 **newBarCharacter:
 */
void appendBarCharacter(BarCharacter **head, BarCharacter *newBarCharacter) {
    if (*head == nullptr) {
        *head = newBarCharacter;
    }
    BarCharacter *current = *head;
    while (current->next != nullptr) {
        current = current->next;
    }
    newBarCharacter->next = current->next;
    current->next = newBarCharacter;
}




/*
 *
 *true == read black, false == read white
 */
bool outerReadBlackOrWhite(const uint16_t sensorReadings[5]) {
    return sensorReadings[0] == 1000 && sensorReadings[4] == 1000;
}


/*
 *Reads a barcode, then displays the read characters (excluding delimiters) on
 *the display.
 *
 *sensorReadings: integer array storing the robots' sensor readings
 */

//TODO: basically, this is following a guideline using the follow() function and
//the outer two sensors are reading the barcode. I need to still:
//account for the narrow white space in between each character
//get the reader to read properly
//compare characters to the delimiter character once i can read properly
//  this is so I can stop the robot when necessary
//  also reprogram the function so that when losing the guide line, it stops and
//      doesnt do the "I'm lost" thing in the checkLost() function
//create functions to translate characters to W and N (wide and narrow)

Errors readBarcode(uint16_t sensorReadings[5]) {
    //initializing required variables
    //barcodeTranslated holds the translated barcode, start checks if the
    //starting delimiter has been gotten, chars read stores how many characters
    //have been read so far
    char barcodeTranslated[9];
    bool start = false;
    bool swap = false;
    int charsRead =0;
    //initializing display
    display.clear();
    display.gotoXY(8, 4);
    display.print("Ready");
    display.gotoXY(7, 5);
    display.print("Press B");
    

    //only run once B is pressed
    if (waitBPress()) {
        
        //Move until the start of the barcode
        while(true){
            //Move forward
            lineSensors.readCalibrated(sensorReadings);
            follow(sensorReadings);
            //If encountered the start of the barcode
            if (outerReadBlackOrWhite(sensorReadings)){
                //Reset Encoders
                Encoders::getCountsAndResetLeft();
                Encoders::getCountsAndResetRight();
                //Play low note (start of new character)
                Buzzer::playNote(NOTE_F(3),100,10);
                //Move to the loop reading the barcode
                break;
            }
        }

        //Reading the barcode | Each iteration is one character being read
        while (true){
            //Temp array to read 1 character
            int barcodeReading[9];
            //Move forward
            //TODO: these two lines may be redundant, since they're in the other
            //while loop
            lineSensors.readCalibrated(sensorReadings);
            follow(sensorReadings);
            //Store current color of bar we are reading (to compare against later)
            bool pastColor = outerReadBlackOrWhite(sensorReadings);
            //For loop reads one character
            for(int x=0;x<9;x++){
                //While loop reads one bar of a character
                while (true) {
                    //Move forward
                    lineSensors.readCalibrated(sensorReadings);
                    follow(sensorReadings);
                    bool currentColor = outerReadBlackOrWhite(sensorReadings);
                    //If a new color is detected, we know that the bar has ended
                    if (currentColor != pastColor) {
                        //Save length of the bar
                        barcodeReading[x] = abs(Encoders::getCountsAndResetLeft());
                    }
                    //Update the color of the bar we are reading
                    pastColor = currentColor;
                    break;
                }
            }

            //Translate character to Wide and Narrows

            //Find biggest and smallest numbers, average to get a midpoint for
            //translation
            int widest=barcodeReading[0], thinnest = barcodeReading[0];
            for (int x = 0; x < 9; x++) {
                if (barcodeReading[x] > widest) {
                    widest = barcodeReading[x];
                }
                if (barcodeReading[x] < thinnest) {
                    thinnest = barcodeReading[x];
                }
            }

            //Translate to W/N based off midpoint
            int midPoint = (widest+thinnest)/2;
            char translatedWN[9];
            for (int x = 0; x < 9; x++) {
                if (barcodeReading[x] > midPoint) {
                    translatedWN[x] = 'W';
                }
                if (barcodeReading[x] < midPoint) {
                    translatedWN[x] = 'N';
                }
            }

            //Error 1: Bad Code (More/less than 3 wide elements)
            int wideCounter =0;
            for (int x = 0; x < 9; x++) {
                if (translatedWN[x] == 'W') {
                    wideCounter++;
                }
            }
            if (wideCounter!=3) {
                return BAD_CODE;
            }

            //Translate to an actual character
            char translatedChar = patternToChar(translatedWN);
            //Error 2: Read more than 8 characters and hasn't found a delimiter
            charsRead++;
            if (charsRead > 8) {
                return TOO_LONG;
            }
            //No match case for the character in code39
            if (translatedChar==0) {
                Motors::setSpeeds(0, 0);
                return BAD_CODE;
            }

            //Handle delimiters
            if (translatedChar == '*') {
                //1st delimiter
                if (!start) {
                    start=true;

                }
                //2nd delimiter
                Motors::setSpeeds(0, 0);
                return ALL_GOOD;
            }
            //Put translated character into the right array
            barcodeTranslated[charsRead]=translatedChar;

            //Move to next character start or off the line
            while (true) {
                lineSensors.readCalibrated(sensorReadings);
                follow(sensorReadings);
                //Error 3: End of center line detected
                if (checkLost(sensorReadings)) {
                    Motors::setSpeeds(0, 0);
                    return OFF_END;
                }
                if (outerReadBlackOrWhite(sensorReadings)) {
                    break;
                }
            }
        }
    }
}


void introScreen() {
    display.gotoXY(3, 0);
    display.print("Michael Flood");
    display.gotoXY(5, 1);
    display.print("Jeong Lee");
    display.gotoXY(8, 4);
    display.print("Lab 4");
    display.gotoXY(0, 5);
    display.print("When Barcodes Attack!");
    display.gotoXY(1, 7);
    display.print("To start, press B");

    if (buttonB.isPressed()) {
        start = true;
        lost = false;
        calibration();
    }
}

void setup() {
    display.init();
    display.setLayout21x8();
}

void loop() {
    uint16_t sensorReadings[5];
    if (start == true && !lost) {
        readBarcode(sensorReadings);
    } else {
        introScreen();
    }
}
