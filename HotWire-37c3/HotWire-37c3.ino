// Heisser Draht Controller für 37c3
//
// #define DEBUG
//
//
// History:
// 19.12.2034 Initial Display von 36c3 auf 37c3
// 17.12.2019 DISPLAY_TIMEOUT von 2500 auf 1000

// Includes
#include <SPI.h>      //  SPI library (Arduino internal)
#include <MFRC522.h>  //  RFID Controller library 
#include <FastLED.h>

// Timing Constants
#define BYTE_DELAY_US        50    // Delay [us] after Byte send to Display using SPI
#define SS_DELAY_US          50    // Delay [us] after SS_Display status change
#define RFID_DEBOUNCE        5000  // Debounce time (same RFID-Tag after debounce Time)
#define SERVER_TIMEOUT       5000  // Time for server to answer
#define ERROR_DEBOUNCE       333   // Debounce time for Errors during the game (only one error each ERROR_DEBOUNCEms)
#define BLINK_LED            250   // Blinking time if LED has to blink
#define DISPLAY_ERROR_DELAY  1000  // show errors for this ammount of time
#define DISPLAY_RESULT_DELAY 2500  // time to change between Errors and Time-Display at end of game
#define DISPLAY_TIMEOUT      1000  // Time to show Messages on Display
#define DISPLAY_GOAL_TIMEOUT 1000  // Time to show Goal Messages on Display

// GPIO - Display
#define DISPLAY_SS_PIN       8    // Chip Select Pin Display-Controller (SPI-Slave)
// GPIO - RFID Reader
#define RFID_RST_PIN         9    // Restet Pin RFID-Controller 
#define RFID_SS_PIN          10   // Chip Select Pin RFID-Controller        
// GPIO - Hotwiref
#define HOTWIRE_WIRE         2    // Wire Contact (Error)
#define HOTWIRE_LEFT         3    // Left Contact 
#define HOTWIRE_RIGHT        4    // Right Contact 
#define HOTWIRE_BUTTON       7    // Green Button
// GPIO - WS2812 LED
#define LED_DATA_PIN         6    // Data Pin for WS2812 LED
#define NUM_LEDS             1    // Only one LED

// Buffersizes
#define SPIBUFLEN            8    // SPI Buffer for Display
#define RFIDBUFLEN           10   // UID of last seen RFID-Tag
#define SERBUFLEN            8    // Buffer for Serial response

// Display Templates
#define MT_GOGO              1
#define MT_STOP              2
#define MT_FAIL              3
#define MT_BRACE             4
#define MT_DOUBLEBRACE       5
#define MT_DOTS1             6
#define MT_DOTS2             7
#define MT_TOPLINE           8
#define MT_MIDDLELINE        9
#define MT_BOTTOMLINE        10
#define MT_HOHO              11    

// Game States
#define S_UNDEFINED          0
#define S_READY              1
#define S_START_KIDSGAME     2
#define S_CHECK_RFID         3
#define S_RFID_NACK          4
#define S_RFID_ACK           5
#define S_GAME_READY         6
#define S_GAME_STARTED       7
#define S_GAME_RUNNING       8
#define S_GAME_ENDED         9
#define S_STORE_RESULT       10
#define S_SAVE_OK            11
#define S_SAVE_ERROR         12

/**********************
 * Debugging Macros
 **********************/ 
#ifdef DEBUG
 #define DEBUG_PRINT(x)    Serial.print(F(">")); Serial.print(x)
 #define DEBUG_PRINT2(x,y)                       Serial.print(x,y)
 #define DEBUG_PRINTLN(x)  Serial.print(F(">")); Serial.println(x)
 #define DEBUG_FLUSH       Serial.flush()
#else
 #define DEBUG_PRINT(x)  
 #define DEBUG_PRINT2(x,y)
 #define DEBUG_PRINTLN(x)
 #define DEBUG_FLUSH       
#endif
/**********************
 * Global Vars
 **********************/ 
// SPI buffer
byte g_spiBuffer[SPIBUFLEN];         // SPI-Buffer for Display communication
byte g_spiBufferLen;                 // SPI-Buffersize for Display communication

// RFID 
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);  // Create MFRC522 instance
byte g_uid[RFIDBUFLEN];                      // UID of last seen RFID-Tad
byte g_uidlen;                               // Length od UID

// LED
CRGB g_led[NUM_LEDS];

// time(r)s
unsigned long g_last_time_displayed; // last time of result display
unsigned long g_result_disp_start;   // start time of result display
unsigned long g_error_disp_start;    // start time of error display
unsigned long g_game_start;          // millis() when game was started
unsigned long g_uidtime;             // Time when last UID has been seen
unsigned long g_state_timer;         // Internal state Timer 
unsigned long g_last_error_timer;    // Only one error each 
unsigned long g_led_blink_timer;     // timer for blinking LED

// states 
byte g_state;                        // state
byte g_new_state;                    // next_state
byte g_display_state;                // state of result display 

// values
byte g_GpioStart;                    // startpoint of this game
byte g_GpioEnd;                      // endpoint of this game
unsigned long g_game_time;           // game result duration 
unsigned int  g_game_errors;         // game result errors

// flags
boolean g_game_newerror;             // new error occured
boolean g_display_showerror;         // show error on display
boolean g_display_time;              // display the time of result (false: errors are displayed)
boolean g_kidsgame;                  // actual game is a kidsgame
boolean g_ledblinkstate;             // actual state of blinking LED

