/* Fuel Rod Prop Code v2.1
   Modes:
   1. ReSupply Risky. Uncharged, 30 minute decay, 5 Red events disables.
   2. Demo Mode, Uncharged, 1 m decay, 2 Red disables.
   3. Sensitivity Setting.
   4. Search N Rescue, Precharged, 30 m decay, 9999 Red events disables.
   5. Hot Potato, Precharged, 30 minute decay, 5 Red events disables.
   6. Rush, Precharged, 5 minute decay, 9999 Red events disables.
   7. BoomBox Prop, Precharged, 9999 no decay, 9999 Red events disables.
   
   Created: March 6, 2016
   Author: Reid Bush
*/
#include <WS2812.h>
#include <EEPROM.h>
const byte ledPIN = 2; // LED WS2812 on D2
const byte audioPIN = 4; // Audio Out on D4
const byte shakePIN = A0; // Shake Sensor on A0
const byte chargePIN = 6; // Charge Button on D6
const byte battPIN = A1; // Batt Sense on A1
byte gameMode = 1; // What game mode (see above) are we in?
byte instability = 0; // 0-255 0=calm 128=warn 250=red alert.
int redCount = 0; // How many red alerts we allow till fuel is tainted. Countdown style, default is 5.
int redLimit = 5; // How many red alerts we allow before discharging.
int threshold = 1; // Threshold for shake detection. 1=max sensitivity. Pull value from EEPROM initially.
int p = 20; // test charge loop delay
int s = 20; // test charge loop delay
byte eepromHigh = 0; // Value to hold EEPROM high byte.
byte eepromLow = 0; // Value to hold EEPROM low byte.
int decayTime = 0; // How long minutes we allow the fuel to last. 9999 = no decay.
unsigned long nextCalm = 0; // next millis to decrement instability.
unsigned long decayEndTime = 0; // Our fuel charge is decayed by this time.
unsigned long decayThirdTime = 0; // 1/3 Our fuel charge is decayed by this time.
unsigned long decay2ThirdTime = 0; // 2/3 Our fuel charge is decayed by this time.
unsigned long preProgTimeOut = 0; // Hold prog button until we reach this time to enter Prog Mode.
unsigned long nextFlash = 0; // Time when the next locate flash should occur.
bool countdown = false; // If on, we go from green to red and explode at end.
bool found = false; // true if the unit was found in S&R mode.
bool gameOn = false; // Is the game mode enabled?
bool charged = false; // Is the canister charged? (Green) or not (Blue)
bool redMode = false; // Are we in red mode?
bool needSensLearn = false; // Do we need to learn sensitivity again? default is to take last value from EEPROM.
bool progMode = false; // Are we in Prog mode?
bool led0Charged = false; // Track if LED0 can be shaken.
bool led1Charged = false; // Track if LED1 can be shaken.
WS2812 LED(3); // Set num of LEDs to 3
cRGB value;

