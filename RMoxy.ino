/////////////////////////////////////////////////////////////////////////
//                                                                     //
//   Minipops drummer firmware for Music Thing Modular Radio Music     //
//   by Johan Berglund, May 2021                                       //
//                                                                     //
/////////////////////////////////////////////////////////////////////////

#include <Audio.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Wire.h>

// WAV files converted to code by wav2sketch
// Minipops 7 samples downloaded from http://samples.kb6.de

#include "AudioSampleAsmpts_1_kick.h"
#include "AudioSampleAsmpts_2_stick.h"
#include "AudioSampleAsmpts_3_snare.h"
#include "AudioSampleAsmpts_4_clap.h"
#include "AudioSampleAsmpts_5_midtom.h"
#include "AudioSampleAsmpts_6_hitom.h"
#include "AudioSampleAsmpts_7_closedhat.h"
#include "AudioSampleAsmpts_8_openhat.h"

#include "patterns.h" // Patterns for rhythm section

/*
 RADIO MUSIC is the intended hardware for this firmware
 https://github.com/TomWhitwell/RadioMusic

 Audio out: Onboard DAC, teensy3.1 pin A14/DAC

 Bank Button: 2
 Bank LEDs 3,4,5,6
 Reset Button: 8
 Reset LED 11
 Reset CV input: 9
 Channel Pot: A9
 Channel CV: A8 // check
 Time Pot: A7
 Time CV: A6 // check
 SD Card Connections:
 SCLK 14
 MISO 12
 MOSI 7
 SS   10
 */
#define LED0 3
#define LED1 4
#define LED2 5
#define LED3 6
#define RESET_LED 11   // Reset LED indicator
#define PATTERN_POT A9 // pin for Channel pot -- RMoxy Pattern
#define RESET_CV 20    // pin for Channel CV -- RMoxy Reset CLK (20/A6)
#define MUTE_POT                                                               \
  A7 // pin for Time pot -- RMoxy Tempo (full CCW for external CLK)
#define MUTE_CV                                                                \
  A8 // pin for Time CV -- RMoxy binary muting, 0V on input or unconnected =
     // nothing muted
#define RESET_BUTTON 8 // Reset button -- RMoxy RUN/SET
#define CLOCK_CV 9     // Reset pulse input -- RMoxy CLK

#define ADC_BITS 13
#define ADC_MAX_VALUE (1 << ADC_BITS)

#define MUTING_MARGIN 100

// GUItool: begin automatically generated code
AudioPlayMemory playMem1; // xy=247,53
AudioPlayMemory playMem2; // xy=248,90
AudioPlayMemory playMem3; // xy=249,125
AudioPlayMemory playMem4; // xy=251,161
AudioPlayMemory playMem5; // xy=252,194
AudioPlayMemory playMem6; // xy=254,227
AudioPlayMemory playMem7; // xy=257,260
AudioPlayMemory playMem8; // xy=259,293
AudioMixer4 mixer1;       // xy=517,133
AudioMixer4 mixer2;       // xy=557,235
AudioMixer4 mixer3;       // xy=707,178
AudioOutputAnalog dac1;   // xy=883,177
AudioConnection patchCord1(playMem1, 0, mixer1, 0);
AudioConnection patchCord2(playMem2, 0, mixer1, 1);
AudioConnection patchCord3(playMem3, 0, mixer1, 2);
AudioConnection patchCord4(playMem4, 0, mixer1, 3);
AudioConnection patchCord5(playMem5, 0, mixer2, 0);
AudioConnection patchCord6(playMem6, 0, mixer2, 1);
AudioConnection patchCord7(playMem7, 0, mixer2, 2);
AudioConnection patchCord8(playMem8, 0, mixer2, 3);
AudioConnection patchCord9(mixer1, 0, mixer3, 0);
AudioConnection patchCord10(mixer2, 0, mixer3, 1);
AudioConnection patchCord11(mixer3, dac1);
// GUItool: end automatically generated code

// Pointers
AudioPlayMemory *rtm[8]{&playMem1, &playMem2, &playMem3, &playMem4,
                        &playMem5, &playMem6, &playMem7, &playMem8};

int currentStep = 0; // pattern step
long patNum = 0;     // selected rhythm pattern
long patProb = 0;
int lastPat = 0;
int nextPat = 0;
int tempoRead = 0;
int muteCVRead = 0;
int mutePotRead = 0;
int mute = 0;
bool buttonPress = 0;
bool buttonPressRead = 0;
bool buttonPressDebounce = 0;
bool buttonPressLast = 0;
bool clkNow = 0;
bool clkLast = 0;
bool resetRead = 0;
bool resetLast = 0;

