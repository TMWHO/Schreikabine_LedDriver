#include <Arduino.h>
#include <FastLED.h>
// #include <math.h>

#define DBG(x) Serial.println(x)
#define DBGL(x) Serial.print(x)


#define NUM_LED 120
#define DATA_PIN 6

constexpr int LED_START = 10;		// erste aktive led auf dem streifen
constexpr int LED_END = 100;		// letzte aktive led auf dem streifen
constexpr int ACTIVE_LED_COUNT = LED_END - LED_START + 1;

CRGB leds[NUM_LED];

const int pinInputSignal = A7;
const uint32_t peakHoldTimeMs = 500;
const float peakReleaseLedPerSecond = 20.0f;

const int pinReset = 4;

constexpr uint8_t START_BYTE = 0xAA;

void sendDB(int16_t db)
{
	uint8_t low = db & 0xFF;
	uint8_t high = (db >> 8) & 0xFF;

	uint8_t checksum = low ^ high;

	Serial1.write(START_BYTE);
	Serial1.write(low);
	Serial1.write(high);
	Serial1.write(checksum);
}

void setup()
{
	Serial.begin(115200);
	Serial1.begin(115200);

	DBG("Serial online!");

	pinMode(pinReset, INPUT_PULLUP);

	FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LED);
	FastLED.setBrightness(255);

	FastLED.clear();
	FastLED.show();

	sendDB(0); // schaltet die 7seg aus
}

int adcSampleMin = 1023;
int adcSampleMax = 0;