void setup() {
  Serial.begin(9600);
  // Set up PIN functions.
  pinMode(audioPIN, OUTPUT);
  pinMode(shakePIN, OUTPUT);
  digitalWrite(shakePIN,LOW);
  pinMode(shakePIN, INPUT);
  pinMode(chargePIN, INPUT);
  pinMode(battPIN, INPUT);
  // Set remaining PINs to prevent quiescent current.
  pinMode(3, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(17, INPUT_PULLUP);
  // LED setup section.
  LED.setColorOrderRGB();
  LED.setOutput(ledPIN);
  // Get sensitivity setting from EEPROM if found.
  eepromHigh = EEPROM.read(0); 
  eepromLow = EEPROM.read(1);
  threshold = word(eepromHigh,eepromLow);
  if (threshold > 1024 || threshold < 1) {
    // value is invalid. May be corrupted or not initialzed yet. Default = 1
    threshold = 1;
    EEPROM.write(0,highByte(threshold));
    EEPROM.write(1,lowByte(threshold));
  }
  // Get last game setting from EEPROM if found.
  gameMode = EEPROM.read(2); 
  if (gameMode > 7 || gameMode < 1) {
    // value is invalid. May be corrupted or not initialzed yet. Default = 1
    gameMode = 1;
    EEPROM.write(2,gameMode);
  }
  // Notify that setup is complete.
  allLEDs(1);
  delay(200);
  allLEDs(0);
  Serial.println(F("Device is initialized."));
  Serial.println(F("Firmware 2.1"));
  Serial.print(F("Threshold: "));
  Serial.println(threshold);
  Serial.print(F("Game Mode:"));
  Serial.println(gameMode);
  preProgTimeOut = millis() + 10000; // We start the device in prog mode, and wait 10 seconds for a shock input to change modes.
  progMode = true;
  showMode(gameMode);
  for (int i = 0; i < gameMode; i++) {
    beep(80);
    delay(300);
  }
}


void loop() {
  // Program mode selection
  if (progMode) {
    int shakeValue = 0;
    shakeValue = analogRead(shakePIN);
    if (shakeValue > 10) { // something is bumping us, assume its a programming request.
      Serial.print("prog shake: ");
      Serial.println(shakeValue);
      gameMode ++;
      if (gameMode > 7) {
        gameMode = 1;
      }
      showMode(gameMode);
      for (int i = 0; i < gameMode; i++) {
        beep(80);
        delay(333);
        preProgTimeOut = millis() + 5000;
      }
    }
  }
      
  // No input pressed and we passed the Prog 10 sec timeout, set game values, exit progMode
  if (millis() > preProgTimeOut && progMode) { 
    progMode = false;
    preProgTimeOut = 0;
    decayEndTime = millis() + (decayTime * 60000);
    nextCalm = millis();
    instability = 0;
    // Common Variables set here. Change if needed for specific modes.
    countdown = false; // Rush mode 5 minute countdown to blow.
    needSensLearn = false;
    gameOn = true;
    EEPROM.write(2,gameMode);

    switch (gameMode) {
      case 1: // Resupply Risky Mode
        led0Charged = false;
        led1Charged = false;
        charged = false;
        decayTime = 30; // Normally 30
        redLimit = 5;
        Serial.println("Mode 1. ReSupply Safe.");
        break;
      case 2: // DEMO mode.
        led0Charged = false;
        led1Charged = false;
        charged = false;
        decayTime = 1;
        redLimit = 2;
        threshold = 1;
        Serial.println("Mode 2. DEMO.");
        break;
      case 3: // Sensitivity Set
        led0Charged = false;
        led1Charged = false;
        charged = false;
        decayTime = 9999;
        redLimit = 9999;
        needSensLearn = true;
        threshold = 1;
        gameOn = false;
        Serial.println("Mode 3. Sensitivity Set.");
        break;
      case 4:// Search & Rescue
        led0Charged = true;
        led1Charged = true;
        charged = true;
        decayTime = 30; // Normally 30
        redLimit = 9999;
        allLEDs(0);
        Serial.println("Mode 4. Search & Rescue.");
        break;
      case 5: // Hot Potato
        led0Charged = true;
        led1Charged = true;
        charged = true;
        decayTime = 30; // Normally 30
        redLimit = 5;
        allLEDs(4); // Green
        Serial.println("Mode 5. Hot Potato.");
        break;
      case 6: // Rush
        led0Charged = true;
        led1Charged = true;
        charged = true;
        allLEDs(4); // Green
        decayTime = 5;
        redLimit = 9999;
        Serial.println("Mode 6. Rush");
        break;
      case 7: // BoomBox Prop Mode
        led0Charged = true;
        led1Charged = true;
        charged = true;
        decayTime = 9999;
        redLimit = 9999;
        Serial.println("Mode 7. BoomBox Prop.");
        break;
    }
    redCount = redLimit;
    decayEndTime = millis() + (decayTime * 60000);
    decayThirdTime = millis() + (decayTime * 60000) / 3;
    decay2ThirdTime = millis() + (decayTime * 120000) / 3;
  }

  // Sensitivity setting requested (mode 2)
  if (needSensLearn) {
    learnShockLevel();
  }

  // Game mode processing
  if (gameOn) {
    switch (gameMode) {
      case 1: // Default Resupply Risky Mode uncharged, 5 red, 30m decay, discharges
        if (charged) {
          shakeProcess(); // Check for shaking and process
          if (redCount == 0) abused(1); // Check to see if device is abused and needs total discharge.
          decayLEDs(1); // Check for decay state and process
        } else {
          allLEDs(3); // Set LEDs blue
          if (digitalRead(chargePIN) == LOW) checkCharge(); // Check for charging base and process
        }
        break;
      case 2: // Demo Mode
        if (charged) {
          shakeProcess(); // Check for shaking and process
          if (redCount == 0) abused(1); // Check to see if device is abused and needs total discharge.
          decayLEDs(1); // Check for decay state and process
        } else {
          allLEDs(3); // Set LEDs blue
          if (digitalRead(chargePIN) == LOW) checkCharge(); // Check for charging base and process
        }
        break;
      case 4: // Search and Rescue Mode - 3 sec beep till shaken, then 25 mins decay till dies - in base wins.
        if (analogRead(shakePIN) > 2 && !found) { // something is bumping us, assume we have been picked up.
          Serial.println("Found! Starting countdown.");
          found = true;
          // alert that we are counting down.
          allLEDs(4); // Green
          beep(100);
          delay(100);
          beep(100);
          delay(100);
          beep(100);
          delay(100);
          beep(100);
          delay(100);
          decayEndTime = millis() + (decayTime * 60000);
          decayThirdTime = millis() + (decayTime * 60000) / 3;
          decay2ThirdTime = millis() + (decayTime * 120000) / 3;
        }
        if (millis() > nextFlash) {
          if (millis() > decayEndTime) {
            // Ran out of time.
            allLEDs(2);
            gameMode = false;
            charged = false;
          } else {
            allLEDs(3);
            beep(300);
            delay(100);
            allLEDs(4);
            beep(50);
            allLEDs(0);
            nextFlash = millis() + 3000;
            value.r = 0;
            value.g = 0;
            value.b = 254;
            if (millis() > decayThirdTime) {
              LED.set_crgb_at(0, value); // Set value at LED found at index 0
              LED.sync(); // Sends the data to the LEDs
              nextFlash = millis() + 2000;
            }
            if (millis() > decay2ThirdTime) {
              LED.set_crgb_at(1, value); // Set value at LED found at index 1
              LED.sync(); // Sends the data to the LEDs
              nextFlash = millis() + 1000;
            }
            if (millis() > (decayEndTime - 20000)) {
              nextFlash = millis() + 500;
            }
          }
        }
        if (digitalRead(chargePIN) == LOW) { // placed in base, WIN.
          allLEDs(4); // Green
          beep(100);
          delay(200);
          beep(100);
          delay(200);
          beep(100);
          delay(200);
          beep(100);
          delay(200);
          gameOn = false;
        }
        break;
      case 5: // Hot Potato Game Mode charged, 5 reds, 30 mins, base discharges, explodes
        // Starts green goes red as it decays, base must be used to neutralize
        shakeProcess(); // Check for shaking and process
        if (redCount == 0) {
          abused(2); // Check to see if device is abused and needs to explode.
          allLEDs(2);
          gameOn = false;
        }
        decayLEDs(2); // Check for decay state and process
        if (digitalRead(chargePIN) == HIGH && !charged) {
          // Not in charge base, discharged, set it off
          abused(2);
          allLEDs(2);
          gameOn = false;
        }
        if (digitalRead(chargePIN) == LOW) {
          abused(1); // Check for charging base and discharge if found.
          led0Charged = true;
          led1Charged = true;
          charged = true;
          allLEDs(4); // Green
          redCount = redLimit;
          decayEndTime = millis() + (decayTime * 60000);
          decayThirdTime = millis() + (decayTime * 60000) / 3;
          decay2ThirdTime = millis() + (decayTime * 120000) / 3;
        }
        break;
      case 6: // Rush Game Mode
        //shakeProcess(); // shake but never disable
        if (digitalRead(chargePIN) == LOW) {
          // In base, 5 min countdown to blow.
          if (countdown) {
            // already in countdown mode
            decayLEDs(2);
            if (millis() > nextFlash) {
              beep(100);
              nextFlash = millis() + 3000;
              if (millis() > decayThirdTime) {
                nextFlash = millis() + 1000;
              }
              if (millis() > decay2ThirdTime) {
                nextFlash = millis() + 500;
              }
            }
            if (millis() > decayEndTime) {
              abused(2); //BOOM
              led0Charged = true;
              led1Charged = true;
              charged = true;
              allLEDs(4); // Green
              redCount = redLimit;
              decayEndTime = millis() + (decayTime * 60000);
              decayThirdTime = millis() + (decayTime * 60000) / 3;
              decay2ThirdTime = millis() + (decayTime * 120000) / 3;
            }
          } else { // Start Countdown mode.
            countdown = true;
            nextFlash = millis() + 5000;
            decayEndTime = millis() + (decayTime * 60000);
            decayThirdTime = millis() + (decayTime * 60000) / 3;
            decay2ThirdTime = millis() + (decayTime * 120000) / 3;
            beep(100);
            value.r = 255;
            value.g = 255;
            value.b = 0;
            LED.set_crgb_at(0, value); // Set value at LED found at index 0
            LED.set_crgb_at(1, value); // Set value at LED found at index 1
            LED.set_crgb_at(2, value); // Set value at LED found at index 2
            LED.sync(); // Sends the data to the LEDs
          }
        } else {
          // Not in base, back to green and countdown reset.
          if (countdown) {
            Serial.println("Mode 6. Rush - exiting countdown.");
            led0Charged = true;
            led1Charged = true;
            charged = true;
            allLEDs(4);
            countdown = false;
          }
        }
        break;
      case 7: // BoomBox Charged, No decay, No explode
        if (charged) { // Charged
          shakeProcess(); // Check for shaking and process
          if (digitalRead(chargePIN) == LOW) {
            abused(1); // Process discharge.
            allLEDs(3); // Set LEDS Blue
            while (digitalRead(chargePIN) == LOW) {
              // Waiting to be removed from can slot or base.
              delay(1);
            }
            delay(10000); // Wait 10 secs to ensure it wasnt just bumped.
          }
        } else { // Not Charged
          if (digitalRead(chargePIN) == LOW) {
            checkCharge(); // Process charge
            allLEDs(4); // Set LEDs Green
            while (digitalRead(chargePIN) == LOW) {
              // Waiting to be removed from can slot or base.
              delay(1);
            }
            delay(10000); // Wait 10 secs to ensure it wasnt just bumped.
          }
        }
        break;
    }
  } // End game mode processing
} // End main loop

void decayLEDs(byte decayMode) { // decayMode -1 normal decay to blue 2 decay to red.
    if (decayTime != 9999 && charged) {
    if (decayMode == 1) {
      value.r = 0;
      value.g = 0;
      value.b = 254;
    } else {
      value.r = 254;
      value.g = 0;
      value.b = 0;
    }
    if (millis() > decayThirdTime && led0Charged) {
      led0Charged = false;
      LED.set_crgb_at(0, value); // Set value at LED found at index 0
      LED.sync(); // Sends the data to the LEDs
      beep(100);
      Serial.println("1/3 of charge depleted.");
    }
    if (millis() > decay2ThirdTime && led1Charged) {
      led1Charged = false;
      LED.set_crgb_at(1, value); // Set value at LED found at index 1
      LED.sync(); // Sends the data to the LEDs
      beep(100);
      Serial.println("2/3 of charge depleted.");
    }
    if (millis() > decayEndTime && charged) {
      charged = false;
      beep(600);
      Serial.println("Charge depleted.");
    }
  }
}

void abused(byte abuseMode) { 
  Serial.println("Red Alert triggered.");
  led0Charged = false;
  led1Charged = false;
  charged = false;
  instability = 0;
  redCount = redLimit;
  if (abuseMode == 1) { // Discharge to blue
    value.r = 0;
    value.b = 128;
    for (p = 2; p < 16; p++) {
      beep(50);
      for(s = 0; s < 250; s++) {
        value.g = s;
        LED.set_crgb_at(0, value); // Set value at LED found at index 0
        LED.set_crgb_at(1, value); // Set value at LED found at index 1
        LED.set_crgb_at(2, value); // Set value at LED found at index 2
        LED.sync(); // Sends the data to the LEDs
        delay(p);
        s=s+(20-p);
      }
      beep(100);
      for(s = 250; s > 2; s--) {
        value.g = s;
        LED.set_crgb_at(0, value); // Set value at LED found at index 0
        LED.set_crgb_at(1, value); // Set value at LED found at index 1
        LED.set_crgb_at(2, value); // Set value at LED found at index 2
        LED.sync(); // Sends the data to the LEDs
        delay(p);
        s=s-(20-p);
      }
    }
    allLEDs(4); // Blue
  } else { // Explode to Red
    value.g = 0;
    value.b = 0;
    for (p = 16; p > 2; p--) {
      beep(100);
      for(s = 0; s < 250; s++) {
        value.r = s;
        LED.set_crgb_at(0, value); // Set value at LED found at index 0
        LED.set_crgb_at(1, value); // Set value at LED found at index 1
        LED.set_crgb_at(2, value); // Set value at LED found at index 2
        LED.sync(); // Sends the data to the LEDs
        delay(p);
        s=s+(20-p);
      }
      beep(100);
      for(s = 250; s > 2; s--) {
        value.r = s;
        LED.set_crgb_at(0, value); // Set value at LED found at index 0
        LED.set_crgb_at(1, value); // Set value at LED found at index 1
        LED.set_crgb_at(2, value); // Set value at LED found at index 2
        LED.sync(); // Sends the data to the LEDs
        delay(p);
        s=s-(20-p);
      }
    }
    allLEDs(2); // Red
    beep(5000);
    delay(5000);
    allLEDs(0); // Off
  }
}

void allLEDs(byte mode) { //0=off, 1=white, 2=Red, 3=Blue, 4=Green
  // We set all LEDs to one color a lot, so write a function to do that.
  value.r = 0;
  value.g = 0;
  value.b = 0;
  switch (mode) {
    case 1:
      value.r = 255;
      value.g = 255;
      value.b = 255;
    break;
    case 2:
      value.r = 255;
    break;
    case 3:
      value.b = 255;
    break;
    case 4:
      value.g = 255;
    break;
  }
  LED.set_crgb_at(0, value); // Set value at LED found at index 0
  LED.set_crgb_at(1, value); // Set value at LED found at index 1
  LED.set_crgb_at(2, value); // Set value at LED found at index 2
  LED.sync(); // Sends the data to the LEDs
  delay(2);
}

void shakeProcess() {
  // Process Shake Sensor
  if (analogRead(shakePIN) > threshold) {
    Serial.print("Instability/Shake Sense: ");
    Serial.print(instability);
    Serial.print(" / ");
    Serial.println(analogRead(shakePIN));
    if (instability >= 220) { // Redmode
      instability = 220;
      beep(200);
      if (!redMode) {
        redMode = true;
        redCount--;
      }
    } else {
      instability += 10;
    }
    value.b = 0;
    value.r = instability; // Amount of red = amount of instability.
    value.g = 220 - instability; // Amount of green = inverse of red.
    if (led0Charged) {
      LED.set_crgb_at(0, value); // Set value at LED found at index 0 if its still charged.
    }
    if (led1Charged) {
      LED.set_crgb_at(1, value); // Set value at LED found at index 1 if its still charged.
    }
    LED.set_crgb_at(2, value); // Set value at LED found at index 2
    LED.sync(); // Sends the data to the LEDs
    delay(2);
  }
  if (millis() > nextCalm) {
    if (instability <= 3) {
      instability = 0;
    } else {
      instability --;
    }
    nextCalm = millis() + 40;
    value.b = 0;
    value.r = instability;
    value.g = 220 - instability;
    if (led0Charged) {
      LED.set_crgb_at(0, value); // Set value at LED found at index 0
    }
    if (led1Charged) {
      LED.set_crgb_at(1, value); // Set value at LED found at index 1
    }
    LED.set_crgb_at(2, value); // Set value at LED found at index 2
    LED.sync();
    delay(2);
    if (instability > 220) {
      beep(400);
    }
    if (redMode && (instability < 220)) { // We are leaving redMode.
      redMode = false;
    }
  }
}

void learnShockLevel() {
    unsigned long learnTimeOut = 0; // learn mode runs for 5 seconds.
    unsigned long ledShowTime = 0; // Every 500ms switch LEDs.
    int shakeValue = 0;
    int ledShow = 0; // controls LED sequence
    // Listen for shake sensor to set sensitivity value for next 5 seconds. Blink red/green during learning phase.
    Serial.println("Sensitivity setting mode.");
    learnTimeOut = millis() + 5000;
    threshold = 1;
    allLEDs(0);
    ledShowTime = millis() + 500;
    while (learnTimeOut > millis()) {
      shakeValue = analogRead(shakePIN);
      if (shakeValue > 0) {
        Serial.print("Shake Value:");
        Serial.println(shakeValue);
      }
      if (threshold < shakeValue) {
        threshold = shakeValue;
        Serial.print("New Sensitivity:");
        Serial.println(threshold);
        learnTimeOut = millis() + 5000;
        value.b = 0;
        value.r = 128;
        value.g = 0;
        LED.set_crgb_at(2, value); // Set value at LED found at index 2
        LED.sync();
        delay(60);
        value.r = 0;
        LED.set_crgb_at(2, value); // Set value at LED found at index 2
        LED.sync();
        delay(2);
      }
      if (millis() > ledShowTime) { // LED Motion show during sensitivity setting.
        ledShowTime = millis() + 500;
        value.b = 0;
        value.r = 0;
        value.g = ledShow;
        LED.set_crgb_at(0, value); // Set value at LED found at index 0
        LED.sync();
        delay(2);
        if (ledShow > 0) {
          ledShow = 0;
        } else {
          ledShow = 250;
        }
      }
    }
    EEPROM.write(0,highByte(threshold));
    EEPROM.write(1,lowByte(threshold));
    needSensLearn = false;
    allLEDs(0);
    beep(200);
    delay(50);
    beep(200);
    Serial.print("Sensitivity:");
    Serial.println(threshold);
}

void showMode(byte gMode) { // Set the LEDs to a binary representation of game mode in white.
  allLEDs(0);
  value.r = 255;
  value.g = 255;
  value.b = 255;
  if (gMode & B00000001) {
    LED.set_crgb_at(0, value); // Set value at LED found at index 0
  }
  if (gMode & B00000010) {
    LED.set_crgb_at(1, value); // Set value at LED found at index 1
  }
  if (gMode & B00000100) {
    LED.set_crgb_at(2, value); // Set value at LED found at index 1
  }
  LED.sync(); // Sends the data to the LEDs
  delay(2);
}

void checkCharge() {
  // Charge input is low. Charging. Set LEDs green.
  led0Charged = true;
  led1Charged = true;
  charged = true;
  value.r = 0;
  value.b = 0;
  for (p = 16; p > 2; p--) {
    beep(100);
    for(s = 0; s < 250; s++) {
      value.g = s;
      LED.set_crgb_at(0, value); // Set value at LED found at index 0
      LED.set_crgb_at(1, value); // Set value at LED found at index 1
      LED.set_crgb_at(2, value); // Set value at LED found at index 2
      LED.sync(); // Sends the data to the LEDs
      delay(p);
      s=s+(20-p);
    }
    beep(100);
    for(s = 250; s > 2; s--) {
      value.g = s;
      LED.set_crgb_at(0, value); // Set value at LED found at index 0
      LED.set_crgb_at(1, value); // Set value at LED found at index 1
      LED.set_crgb_at(2, value); // Set value at LED found at index 2
      LED.sync(); // Sends the data to the LEDs
      delay(p);
      s=s-(20-p);
    }
  }
  allLEDs(4); // Green
  instability = 0;
  if (decayTime != 9999) { // Reset decay times if used.
    decayEndTime = millis() + (decayTime * 60000);
    decayThirdTime = millis() + (decayTime * 60000) / 3;
    decay2ThirdTime = millis() + (decayTime * 120000) / 3;
  }
  Serial.println("Device is charged.");
}

void beep(int period) {
  digitalWrite(audioPIN,HIGH);
  delay(period);
  digitalWrite(audioPIN,LOW);
}

