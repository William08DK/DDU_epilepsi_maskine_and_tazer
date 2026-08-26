#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// ========== PIN-DEFINITIONER ==========
// #define Buzz_Buzz 3
#define LED_PIN   6
#define Stage_1   7
#define Stage_2   8
#define Stage_3   9
#define Stage_4  10

#define LED_COUNT 60

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ========== TILSTANDS-MASKINE ==========
enum Mode {
  MODE_OFF = 0,
  MODE_RAINBOW,
  MODE_THEATER,
  MODE_COLORWIPE,
  MODE_EPILEPSI
};

Mode currentMode = MODE_OFF;

// ========== TIMING ==========
unsigned long previousMillis = 0;
const unsigned long frameInterval = 20;

// ========== ANIMATIONS-VARIABLER ==========
long firstPixelHue = 0;
int colorWipePos = 0;
int colorWipeColorIndex = 0;
unsigned long epilepsiNext = 0;

bool epilepsiIsOn = false;
unsigned long epilepsiInterval = 15;

// ========== TAZER ==========
int Tazer = 0;  // husker om tazeren skal være tændt denne gang

void setup() {
  Serial.begin(9600);

  // --- SEED RANDOM ---
  // Læs en flydende analog pin for at få et rigtigt tilfældigt starttal
  randomSeed(analogRead(A0));   // A0 skal være ubrugt / flydende

  // pinMode(Buzz_Buzz, OUTPUT);
  pinMode(Stage_1, INPUT_PULLDOWN);
  pinMode(Stage_2, INPUT_PULLDOWN);
  pinMode(Stage_3, INPUT_PULLDOWN);
  pinMode(Stage_4, INPUT_PULLDOWN);

#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  strip.begin();
  strip.show();
  strip.setBrightness(100);

  // digitalWrite(Buzz_Buzz, LOW);   // start med tazeren slukket
  Serial.println("Setup færdig");
}

// ========== ANIMATIONER ==========
void updateRainbow() {
  strip.rainbow(firstPixelHue);
  strip.show();
  firstPixelHue += 256;
  if (firstPixelHue >= 5 * 65536L) firstPixelHue = 0;
}

void updateTheaterChase() {
  static int a = 0, b = 0;
  static uint32_t colors[] = {
    strip.Color(127, 127, 127),
    strip.Color(127, 0, 0),
    strip.Color(0, 0, 127)
  };
  static int colorIndex = 0;

  strip.clear();
  for (int c = b; c < strip.numPixels(); c += 3) {
    strip.setPixelColor(c, colors[colorIndex]);
  }
  strip.show();

  b++;
  if (b >= 3) {
    b = 0;
    a++;
    if (a >= 10) {
      a = 0;
      colorIndex = (colorIndex + 1) % 3;
    }
  }
}

void updateColorWipe() {
  static uint32_t colors[] = {
    strip.Color(255, 0, 0),
    strip.Color(0, 255, 0),
    strip.Color(0, 0, 255)
  };

  if (colorWipePos < strip.numPixels()) {
    strip.setPixelColor(colorWipePos, colors[colorWipeColorIndex]);
    strip.show();
    colorWipePos++;
  } else {
    colorWipePos = 0;
    colorWipeColorIndex = (colorWipeColorIndex + 1) % 3;
  }
}

void updateEpilepsi() {
  unsigned long now = millis();
  if (now >= epilepsiNext) {
    if (epilepsiIsOn) {
      strip.clear();
      strip.show();
      epilepsiIsOn = false;
      epilepsiInterval = random(8, 25);
    } else {
      uint32_t c = strip.Color(random(0, 256), random(0, 256), random(0, 256));
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, c);
      }
      strip.show();
      epilepsiIsOn = true;
      epilepsiInterval = random(5, 20);
    }
    epilepsiNext = now + epilepsiInterval;
  }
}

void turnOff() {
  strip.clear();
  strip.show();
}

// ========== HOVED-LOOP ==========
void loop() {
  bool btn1 = (digitalRead(Stage_1) == HIGH);
  bool btn2 = (digitalRead(Stage_2) == HIGH);
  bool btn3 = (digitalRead(Stage_3) == HIGH);
  bool btn4 = (digitalRead(Stage_4) == HIGH);

  Mode newMode = MODE_OFF;
  if (btn1)      newMode = MODE_RAINBOW;
  else if (btn2 && btn1) newMode = MODE_THEATER;
  else if (btn3 && btn2 && btn1) newMode = MODE_COLORWIPE;
  else if (btn4 && btn3 && btn2 && btn1) newMode = MODE_EPILEPSI;

  // === Mode skiftet ===
  if (newMode != currentMode) {
    currentMode = newMode;
    previousMillis = 0;
    firstPixelHue = 0;
    colorWipePos = 0;
    colorWipeColorIndex = 0;
    epilepsiIsOn = false;
    epilepsiNext = 0;

    if (currentMode == MODE_OFF) {
      turnOff();
      Serial.println("OFF");
      // Tazer = 0;
      // digitalWrite(Buzz_Buzz, LOW);
    }
    else if (currentMode == MODE_RAINBOW) {
      Serial.println("rainbow");
      // Tazer = 0;
      // digitalWrite(Buzz_Buzz, LOW);
    }
    else if (currentMode == MODE_THEATER) {
      Serial.println("theaterChase");
      // Tazer = 0;
      // digitalWrite(Buzz_Buzz, LOW);
    }
    else if (currentMode == MODE_COLORWIPE) {
      Serial.println("colorWipe");
      // Tazer = 0;
      // digitalWrite(Buzz_Buzz, LOW);
    }
    else if (currentMode == MODE_EPILEPSI) {
      Serial.println("epilepsi");
      strip.setBrightness(255);
      // Tazer = 1;
    }
  }

  // Opdater animation
  unsigned long now = millis();
  if (now - previousMillis >= frameInterval) {
    previousMillis = now;
    switch (currentMode) {
      case MODE_RAINBOW:   updateRainbow();      break;
      case MODE_THEATER:   updateTheaterChase(); break;
      case MODE_COLORWIPE: updateColorWipe();    break;
      case MODE_EPILEPSI:  updateEpilepsi();     break;
      case MODE_OFF:       break;
    }
  }
  /*
  if (Tazer == 1) {
    digitalWrite(Buzz_Buzz, HIGH);
  } else {
    digitalWrite(Buzz_Buzz, LOW);
  }
  */
}