/* ###############################################################################################
 * ### Init                                                                                    ###
 * ############################################################################################### */
  
/*******************
 * Init Global Vars
 *******************/
void initGlobalVars() {  
  int i;
  DEBUG_PRINT(F("  - Global Vars ... "));  
  // initial state
  g_state               = S_UNDEFINED;
  g_new_state           = S_READY;
  g_display_state       = 0;
  // time(r)s
  g_state_timer         = 0;
  g_last_time_displayed = 0;
  g_error_disp_start    = 0;
  g_game_start          = 0;
  g_game_time           = 0;
  g_result_disp_start   = 0; 
  g_game_errors         = 0;
  g_GpioStart           = 0;
  g_GpioEnd             = 0;
  g_last_error_timer    = 0;
  g_led_blink_timer     = 0;
  // flags  
  g_game_newerror       = false;
  g_display_showerror   = false;
  g_display_time        = true;
  g_kidsgame            = false;
  // SPI buffer
  g_spiBufferLen        = 0;
  for (i=0; i++; i<SPIBUFLEN) {
    g_spiBuffer[i]      = 0;
  }    
  g_uidlen = 0;
  for (i=0; i++; i<RFIDBUFLEN) {
    g_uid[i]            = 0;
  }    
  DEBUG_PRINTLN(F("done."));
  DEBUG_FLUSH;
}


/************
 * Init GPIO
 ************/
void initGPIO() {
  DEBUG_PRINT(F("  - GPIO ... "));    
  pinMode(HOTWIRE_WIRE, INPUT_PULLUP);  
  pinMode(HOTWIRE_LEFT, INPUT_PULLUP);  
  pinMode(HOTWIRE_RIGHT, INPUT_PULLUP);  
  pinMode(HOTWIRE_BUTTON, INPUT_PULLUP);  
  DEBUG_PRINTLN(F("done."));
  DEBUG_FLUSH;
}


/***********
 * Init LED
 ***********/
void initLED() {
  byte i;
  DEBUG_PRINT(F("  - LED ... "));    
  FastLED.addLeds<WS2812, LED_DATA_PIN, RGB>(g_led, NUM_LEDS);
  for (i=0; i<5; i++) {
    SetLED(255,0,255);
    delay(200);
    SetLED(0,0,0);    
    delay(200);
  }  
  DEBUG_PRINTLN(F("done."));
  DEBUG_FLUSH;
}


/***********
 * Init SPI
 ***********/
void initSpi() {
  DEBUG_PRINT(F("  - SPI ... "));  
  pinMode(DISPLAY_SS_PIN, OUTPUT);  
  digitalWrite(DISPLAY_SS_PIN, HIGH); 
  SPI.begin();  
  SPI.setDataMode (SPI_MODE0);             // (CPOL = 0) and (CPHA = 0)
  SPI.setBitOrder(MSBFIRST);               // MSB first
  SPI.setClockDivider(SPI_CLOCK_DIV8);     // 1 MHz 
  DEBUG_PRINTLN(F("done."));
  DEBUG_FLUSH;
}

/************
 * Init RFID
 ************/
void initRFID() {
  DEBUG_PRINTLN(F("  - RFID Reader ..."));  
  mfrc522.PCD_Init();       
  #ifdef DEBUG
    DEBUG_PRINT(F("    "));  
    mfrc522.PCD_DumpVersionToSerial();  
  #endif
  DEBUG_PRINTLN(F("    done."));
  DEBUG_FLUSH;
}


/***************
 * Init Display
 ***************/
void initDisplay() {
  DEBUG_PRINT(F("  - Display ... "));    
  DisplayOff();  
  // showTemplate(MT_HOHO);
  DEBUG_PRINTLN(F("done."));
  DEBUG_FLUSH;
}

/************
 * Main Init 
 ************/
void setup() {  
  // Init Serial Port
  Serial.begin(115200);
  
  // Welcome Message
  DEBUG_PRINTLN(F("Hot-Wire Controller 37c3"));
  DEBUG_PRINTLN(F("========================"));
  DEBUG_PRINTLN(F("(c) 2023 by Dario Carlucio"));
  DEBUG_PRINTLN(F("arduinoCHAR(64)carluccio.de"));
  DEBUG_PRINTLN();
  DEBUG_PRINTLN(F("Init:"));
  DEBUG_FLUSH;  
  
  // Init everything
  initGlobalVars();        // Global Vars
  initGPIO();              // GPIO
  initSpi();               // SPI
  initRFID();              // RFID
  initDisplay();           // Display  
  initLED();               // WS2812 LED

  DEBUG_PRINTLN(F("Init completed."));
  DEBUG_PRINTLN();
  DEBUG_PRINTLN(F("Starting Main Loop."));
  DEBUG_FLUSH;
}

/* ###############################################################################################
 * ### LED                                                                                     ###
 * ############################################################################################### */

/*******************
 * Set LED to color
 *******************/
void SetLED(byte r, byte g, byte b) {
   g_led[0] = CRGB(r,g,b); 
   FastLED.show();   
}

/* ###############################################################################################
 * ### RFID                                                                                    ###
 * ############################################################################################### */

 /****************************************
  *  - Lock for new RFID-Tag
  *  - return true if a new Tag was detected  
  ****************************************/
