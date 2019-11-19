/**
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
  /////////////////////////////////////////////////////////////////////////////
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
#include <EEPROM.h>

#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_1_PIN        10         // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_2_PIN        11         // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1

#define NR_OF_READERS   2

byte ssPins[] = {SS_1_PIN, SS_2_PIN};

MFRC522 mfrc522[NR_OF_READERS];   // Create MFRC522 instance.

//LCD config.
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

bool programMode = false;

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader


//Output pins
const int RED_LED = 12;
const int GREEN_LED = 13;
const bool PLS_SIGNAL = 8;    //ModBus????


//Variables
boolean door_opened = false;
boolean first_read = false;
boolean normal_mode = true;
boolean countdown = false;
int timer = 0;
int user_added = 0;
int  add_ID_counter = 0;

unsigned long nextTimeout = 0;

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
  lcd.setCursor(0, 0);
  lcd.print("Scan card to be");
  lcd.setCursor(0, 1);
  lcd.print("granted ACCESS!");
}

byte ActualUID[4];                     //This will store the ID each time we read a new ID code

byte USER1[4] = {0x25, 0x8D, 0x1F, 0xE3} ; //Master ID code Change it for yor tag. First use the READ exampel and check your ID

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
      if (add_ID_counter > 300)
      {
        countdown = false;
        first_read = false;
        add_ID_counter = 0;
        lcd.setCursor(0, 0);
        lcd.print("New ID  canceled");
        lcd.setCursor(0, 1);
        lcd.print("                ");
        Serial.println("Last action: New user canceled");
        delay(3000);
        lcd.setCursor(0, 0);
        lcd.print("Scan card to be");
        lcd.setCursor(0, 1);
        lcd.print("granted ACCESS!");
      }

      if (add_ID_counter == 50)
      {
        lcd.setCursor(15, 1);
        lcd.print("5");
      }

      if (add_ID_counter == 100)
      {
        lcd.setCursor(15, 1);
        lcd.print("4");
      }

      if (add_ID_counter == 150)
      {
        lcd.setCursor(15, 1);
        lcd.print("3");
      }

      if (add_ID_counter == 200)
      {
        lcd.setCursor(15, 1);
        lcd.print("2");
      }

      if (add_ID_counter == 250)
      {
        lcd.setCursor(15, 1);
        lcd.print("1");
      }


      add_ID_counter = add_ID_counter + 1;
      delay(10);
    }


    for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
      // Check if there are any new ID card in front of the sensor
      if ( mfrc522[reader].PICC_IsNewCardPresent())
      {
        //Select the found card
        if ( mfrc522[reader].PICC_ReadCardSerial())
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
              add_ID_counter = 0;

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
              delay(5000);

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
              door_opened = true;
              first_read = true;
              countdown = true;
              granted();
              delay(5000);
              lcd.setCursor(0, 0);
              lcd.print("Put  MASTER card");
              lcd.setCursor(0, 1);
              lcd.print("for new ID     6");
              Serial.println("Last use: MASTER CARD");

            }
            else if (compareArray(ActualUID, USER2))
            {
              lcd.setCursor(0, 0);
              lcd.print(" Access granted ");
              lcd.setCursor(0, 1);
              lcd.print("     USER 2     ");
              door_opened = true;
              first_read = true;
              granted();
              delay(5000);
              lcd.setCursor(0, 0);
              lcd.print("Scan card to be");
              lcd.setCursor(0, 1);
              lcd.print("granted ACCESS!");
              Serial.println("Last use: USER2");

            }
            else if (compareArray(ActualUID, USER3))
            {
              lcd.setCursor(0, 0);
              lcd.print(" Access granted ");
              lcd.setCursor(0, 1);
              lcd.print("     USER 3     ");
              door_opened = true;
              first_read = true;
              granted();
              delay(5000);
              lcd.setCursor(0, 0);
              lcd.print("Scan card to be");
              lcd.setCursor(0, 1);
              lcd.print("granted ACCESS!");
              Serial.println("Last use: USER3");
            }
            else if (compareArray(ActualUID, USER4))
            {
              lcd.setCursor(0, 0);
              lcd.print(" Access granted ");
              lcd.setCursor(0, 1);
              lcd.print("     USER 4     ");
              door_opened = true;
              first_read = true;
              granted();
              delay(5000);
              lcd.setCursor(0, 0);
              lcd.print("Scan card to be");
              lcd.setCursor(0, 1);
              lcd.print("granted ACCESS!");
              Serial.println("Last use: USER4");
            }
            else if (compareArray(ActualUID, USER5))
            {
              lcd.setCursor(0, 0);
              lcd.print(" Access granted ");
              lcd.setCursor(0, 1);
              lcd.print("     USER 5     ");
              door_opened = true;
              first_read = true;
              granted();
              delay(5000);
              lcd.setCursor(0, 0);
              lcd.print("Scan card to be");
              lcd.setCursor(0, 1);
              lcd.print("granted ACCESS!");
              Serial.println("Last use: USER5");
            }
            else
            {
              lcd.setCursor(0, 0);
              lcd.print(" Access  denied ");
              lcd.setCursor(0, 1);
              lcd.print("   UNKNOWN ID   ");
              door_opened = false;
              first_read = false;
              denied();
              delay(3000);
              lcd.setCursor(0, 0);
              lcd.print("Scan card to be");
              lcd.setCursor(0, 1);
              lcd.print("granted ACCESS!");
              Serial.println("Last use: UNKNOWN ID, Access DENIED");
            }
          }
        }

      }
  }//end  normal mode

  if (!normal_mode)
  {
    // Revisamos si hay nuevas tarjetas  presentes
    for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
      if ( mfrc522[reader].PICC_IsNewCardPresent())
      {
        //Seleccionamos una tarjeta
        if ( mfrc522[reader].PICC_ReadCardSerial())
        {
          // Enviamos serialemente su UID

          for (byte i = 0; i < mfrc522[reader].uid.size; i++) {

            ActualUID[i] = mfrc522[reader].uid.uidByte[i];
          }

          //Compare the read ID and the stored USERS
          if (user_added == 4)
          {
            lcd.setCursor(0, 0);
            lcd.print("  User list is  ");
            lcd.setCursor(0, 1);
            lcd.print("      FULL      ");
            lcd.setCursor(0, 0);
            lcd.print("Scan card to be");
            lcd.setCursor(0, 1);
            lcd.print("granted ACCESS!");
            Serial.println("Last action: User list is full.");
          }

          if (user_added == 3)
          {
            USER5[0] = ActualUID[0];
            USER5[1] = ActualUID[1];
            USER5[2] = ActualUID[2];
            USER5[3] = ActualUID[3];
            user_added = user_added + 1;
            lcd.setCursor(0, 0);
            lcd.print("New user stored ");
            lcd.setCursor(0, 1);
            lcd.print("   as  USER 5   ");
            normal_mode = true;
            first_read = false;
            delay(3000);
            Serial.println("New user stored as USER 5");
            lcd.setCursor(0, 0);
            lcd.print("Scan card to be");
            lcd.setCursor(0, 1);
            lcd.print("granted ACCESS!");
          }

          if (user_added == 2)
          {
            USER4[0] = ActualUID[0];
            USER4[1] = ActualUID[1];
            USER4[2] = ActualUID[2];
            USER4[3] = ActualUID[3];
            user_added = user_added + 1;
            lcd.setCursor(0, 0);
            lcd.print("New user stored ");
            lcd.setCursor(0, 1);
            lcd.print("   as  USER 4   ");
            normal_mode = true;
            first_read = false;
            delay(3000);            
            Serial.println("New user stored as USER 4");
            lcd.setCursor(0, 0);
            lcd.print("Scan card to be");
            lcd.setCursor(0, 1);
            lcd.print("granted ACCESS!");

          }

          if (user_added == 1)
          {
            USER3[0] = ActualUID[0];
            USER3[1] = ActualUID[1];
            USER3[2] = ActualUID[2];
            USER3[3] = ActualUID[3];
            user_added = user_added + 1;
            lcd.setCursor(0, 0);
            lcd.print("New user stored ");
            lcd.setCursor(0, 1);
            lcd.print("   as  USER 3   ");
            normal_mode = true;
            first_read = false;
            delay(3000);
            Serial.println("New user stored as USER 3");
            lcd.setCursor(0, 0);
            lcd.print("Scan card to be");
            lcd.setCursor(0, 1);
            lcd.print("granted ACCESS!");
          }

          if (user_added == 0)
          {
            USER2[0] = ActualUID[0];
            USER2[1] = ActualUID[1];
            USER2[2] = ActualUID[2];
            USER2[3] = ActualUID[3];
            user_added = user_added + 1;
            lcd.setCursor(0, 0);
            lcd.print("New user stored ");
            lcd.setCursor(0, 1);
            lcd.print("   as  USER 2   ");
            normal_mode = true;
            first_read = false;
            delay(3000);
            lcd.setCursor(0, 0);
            lcd.print("Scan card to be");
            lcd.setCursor(0, 1);
            lcd.print("granted ACCESS!");
            Serial.println("New user stored as USER 2");
          }

        }

      }
  }//end  ID add mode

}

/**
  Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted () {
  digitalWrite(RED_LED, LOW);       // Make sure the red LED is off
  digitalWrite(GREEN_LED, HIGH);    // Make sure the green LED turns on
  digitalWrite(PLS_SIGNAL, HIGH);   // Give access to fourth floor
  delay (5000);                     
  digitalWrite(PLS_SIGNAL, LOW);    // Make sure the access to the fourth floor is closed after 5 seconds
  digitalWrite(RED_LED, LOW);       // Make sure red LED is off
  digitalWrite(GREEN_LED, LOW);     // Make sure green LED is off after 5 seconds
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(RED_LED, HIGH);     // Make sure red LED is on
  digitalWrite(GREEN_LED, LOW);    // Make sure green LED is off
  digitalWrite(PLS_SIGNAL, LOW);   // Make sure PLS signal is low
  delay(3000);
  digitalWrite(PLS_SIGNAL, LOW);    // Turn off acccess to fourth floor
  digitalWrite(GREEN_LED, LOW);     // Make sure the green LED is off
  digitalWrite(RED_LED, LOW);       // Make sure the red LED is off after 2 seconds
}


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
  Starts the timer and set the timer to expire after the
  number of milliseconds given by the parameter duration.

  @param duration The number of milliseconds until the timer expires.
*/
void startTimer(unsigned long duration)
{
  nextTimeout = millis() + duration;
}
