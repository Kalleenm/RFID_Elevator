/*
  Arduino program for giving access to restricted area used in an elevator situation.
  When you svan your card/chip you will be granted access to the fourth floor.

  author Komolvae.

  Pin layout used:
  ----------------------------------------------------------------------------------------
                     MFRC522      Arduino                  Cable cat 5e
                     Reader/LCD   Yün                      Colour
  Signal             LEDS         Pin                      Management
  ----------------------------------------------------------------------------------------
  //LEDS
  5V Red Led         +            12                       Orange          2
  0V Red Led         -            GND + 330k ohm           Black
  5V Green Led       +            13                       Orange white    2
  0V Green Led       -            GND + 330k ohm           Black

  //LCD
  Ground             LCD 1        GND                      Black
  VDD(5V)            LCD 2        5V                       Red
  Cotrast Adjust     LCD 3        Midpin potetiometer
  Register Select    LCD 4        7                        Orange white    1
  Read/Write Select  LCD 5        GND                      Black
  Enable             LCD 6        6~                       Orange          1
  Data Lines D4      LCD 11       5~                       Brown white     1
  Data Lines D5      LCD 12       4                        Brown           1
  Data Lines D6      LCD 13       3~                       Green white     1
  Data Lines D7      LCD 14       2                        Green           1
  Backlight Power    LCD 15       5V                       Red
  Backlight Ground   LCD 16       GND                      Black

  5v                                                       Blue white      1
  0V                                                       Blue            1
  //RFID
  RFID 3,3V                                                Brown           2
  RST/Reset          RST          9                        Brown white     2
  SPI SDA            SDA(SS)      10                       Green white     2
  SPI MOSI           MOSI         ICSP-4                   Blue white      2
  SPI MISO           MISO         ICSP-1                   Blue            2
  SPI SCK            SCK          ICSP-3                   Green           2
*/

//RFID config.
#include <EEPROM.h>
#include <MFRC522.h>
#include <SPI.h>

#define RST_PIN       9    //Pin 9 is for the RC522 reset

//Each SS_x_PIN variable indicates the unique SS pin for another RFID reader
#define SS_1_PIN      10   //Pin 10 is the SS (SDA) of RC522 reader 1
#define SS_2_PIN      11   //Pin 11 is the SS (SDA) of RC522 reader 2

//Must have on for each SS_X_PIN for each reader
#define NR_OF_READERS 2

byte ssPins[] = {SS_1_PIN, SS_2_PIN};
int a = 0;
int b = 0;

MFRC522 mfrc522[NR_OF_READERS]; //Create MFRC522 instance.

//LCD config.
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// Constants representing the states in the state machine
const int S_SCAN_CARD = 0;
const int S_CARD_VERIFIED = 1;
const int S_CARD_DENIED = 2;
const int S_ADD_USER = 3;

//Output pins
const int RED_LED = 12;
const int GREEN_LED = 13;
const bool SIGNAL_PLS = 8;    //ModBus?????

// A variable holding the current state
int currentState = S_SCAN_CARD;

//Variables for RFID
boolean match = false; //card match set to false
boolean programMode = false;  // initialize programming mode to false
boolean replaceMaster = false;
uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader
byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM

unsigned long nextTimeout = 0;
int user_added = 0;
int add_ID_counter = 0;

void setup()
{
  Serial.begin(9600);
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)   // Init each MFRC522 card
  {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN);
  }

  // Initialize MFRC522 Hardware
  Serial.println("Approximate your card to the reader...\n");


  // Arduino Pin Comfiguration
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  //Make sure the LED's are off
  analogWrite(RED_LED, 0);
  analogWrite(GREEN_LED, 0);

  //Config of the LCD screen
  lcd.begin(16, 2);
}


byte ActualUID[4];                     //This will store the ID each time we read a new ID code

byte USER1[4] = {0x25, 0x8D, 0x1F, 0xE3} ; //Master ID code Change it for yor tag. First use the READ exampel and check your ID