boolean newTag() {
  boolean UIDchanged;
  UIDchanged = false;
  // Look for new cards
  if ( mfrc522.PICC_IsNewCardPresent()) {
    // DEBUG_PRINTLN(F("IsNewCardPresent"));
    // Select one of the cards
    // DEBUG_PRINTLN(F("Select one of the cards"));
    if (  mfrc522.PICC_ReadCardSerial()) {
      //DEBUG_PRINTLN(millis());
      UIDchanged = saveUID(mfrc522.uid.uidByte, mfrc522.uid.size);
    }
  } 
  return(UIDchanged);
}    


/********************************************* 
 * Save UID of last RFID-Tag 
 * - save only if UID changed
 * - or if same UID after RFID_DEBOUNCE
 ********************************************/   
boolean saveUID (byte *buffer, byte bufferSize) {
  byte i; 
  boolean same; 
  same = true;
  // RFID-UID different from buffer g_uid?
  if (bufferSize != g_uidlen) {
    same = false;
    // DEBUG_PRINT(F("buffersize"));
  } else {
    for (i = 0; i < bufferSize; i++) {
      if (buffer[i] != g_uid[i]) {
        same = false;
        //DEBUG_PRINT(F("bufferByte"));
      }
    }
  }
  // Debounceing time passed?
  if ((millis() - g_uidtime) > RFID_DEBOUNCE) {
      same = false;
      //DEBUG_PRINT(F("Debounce"));
  }
  // UID changed
  if (!same) {    
    g_uidlen = bufferSize;
    g_uidtime = millis(); 
    for (i = 0; i < bufferSize; i++) {      
      g_uid[i] = buffer[i];
    }
    printUID();
  }
  return(!same);
}

/********************************************* 
 * Print UID of last RFID-Tag 
 ********************************************/   
void printUID() {
  byte i;
  DEBUG_PRINT(F("UID:"));
  for (i = 0; i < g_uidlen; i++) {
    if ( g_uid[i] < 0x10) {
      DEBUG_PRINT2(0, HEX);
    }
    DEBUG_PRINT2(g_uid[i], HEX);
  }
  DEBUG_PRINTLN();
}


/* ###############################################################################################
 * ### GAMESERVER                                                                              ###
 * ############################################################################################### */

/************************************************************ 
 * check_UID
 * - send UID to GameServer 
 *   CMD: Nxxxxxxxx<CR>
 *        xxxx     = UID [HEX] 
 *        <CR>     = 0x0D (dec:13)
 *   e.g: N02AF21C1<CR>
 *                 - UID: 02:AF:21:C1
 * - wait for Result (Server responds with:)
 *   - 0<CR>       - not allowed to play, because just completed with Zero Errors
 *   - ddd<CR>     - allowed to play
 *        dddd = number of tries so far
 * - timeout if no char received for SERVER_TIMEOUT ms
 * - return:
 *   - true if first char received != "0";
 *   - false 
 *     - if first char received == "0";
 *     - false if no char received during SERVER_TIMEOUT ms
 ************************************************************/
boolean check_UID() {
    boolean response_received; 
    boolean response_ok; 
    unsigned long timeout_timer;
    byte buf[SERBUFLEN]; 
    byte bufpos; 
    response_received = false;
    response_ok = false;  
    clear_Serial_Input_Buffer();
    // Send CMD: Nxxxxxxxx<CR>
    DEBUG_PRINTLN(F("Send UID to Gameserver:"));
    Serial.print(F("N"));
    // Send only the first four Bytes of UID instead of g_uidlen   
    for (bufpos=0; bufpos<4; bufpos++) {
      if ( g_uid[bufpos] < 0x10) {
        Serial.print(F("0"));
      }
      Serial.print(g_uid[bufpos], HEX);
    }    
    Serial.write(0x0d);  // <CR>    
    // receive response
    DEBUG_PRINTLN(F("\nReceive Response:"));
    timeout_timer = millis();
    bufpos = 0; 
    while (!response_received) {
      if (Serial.available()) {
        DEBUG_PRINTLN(F("."));
        timeout_timer = millis();
        if (bufpos < SERBUFLEN) {          
          buf[bufpos] = Serial.read();                    
          DEBUG_PRINTLN(buf[bufpos]);          
          if (buf[bufpos] == 0x0d) {
            response_received = true;
            DEBUG_PRINTLN(F("Got Response"));
          }
          bufpos++;
        }
      } 
      if ((millis() - timeout_timer) > SERVER_TIMEOUT) {
        response_received = true;
        buf[0] = 0x30;
        DEBUG_PRINTLN(F("Response Timeout"));
      }
    }
    // Check Response
    if (buf[0] != 0x30){
      response_ok = true;
      DEBUG_PRINTLN(F("Response: ACK"));
    } else {      
      DEBUG_PRINTLN(F("Response: NACK"));
    } 
    return response_ok;
}


/************************************************************ 
 * store_Result
 * - send Result to GameServer 
 *  CMD: Rxxxxxxxx,ffff,mm..mm<CR>
 *       xxxx     = UID [HEX] 
 *       ffff     = Errors [DEC]
 *       mmmm     = time in ms  [DEC] 
 *       <CR>     = 0x0a
 *  e.g: R02af21c1,0,47295<CR>    
 *                - UID: 02:AF:21:C1
 *                - zero Errors
 *                - 47,295 seconds    
 *  - Server responds with:               
 *    - ACK<CR>    - in any case 
 *    - NACK<CR>   - only if error occured (e.g. no connection to database)
 * - timeout if no char received for SERVER_TIMEOUT ms
 * - return:
 *   - true if first char received == "A";
 *   - false 
 *     - if first char received == "N";
 *     - false if no char received during SERVER_TIMEOUT ms
 ************************************************************/
