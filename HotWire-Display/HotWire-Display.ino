/* 7 Segment Controller
 * - Controls Common-Annode 4-Digit 7-Segment LED-Display
 * - Receives Commands 
 *   - via Serial Port (115200 Bit/s) 
 *   - via SPI         (slave)
 *     SPI communikation must release SS after each command
 * ================================================================================================
 * Display-Connection:
 * ===================
 *    4th Digit: D5 (1000)      Segment A:  D6   
 *    3th Digit: D4 (100)       Segment B:  A3   
 *    2nd Digit: D2 (10)        Segment C:  A2   
 *    1st Digit: D3 (1)         Segment D:  A0   
 *                              Segment E:  A1   
 *                              Segment F:  A4   
 *                              Segment G:  A5   
 *                              Segment DP: D7   
 * ------------------------------------------------------------------------------------------------ 
 * SPI-Connection
 * ==============
 *    D12 ---o MISO
 *    D13 ---o SCK
 *    D10 ---o SS
 *    D11 ---o MOSI
 *           +
 *    GND ---o GND
 * ================================================================================================
 * Commands:               Payload:                         Example:
 * ------------------------------------------------------------------------------------------------ 
 * "H": Display Help       none                             0x48 
 * ------------------------------------------------------------------------------------------------
 * "O": Display Off        none                             0x4f 
 * ------------------------------------------------------------------------------------------------
 * "T": Test Display       none                             0x54 
 * ------------------------------------------------------------------------------------------------
 * "B": Set Brightness     1 Byte (0-100)                   0x42 0x32             Brightness 50%
 * ------------------------------------------------------------------------------------------------
 * "N": Display Number     1 uint_16 MSB first: value       0x4E 0x0C 0x46 0x03           "3.142"
 *                         1 Byte:                          0x4E 0x01 0x3a 0x02           " 3.14"
 *                           Bit 4:                         0x4E 0x01 0x3a 0x12           "03.14"
 *                              1: Leading Zeros            0x4E 0x00 0x00 0x12           "00.00"
 *                              0: no Leading Zeros         0x4E 0x00 0x00 0x12           "00.00"
 *                           Bit 0-3: Position of DOT
 *                             (0: xxxx., 1: xxx.x, 
 *                              2: xx.xx, 3: x.xxx, 
 *                              4: No Dot)
 * ------------------------------------------------------------------------------------------------
 * "C": Display Characters 4 Byte Payload                   0x43 0x41 0x46 0x46 0x45      "AFFE" 
 *                           0x30 to 0x39 (0-9),            0x43 0x41 0x46 0x46 0x45      "AFFE"
 *                           0x41 to 0x5A (A-Z),            0x43 0x44 0x45 0x41 0x44      "DEAD"
 *                           0x2D (-)
 * ------------------------------------------------------------------------------------------------
 * "S": Display Segments   4 Byte Payload: 
 *                           0x01 SegA || 0x02 SegB ||      0x53 0xFF 0xf6 0x06 0x3f      "8.H.10" 
 *                           0x04 SegC || 0x08 SegD ||      0x53 0x7D 0xDC 0x7D 0x5C      "Go.Go" 
 *                           0x10 SegE || 0x20 SegF || 
 *                           0x40 SegG || 0x80 SegDot
 * ================================================================================================
 */

// Includes 
#include "SevSegDC.h"         // ATTENTION, not original SevSeg-Library, but modiefied from DC
#include <SPI.h>

// Hardware Definition
#define NUMDIGITS             4
#define BRIGHTNESS            100
#define RESISTORONSEGMENTS    true                             // 'false' means resistors are on digit pins
#define HARDWARECONFIG        N_TRANSISTORS
#define UPDATEWITHDELAYS      false                            // Default. Recommended
#define LEADINGZEROS          false                            // Use 'true' if you'd like to keep the leading zeros
#define BUFLEN                6                                // Legth of Serial & SPI Input Buffer

#define SPITIMEOUT            1000                             // SPI TIMEOUT in ms

#define SPI_SS   10
#define SPI_MOSI 11
#define SPI_MISO 12
#define SPI_SCK  13

