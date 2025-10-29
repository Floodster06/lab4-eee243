#include <Arduino.h>
#include <Pololu3piPlus32U4Buttons.h>
#include <Pololu3piPlus32U4.h>

using namespace Pololu3piPlus32U4;
ButtonB buttonB;
OLED display;
LineSensors lineSensors;
bool start = false;
bool lost = false;

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
void follow() {
    uint16_t sensorReadings[5];

    lineSensors.readCalibrated(sensorReadings);

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

    /*
    //Turn around and play a note if hit a T-intersection
    if (checkT(sensorReadings)) {
        Buzzer::playNote(NOTE_A(4), 1000, 10);
        Motors::setSpeeds(-100, 100);
        delay(370);
        Motors::setSpeeds(0, 0);
    }
    */

    //Stop in place and play a note if lost
    if (checkLost(sensorReadings)) {
        lost = true;
        Motors::setSpeeds(0, 0);
        display.clear();
        display.gotoXY(7, 3);
        display.print("I'm Lost!");
        Buzzer::playNote(NOTE_F(4), 1000, 10);
        delay(5000);
        display.clear();
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
 *1 == read black, 2 == read white
 */
int outerReadBlackOrWhite(uint16_t sensorReadings[5]) {
    //TODO: make sure that I don't have to change the if condition to >= a value
    if (sensorReadings[0] == 1000 && sensorReadings[4] == 1000) {
        return 1;
    }
    return 2;
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

void readBarcode(uint16_t sensorReadings[5]) {
    //initializing required variables
    //TODO:Changer this back to a float later
    int barcodeReadings[8][9];
    bool start = false;
    bool swap = false;
    int indexInner = 0;
    int indexOuter = 0;
    //initializing display
    display.clear();
    display.gotoXY(8, 4);
    display.print("Ready");
    display.gotoXY(7, 5);
    display.print("Press B");
    //only run once B is pressed
    if (waitBPress()) {
        while (!lost) {
            follow();
            //reset inner index
            if (indexInner == 8) {
                indexInner = 0;
                indexOuter++;
            }

            //Start reading barcode and reset encoders
            if (!start && (outerReadBlackOrWhite(sensorReadings) == 1)) {
                start = true;
                Encoders::getCountsAndResetLeft();
                Encoders::getCountsAndResetRight();
            }

            //on color swap, store length
            if (swap && start) {
                barcodeReadings[indexOuter][indexInner] =
                ( Encoders::getCountsAndResetLeft() +
                  Encoders::getCountsAndResetRight()) / 2;
                swap = false;
                indexInner++;
            }

            //detect color swap
            if (((indexInner % 2 == 0 && outerReadBlackOrWhite(sensorReadings) ==
                1)
                || (indexInner % 2 == 1 && outerReadBlackOrWhite(sensorReadings)
                == 2))&&start) {
                swap = true;
            }


        }
        display.clear();
        display.gotoXY(0, 0);
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 9; y++) {
                display.gotoXY(x,y);
                display.print(barcodeReadings[x][y]);
            }
        }
        delay(10000);
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