boolean store_Result() {
    boolean response_received; 
    boolean response_ok; 
    unsigned long timeout_timer;
    byte buf[SERBUFLEN]; 
    byte bufpos; 
    response_received = false;
    response_ok = false;    
    DEBUG_PRINTLN(F("Send Result to Gameserver:"));    
    clear_Serial_Input_Buffer();
    // Send CMD: R02af21c1,0,47295<CR>
    Serial.print(F("R"));
    // Send only the first four Byted of UID instead of g_uidlen   
    if (g_kidsgame) {
      Serial.print(F("00000000"));
    } else {
      for (bufpos = 0; bufpos<4; bufpos++) {
        if ( g_uid[bufpos] < 0x10) {
          Serial.print(F("0"));
        }
        Serial.print(g_uid[bufpos], HEX);
      }
    }    
    Serial.print(F(","));
    Serial.print(g_game_errors);         
    Serial.print(F(","));
    Serial.print(g_game_time);
    Serial.write(0x0a);  // <CR>
    Serial.flush();
    // receive response
    DEBUG_PRINTLN(F("\nReceive Response from Gameserver:"));
    timeout_timer = millis();
    bufpos = 0; 
    while (!response_received) {
      if (Serial.available()) {
        DEBUG_PRINTLN(F("."));
        timeout_timer = millis();
        if (bufpos < SERBUFLEN) {          
          buf[bufpos] = Serial.read();                                        
          DEBUG_PRINTLN(buf[bufpos]);
          if (buf[bufpos] == 0x0d) {
            response_received = true;
            DEBUG_PRINTLN(F("Got Response"));            
          }
          bufpos++;
        }
      } 
      if ((millis() - timeout_timer) > SERVER_TIMEOUT) {
        response_received = true;
        buf[0] = 0;
        DEBUG_PRINTLN(F("Response Timeout"));
      }
    }
    // Check Response    
    if ((buf[0] == 0x41) || (buf[0] == 0x61)) {  // 'A' or 'a'    
      response_ok = true;
      DEBUG_PRINTLN(F("Response: ACK"));
    } else {      
      DEBUG_PRINTLN(F("Response: NACK"));
    } 
    return response_ok;
}

void clear_Serial_Input_Buffer() {
  byte c;
  while (Serial.available() > 0) {
    c = Serial.read();
  }
}


/* ###############################################################################################
 * ### DISPLAY                                                                                 ###
 * ############################################################################################### */

/*********************
 * switch Display Off 
 *********************/
void DisplayOff() {    
  g_spiBufferLen = 1;
  g_spiBuffer[0] = 0x4F;                   // "O"
  spiDisplaySend ();  
}

/************************************************* 
  *  Update Display during the Game 
  *  - Show Time running
  *  - if Error occured:
  *    -> show Errorcounter for DISPLAY_ERROR_DELAY
  ************************************************/   
void updateGameDisplay() {  
  // New error occured
  // - show error
  // - start timer
  if (g_game_newerror) {
    showError(g_game_errors);
    g_display_showerror = true;
    g_error_disp_start = millis();
    g_game_newerror = false;
  }    
  // cancel error display after delay period is over
  if (g_display_showerror) {    
    if ( (millis() - g_error_disp_start) > DISPLAY_ERROR_DELAY) {
      g_display_showerror = false;
    }
  }
  // if Game is running, and no error has to be displayed
  // - refresh display every 10ms
  if (!g_display_showerror) {                  
    if (millis() - g_last_time_displayed > 10) {          
      gameShowTime(millis() - g_game_start);
      g_last_time_displayed = millis();
    }
  }     
}


/************************************************ 
  *  Update Display after the Game
  *  - switch between Display states after Game
  *    - state 0: display Time of last game
  *    - state 1: display Errors of last game
  *    - state 2: display "37c3"
  *  - switch every DISPLAY_RESULT_DELAY
  *  - only if last Game exists g_game_time != 0
  *  ToDo: Show Highscore  
  ***********************************************/   
void updateResultDisplay(){
  if (g_game_time != 0) {
    if (millis() - g_result_disp_start > DISPLAY_RESULT_DELAY) {        
      g_result_disp_start  = millis(); 
      switch (g_display_state) {
      case 0: 
        gameShowTime(g_game_time);
        g_display_state = 1;
        break;
      case 1: 
        showError(g_game_errors);
        g_display_state = 2;
        break;
      case 2:
        setDisplayString ("37c3");                 // Display: "37c3"
        g_display_state = 0;
        break;               
      }
    }        
  }  
}

/***************************************
  *  Show Game-Time on SPI Display
  *  t: time in ms
  *  - up to 9s:    x.xxx    e.g.  4.711
  *  - up to 99s:   xx.xx    e.g.  47.11
  *  - up to 999s:  xxx.x    e.g.  471.1
  *  - up to 9999s: xxxx.    e.g.  4711.
  *  - over  9999s: 9999.
  ***************************************/
void gameShowTime(unsigned long t) {  
  unsigned int d;      // digits to be displayed 
  unsigned long hs;    // second/100
  byte dp;             // position of decimal point
  hs = t/10;  
  // calc gametime  
    if ( hs < 1000 ) {  // 0 - 999 = x.xxx seconds
      d = t;
      dp = 3;  
    } else if ( hs < 10000 ) {  // 0 - 9999 = xx.xx seconds
      d = hs;
      dp = 2;  
    } else if ( hs < 100000 ) { // 0 - 99999 = xxx.x seconds
      d = hs/10;
      dp = 1;      
    } else if ( hs < 1000000 ) { // 0 - 999999 = xxxx seconds
      d = hs/100;
      dp = 4;      
    } else { 
      d = 9999;
      dp = 0; 
    }
    showNumber(d, dp);
}  