// Constants
byte const DIGITPINS[] =      {5, 4, 2, 3};                    // Digits 1000, 100, 10 , 1
byte const SEGMENTPINS[] =    {6, A3, A2, A0, A1, A4, A5, 7};  // Segments: A,B,C,D,E,F,G,Period

// Instantiate 7-Segment Controller Object
SevSeg sevseg;                                                 

// Global vars  
// Serial Buffer
unsigned long g_sertime;    // millis() when last char over serial received
byte g_serlastpos;          // last position of Serial-Buffer
byte g_serbufpos;           // number of chars received from serial 
byte g_serbuf[BUFLEN];      // serial char buffer 
// SPI Buffer
byte g_spibufpos;           // number of chars received from SPI
byte g_spibuf[BUFLEN];      // serial char buffer 


void DisplayHelp(){
   Serial.println(F("------------------------------------------------------------------------------------------"));
   Serial.println(F("Command:                  Payload:                    Example:                            "));
   Serial.println(F("------------------------------------------------------------------------------------------"));
   Serial.println(F("H - Display Help          none                        0x48                                "));
   Serial.println(F("O - Display Off           none                        0x4f                                "));
   Serial.println(F("T - Test Display          none                        0x54                                "));
   Serial.println(F("B - Set Brightness        byte[1]                     0x42 0x32                   50%     "));
   Serial.println(F("N - Display Number        uint_16 MSB first: value    0x4E 0x0C 0x46 0x03         3.142   "));
   Serial.println(F("    leading Zeros         byte[3], Bit 4              0x4E 0x01 0x3a 0x02         3.14    "));
   Serial.println(F("    Position of DOT       byte[3], Bit 1-3            0x4E 0x01 0x3a 0x12         03.14   "));
   Serial.println(F("C - Display Characters    byte[1-4] Payload           0x43 0x41 0x46 0x46 0x45    AFFE    "));
   Serial.println(F("S - Display Segments      4 Byte Payload              0x53 0x30 0x30 0x30 0x30    ||||    "));
   Serial.println(F("------------------------------------------------------------------------------------------"));
}

void setup() {
  // Init Serial Port
  Serial.begin(115200);
  
  // Welcome Message
  Serial.println(F("Serial Display Controller v1.1"));
  Serial.println(F("=============================="));
  Serial.println(F("(c) 2018 by Dario Carlucio"));
  Serial.println(F("arduinoCHAR(64)carluccio.de"));
  Serial.flush();
  delay(100);

  // Init GloBal Vars
  initGlobals();

  // Init SPI
  initSpi();

  // Init Display
  initDisplay();

  // Init finished
  Serial.println(F("Init completed."));
  Serial.println(F("Starting program."));
  Serial.println(F("=============================="));
  Serial.println(F("Send chars to be displayed "));
  Serial.println(F("Send 'H' for Help."));  
  Serial.println(F("=============================="));
  Serial.flush();
}

void initGlobals() {   
  int i;
  Serial.print(F("Init Global Vars ... "));  
  g_sertime = millis();  
  g_serlastpos = 0;  
  g_serbufpos = 0;
  g_spibufpos = 0;
  for (i=0; i++; i<BUFLEN) {
    g_serbuf[i] = 0;
    g_spibuf[i] = 0;
  }    
  Serial.println(F("done."));
  Serial.flush();
}

void initSpi() {   
  Serial.print(F("Init SPI ... "));
  // turn on SPI in slave mode    
  SPCR |= _BV(SPE);           
  // have to send on master in so it set as output
  pinMode(SPI_MISO, OUTPUT);      
  // now turn on interrupt
  SPI.attachInterrupt();
  Serial.println(F("done."));
  Serial.flush();
}
  
void initDisplay() {   
  Serial.print(F("Init Display ... "));
  sevseg.begin(HARDWARECONFIG, NUMDIGITS, DIGITPINS, SEGMENTPINS, RESISTORONSEGMENTS, UPDATEWITHDELAYS, LEADINGZEROS);
  sevseg.setBrightness(BRIGHTNESS);  
  testDisplay();
  Serial.println(F("done."));
  Serial.flush();
}

