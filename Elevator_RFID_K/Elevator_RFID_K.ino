/**
  Arduino program for giving access to restricted area used in an elevator situation.
  When you svan your card/chip you will be granted access to the fourth floor.

  author Komolvae.

  Pin layout used:
  +------------------------------------------------------------------------------------------+
  |                    MFRC522      Arduino                  Cable cat 5e                    |
  |                    Reader/LCD   Yün                      Colour                          |
  | Signal             LEDS         Pin                      Management                      |
  | /////////////////////////////////////// LCD  /////////////////////////////////////////// |
  | Ground             LCD 1        GND                      Black                           |
  | VDD(5V)            LCD 2        5V                       Red                             |
  | Cotrast Adjust     LCD 3        Midpin potetiometer                                      |
  | Register Select    LCD 4        7                        Orange white    1               |
  | Read/Write Select  LCD 5        GND                      Black                           |
  | Enable             LCD 6        6~                       Orange          1               |
  | Data Lines D4      LCD 11       5~                       Brown white     1               |
  | Data Lines D5      LCD 12       4                        Brown           1               |
  | Data Lines D6      LCD 13       3~                       Green white     1               |
  | Data Lines D7      LCD 14       2                        Green           1               |
  | Backlight Power    LCD 15       5V                       Red                             |
  | Backlight Ground   LCD 16       GND                      Black                           |
  |  /////////////////////////////////////// Power ///////////////////////////////////////// |
  | 5v                                                       Blue white      1               |
  | 0V                                                       Blue            1               |
  |                                                                                          |
  | //////////////////////////////////////// LEDS ////////////////////////////////////////// |
  | 5V Red Led         +            12                       Orange          2               |
  | 0V Red Led         -            GND + 330k ohm           Black                           |
  | 5V Green Led       +            13                       Orange white    2               |
  | 0V Green Led       -            GND + 330k ohm           Black                           |
  |                                                                                          |
  | /////////////////////////////////////// RFID /////////////////////////////////////////// |
  | RFID 3,3V                                                Brown           2               |
  | RST/Reset          RST          9                        Brown white     2               |
  | SPI SDA            SDA(SS)      10,11                    Green white     2               |
  | SPI MOSI           MOSI         ICSP-4                   Blue white      2               |
  | SPI MISO           MISO         ICSP-1                   Blue            2               |
  | SPI SCK            SCK          ICSP-3                   Green           2               |
  | //////////////////////////////////////////+///////////////////////////////////////////// |
  +------------------------------------------------------------------------------------------+

                   Simple Work Flow (not limited to) :
  +<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+
  |                       +-------------+                                |
  |         +-------------+  READ TAGS  +-------------+                  |
  |         |             +-------------+             |                  |
  |         |                                         |                  |
  |  +------V------+                            +-----V------+           |
  |  | MASTER TAG  |                            | OTHER TAGS |           |
  |  +------+------+                            +-----+------+           |
  |         |                                         |                  |
  |  +------V------+                          +-------V--------+         |
  +--|GRANT ACCESS |                          |                |         |
  |  +------+------+                   +------V------+  +------V------+  |
  |         |                          |  KNOW TAGS  |  |UNKNOWN TAGS |  |
  |  +------V------+                   +------+------+  +------+------+  |
  |  | MASTER TAG  |                          |                |         |
  |  +------+------+                   +------V------+  +------V------+  |
  |         |                          |GRANT ACCESS |  | DENY ACCESS |  |
  |  +------V------+                   +------+------+  +------+------+  |
  |  | UNKNOWN TAG |                          |                |         |
  |  +------+------+                          |                +--------->
  |         |                                 |                          |
  |  +------V------+                          +--------------------------+
  |  |   ADD TO    |
  |  |  USER LIST  |
  |  +-------------+
  |                  +------------+
  +------------------+    EXIT    |
                     +------------+
*/

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

#define RST_PIN         9         // Configurable, see typical pin layout above
#define SS_1_PIN        10        // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_2_PIN        11        // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1

#define NR_OF_READERS   2

byte ssPins[] = {SS_1_PIN, SS_2_PIN};

// Create MFRC522 instance.
MFRC522 mfrc522[NR_OF_READERS];

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

//Output pins
const int RED_LED = 12;
const int GREEN_LED = 13;
const bool PLS_SIGNAL = 8;

//Variables
boolean first_read = false;
boolean normal_mode = true;
boolean countdown = false;
int timer = 0;
int user_added = 0;

