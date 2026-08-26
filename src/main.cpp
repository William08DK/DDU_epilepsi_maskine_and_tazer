#include <Arduino.h>                    // Standard Arduino-bibliotek (pins, millis, delay osv.)
#include <Adafruit_NeoPixel.h>          // Bibliotek til NeoPixel / WS2812 LED-strips
#ifdef __AVR__
  #include <avr/power.h>                // Kun nødvendig på små AVR-chips (f.eks. Trinket)
#endif

// ========== PIN-DEFINITIONER ==========
#define Buzz_Buzz 3                     // Pin til buzzer / "tazer"-lyd
#define LED_PIN   6                     // Datapin til LED-strippen
#define Stage_1   7                     // Knap 1 → Rainbow
#define Stage_2   8                     // Knap 2 → Theater Chase
#define Stage_3   9                     // Knap 3 → Color Wipe
#define Stage_4  10                     // Knap 4 → Epilepsi / strobe

#define LED_COUNT 60                    // Antal LEDs på strippen

int do_you_get_tazed = 0;               // Bruges til at styre tilfældig buzzer-aktivering

// Opretter LED-strip objektet (60 LEDs, pin 6, GRB farverækkefølge, 800 kHz)
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ========== TILSTANDS-MASKINE ==========
// Mulige animationstilstande
enum Mode {
  MODE_OFF = 0,                         // Alle lys er slukkede
  MODE_RAINBOW,                         // Regnbue-animation
  MODE_THEATER,                         // Theater Chase (løbende lys)
  MODE_COLORWIPE,                       // Color Wipe (farve fylder strippen)
  MODE_EPILEPSI                         // Hurtig strobe / "epilepsi"-effekt
};

Mode currentMode = MODE_OFF;            // Holder styr på den aktuelle mode

// ========== TIMING ==========
unsigned long previousMillis = 0;       // Tidspunkt for sidste frame-opdatering
const unsigned long frameInterval = 20; // Opdateringsinterval i ms (20 ms ≈ 50 FPS)

// ========== ANIMATIONS-VARIABLER ==========
long firstPixelHue = 0;                 // Start-farve til rainbow-animationen
int theaterStep = 0;                    // (ikke aktivt brugt længere)
int colorWipePos = 0;                   // Nuværende position i colorWipe
uint32_t colorWipeColor = 0;            // (ikke aktivt brugt længere)
int colorWipeColorIndex = 0;            // Hvilken farve der bruges i colorWipe (0=rød, 1=grøn, 2=blå)
unsigned long epilepsiNext = 0;         // Tidspunkt for næste epilepsi-flash
int random_delay = 20;                  // (ældre variabel – bruges ikke længere)

void setup() {
  Serial.begin(9600);                   // Start seriel kommunikation (til debug)

  pinMode(Buzz_Buzz, OUTPUT);           // Buzzer er en udgang
  pinMode(Stage_1, INPUT_PULLUP);       // Knapper med intern pull-up (trykket = LOW)
  pinMode(Stage_2, INPUT_PULLUP);
  pinMode(Stage_3, INPUT_PULLUP);
  pinMode(Stage_4, INPUT_PULLUP);

  // Speciel kode kun til Adafruit Trinket 5V 16 MHz
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  strip.begin();                        // Initialiser LED-strippen (skal kaldes)
  strip.show();                         // Send "sluk alt" med det samme
  strip.setBrightness(50);              // Standard lysstyrke (0-255). 50 ≈ 20 %
}

// ========== NON-BLOCKING ANIMATIONSFUNKTIONER ==========
// Disse funktioner opdaterer kun ÉN frame ad gangen,
// så knapperne kan læses hele tiden uden forsinkelse.

bool epilepsiIsOn = false;              // true = viser farve, false = viser sort (strobe)
unsigned long epilepsiInterval = 15;    // Hvor længe den nuværende flash/black skal vare

// Regnbue: farverne "løber" hen over hele strippen
void updateRainbow() {
  strip.rainbow(firstPixelHue);         // Sæt regnbue-farver ud fra start-hue
  strip.show();                         // Send farverne til LEDs
  firstPixelHue += 256;                 // Flyt farvehjulet lidt frem
  if (firstPixelHue >= 5 * 65536L) {     // Efter 5 fulde omdrejninger → start forfra
    firstPixelHue = 0;
  }
}

// Theater Chase: tre farver (hvid, rød, blå) der "løber" hen over strippen
void updateTheaterChase() {
  static int a = 0, b = 0;              // static = husker værdierne mellem kald
  static uint32_t colors[] = {          // De tre farver der skiftes imellem
    strip.Color(127, 127, 127),         // Hvid (halv styrke)
    strip.Color(127, 0, 0),             // Rød
    strip.Color(0, 0, 127)              // Blå
  };
  static int colorIndex = 0;            // Hvilken farve der er aktiv lige nu

  strip.clear();                        // Start med at slukke alle LEDs
  // Tænd hver 3. LED startende fra position b
  for (int c = b; c < strip.numPixels(); c += 3) {
    strip.setPixelColor(c, colors[colorIndex]);
  }
  strip.show();                         // Opdater strippen

  b++;                                  // Flyt mønsteret ét trin
  if (b >= 3) {                         // Når de tre faser er kørt...
    b = 0;
    a++;
    if (a >= 10) {                      // Efter 10 gentagelser → skift farve
      a = 0;
      colorIndex = (colorIndex + 1) % 3;
    }
  }
}

