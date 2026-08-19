#include <Arduino.h>
#include <FastLED.h>
// #include <math.h>

#define DBG(x) Serial.println(x)

#define NUM_LED 160
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
	FastLED.setBrightness(255);

	FastLED.clear();
	FastLED.show();
}

float getRMS()
{
	const int samples = 256;

	float sum = 0.0f;
	float values[samples];

	for (int i = 0; i < samples; i++)
	{
		values[i] = analogRead(pinInputSignal);
		sum += values[i];
	}

	float offset = sum / samples;

	float sumSquares = 0.0f;

	for (int i = 0; i < samples; i++)
	{
		float value = values[i] - offset;
		sumSquares += value * value;
	}

	return sqrtf(sumSquares / samples);
}

// Kalibrierwert mit dB-Meter bestimmen
float calibration = 33.42; //dB=20⋅log10​(RMS)+K => K=dB−20⋅log10​(RMS)	: db(leveldBMeter), RMS(rms)

void loop()
{
	float rms = getRMS();

    // RMS -> dB SPL
    float dbspl = 20.0f * log10(rms) + calibration;

    // Schutz gegen NaN / Infinity
    if (!isfinite(dbspl))
    {
        Serial.println("Ungültiger dB-Wert!");
        return;
    }


	if (!isfinite(dbspl)) { return; }			// iwos mochen wenn die checks failn...

	static float dbSmooth = 0;
	if (dbspl > dbSmooth) { dbSmooth += (dbspl - dbSmooth) * 0.4f; }	// attack
	else { dbSmooth += (dbspl - dbSmooth) * 0.1f; }	// release

	float dbMin = 40.0f;
	float dbMax = 120.0f;

	int ledCount = map(dbSmooth, dbMin, dbMax, 0, NUM_LED);
	ledCount = constrain(ledCount, 0, NUM_LED);

	// static float peakLedCount = 0.0f;
	// static uint32_t peakStartedAt = 0;
	// static uint32_t peakUpdatedAt = 0;
	// uint32_t now = millis();

	// if (ledCount >= peakLedCount)
	// {
	// 	peakLedCount = ledCount;
	// 	peakStartedAt = now;
	// }
	// else if (peakLedCount > ledCount && (uint32_t)(now - peakStartedAt) >= peakHoldTimeMs)
	// {
	// 	float elapsedSeconds = (now - peakUpdatedAt) / 1000.0f;
	// 	peakLedCount -= peakReleaseLedPerSecond * elapsedSeconds;
	// 	if (peakLedCount < ledCount) peakLedCount = ledCount;
	// }

	// peakUpdatedAt = now;

	fill_solid(leds, NUM_LED, CRGB::Black);

	for (int i = 0; i < ledCount; i++)
	{
		float percent = (float)i / NUM_LED;

		if (percent < 0.5f)			leds[i] = CRGB::Green;
		else if (percent < 0.8f)	leds[i] = CRGB::Yellow;
		else						leds[i] = CRGB::Red;
	}

	// if (peakLedCount > 0.0f)
	// {
	// 	int peakIndex = constrain((int)ceilf(peakLedCount) - 1, 0, NUM_LED - 1);
	// 	leds[peakIndex] = CRGB::White;
	// }

	FastLED.show();

	// Debug
	Serial.print("RMS: ");
	Serial.print(rms);

	Serial.print("  dB SPL: ");
	Serial.println(dbSmooth);
}