void processSerial() {   
  byte c;
  while (Serial.available()) {
    c = Serial.read();     
    if (g_serbufpos < BUFLEN) {
      g_serbuf[g_serbufpos] = c;    
      g_serbufpos++;
    }
  }
}

// SPI interrupt routine 
ISR (SPI_STC_vect) {
  byte c = SPDR;     
  if (g_spibufpos < BUFLEN) {
    g_spibuf[g_spibufpos] = c;    
    g_spibufpos++;
  }
}

void clearSerialBuffer() {
  g_serbuf[0] = 0;
  g_serbufpos = 0;
  g_serlastpos = 0;
}


void parseSerialCmd(){
  unsigned int v;
  byte cp;
  byte digit;
  byte f;
  byte i;
  byte mask;

  if ((g_serbufpos != 0) && (g_serlastpos != g_serbufpos)){
    g_serlastpos = g_serbufpos;
    g_sertime = millis();   
  
    // "H": Display Help
    if (g_serbuf[0] == 0x48 ) {
      DisplayHelp();    
      clearSerialBuffer();   
    }
    // "O": Display Off        
    if (g_serbuf[0] == 0x4f ) {
      Serial.println(F("Serial: Display OFF"));   
      sevseg.blank();
      clearSerialBuffer();   
    }
    // "T": Test Display 
    if (g_serbuf[0] == 0x54) {
      Serial.println(F("Serial: Test Display"));
      testDisplay();
      clearSerialBuffer();
    }
    // "B": Set Brightness     
    if ((g_serbuf[0] == 0x42) && (g_serbufpos == 2)) {
      Serial.print(F("Serial: Set Brightness: "));    
      Serial.println(g_serbuf[1]);    
      sevseg.setBrightness(&g_serbuf[1]);
      clearSerialBuffer();
    }
    // "N": Display Number     1 uint_16 MSB first: value       0x4E 0x0C 0x46 0x03  "3.142"
    if ((g_serbuf[0] == 0x4e) && (g_serbufpos == 4)) {
      Serial.print(F("Serial: Display Number: "));
      v = (g_serbuf[1] <<8) + g_serbuf[2];
      Serial.print(v);
      // leading Zeros
      if ( (g_serbuf[3] & 0x10) == 0x10) {
        Serial.print(F(" with leading Zeros"));
        sevseg.setLeadingZeros(true);
      } else {
        Serial.print(F(" without leading Zeros"));
        sevseg.setLeadingZeros(false);
      }
      Serial.print(F(" Format "));
      cp = (g_serbuf[3] & 0x0f);
      switch (cp){
        case 0x00:
          Serial.print(F("xxxx."));
          break;
        case 0x01:
          Serial.print(F("xxx.x"));
          break;
        case 0x02:
          Serial.print(F("xx.xx"));
          break;
        case 0x03:
          Serial.print(F("x.xxx"));
          break;
        case 0x04:
          Serial.print(F("xxxx"));
          break;    
      }
      Serial.println(F("."));    
      sevseg.setNumber(v,cp); 
      clearSerialBuffer();
    }
    // "C": Display Characters
    if ((g_serbuf[0] == 0x43) && (g_serbufpos == 5)) {    
      sevseg.setChars(&g_serbuf[1]);                 // Ser Charaters    
      Serial.print(F("Serial: Display Characters: "));    
      for (i=1; i<5; i++) {
        Serial.print(char(g_serbuf[i]));    
      }
      Serial.print(F(" Format "));
      for (i=0; i<4; i++) {
        Serial.print(F("x"));
        mask = (0x08 >> i);      
        if ((g_serbuf[5] & mask ) == mask){
          Serial.print(F("."));        
        }            
      }    
      Serial.print(F(" ["));
      Serial.print(g_serbuf[5]);
      Serial.print(F("]"));    
      Serial.println(F(""));    
      clearSerialBuffer();
    }
    // "S": Display Segments     
    if ((g_serbuf[0] == 0x53) && (g_serbufpos == 5)) {
      Serial.print(F("Serial: Display Segments: "));        
      for (digit=1; digit <5; digit++) {
         Serial.println(F(""));        
         Serial.print(F(" Digit "));      
         Serial.print(digit);      
         Serial.print(F(" Segments:"));  
         f = 0;
         for (i=0; i<8; i++) {    
           mask = (0x01 << i);
           if ((g_serbuf[digit] & mask ) == mask ){
             if (f != 0) {
               Serial.print(F(","));
             }
             if (i<7) {
               Serial.print(char (0x41 + i));
             } else {
               Serial.print(F("DP"));      
             }           
             f = 1;
           }
         }
      }
      Serial.println(F(""));        
      sevseg.setSegments(&g_serbuf[1]);
      clearSerialBuffer();
    }
  }
  // Timeout
  if ( ((millis() - g_sertime ) > 5000) && (g_serbufpos != 0) ) {
    Serial.println(F("Serial: Timeout"));    
    Serial.print(F(" Bufferlen:"));    
    Serial.print(g_serbufpos);
    Serial.print(F(" Buffer:"));    
    for (i=0; i<g_serbufpos; i++) {  
      Serial.print(g_serbuf[i]);
      Serial.print(F(" "));    
    }
    Serial.println(F(" "));    
    clearSerialBuffer();    
  }
}