unsigned long currentMillis = 0L;
unsigned long statusPreviousMillis = 0L;
unsigned long stepTimerMillis = 0L;
unsigned long debounceTimerMillis = 0L;
unsigned long debounceTime = 10;

void setup() {
  // put your setup code here, to run once:
  AudioMemory(50);
  dac1.analogReference(INTERNAL); // normal volume
  // dac1.analogReference(EXTERNAL); // louder
  mixer1.gain(0, 0.27);
  mixer1.gain(1, 0.27);
  mixer1.gain(2, 0.27);
  mixer1.gain(3, 0.27);
  mixer2.gain(0, 0.27);
  mixer2.gain(1, 0.27);
  mixer2.gain(2, 0.27);
  mixer2.gain(3, 0.27);
  mixer3.gain(0, 0.5);
  mixer3.gain(1, 0.5);

  analogReadRes(ADC_BITS);

  pinMode(RESET_BUTTON, INPUT);
  pinMode(CLOCK_CV, INPUT);
  pinMode(RESET_CV, INPUT);
  pinMode(RESET_LED, OUTPUT);
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
}

void loop() {
  currentMillis = millis();
  patNum = map(analogRead(PATTERN_POT), 0, ADC_MAX_VALUE, 0, 3);
  lastPat = floor(patNum);
  nextPat = ceil(patNum);
  patProb = 1 - (patNum - lastPat);
  buttonPressRead = digitalReadFast(RESET_BUTTON);

  if (buttonPressRead != buttonPressDebounce) {
    debounceTimerMillis = currentMillis;
  }

  if ((currentMillis - debounceTimerMillis) > debounceTime) {
    buttonPress = buttonPressRead;
  }

  buttonPressDebounce = buttonPressRead;

  // mutePotRead =

  // muteCVRead = map(analogRead(MUTE_CV), MUTING_MARGIN,
  //                  ADC_MAX_VALUE - MUTING_MARGIN, 255, 0);

  mute = map(analogRead(MUTE_POT), 0, ADC_MAX_VALUE, 255, 0);

  resetRead = digitalReadFast(RESET_CV);

  if (resetRead && !resetLast) { // if reset is going high, go to step 0
    currentStep = 0;
  }

  resetLast = resetRead;
  clkNow = digitalReadFast(CLOCK_CV);

  if (buttonPress && !buttonPressLast) {
    // TODO
  }

  if (clkNow && !clkLast) { // ext CLK rising or internal clock timer reach
    for (int i = 0; i < 8; i++) {
      float rnd = rand();
      int lastPatternBit = bitRead(pattern[lastPat][currentStep], 7 - i);
      int nextPatternBit = bitRead(pattern[nextPat][currentStep], 7 - i);
      bool play = (lastPatternBit && rnd > patProb) ||
                  (nextPatternBit && rnd < patProb);

      if (play && bitRead(mute, i)) {
        playRtm(i);
      }
    }

    currentStep++;
    stepTimerMillis = currentMillis; // reset interval timing for internal clock
  }

  clkLast = clkNow;
  resetRead = digitalReadFast(RESET_CV);

  if ((resetRead && !resetLast) || (currentStep == 32) ||
      pattern[patNum][currentStep] == 255) {
    currentStep = 0; // start over if we are at step 0 if we passed 15 or next
                     // step pattern value is 255 (reset)
  }

  resetLast = resetRead;
  buttonPressLast = buttonPress;
}

// play rhythm samples
void playRtm(int i) {
  digitalWrite(LED0, 0);
  digitalWrite(LED1, 0);
  digitalWrite(LED2, 0);
  digitalWrite(LED3, 0);

  switch (i) {
  case 0:
    digitalWrite(LED0, 1);
    rtm[i]->play(AudioSampleAsmpts_1_kick); // GU
    break;
  case 1:
    digitalWrite(LED0, 1);
    rtm[i]->play(AudioSampleAsmpts_2_stick); // BG2
    break;
  case 2:
    digitalWrite(LED1, 1);
    rtm[i]->play(AudioSampleAsmpts_3_snare); // BD
    break;
  case 3:
    digitalWrite(LED1, 1);
    rtm[i]->play(AudioSampleAsmpts_4_clap); // CL
    break;
  case 4:
    digitalWrite(LED2, 1);
    rtm[i]->play(AudioSampleAsmpts_5_midtom); // CW
    break;
  case 5:
    digitalWrite(LED2, 1);
    rtm[i]->play(AudioSampleAsmpts_6_hitom); // MA
    break;
  case 6:
    digitalWrite(LED3, 1);
    rtm[i]->play(AudioSampleAsmpts_7_closedhat); // CY
    break;
  case 7:
    digitalWrite(LED3, 1);
    rtm[i]->play(AudioSampleAsmpts_8_openhat); // QU
    break;
  }
}