/******************************
 *  Show ERRORS on SPI Display
 *  - e:  Number of Errors  
 ******************************/
void showError(unsigned int e) {      
  int r;  
  if (e > 999) {
    setDisplayString ("F---");
  } else {
    setDisplayString ("F   ");
  }
  if (e > 99) {
    r = (e / 100);
    e -= r * 100;
    g_spiBuffer[2] = 0x30 + r;    
  }
  if (e > 9) {
    r = (e / 10);
    e -= r * 10;
    g_spiBuffer[3] = 0x30 + r;    
  }  
  g_spiBuffer[4] = 0x30 + e;      
  spiDisplaySend ();  
}

 
/****************************************
  *  Show Decimal Number on SPI Display
  *  - d:  Number to be displayed 0-9999
  *  - dp: Position of Decimal Point 
  *        0: xxxx.  ->  4711.
  *        1: xxx.x  ->  471.1
  *        2: xx.xx  ->  47.11
  *        3: x.xxx  ->  4.711
  *        4: xxxx   ->  4711  (no DP)
  ****************************************/
void showNumber(unsigned int d, byte dp) {    
  byte msb;
  byte lsb;  
  msb = (byte) ((d - msb) >> 8);
  lsb = (byte) (d & 0xff);  
  g_spiBufferLen = 4;
  g_spiBuffer[0] = 0x4E;                   // "N"
  g_spiBuffer[1] = msb;                    // MSB
  g_spiBuffer[2] = lsb;                    // LSB 
  g_spiBuffer[3] = dp;                     // Position of DP
  spiDisplaySend ();  
}


/******************************
  *  copy String to SPI Buffer
  *  - s: sting of length 4  
  *****************************/
void setDisplayString (String s) {
  g_spiBufferLen = 5; 
  g_spiBuffer[0] = 'C';
  g_spiBuffer[1] = s.charAt(0);
  g_spiBuffer[2] = s.charAt(1);
  g_spiBuffer[3] = s.charAt(2);
  g_spiBuffer[4] = s.charAt(3);  
  spiDisplaySend (); 
}


/******************************
  *  Show Template 
  *  - num: Number of Template
  *    - 1: go.go 
  *****************************/
void showTemplate(byte num) {      
  g_spiBufferLen = 5; 
  switch (num) {
    case MT_GOGO:  
      // go.go
      g_spiBuffer[0] = 0x53;
      g_spiBuffer[1] = 0x7d;
      g_spiBuffer[2] = 0xdc;
      g_spiBuffer[3] = 0x7d;
      g_spiBuffer[4] = 0x5c;
      break;
    case MT_STOP:  
      // STOP
      setDisplayString ("STOP");      
      break;
    case MT_FAIL:  
      // Fail
      setDisplayString ("FAIL");
      break;
    case MT_BRACE:  
      // [  ]
      g_spiBuffer[0] = 0x53;
      g_spiBuffer[1] = 0x39;
      g_spiBuffer[2] = 0x00;
      g_spiBuffer[3] = 0x00;
      g_spiBuffer[4] = 0x0f;
      break;
    case MT_DOUBLEBRACE:  
      // [][]
      g_spiBuffer[0] = 0x53;
      g_spiBuffer[1] = 0x39;
      g_spiBuffer[2] = 0x0f;
      g_spiBuffer[3] = 0x39;
      g_spiBuffer[4] = 0x0f;
      break;
    case MT_DOTS1:  
      // oÂ°oÂ°
      g_spiBuffer[0] = 0x53;
      g_spiBuffer[1] = 0x5c;
      g_spiBuffer[2] = 0x63;
      g_spiBuffer[3] = 0x5c;
      g_spiBuffer[4] = 0x63;
      break;
    case MT_DOTS2:
      // Â°oÂ°o
      g_spiBuffer[0] = 0x53;
      g_spiBuffer[1] = 0x63;
      g_spiBuffer[2] = 0x5c;
      g_spiBuffer[3] = 0x63;
      g_spiBuffer[4] = 0x5c;
      break;
    case MT_TOPLINE:  
      // ---- (Top)
      g_spiBuffer[0] = 0x53;
      g_spiBuffer[1] = 0x01;
      g_spiBuffer[2] = 0x01;
      g_spiBuffer[3] = 0x01;
      g_spiBuffer[4] = 0x01;
      break;
    case MT_BOTTOMLINE:
      // // ---- (Bottom)
      g_spiBuffer[0] = 0x53;
      g_spiBuffer[1] = 0x08;
      g_spiBuffer[2] = 0x08;
      g_spiBuffer[3] = 0x08;
      g_spiBuffer[4] = 0x08;
      break;
     case MT_HOHO:  
      // HOHO
      setDisplayString ("HOHO");      
      break;
    case MT_MIDDLELINE:
    default:  
      // ----
      setDisplayString ("----");      
      break;
  }    
  spiDisplaySend ();  
}


/************************************************** 
  *  Send SPI Buffer to SPI Display
  *  - g_spiBufferLen: How many byted shal be send
  *  - g_spiBuffer[i]: SPI Buffer  
  *************************************************/   
