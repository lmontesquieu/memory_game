/***************************************************
Code for Simon-like memory game, using as "tones"
stored mp3 files from a DFPlayer Mini.
 ****************************************************/
#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

const int LED[] = {2, 3, 4}; // LED pins
const int BUTTON[] = {6, 7, 8}; // Button pins
const int loopDelayMS = 100; // Loop Delay in milliseconds
const int gameLoopDelayMS = 800; // Game-loop Delay in milliseconds

// Game states
enum { START, SELECT, PLAY, WIN, LOSE, JUKEBOX } state = START;
// Game difficulties
enum { EASY, HARD } difficulty = EASY;
int val1, val2, val3 = LOW; // To store button inputs

// Just a "high enough" number to generate the random sequence
const int SEQUENCE_LENGTH = 50;
int random_sequence[SEQUENCE_LENGTH];
// Playable songs excludes start song, win and lose songs.
// Playable songs stored in folders named 01, 02, etc.
// Jukebox songs (full songs) are also stored in a separate folder.
const int PLAYABLE_SONGS = 7;
// Number of music segments on each of the playable songs' folders.
// These segments correspond to the sound heard at each button press
// For testing, these numbers can be reduced, e.g. to 3, for winning at the
// third press.
const int files_per_folder[PLAYABLE_SONGS] = {20,18,18,20,15,11,17};

const int START_SONG_FOLDER = 98;
const int GAMEOVER_SONG_FOLDER = 99;
const int WIN_SONG_FOLDER = 97;
const int JUKEBOX_FOLDER = 95;

// Using initialization from DFPlayer's wiki:
// https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299#Connection_Diagram
// Note: On Arduino Pro, I observed conflicts when using both the USB/Serial port
// (for debugging) and the software serial.
#if (defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO) || defined(ESP8266) || defined(ARDUINO_AVR_PRO))   // Using a soft serial port
#include <SoftwareSerial.h>
SoftwareSerial softSerial(/*rx =*/10, /*tx =*/11);
#define FPSerial softSerial
#elif (defined(ARDUINO_SAM_ZERO)) // Arduino M0 can use a dedicated serial port for the DFPlayer
#define FPSerial Serial1
#else
#define FPSerial Serial
#endif

DFRobotDFPlayerMini myDFPlayer;