/* Parse SPI Command 
 * - read g_spibuf 
 * - parse command
 * - execute command
 * 
 * This is called after SS goes HIGH, this means
 *   there must be a valid command in the buffer
 */
void parseSpiCmd() {
  unsigned int v;
  byte cp;
  byte i;
  byte spibufpos;           
  byte spibuf[BUFLEN];      
  boolean spiOK;

  // Flag: valid command detected
  spiOK = false;
  
  // copy SPI Buffer 
  // so that further commands does not overwrite
  spibufpos = g_spibufpos;
  for (i=0; i < spibufpos; i++) {
    spibuf[i] = g_spibuf[i];
  }
  // clear SPI Buffer 
  g_spibufpos = 0;
  g_spibuf[0] = 0;

 /*************************
  * "H": Display Help
  * - not supported on SPI
  *************************/  
  if (spibuf[0] == 0x48 ) {
    Serial.println(F("Help not supported on SPI"));       
    spiOK = true;  
  }

 /*******************
  * "O": Display Off 
  *******************/  
  if (spibuf[0] == 0x4f ) {    
    sevseg.blank();
    spiOK = true;
  }
  
 /******************** 
  * "T": Test Display 
  ********************/
  if (spibuf[0] == 0x54) {    
    testDisplay();
    spiOK = true;
  }
  
 /********************** 
  * "B": Set Brightness     
  **********************/  
  if ((spibuf[0] == 0x42) && (spibufpos == 2)) {
    sevseg.setBrightness(&spibuf[1]);
    spiOK = true;
  }
  
 /********************************************* 
  *  "N": Display Number     
  *  - [1]+[2]: uint_16 MSB first
  *  - [3]    
  *     Bit 4   : with leading Zeros
  *     Bit 0-3 : Position of collon
  *  example: 0x4E 0x0C 0x46 0x03  "3.142"
  ********************************************/   
  if ((spibuf[0] == 0x4e) && (spibufpos == 4)) {    
    v = (spibuf[1] <<8) + spibuf[2];    
    // leading Zeros
    if ( (spibuf[3] & 0x10) == 0x10) {      
      sevseg.setLeadingZeros(true);
    } else {
      sevseg.setLeadingZeros(false);
    }
    // collon Position
    cp = (spibuf[3] & 0x0f);    
    sevseg.setNumber(v, cp); 
    spiOK = true;
  }

 /********************************************* 
  * "C": Display Characters
  *  - [1]-[4]: Characters
  *  example: 0x43 0x44 0x45 0x41 0x44   "DEAD" 
  *********************************************/  
  if ((spibuf[0] == 0x43) && (spibufpos == 5)) {    
    sevseg.setChars(&spibuf[1]);                 // Ser Charaters    
    // Serial.print(F("SPI: Display Characters: "));    
    // for (i=1; i<5; i++) {
    //   Serial.print(char(g_spibuf[i]));    
    // }
    // Serial.print(F(" Format "));
    // for (i=0; i<4; i++) {
      // Serial.print(F("x"));
      // mask = (0x08 >> i);      
      // if ((spibuf[5] & mask ) == mask){
      //   Serial.print(F("."));        
      // }            
    // }    
    // Serial.print(F(" ["));
    // Serial.print(spibuf[5]);
    // Serial.print(F("]"));    
    // Serial.println(F(""));    
    spiOK = true;
  }
  
 /********************************************* 
  * "S": Display Segments
  *  - [1]-[4]: Segments
  *    - Bit 0: A  |  Bit 1: B  |  Bit 2: C  |  Bit 3: D 
  *    - Bit 4: E  |  Bit 5: F  |  Bit 6: G  |  Bit 7: DP
  *    example: 0x53 0x7D 0xDC 0x7D 0x5C  "Go.Go" 
  *******************************************************/   
  if ((spibuf[0] == 0x53) && (spibufpos == 5)) {
    sevseg.setSegments(&spibuf[1]);
    spiOK = true;
  }

 /********************************************* 
  * Error if no valid command was detected
  * This function is only called, if 
  * - something is in the buffer 
  *   AND
  * - SS is HIGH
  * This means 
  *  - Transmission is over 
  *  - Command will be parsed, 
  * If no valid command was detected at this Time, 
  * we display an Error Message on Serial
  *******************************************************/   
  // 
  if ( !spiOK ) {  
    Serial.println(F("SPI: Error"));    
    Serial.print(F(" Bufferlen:"));    
    Serial.print(spibufpos);
    Serial.print(F(" Buffer:"));
    for (i=0; i<spibufpos; i++) {
      Serial.print(spibuf[i]);
      Serial.print(F(" "));    
    }
    Serial.println(F(" "));  
  }
}