void spiDisplaySend () {
  byte i;
  byte c;
  // (Re-)Configure SPI
  SPI.setDataMode (SPI_MODE0);             // (CPOL = 0) and (CPHA = 0)
  SPI.setBitOrder(MSBFIRST);               // MSB first
  SPI.setClockDivider(SPI_CLOCK_DIV8);     // 1 MHz 
  // activate Chip Select
  digitalWrite(DISPLAY_SS_PIN, LOW);  
  delayMicroseconds(SS_DELAY_US);
  // send buffer
  for (i=0; i<g_spiBufferLen; i++) {
    c = g_spiBuffer[i];
    delayMicroseconds(BYTE_DELAY_US);
    SPI.transfer(c);
    delayMicroseconds(BYTE_DELAY_US);
  }  
  delayMicroseconds(SS_DELAY_US);
  // release Chip Select
  digitalWrite(DISPLAY_SS_PIN, HIGH);  
  delayMicroseconds(SS_DELAY_US);
}


/* ###############################################################################################
 * ### GAMESTATEHANDLER                                                                        ###
 * ############################################################################################### */

/*******************************************
 *  Add Error during the Game 
 *  - only one Error each ERROR_DEBOUNCE ms 
 *******************************************/
void addError(void) {
  if ((millis() - g_last_error_timer) > ERROR_DEBOUNCE) {
    g_last_error_timer = millis(); 
    g_game_errors++; 
    g_game_newerror = true;       
  }
}

/*******************************************
 *  Read and ebounce GPIO
 *  - read gpio tree times
 *  - if all readings == desired return true
 *  - else return false
 *******************************************/
boolean read_with_debounce(byte gpio, byte desired) {
  byte i;
  boolean res;
  res = true;
  for (i=0; i<3; i++) {
    if (digitalRead(gpio) != desired) {
      res = false;
      break;
    }
  }
  return(res);
}
/**********************************************************************
 * Game State-Machine
 * S_READY             - Ready: 
 *                       - LED: WHITE
 *                       - Display: last result (if exists)
 *                       - Set g_result_disp_start = millis() to toggle last result (Error / time)
 *                     -> S_START_KIDSGAME if Button pressed 
 *                     -> S_CHECK_RFID     if New RFID found 
 * S_START_KIDSGAME    - Start Kidsgame: 
 *                       - LED: Blue
 *                       - Display: "KIDS"
 *                       - g_kidsgame = true
 *                     -> S_GAME_READY if DISPLAY_TIMEOUT ms passed
 * S_CHECK_RFID        - Check RFID Token: 
 *                       - LED: Yellow
 *                       - Display: "RFID"
 *                       - send UID to GameServer and wait for Result (Timeout 5 seconds)
 *                     -> S_RFID_NACK if result = NACK
 *                     -> S_RFID_ACK if result = ACK
 * S_RFID_NACK         - RFID Token not valid: 
 *                       - LED: RED
 *                       - Display: "FAIL"
 *                     -> S_READY if DISPLAY_TIMEOUT ms passed
 * S_RFID_ACK         - RFID Token is valid: 
 *                       - LED: Green
 *                       - Display: " OK "
 *                       - g_kidsgame = false
 *                     -> S_GAME_READY if DISPLAY_TIMEOUT ms passed
 * S_GAME_READY        - Game has been started
 *                       - waiting for startcondition (contact to startpoint)
 *                       - SHOW "----"
 *                       - LED unchanged (green: Challenge, blue: Kidsgame) 
 *                       - g_game_errors = 0;
 *                     -> S_GAME_STARTED if contact to one endpoint:
 *                        g_GpioStart = HOTWIRE_LEFT / HOTWIRE_RIGHT        
 *                        g_GpioEnd =  HOTWIRE_RIGHT / HOTWIRE_LEFT 
 * S_GAME_STARTED      - Start Condition occured
 *                       - SHOW "GOGO"                          
 *                       - wait to release StartPoint
 *                     -> S_GAME_RUNNING 
 *                        - if contact to g_GpioStart released
 *                        - g_game_start = NOW
 * S_GAME_RUNNING      - Game is running
 *                       - Add error if contact to HOTWIRE_WIRE using updateGameError();
 *                       - SHOW Time / Error using updateGameDisplay();
 *                       - LED unchanged (green: challeng, blue: Kidsgame)
 *                     -> S_GAME_Started 
 *                        - if contact to g_GpioStart
 *                        - AND g_game_errors == 0
 *                     -> S_GAME_Ended
 *                        - if contact to g_GpioEnd
 *                        - g_game_time = NOW - g_game_start
 * S_GAME_ENDED        - Game finished
 *                       - Display: "GOAL"
 *                     -> S_STORE_RESULT 
 *                        - if NOT g_kidsgame 
 *                     -> S_READY 
 *                        - if g_kidsgame
 *                        - AND DISPLAY_TIMEOUT ms passed
 * S_STORE_RESULT      - Send Challenge result to GameServer
 *                       - send result to Gameserver
 *                       - Wait for ACK / NACK
 *                     -> S_SAVE_OK if ACK
 *                     -> S_SAVE_ERROR if NACK
 * S_SAVE_OK           - Result was stored             
 *                     - Display: "SAVE"
 *                     -> S_READY 
 *                        - if DISPLAY_TIMEOUT ms passed
 * S_SAVE_ERROR        - Result was NOT stored            
 *                     - LED: Purple
 *                     - Display: "ERR"
 *                     -> S_READY 
 *                        - DISPLAY_TIMEOUT ms passed
 *                        - AND if Kidsbutten pressed 
 *                        - AND if Wirecontact
 ***********************************************************************/