float getRMS()
{

	// // //////calibrierung
	// const int samples = 2048;

	// float sum = 0.0f;

	// // Mittelwert bestimmen
	// for (int i = 0; i < samples; i++)
	// {
	// 	sum += analogRead(pinInputSignal);
	// }

	// float offset = sum / samples;

	// // RMS bestimmen
	// float sumSquares = 0.0f;

	// for (int i = 0; i < samples; i++)
	// {
	// 	float value = analogRead(pinInputSignal) - offset;
	// 	sumSquares += value * value;
	// }

	// return sqrtf(sumSquares / samples);


	/// für live betrieb...heavy as fuck mit den float array....aber so garantieren wir das wir mit dem SELBEN samples arbeit...bei calli egal
	const int samples = 256;			// eventuell nochmal unpassen, wenn dor led streifen faxen mocht

	float sum = 0.0f;
	float values[samples];
	adcSampleMin = 1023;
	adcSampleMax = 0;

	for (int i = 0; i < samples; i++)
	{
		values[i] = analogRead(pinInputSignal);
		sum += values[i];
		if (values[i] < adcSampleMin) adcSampleMin = values[i];
		if (values[i] > adcSampleMax) adcSampleMax = values[i];
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
float calibration = 40.00f; //dB=20⋅log10​(RMS)+K => K=dB−20⋅log10​(RMS)	: db(leveldBMeter), RMS(rms)

int16_t dbSend = 0;

const float calibrationScale = 1.16f;
const float calibrationOffset = -7.40f;

const float calibrationTable[][2] = {
	{59.0f, 65.0f},
	{61.0f, 71.0f},
	{69.0f, 80.0f},
	{75.0f, 85.0f},
	{81.0f, 90.0f},
	{85.0f, 95.0f},
	{90.0f, 97.0f}
};
const uint8_t calibrationTableSize = sizeof(calibrationTable) / sizeof(calibrationTable[0]);

float calibrateDb(float db)
{
	if (db <= calibrationTable[0][0]) return db + (calibrationTable[0][1] - calibrationTable[0][0]);

	for (uint8_t i = 1; i < calibrationTableSize; i++)
	{
		if (db <= calibrationTable[i][0])
		{
			float inputRange = calibrationTable[i][0] - calibrationTable[i - 1][0];
			float targetRange = calibrationTable[i][1] - calibrationTable[i - 1][1];
			float fraction = (db - calibrationTable[i - 1][0]) / inputRange;
			return calibrationTable[i - 1][1] + fraction * targetRange;
		}
	}

	const uint8_t last = calibrationTableSize - 1;
	return db + (calibrationTable[last][1] - calibrationTable[last][0]);
}

int dBMin = 60;


void loop()
{

	// rechn dB scheis aus
	float rms = getRMS();

	if (rms < 0.001f)	rms = 0.001f;
	if (!isfinite(rms) || rms <= 0.0f) { return; }

	// RMS -> dB SPL
	float dbRaw = 20.0f * log10(rms) + calibration;
	float dbBeforeLut = calibrationScale * dbRaw + calibrationOffset;
	float dbspl = calibrateDb(dbBeforeLut);

	if (!isfinite(dbspl)) { return; }			// iwos mochen wenn die checks failn...

	static float dbSmooth = 0;
	if (dbspl > dbSmooth) { dbSmooth += (dbspl - dbSmooth) * 0.4f; }	// attack
	else { dbSmooth += (dbspl - dbSmooth) * 0.2f; }	// release




	////// Send den scheis

	
	// static int dbSmoothMax = 0;
	
	// if (dbSmooth > dbSmoothMax) dbSmoothMax = dbSmooth;		
	// if (dbSmooth >= dBMin)	sendDB(dbSmoothMax);	
	// if (digitalRead(pinReset) == LOW)
	// {
	// 	dbSmoothMax = 0;
	// 	sendDB(0); // schaltet die 7seg aus
	// 	DBG("betatigt");
	// }	


	sendDB(dbSmooth);		// test ohne reset button
	// sendDB(123);



	// led streifen stuff
	float dbMin = 60.0f;
	float dbMax = 110.0f;

	int ledCount = map(dbSmooth, dbMin, dbMax, 0, ACTIVE_LED_COUNT);
	ledCount = constrain(ledCount, 0, ACTIVE_LED_COUNT);

	static float peakLedCount = 0.0f;
	static uint32_t peakStartedAt = 0;
	static uint32_t peakUpdatedAt = 0;
	uint32_t now = millis();

	if (ledCount >= peakLedCount)
	{
		peakLedCount = ledCount;
		peakStartedAt = now;
	}
	else if (peakLedCount > ledCount && (uint32_t)(now - peakStartedAt) >= peakHoldTimeMs)
	{
		float elapsedSeconds = (now - peakUpdatedAt) / 1000.0f;
		peakLedCount -= peakReleaseLedPerSecond * elapsedSeconds;
		if (peakLedCount < ledCount) peakLedCount = ledCount;
	}

	peakUpdatedAt = now;

	fill_solid(leds, NUM_LED, CRGB::Black);

	for (int i = 0; i < ledCount; i++)
	{
		float percent = (float)i / ACTIVE_LED_COUNT;
		int physicalIndex = LED_END - i;
		CRGB color;

		if (percent < 0.5f)
		{
			uint8_t blendAmount = (uint8_t)(percent * 2.0f * 255.0f);
			color = blend(CRGB::Green, CRGB::Yellow, blendAmount);
		}
		else
		{
			uint8_t blendAmount = (uint8_t)((percent - 0.5f) * 2.0f * 255.0f);
			color = blend(CRGB::Yellow, CRGB::Red, blendAmount);
		}

		leds[physicalIndex] = color;
	}

	if (peakLedCount > 0.0f)
	{
		int peakIndex = constrain((int)ceilf(peakLedCount) - 1, 0, ACTIVE_LED_COUNT - 1);
		leds[LED_END - peakIndex] = CRGB::White;
	}

	FastLED.show();

	DBGL("RMS: ");
	DBGL(rms);

	DBGL("  dB SPL: ");
	DBGL(dbSmooth);
	DBGL("  ADC min/max: ");
	DBGL(adcSampleMin);
	DBGL("/");
	DBG(adcSampleMax);
}



// Stufe 2 – 2-Punkt-Kalibrierung

// Wenn die Kennlinie linear ist:

// float a = ...;
// float b = ...;


// float dbspl = a * log10f(rms) + b;

// Fertig.

// Stufe 3 – nur wenn nötig LUT

// Wenn du beispielsweise feststellst:

// 40 dB → sehr gut
// 50 dB → sehr gut
// 60 dB → sehr gut
// 70 dB → -1 dB
// 80 dB → -3 dB
// 90 dB → -5 dB
// 100 dB → -8 dB

// dann würde ich nicht versuchen, das mit einer komplizierten mathematischen Funktion zu erschlagen. Dann ist eine LUT tatsächlich sinnvoll:

// const float calibrationTable[][2] = {
//     {40, ...},
//     {50, ...},
//     {60, ...},
//     {70, ...},
//     {80, ...},
//     {90, ...},
//     {100, ...}
// };

// und zwischen den Punkten wird interpoliert.