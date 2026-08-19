#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h>
#endif

#define LED_PIN   6
#define button_1  2

#define LED_COUNT   60

int random_delay = 0;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


void setup() {
  Serial.begin(9600);
  pinMode(button_1, INPUT_PULLUP);
  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  // END of Trinket-specific code.

  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)

}

void loop() {
  digitalRead(button_1);
  if (digitalRead(button_1) == HIGH) {
    Serial.println("on");
      // Fill along the length of the strip in various colors...
  epilepsi(strip.Color(0,   0,   0), 0); // Red
  epilepsi(strip.Color(  random(0,255), random(0,255), random(0,255)), 255); // Green
  random_delay = random(10, 30);
  } if (digitalRead(button_1) == LOW) {
  epilepsi(strip.Color(0,   0,   0), 0);
  }


}

void epilepsi(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
  }
      strip.show();                          //  Update strip to match
      delay(random_delay);
}