// Color Wipe: fylder strippen op med én farve ad gangen (rød → grøn → blå)
void updateColorWipe() {
  static uint32_t colors[] = {
    strip.Color(255, 0, 0),             // Rød
    strip.Color(0, 255, 0),             // Grøn
    strip.Color(0, 0, 255)              // Blå
  };

  if (colorWipePos < strip.numPixels()) {
    // Tænd næste LED med den aktuelle farve
    strip.setPixelColor(colorWipePos, colors[colorWipeColorIndex]);
    strip.show();
    colorWipePos++;                     // Gå til næste LED
  } else {
    // Hele strippen er fyldt → start forfra med næste farve
    colorWipePos = 0;
    colorWipeColorIndex = (colorWipeColorIndex + 1) % 3;
    // Valgfrit: ryd strippen før næste farve
    // strip.clear(); strip.show();
  }
}

// Epilepsi / strobe: meget hurtig skiften mellem fuld farve og helt sort
// Det er denne effekt der er designet til at "gøre ondt i øjnene"
void updateEpilepsi() {
  unsigned long now = millis();         // Hent nuværende tid i millisekunder

  if (now >= epilepsiNext) {            // Er det tid til næste flash?
    if (epilepsiIsOn) {
      // Vi viser farve lige nu → sluk ALT (det vigtigste for strobe-effekten)
      strip.clear();
      strip.show();
      epilepsiIsOn = false;
      epilepsiInterval = random(8, 25); // Sort periode (8-25 ms)
    } else {
      // Vi er sorte lige nu → tænd hele strippen med en tilfældig farve
      uint32_t c = strip.Color(random(0, 256), random(0, 256), random(0, 256));
      for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, c);
      }
      strip.show();
      epilepsiIsOn = true;
      epilepsiInterval = random(5, 20); // Farve-flash (5-20 ms) – meget kort!
    }
    epilepsiNext = now + epilepsiInterval; // Planlæg næste skift
  }
}

// Slukker alle LEDs med det samme
void turnOff() {
  strip.clear();
  strip.show();
}

// ========== HOVED-LOOP ==========
void loop() {
  // Læs knapperne (INPUT_PULLUP → trykket = LOW, ikke trykket = HIGH)
  bool btn1 = (digitalRead(Stage_1) == LOW);  // Rainbow
  bool btn2 = (digitalRead(Stage_2) == LOW);  // Theater Chase
  bool btn3 = (digitalRead(Stage_3) == LOW);  // Color Wipe
  bool btn4 = (digitalRead(Stage_4) == LOW);  // Epilepsi

  // Bestem hvilken mode der skal køre (prioritet: 1 > 2 > 3 > 4)
  Mode newMode = MODE_OFF;
  if (btn1)      newMode = MODE_RAINBOW;
  else if (btn2) newMode = MODE_THEATER;
  else if (btn3) newMode = MODE_COLORWIPE;
  else if (btn4) newMode = MODE_EPILEPSI;

  // Hvis mode er ændret → nulstil alle animations-variabler
  if (newMode != currentMode) {
    currentMode = newMode;
    previousMillis = 0;                 // Tving øjeblikkelig opdatering
    firstPixelHue = 0;
    colorWipePos = 0;
    colorWipeColorIndex = 0;
    epilepsiIsOn = false;
    epilepsiNext = 0;

    // Print hvilken mode der er startet + juster lysstyrke
    if (currentMode == MODE_OFF) {
      turnOff();
      Serial.println("OFF");
      // strip.setBrightness(50);       // Kan genoprette normal lysstyrke her hvis ønsket
    } else if (currentMode == MODE_RAINBOW) {
      Serial.println("rainbow");
    } else if (currentMode == MODE_THEATER) {
      Serial.println("theaterChase");
    } else if (currentMode == MODE_COLORWIPE) {
      Serial.println("colorWipe");
    } else if (currentMode == MODE_EPILEPSI) {
      Serial.println("epilepsi");
      strip.setBrightness(255);         // Maksimal lysstyrke kun i epilepsi-mode
    }
  }

  // Opdater den aktuelle animation (non-blocking)
  unsigned long now = millis();
  if (now - previousMillis >= frameInterval) {
    previousMillis = now;

    switch (currentMode) {
      case MODE_RAINBOW:   updateRainbow();      break;
      case MODE_THEATER:   updateTheaterChase(); break;
      case MODE_COLORWIPE: updateColorWipe();    break;
      case MODE_EPILEPSI:  updateEpilepsi();     break;
      case MODE_OFF:       /* allerede slukket */ break;
    }
  }

  // ========== BUZZER / "TAZER"-EFFEKT ==========
  // Kun aktiv mens Epilepsi-mode kører
  if (currentMode == MODE_EPILEPSI) {
    // Tilfældig chance for at aktivere tazeren
    do_you_get_tazed = random(0, 10);
    if (do_you_get_tazed <= 5) {
      digitalWrite(Buzz_Buzz, HIGH);    // Tænd tazer
      Serial.println("GET TAZED");
    } else {
      digitalWrite(Buzz_Buzz, LOW);     // Sluk tazer
    }
  } else {
    digitalWrite(Buzz_Buzz, LOW);       // Sørg for at tazeren er slukket i andre modes
  }
}