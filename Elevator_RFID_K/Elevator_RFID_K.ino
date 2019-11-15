/**
   --------------------------------------------------------------------------------------------------------------------
   Example sketch/program showing how to read data from more than one PICC to serial.
   --------------------------------------------------------------------------------------------------------------------
   This is a MFRC522 library example; for further details and other examples see: https://github.com/miguelbalboa/rfid

   Example sketch/program showing how to read data from more than one PICC (that is: a RFID Tag or Card) using a
   MFRC522 based RFID Reader on the Arduino SPI interface.

   Warning: This may not work! Multiple devices at one SPI are difficult and cause many trouble!! Engineering skill
            and knowledge are required!

   @license Released into the public domain.

   Typical pin layout used:
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
   SPI SS 1    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required *
   SPI SS 2    SDA(SS)      ** custom, take a unused pin, only HIGH/LOW required *
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15

     Simple Work Flow (not limited to) :
                                     +---------+
  +----------------------------------->READ TAGS+^------------------------------------------+
  |                              +--------------------+                                     |
  |                              |                    |                                     |
  |                              |                    |                                     |
  |                         +----v-----+        +-----v----+                                |
  |                         |MASTER TAG|        |OTHER TAGS|                                |
  |                         +--+-------+        ++-------------+                            |
  |                            |                 |             |                            |
  |                            |                 |             |                            |
  |                      +-----v---+        +----v----+   +----v------+                     |
  |         +------------+READ TAGS+---+    |KNOWN TAG|   |UNKNOWN TAG|                     |
  |         |            +-+-------+   |    +-----------+ +------------------+              |
  |         |              |           |                |                    |              |
  |    +----v-----+   +----v----+   +--v--------+     +-v----------+  +------v----+         |
  |    |MASTER TAG|   |KNOWN TAG|   |UNKNOWN TAG|     |GRANT ACCESS|  |DENY ACCESS|         |
  |    +----------+   +---+-----+   +-----+-----+     +-----+------+  +-----+-----+         |
  |                       |               |                 |               |               |
  |       +----+     +----v------+     +--v---+             |               +--------------->
  +-------+EXIT|     |DELETE FROM|     |ADD TO|             |                               |
          +----+     |  EEPROM   |     |EEPROM|             |                               |
                     +-----------+     +------+             +-------------------------------+


*/

#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_1_PIN        10         // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_2_PIN        11          // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1

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

/**
   Initialize.
*/
void setup() {
  SPI.begin();        // Init SPI bus
  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  // Arduino Pin Comfiguration
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(PLS_SIGNAL, OUTPUT);

  digitalWrite(RED_LED, LED_OFF);
  digitalWrite(GREEN_LED, LED_OFF);
  digitalWrite(PLS_SIGNAL, LOW);

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }

  //Config of the LCD screen
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Place card here!");
  lcd.setCursor(0, 1);
  lcd.print("                ");
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
        delay(4000);
        lcd.setCursor(0, 0);
        lcd.print("Place card here!");
        lcd.setCursor(0, 1);
        lcd.print("                ");
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
              delay(3000);
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
              door_opened = true;
              first_read = true;
              delay(3000);
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
              door_opened = true;
              first_read = true;
              delay(3000);
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
              door_opened = true;
              first_read = true;
              delay(3000);
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
              door_opened = true;
              first_read = true;
              delay(3000);
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
              door_opened = false;
              first_read = false;
              delay(3000);
              lcd.setCursor(0, 0);
              lcd.print("Place card here!");
              lcd.setCursor(0, 1);
              lcd.print("  Door CLOSED  ");
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
            lcd.print("Place card here!");
            lcd.setCursor(0, 1);
            lcd.print("                ");
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
            lcd.setCursor(0, 0);
            lcd.print("Place card here!");
            lcd.setCursor(0, 1);
            lcd.print("                ");
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
            lcd.setCursor(0, 0);
            lcd.print("Place card here!");
            lcd.setCursor(0, 1);
            lcd.print("                ");
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
            lcd.setCursor(0, 0);
            lcd.print("Place card here!");
            lcd.setCursor(0, 1);
            lcd.print("                ");
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
            lcd.print("Place card here!");
            lcd.setCursor(0, 1);
            lcd.print("                ");
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
void granted ( uint16_t setDelay) {
  digitalWrite(RED_LED, LED_OFF);  // Turn off red LED
  digitalWrite(GREEN_LED, LED_ON);   // Turn on green LED
  digitalWrite(PLS_SIGNAL, HIGH);     // Unlock door!
  delay(setDelay);          // Hold door lock open for given seconds
  digitalWrite(PLS_SIGNAL, LOW);    // Relock door
  delay(1000);            // Hold green LED on for a second
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure green LED is off
  digitalWrite(RED_LED, LED_ON);   // Turn on red LED
  delay(1000);
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(RED_LED, LED_OFF);  // Make sure red LED is off
  digitalWrite(GREEN_LED, LED_ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(RED_LED, LED_OFF);  // Make sure red LED is off
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(RED_LED, LED_ON);   // Make sure red LED is on
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure green LED is off
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(RED_LED, LED_OFF);  // Make sure Red LED is off
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure Green LED is off
  digitalWrite(PLS_SIGNAL, HIGH);    // Make sure Door is Locked
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(RED_LED, LED_OFF);  // Make sure red LED is off
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure green LED is on
  delay(200);
  digitalWrite(GREEN_LED, LED_ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(GREEN_LED, LED_ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(GREEN_LED, LED_ON);   // Make sure green LED is on
  delay(200);
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(RED_LED, LED_OFF);  // Make sure red LED is off
  digitalWrite(GREEN_LED, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(RED_LED, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(RED_LED, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(RED_LED, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(RED_LED, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(RED_LED, LED_ON);   // Make sure red LED is on
  delay(200);
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