void Gamecontroller() {
  boolean state_changed;
  byte i;
  state_changed = (g_state != g_new_state);
  if (state_changed) {
    g_state = g_new_state;
    DEBUG_PRINT(F("S:"));
    DEBUG_PRINTLN(g_state);
  }
  switch (g_state) {    
  // -------------------------------------------------------------------------
  case S_READY:                                     // STATE 1: Ready
    // - LED: RED
    // - Display: last result (if exists) else show "37c3"
    // -> S_START_KIDSGAME if Button pressed 
    // -> S_CHECK_RFID     if New RFID found     
    if (state_changed) {
      SetLED(255,255,255);                         // LED: WHITE
      if (g_game_time == 0) {                      // Display: HOHO (no last result available)
        setDisplayString ("37c3");                 // Display: "37c3"
        delay(100);
        setDisplayString ("37c3");                 // Display: "37c3"
        DEBUG_PRINTLN(F("37C3"));        
      } else {
        updateResultDisplay();  
      }
      g_result_disp_start = millis();               
    }
    if (g_game_time != 0) {                         // Display: last result (if exists)
      updateResultDisplay();  
    }    
    if (read_with_debounce(HOTWIRE_BUTTON, LOW)) {        // Button pressed?      
      g_new_state = S_START_KIDSGAME;
    }
    if (newTag()) {                                 // New RFID Tag found?
      g_new_state = S_CHECK_RFID;
    }
    break;
  // -------------------------------------------------------------------------
  case S_START_KIDSGAME:                            // STATE 2: Start Kidsgame
    // - LED: Blue
    // - Display: "HAVE" - "FUN"
    // - g_kidsgame = true
    // -> S_GAME_READY if DISPLAY_TIMEOUT ms passed      
    if (state_changed) {
      g_state_timer = millis();
      SetLED(0,0,255);                              // LED: Blue
      setDisplayString ("HAVE");                    // Display: "HAVE"
      delay(1000);
      setDisplayString ("FUN ");                    // Display: "FUN"
      g_kidsgame = true;        
    }
    if ((millis() - g_state_timer) > DISPLAY_TIMEOUT) {
      g_new_state = S_GAME_READY;
    }
    break;
  // -------------------------------------------------------------------------
  case S_CHECK_RFID:                                // STATE 3: Check RFID Token
    // - LED: Yellow
    // - Display: "RFID"
    // - send UID to GameServer and wait for Result (Timeout 5 seconds)
    // -> S_RFID_NACK if result = NACK
    // -> S_RFID_ACK if result = ACK
    if (state_changed) {       
      g_state_timer = millis(); 
      SetLED(255,255,0);                            // LED: Yellow
      setDisplayString ("RFID");                    // Display: "RFID"
      if (check_UID()) {                            // send UID to GameServer 
        g_new_state = S_RFID_ACK;
      } else {
        g_new_state = S_RFID_NACK;
      }        
      while ((millis() - g_state_timer) < DISPLAY_TIMEOUT) {
        delay(50);
      }
    }
    break;
  // -------------------------------------------------------------------------
  case S_RFID_NACK:                                 // STATE 4: RFID Token not valid
    // - LED: RED
    // - Display: "FAIL"
    // -> S_READY if DISPLAY_TIMEOUT ms passed      
    if (state_changed) {
      g_state_timer = millis();
      SetLED(255,0,0);                              // LED: RED
      setDisplayString ("FAIL");                    // Display: "FAIL"
    }
    if ((millis() - g_state_timer) > DISPLAY_TIMEOUT) {
      g_new_state = S_READY;
    }      
    break;
  // -------------------------------------------------------------------------
  case S_RFID_ACK:                                 // STATE 5: RFID Token is valid
    // - LED: Green
    // - Display: "----"
    // - g_kidsgame = false
    // -> S_GAME_READY if DISPLAY_TIMEOUT ms passed      
    if (state_changed) {
      g_state_timer = millis();
      SetLED(0,255,0);                              // LED: green      
      setDisplayString ("----");                    // Display: "----"
      g_kidsgame = false;        
    }
    if ((millis() - g_state_timer) > DISPLAY_TIMEOUT) {
      g_new_state = S_GAME_READY;
    }      
    break;
  // -------------------------------------------------------------------------
  case S_GAME_READY:                               // STATE 6: Game has been started
     // - waiting for startcondition (contact to startpoint)
     // - SHOW "----"
     // - LED unchanged (green: Challenge, blue: Kidsgame)
     // - g_game_errors = 0;
     // -> S_GAME_STARTED if contact to one endpoint:
     //    - g_GpioStart = HOTWIRE_LEFT / HOTWIRE_RIGHT        
     //    - g_GpioEnd =  HOTWIRE_RIGHT / HOTWIRE_LEFT 
    if (state_changed) {                      
      setDisplayString ("----");                    // Display: "----"      
      g_game_errors = 0;
    }
    if (read_with_debounce(HOTWIRE_LEFT, LOW)) {    // Start = Left side      
      DEBUG_PRINT(F("Start: left"));
      g_GpioStart = HOTWIRE_LEFT;
      g_GpioEnd = HOTWIRE_RIGHT;
      g_new_state = S_GAME_STARTED;               
    }
    if (read_with_debounce(HOTWIRE_RIGHT, LOW)) {   // Start = Right side          
      DEBUG_PRINT(F("Start: right"));
      g_GpioStart = HOTWIRE_RIGHT;
      g_GpioEnd = HOTWIRE_LEFT;
      g_new_state = S_GAME_STARTED;               
    }
    break;
  // -------------------------------------------------------------------------    
  case S_GAME_STARTED:                             // STATE 7: Start Condition occured
    // - SHOW "GOGO"
    // - wait to release StartPoint
    // -> S_GAME_RUNNING 
    // - if contact to g_StartPoint released
    // - g_game_start = NOW
    if (state_changed) {                
      showTemplate(MT_GOGO);                       // - Display: "GOGO"        
    }
    if (read_with_debounce(g_GpioStart, HIGH)) {   // Start released    
      g_game_start = millis();        
      g_new_state = S_GAME_RUNNING ;
    }
    break;
  // -------------------------------------------------------------------------    
  case S_GAME_RUNNING:                             // STATE 8: Game is running
    // - Add error if contact to HOTWIRE_WIRE using updateGameError();
    // - SHOW Time / Error using updateGameDisplay();
    // - LED unchanged (green: challeng, blue: Kidsgame)
    // -> S_GAME_STARTED 
    //    - if contact to g_GpioStart
    //    - THIS HAS BEEN REMOVED [AND g_game_errors == 0]
    // -> S_GAME_ENDED
    //    - if contact to g_GpioEnd
    //    - g_game_time = NOW - g_game_start
    if (read_with_debounce(HOTWIRE_WIRE, LOW)) {   // Add Error    
      addError();
    }
    updateGameDisplay();
    if (read_with_debounce(g_GpioStart, LOW)) {    // Restart game (if Errors = 0)    
      // THIS HAS BEEN REMOVED  if (g_game_errors == 0) {
      g_game_errors = 0;
      g_new_state = S_GAME_STARTED;
      // THIS HAS BEEN REMOVED }
    }
    if (read_with_debounce(g_GpioEnd, LOW)) {      // End if game    
      g_game_time = millis() - g_game_start;
      g_new_state = S_GAME_ENDED;
    }
    break;
  // -------------------------------------------------------------------------    
  case S_GAME_ENDED:                               // STATE 9: Game finished
    // - Display: "GOAL"
    // -> S_STORE_RESULT 
    //    - if NOT g_kidsgame 
    // -> S_READY 
    //    - if g_kidsgame
    //    - AND DISPLAY_TIMEOUT ms passed
    if (state_changed) {
      g_state_timer = millis();        
      setDisplayString ("GOAL");                    // Display: "GOAL"
    }
    if (!g_kidsgame) {
        g_new_state = S_STORE_RESULT;
    } 
    if (g_kidsgame) {
      g_uidlen = 4;
      for (i=0; i++; i<RFIDBUFLEN) {
        g_uid[i]            = 0;
      }
      g_new_state = S_STORE_RESULT;
      // if ((millis() - g_state_timer) > DISPLAY_GOAL_TIMEOUT) {
      //   g_new_state = S_READY;
      // }
    }       
    break;
  // -------------------------------------------------------------------------    
  case S_STORE_RESULT:                             // STATE 10: send Challenge-Result to GameServer
    // - send result to Gameserver
    // - Wait for ACK / NACK
    // -> S_SAVE_OK if ACK
    // -> S_SAVE_ERROR if NACK
    if (store_Result()) {
      g_new_state = S_SAVE_OK;
    } else{
      g_new_state = S_SAVE_ERROR;
    }
    break;
  // -------------------------------------------------------------------------    
  case S_SAVE_OK:                                  // STATE 11: Result was stored             
    // - Display: "SAVE"
    // -> S_READY 
    //    - if DISPLAY_TIMEOUT ms passed
    if (state_changed) {
      g_state_timer = millis();        
      setDisplayString ("SAVE");                    // Display: "SAVE"
    }
    if ((millis() - g_state_timer) > DISPLAY_TIMEOUT) {      
      g_new_state = S_READY;      
    }
    break;
  // -------------------------------------------------------------------------    
  case S_SAVE_ERROR:                               // STATE 12: Result was NOT stored            
    // - LED: BLINK Purple
    // - Display: "ERR"
    // -> S_READY 
    //    - DISPLAY_TIMEOUT ms passed
    //    - AND if Kidsbutten pressed 
    //    - AND if Wirecontact
    if (state_changed) {
      g_state_timer = millis();        
      g_led_blink_timer  = millis();
      g_ledblinkstate = true;        
      setDisplayString ("ERR ");                   // Display: "ERR "
    }
    if ((millis() - g_led_blink_timer) > BLINK_LED) {
      g_led_blink_timer  = millis();
      g_ledblinkstate != g_ledblinkstate;
    }
    if (g_ledblinkstate) {
      SetLED(255,0,255);                           // LED: Purple  
    } else {
      SetLED(0,0,0);                               // LED: off 
    }
    // Continue after 
    // connect Handle to wire (Error)
    // Press and release Button 
    if ((millis() - g_state_timer) > DISPLAY_TIMEOUT) {
      if (read_with_debounce(HOTWIRE_WIRE, LOW)) {
        if (read_with_debounce(HOTWIRE_BUTTON, LOW)) {
          SetLED(255,255,255);                           // LED: White
          while (read_with_debounce(HOTWIRE_BUTTON, LOW)) {
            delay(100);
          }
          delay(250);
          SetLED(0,0,0);                           // LED: off
          delay(250);
          g_new_state = S_READY;
        }        
      }        
    }
    break;
  }  
}

   
/************
 * Main Loop
 ************/
void loop() {
  Gamecontroller();
}    