byte USER2[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER2
byte USER3[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER3
byte USER4[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER4
byte USER5[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER5

void loop()
{
  // The state machine implemented using switch-case
  switch (currentState)
  {
    // Scan card state
    case S_SCAN_CARD:
      setLights(3);                                     //turn both leds off
      lcd.setCursor(0, 0);
      lcd.print("Scan card to be");
      lcd.setCursor(0, 1);
      lcd.print("granted acccess");
      getID();
      break;

    // Card Verified state
    case S_CARD_VERIFIED:
        setLights(2);                                 // Set to green only
        startTimer(5000);
        changeStateTo(S_SCAN_CARD);
      
      break;

    // Card denied state
    case S_CARD_DENIED:
      if (timerHasExpired())
      {
        setLights(1);                                  // Set red and yellow
        startTimer(1000);
        changeStateTo(S_SCAN_CARD);
      }
      break;

    // Add user state
    case S_ADD_USER:
      {
        lcd.setCursor(0, 0);
        lcd.print("");
        lcd.setCursor(0, 1);
        lcd.print("                ");
        changeStateTo(S_SCAN_CARD);
      }
      break;
  }
}


/**
   Sets the LEDS according to the phase provided.
   Phase 1: Red only
   Phase 2: Green only
   Phase 3: Red and green off

   @param phase The phase to use to set the pattern of the LED's accordingly
*/
void setLights (int phase)
{
  switch (phase)
  {
    //Red led on
    case 1:
      turnLedOn(RED_LED);
      turnLedOff(GREEN_LED);
      break;

    //Green led on
    case 2:
      turnLedOff(RED_LED);
      turnLedOn(GREEN_LED);
      break;

    //Both off
    case 3:
      turnLedOff(RED_LED);
      turnLedOff(GREEN_LED);
      break;

    default:
      // Show a pattern that is not part of the phases
      // like all on
      turnLedOn(RED_LED);
      turnLedOn(GREEN_LED);
      break;
  }
}

/**
   Prints the state to Serial Monitor as a text, based
   on the state-constant provided as the parameter state

   @param state The state to print the tekst-representation for
*/
void printState(int state)
{
  switch (state)
  {
    case S_SCAN_CARD:
      Serial.println("S_SCAN_CARD");
      break;

    case S_CARD_VERIFIED:
      Serial.println("S_CARD_VERIFIED");
      break;

    case S_CARD_DENIED:
      Serial.println("S_CARD_DENIED");
      break;

    case S_ADD_USER:
      Serial.println("S_ADD_USER");
      break;

    default:
      Serial.println("!!UNKNOWN!!");
      break;
  }
}

/**
   Change the state of the statemachine to the new state
   given by the parameter newState

   @param newState The new state to set the statemachine to
*/
void changeStateTo(int newState)
{
  // At this point, we now what the current state is (the value
  // of the global variable currentState), and we know what the
  // next state is going to be, given by the parameter newState.
  // By using the printState()-funksjon, we can now print the
  // full debug-message:
  Serial.print("State changed from ");
  printState(currentState);
  Serial.print(" to ");
  printState(newState);
  Serial.println(); // To add a new line feed

  // And finally, set the current state to the new state
  currentState = newState;
}

/**
   Checks if the timer has expired. If the timer has expired,
   true is returned. If the timer has not yet expired,
   false is returned.

   @return true if timer has expired, false if not
*/
boolean timerHasExpired()
{
  boolean hasExpired = false;
  if (millis() > nextTimeout)
  {
    hasExpired = true;
  }
  else
  {
    hasExpired = false;
  }
  return hasExpired;
}

/**
   Starts the timer and set the timer to expire after the
   number of milliseconds given by the parameter duration.

   @param duration The number of milliseconds until the timer expires.
*/
void startTimer(unsigned long duration)
{
  nextTimeout = millis() + duration;
}


/**
   Turns on the LED connceted to the pin given by the
   parameter pin
   @param pin The pin that the LED is connected to
*/
void turnLedOn(int pin)
{
  analogWrite(pin, 255);
}

/**
   Turns on the LED connceted to the pin given by the
   parameter pin
   @param pin The pin that the LED is connected to
*/
void turnLedOff(int pin)
{
  analogWrite(pin, 0);
}

////////////////////////////////////RFID METHODS//////////////////////////////////////////

///////////////////////////////////////// Get Tag's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading Tags
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    // Look for new cards


    if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
      Serial.print(F("Reader: "));
      Serial.println(reader + 1);
      // Show some details of the PICC (that is: the tag/card)
      Serial.print(F("Card UID: "));
      dump_byte_array(mfrc522[reader].uid.uidByte, mfrc522[reader].uid.size);
      Serial.println("");
      if (compareArray(ActualUID, USER1))
      {
        lcd.setCursor(0, 0);
        lcd.print(" Access granted ");
        lcd.setCursor(0, 1);
        lcd.print("  MASTER  USER  ");
        startTimer(3000);
        lcd.setCursor(0, 0);
        lcd.print("Put  MASTER card");
        lcd.setCursor(0, 1);
        lcd.print("for new ID     6");

      }
      else if (compareArray(ActualUID, USER2))
      {
        lcd.setCursor(0, 0);
        lcd.print(" Access granted ");
        lcd.setCursor(0, 1);
        lcd.print("     USER 2     ");
        startTimer(3000);
        lcd.setCursor(0, 0);
        lcd.print("Last use: USER2");
        lcd.setCursor(0, 1);
        lcd.print("  Door OPENED  ");
      }
      else if (compareArray(ActualUID, USER3))
      {
        lcd.setCursor(0, 0);
        lcd.print(" Access granted ");
        lcd.setCursor(0, 1);
        lcd.print("     USER 3     ");
        startTimer(3000);
        lcd.setCursor(0, 0);
        lcd.print("Last use: USER3");
        lcd.setCursor(0, 1);
        lcd.print("  Door OPENED  ");
      }
      else if (compareArray(ActualUID, USER4))
      {
        lcd.setCursor(0, 0);
        lcd.print(" Access granted ");
        lcd.setCursor(0, 1);
        lcd.print("     USER 4     ");
        startTimer(3000);
        lcd.setCursor(0, 0);
        lcd.print("Last use: USER4");
        lcd.setCursor(0, 1);
        lcd.print("  Door OPENED  ");
      }
      else if (compareArray(ActualUID, USER5))
      {
        lcd.setCursor(0, 0);
        lcd.print(" Access granted ");
        lcd.setCursor(0, 1);
        lcd.print("     USER 5     ");
        startTimer(3000);
        lcd.setCursor(0, 0);
        lcd.print("Last use: USER5");
        lcd.setCursor(0, 1);
        lcd.print("  Door OPENED  ");
      }
      else
      {
        lcd.setCursor(0, 0);
        lcd.print(" Access  denied ");
        lcd.setCursor(0, 1);
        lcd.print("   UNKNOWN ID   ");
        startTimer(3000);
        lcd.setCursor(0, 0);
        lcd.print("Place card here!");
        lcd.setCursor(0, 1);
        lcd.print("  Door CLOSED  ");
      }
    }


    // Halt PICC
    mfrc522[reader].PICC_HaltA();
    // Stop encryption on PCD
    mfrc522[reader].PCD_StopCrypto1();
  } //if (mfrc522[reader].PICC_IsNewC
} //for(uint8_t reader




//Compare the 4 bytes of the users and the received ID
boolean compareArray(byte array1[], byte array2[])
{
  if (array1[0] != array2[0])return (false);
  if (array1[1] != array2[1])return (false);
  if (array1[2] != array2[2])return (false);
  if (array1[3] != array2[3])return (false);
  return (true);
}

/**
  Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : "");
    Serial.print(buffer[i], HEX);
  }
}

void countdown()
{
if(add_ID_counter > 300)
      {
        add_ID_counter = 0;
        lcd.setCursor(0,0);
        lcd.print("New ID  canceled");
        lcd.setCursor(0,1);
        lcd.print("                ");
        startTimer(4000);
        lcd.setCursor(0,0);
        lcd.print("Place card here!");
        lcd.setCursor(0,1);
        lcd.print("                ");
      }

      if(add_ID_counter == 50)
      {
        lcd.setCursor(15,1);
        lcd.print("5");
      }

      if(add_ID_counter == 100)
      {
        lcd.setCursor(15,1);
        lcd.print("4");
      }

      if(add_ID_counter == 150)
      {
        lcd.setCursor(15,1);
        lcd.print("3");
      }

      if(add_ID_counter == 200)
      {
        lcd.setCursor(15,1);
        lcd.print("2");
      }

      if(add_ID_counter == 250)
      {
        lcd.setCursor(15,1);
        lcd.print("1");
      }

      
      add_ID_counter = add_ID_counter+1;
      startTimer(10);
    }