void setup() {
  #if (defined ESP32)
    FPSerial.begin(9600, SERIAL_8N1, /*rx =*/D3, /*tx =*/D2);
  #else
    FPSerial.begin(9600);
  #endif

  for(int x=0; x<3; x++) {
    pinMode(LED[x], OUTPUT);
  }
  for(int x=0; x<3; x++) {
    pinMode(BUTTON[x], INPUT);
  }
  // provide some "random" seed
  randomSeed(analogRead(0));

  //Use serial to communicate with the DFPlayer.
  if (!myDFPlayer.begin(FPSerial, /*isACK = */true, /*doReset = */true)) {
    // show some blinking pattern in case something went wrong while initializing the DFPlayer
    while(true){
      while(true){
        digitalWrite(LED[0], HIGH);
        delay(100);
        digitalWrite(LED[0], LOW);
        delay(500);
      }
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }

  //DFPlayer Mini online.
  myDFPlayer.EQ(DFPLAYER_EQ_CLASSIC); // Equalize for classical music
  myDFPlayer.enableDAC();  //Enable On-chip DAC
  myDFPlayer.volume(30);  //Set volume value. From 0 to 30
}

void show_sequence(int showsteps, int folder, int led_sequence[]) {
  //Show the player the sequence he/she should enter
  for(int i=0; i<showsteps; i++) {
    int led1 = (led_sequence[i] & 1);
    int led2 = (led_sequence[i] & 2) >> 1;
    int led3 = (led_sequence[i] & 4) >> 2;
    digitalWrite(LED[0], led1);
    digitalWrite(LED[1], led2);
    digitalWrite(LED[2], led3);
    myDFPlayer.playFolder(folder,i+1);
    unsigned long times = millis();
    while(true){
      if (myDFPlayer.available()) {
        if(myDFPlayer.readType() == DFPlayerPlayFinished){
          //Finished playing
          break;
        }
      }
      if((millis() - times) > 5000){
        //Account for a possible error on the DFPlayer.
        //Timer expired while showing
        myDFPlayer.pause();
        break;
      }
    }
    digitalWrite(LED[0], LOW);
    digitalWrite(LED[1], LOW);
    digitalWrite(LED[2], LOW);
    // Allow a visual "change" in case the same led is consecutively repeated
    delay(50);
  }
}

bool read_sequence(int steps, int folder, int led_sequence[]) {
  //Reading sequence (player's inputs)
  for(int i=0; i<steps; i++) {
    bool buttons_pressed = false;
    while(true){
      int _val1 = digitalRead(BUTTON[0]);
      int _val2 = digitalRead(BUTTON[1]);
      int _val3 = digitalRead(BUTTON[2]);
      if (_val1==HIGH || _val2==HIGH || _val3==HIGH){
        // wait some time to allow for simultaneous key presses (and avoid bouncing)
        delay(80);
        // read again after delay
        val1 = digitalRead(BUTTON[0]);
        val2 = digitalRead(BUTTON[1]);
        val3 = digitalRead(BUTTON[2]);
        // has input remained constant for the established delay time?
        if ((val1==_val1) & (val2==_val2) & (val3==_val3)){
          buttons_pressed = true;
        }
      }
      if (buttons_pressed){
        int code = (val1) | (val2 << 1) | (val3 << 2);
        //code contains the decimal value from the buttons pressed
        if(code == led_sequence[i]){
          digitalWrite(LED[0], val1);
          digitalWrite(LED[1], val2);
          digitalWrite(LED[2], val3);
          myDFPlayer.playFolder(folder,i+1);
          unsigned long times = millis();
          while(true){
            if (myDFPlayer.available()) {
              if(myDFPlayer.readType() == DFPlayerPlayFinished){
                //Finished playing
                break;
              } 
            }
            // Account for a possible error on the DFPlayer.
            // If a segment is longer than 5 seconds, this would just cut it
            if((millis() - times) > 5000){
              //Timer expired while reading
              myDFPlayer.pause();
              break;
            }
          }
          digitalWrite(LED[0], LOW);
          digitalWrite(LED[1], LOW);
          digitalWrite(LED[2], LOW);
          val1 = val2 = val3 = LOW;
          break;
        } else {
          //Wrong key pressed!
          return false;
        }
      }
    }
  }
  // Well done!
  return true;
}

void show_start_sequence(){
  myDFPlayer.playFolder(START_SONG_FOLDER,1);
  unsigned long times = millis();
  unsigned long displaytimes;
  unsigned short display = 4;
  while(true){
    displaytimes = millis();
    if ((displaytimes % 350) == 0){
      // just a rotating sequence on the leds
      display = ((display << 1) | ((display & 4) >> 2)) & 7;
      digitalWrite(LED[0], (display & 1));
      digitalWrite(LED[1], (display & 2));
      digitalWrite(LED[2], (display & 4));
    }
    if (myDFPlayer.available()) {
      if(myDFPlayer.readType() == DFPlayerPlayFinished){
        //Finished playing start song
        break;
      } 
    }
    if((millis() - times) > 10000){
      //Timer expired after 10 seconds. Just in case.
      myDFPlayer.pause();
      break;
    }
    // required delay for the led sequence's modulo operation
    delay(1);
  }
  digitalWrite(LED[0], LOW);
  digitalWrite(LED[1], LOW);
  digitalWrite(LED[2], LOW);
  return;
}

void show_gameover_sequence(){
  myDFPlayer.playFolder(GAMEOVER_SONG_FOLDER,1);
  unsigned long times = millis();
  unsigned long displaytimes;
  unsigned short display = 7;
  while(true){
    displaytimes = millis();
    if ((displaytimes % 100) == 0){
      // just a blinking sequence on the leds
      display = display ^ 7;
      digitalWrite(LED[0], (display & 1));
      digitalWrite(LED[1], (display & 2));
      digitalWrite(LED[2], (display & 4));
    }
    if (myDFPlayer.available()) {
      if(myDFPlayer.readType() == DFPlayerPlayFinished){
        //Finished playing "game over" song
        break;
      } 
    }
    if((millis() - times) > 10000){
      //Timer expired after 10 seconds. Just in case.
      myDFPlayer.pause();
      break;
    }
    // required delay for the led sequence's modulo operation
    delay(1);
  }
  digitalWrite(LED[0], LOW);
  digitalWrite(LED[1], LOW);
  digitalWrite(LED[2], LOW);
  return;
}

void show_win_sequence(){
  myDFPlayer.playFolder(WIN_SONG_FOLDER,1);
  unsigned long times = millis();
  unsigned long displaytimes;
  unsigned short display = 1;
  short direction = 1;
  while(true){
    displaytimes = millis();
    if ((displaytimes % 150) == 0){
      // just a continuous L->R, then R->L sequence on the leds
      if (direction == 1) {
        display = (display << 1);
      } else {
        display = (display >> 1);
      }
      if (display == 4) {
        direction = 0;
      }
      if (display == 1) {
        direction = 1;
      }
      digitalWrite(LED[0], (display & 1));
      digitalWrite(LED[1], (display & 2));
      digitalWrite(LED[2], (display & 4));
    }
    if (myDFPlayer.available()) {
      if(myDFPlayer.readType() == DFPlayerPlayFinished){
        //Finished playing winning song
        break;
      } 
    }
    if((millis() - times) > 10000){
      //Timer expired after 10 seconds. Just in case.
      myDFPlayer.pause();
      break;
    }
    // required delay for the led sequence's modulo operation
    delay(1);
  }
  digitalWrite(LED[0], LOW);
  digitalWrite(LED[1], LOW);
  digitalWrite(LED[2], LOW);
  return;
}

void loop() {
  int selected_song = 1;
  // Game's state machine
  switch (state) {
    case START:
      //Starting game
      show_start_sequence();
      state = SELECT;
      break;
    case SELECT:
      while (true){
        val1 = digitalRead(BUTTON[0]);
        val2 = digitalRead(BUTTON[1]);
        val3 = digitalRead(BUTTON[2]);
        if (val1 == HIGH){
          difficulty = EASY;
          break;
        } else if (val2 == HIGH){
          difficulty = HARD;
          break;
        } else if (val3 == HIGH){
          state = JUKEBOX;
          break;
        }
      }
      val1 = val2 = val3 = LOW;
      if(state != JUKEBOX){
        state = PLAY;
      }
      break;
    case PLAY:
      // generate sequence as per selected difficulty
      for (int i=0; i<SEQUENCE_LENGTH; i++){
        while(true){
          random_sequence[i] = random(1,8);
          if(difficulty==EASY){
            // EASY sequence is a single button at once
            // therefore, discard values with multiple keys (3,5,6,7)
            if((random_sequence[i]==1)||(random_sequence[i]==2)||(random_sequence[i]==4)){
              break;
            }
          } else {
            break;
          }
        }
      }
      // random function's higher limit is exclusive (therefore, adding 1)
      selected_song = random(1,PLAYABLE_SONGS+1);
      for(int i=1; i<=files_per_folder[selected_song-1]; i++){
        show_sequence(i, selected_song, random_sequence);
        bool success = read_sequence(i, selected_song, random_sequence);
        if(!success){
          state = LOSE;
          break;
        } else {
          //Partial-sequence completed
          delay(gameLoopDelayMS);
        }
      }
      if (state!=LOSE){
        //Full sequence completed! You won!
        state = WIN;
      }
      break;
    case WIN:
      //You win! Are you Louis Armstrong?
      show_win_sequence();
      state = START;
      break;
    case LOSE:
      //You lose! Better luck next time
      show_gameover_sequence();
      state = START;
      break;
    case JUKEBOX:
      //Playing in loop folder JUKEBOX_FOLDER
      myDFPlayer.loopFolder(JUKEBOX_FOLDER);
      delay(200);
      while (true){
        val1 = digitalRead(BUTTON[0]);
        val2 = digitalRead(BUTTON[1]);
        val3 = digitalRead(BUTTON[2]);
        if (val1 == HIGH){
          //If 1 or 2 are pressed, start game
          difficulty = EASY;
          state = PLAY;
          break;
        } else if (val2 == HIGH){
          difficulty = HARD;
          state = PLAY;
          break;
        } else if (val3 == HIGH){
          //Pressed 3 on Jukebox mode (skip to next song)
          myDFPlayer.next();
          delay(200);
        }
      }
      myDFPlayer.pause();
      myDFPlayer.disableLoop();
      delay(1000);
      break;
    default:
      // Should not happen (TM)
      state = START;
      break;
  }
  delay(loopDelayMS);
}