void delayWithRefresh(int ms) {   
  unsigned long timestamp;
  timestamp = millis();
  while ( (millis() - timestamp) < ms) {
    sevseg.refreshDisplay();     
  }    
}

/* Display Test
 * Shows three times "----" in the Display
 * !!! BLOCKING !!!
 */
void testDisplay() {   
  unsigned long timestamp;
  byte c;
  byte i;
  byte v[4];
  int d;
  d = 50;
  // Write Test
  // sevseg.setChars("TEST");
  // delayWithRefresh (1500);    
  
  // Test Segments
  for (c=0; c<4; c++) {    
    v[c] = 0;
  }
  for (c=0; c<4; c++) {       
    for (i=0; i<8; i++) {    
      v[c] = (1 << i);
      sevseg.setSegments(&v[0]);     
      v[c] = 0;
      delayWithRefresh (d);        
    }
  }
  delayWithRefresh (d);        
  sevseg.blank();  
  delayWithRefresh (d);        
  
  // Test Numbers
  sevseg.setLeadingZeros(true);
  for (i=1; i<10; i++) {    
    delayWithRefresh (d);    
    sevseg.setNumber(i*1000 + i * 100 + i * 10 + i);     
  }
  delayWithRefresh (d);    
  sevseg.setNumber(0);     
  delayWithRefresh (11 *  d);    
  //7 Test  
  sevseg.setChars("init");
  delayWithRefresh (10 * d);    
  sevseg.setChars("ende");
  delayWithRefresh (10 * d);    
  sevseg.setChars("----");  
  delayWithRefresh (10 * d);    
  sevseg.blank();  
}

/* Main Loop
 * - Read Serial 
 * - parse command
 * - Multiplex Display
 */
void loop() {   
  // get Chars from Serial 
  processSerial();
  
  // parse Serial Buffer
  parseSerialCmd();

  // parse SPI  Buffer
  if ( (g_spibufpos > 0) && (digitalRead(SPI_SS)) ) {
    parseSpiCmd();  
  } 

 // Refresh Display
  sevseg.refreshDisplay(); 
}
