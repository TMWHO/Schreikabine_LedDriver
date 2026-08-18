#include <Arduino.h>
#include <FastLED.h>
// #include <math.h>

#define DBG(x) Serial.println(x)

#define NUM_LED 20
#define DATA_PIN 4
CRGB leds[NUM_LED];

const int pinInputSignal = A7;

float getRMS()
{
	const int samples = 128;
	float offset = 0;

	// DC Offset 
	for (int i = 0; i < samples; i++) { offset += analogRead(pinInputSignal); }

	offset /= samples;

	// RMS berechnen
	float sumSquares = 0;

	for (int i = 0; i < samples; i++)
	{
		float value = analogRead(pinInputSignal) - offset;
		sumSquares += value * value;
	}
	return sqrt(sumSquares / samples);
}

// Kalibrierwert mit deinem dB-Meter bestimmen
float calibration = 43.42; //dB=20⋅log10​(RMS)+K => K=dB−20⋅log10​(RMS)	: db(leveldBMeter), RMS(rms)

// Glättung
float dbSmooth = 0;

void setup()
{
	Serial.begin(9600);
	DBG("Serial online!");

	FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LED);

	FastLED.clear();
	FastLED.show();
}

void loop()
{
	float rms = getRMS();

	// RMS -> dB SPL
	float dbspl = 20.0f * log10(rms) + calibration;

	// Glätten
	// dbSmooth = dbSmooth * 0.7f + dbspl * 0.3f;
	if (dbspl > dbSmooth)
	{
		// schnell hoch
		dbSmooth += (dbspl - dbSmooth) * 0.4f;
	}
	else
	{
		// langsamer runter
		dbSmooth += (dbspl - dbSmooth) * 0.1f;
	}

	// Wertebereich für LED Balken
	float dbMin = 40.0f;
	float dbMax = 100.0f;

	int ledCount = map(dbSmooth, dbMin, dbMax, 0, NUM_LED);
	ledCount = constrain(ledCount, 0, NUM_LED);

	// Alle LEDs aus
	fill_solid(leds, NUM_LED, CRGB::Black);

	// LEDs einschalten
	for (int i = 0; i < ledCount; i++)
	{
		float percent = (float)i / NUM_LED;

		if (percent < 0.5f)
			leds[i] = CRGB::Green;
		else if (percent < 0.8f)
			leds[i] = CRGB::Yellow;
		else
			leds[i] = CRGB::Red;
	}

	// EINMAL pro Loop
	FastLED.show();

	// Debug
	Serial.print("RMS: ");
	Serial.print(rms);

	Serial.print("  dB SPL: ");
	Serial.println(dbSmooth);
}