unsigned long nextTimeout = 0;
long interval = 1000;
long previousMillis = 0;
int ledState = LOW;
/**
   Initialize.
*/
void setup() {
  SPI.begin();        // Init SPI bus
  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial monitor is opened (added for Arduinos based on ATMEGA32U4)

  // Arduino Pin Comfiguration
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(PLS_SIGNAL, OUTPUT);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(PLS_SIGNAL, LOW);

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader + 1);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }

  //Config of the LCD screen
  lcd.begin(16, 2);
  idleLCDState();
}

byte ActualUID[4];                         //This will store the ID each time we read a new ID code

byte USER1[4] = {0x77, 0x5A, 0x63, 0x3C} ; //Master ID code Change it for yor tag. First use the READ exampel and check your ID

byte USER2[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER2
byte USER3[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER3
byte USER4[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER4
byte USER5[4] = {0x00, 0x00, 0x00, 0x00} ; //Empty ID of USER5


/**
   Main loop.
*/
void loop() {

  if (normal_mode)
  {

    if (countdown)
    {
      startTimer(8000);
      unsigned long startMillis = millis();
      Serial.println("First Millis");
      Serial.print(millis());


      if (millis() - startMillis <= interval)
      {
        lcd.setCursor(15, 1);
        lcd.print("5");
      } else if (millis() - startMillis <= 2*interval)
      {
        lcd.setCursor(15, 1);
        lcd.print("4");
      } else if (millis() - startMillis <= 3*interval)
      {
        lcd.setCursor(15, 1);
        lcd.print("3");
      } else if (millis() - startMillis <= 4*interval )
      {
        lcd.setCursor(15, 1);
      } else if (millis() - startMillis <= 5*interval)
      {
        lcd.setCursor(15, 1);
        lcd.print("1");
      } else
      {
        countdown = false;
        first_read = false;
        lcd.setCursor(0, 0);
        lcd.print("New ID canceled");
        lcd.setCursor(0, 1);
        lcd.print("                ");
        Serial.println("Last action: New user canceled");
        delay(2000);
        idleLCDState();
      }
    }

    for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
      // Check if there are any new ID card in front of the sensor
      if ( mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial())
      {
        // We store the read ID into 4 bytes with a for loop
        for (byte i = 0; i < mfrc522[reader].uid.size; i++) {
          ActualUID[i] = mfrc522[reader].uid.uidByte[i];
        }
        //Compare the UID and check if the new iD is on the user listz
        if (first_read)
        {
          if (compareArray(ActualUID, USER1))
          {
            countdown = false;
            nextTimeout = 0;
            normal_mode = false;
            lcd.setCursor(0, 0);
            lcd.print("Place New ID in:");
            lcd.setCursor(0, 1);
            lcd.print("       3        ");
            delay(1000);
            lcd.setCursor(0, 1);
            lcd.print("       2        ");
            delay(1000);
            lcd.setCursor(0, 1);
            lcd.print("       1       ");
            delay(1000);
            lcd.setCursor(0, 1);
            lcd.print("      NOW!      ");

          }
          else
          {
            first_read = false;

          }
        }

        if (!first_read)
        {
          if (compareArray(ActualUID, USER1))
          {
            lcd.setCursor(0, 0);
            lcd.print(" Access granted ");
            lcd.setCursor(0, 1);
            lcd.print("  MASTER  USER  ");
            setPhase (1);
            delay(2000);
            lcd.setCursor(0, 0);
            lcd.print("Scan MASTERCARD");
            lcd.setCursor(0, 1);
            lcd.print("to add new ID  6");
            Serial.println("Last use: MASTER CARD");
            countdown = true;
            first_read = true;
          }
          else if (compareArray(ActualUID, USER2))
          {
            printAccessGranted(2);
          }
          else if (compareArray(ActualUID, USER3))
          {
            printAccessGranted(3);
          }
          else if (compareArray(ActualUID, USER4))
          {
            printAccessGranted(4);
          }
          else if (compareArray(ActualUID, USER5))
          {
            printAccessGranted(5);
          }
          else
          {
            lcd.setCursor(0, 0);
            lcd.print(" Access  denied ");
            lcd.setCursor(0, 1);
            lcd.print("   UNKNOWN ID   ");
            first_read = false;
            setPhase (2);
            idleLCDState();
            Serial.println("Last use: UNKNOWN ID, Access DENIED");
          }
        }


      }
  }//end  normal mode

  if (!normal_mode)
  {
    // Revisamos si hay nuevas tarjetas  presentes
    for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
      if ( mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial())
      {
          // Enviamos serialemente su UID

          for (byte i = 0; i < mfrc522[reader].uid.size; i++) {
            ActualUID[i] = mfrc522[reader].uid.uidByte[i];
          }

          //Compare the read ID and the stored USERS
          if (user_added == 4) // FULL
          {
            lcd.setCursor(0, 0);
            lcd.print("  User list is  ");
            lcd.setCursor(0, 1);
            lcd.print("      FULL      ");
            idleLCDState();
            Serial.println("Last action: User list is full.");
          }

          if (user_added == 3) // 5
          {
            printAddedUser(5, USER5);
          }

          if (user_added == 2) // 4
          {
            printAddedUser(4, USER4);

          }

          if (user_added == 1) //3
          {
            printAddedUser(3, USER3);
          }

          if (user_added == 0) // 2
          {
            printAddedUser(2, USER2);
          }

        

      }
  }//end  ID add mode
}

///////////////////////////////////// FUNCTIONS ////////////////////////////////////////
/**
  Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

//////////////////////////////// Print Access to LCD  //////////////////////////////////
void printAccessGranted(int user)
{
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Access granted");
  lcd.setCursor(5, 1);
  lcd.print("USER ");
  lcd.print(user);
  first_read = true;
  setPhase (1);
  idleLCDState();
  Serial.print("Last use: USER");
  Serial.print(user);
  Serial.println("");
}

////////////////////////////// Print added user to LCD /////////////////////////////////
void printAddedUser(int user, byte USER[])
{
  USER[0] = ActualUID[0];
  USER[1] = ActualUID[1];
  USER[2] = ActualUID[2];
  USER[3] = ActualUID[3];
  user_added = user_added + 1;
  lcd.setCursor(0, 0);
  lcd.print("New user stored ");
  lcd.setCursor(0, 1);
  lcd.print("   as  USER ");
  lcd.print(user);
  normal_mode = true;
  first_read = false;
  delay(3000);
  idleLCDState();
  Serial.print("New user stored as USER ");
  Serial.print(user);
  Serial.println("");
}

//////////////// Compare the 4 bytes of the users and the received ID //////////////////
boolean compareArray(byte array1[], byte array2[])
{
  if (array1[0] != array2[0])return (false);
  if (array1[1] != array2[1])return (false);
  if (array1[2] != array2[2])return (false);
  if (array1[3] != array2[3])return (false);
  return (true);
}

//////////////////////////////// Print idle LCD state  /////////////////////////////////
void idleLCDState()
{
  lcd.setCursor(0, 0);
  lcd.print("Scan card to be");
  lcd.setCursor(0, 1);
  lcd.print("granted ACCESS!");
}

////////////////////////////////// Starts the timer ////////////////////////////////////
/**
  Starts the timer and set the timer to expire after the
  number of milliseconds given by the parameter duration.

  @param duration The number of milliseconds until the timer expires.
*/
void startTimer(unsigned long duration)
{
  nextTimeout = millis() + duration;
}

//////////////////////// Checks if the timer has expired ///////////////////////////////
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


void setPhase (int phase)
{
  switch (phase)
  {
    case 1: // ACCESS GRANTED
      digitalWrite(RED_LED, LOW);      // Make sure red LED is off
      digitalWrite(GREEN_LED, HIGH);   // Make sure green LED is on
      digitalWrite(PLS_SIGNAL, HIGH);  // Make sure PLS signal is high
      break;

    case 2: // ACCESS DENIED
      digitalWrite(RED_LED, HIGH);     // Make sure red LED is on
      digitalWrite(GREEN_LED, LOW);    // Make sure green LED is off
      digitalWrite(PLS_SIGNAL, LOW);   // Make sure PLS signal is low
      break;

    case 3: // NEW USER ADDED
      digitalWrite(RED_LED, LOW);      // Make sure red LED is on
      digitalWrite(GREEN_LED, HIGH);   // Make sure green LED is off
      digitalWrite(PLS_SIGNAL, LOW);   // Make sure PLS signal is low
      break;

    case 4: // NEW USER CANCELED
      digitalWrite(RED_LED, HIGH);     // Make sure red LED is on
      digitalWrite(GREEN_LED, LOW);    // Make sure green LED is off
      digitalWrite(PLS_SIGNAL, LOW);   // Make sure PLS signal is low
      break;

  }
}
