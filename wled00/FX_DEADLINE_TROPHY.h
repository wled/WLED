#pragma once

// "DEADLINE ISLAND"
// effect: jtruk for Deadline 2026
// compilation warnings: jtruk for Deadline 2026
// Seems to need segments flipping in the WLED panel before the effect works. Maybe that's true of them all?

#include "FX.h"
#include "../usermods/DEADLINE_TROPHY/DeadlineTrophy.h"

extern uint16_t mode_static(void);

//////////////////////////////////////////////////////////
//  Deadline Trophy FX
//////////////////////////////////////////////////////////
// There's a lot of useful stuff in
// FX.cpp -> see the DEV INFO, also, other FX for "inspiration" :)
// wled_math.cpp
// util.cpp
// -- you can also reinvent the wheel.
// I have never tested how efficient we really _HAVE_ to be,
// thus all the helper functions. ..qm
//////////////////////////////////////////////////////////
//
// Notes:
// - the if (...SEGMENT) { ... } is due to "this FX will be evaluated for every segment on its own"
//   remember, that you also have to set it for each segment to be actually active.
// - for "state" variables, there are two uint8_t SEGMENT.aux0 and SEGMENT.aux1,
//   but you can also declare any variable "static" to be more flexible (QM figures that makes sense here)
// - the EVERY_NTH_CALL(n) { ... } helper is great for debugging.

const float dPixelW = 1.0 / 26.0;
const float dPixelH = 1.0 / 12.0;
const uint8_t samplesPerPixelDimension = 1;
const float dSampleW = dPixelW / samplesPerPixelDimension;
const float dSampleH = dPixelH / samplesPerPixelDimension;
const uint8_t samplesPerPixel = samplesPerPixelDimension * samplesPerPixelDimension;

static int state = 0;
static int stateCounter = 0;

const uint8_t bufferW = 48;
const uint8_t bufferH = 48;
DeadlineTrophy::FloatRgb buffer[bufferH][bufferW];
const uint letterInterlace = 4;

const DeadlineTrophy::FloatRgb fontLetterA[7][8] = {
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.941,0.878,0.000},{0.816,0.690,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.941,0.878,0.000},{0.941,0.878,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.816,0.690,0.000},{0.816,0.690,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000}},
};

const DeadlineTrophy::FloatRgb fontLetterD[7][8] = {
	{{0.000,0.000,0.000},{0.941,0.941,0.941},{0.941,0.878,0.000},{0.816,0.690,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.941,0.878,0.000},{0.941,0.878,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.816,0.690,0.000},{0.816,0.690,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.000,0.000,0.000}},
};

const DeadlineTrophy::FloatRgb fontLetterE[7][8] = {
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.941,0.878,0.000},{0.816,0.690,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.941,0.878,0.000},{0.941,0.878,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.816,0.690,0.000},{0.816,0.690,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000}},
};

const DeadlineTrophy::FloatRgb fontLetterI[7][8] = {
	{{0.000,0.000,0.000},{0.941,0.941,0.941},{0.941,0.878,0.000},{0.816,0.690,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000}},
};

const DeadlineTrophy::FloatRgb fontLetterL[7][8] = {
	{{0.000,0.000,0.000},{0.941,0.941,0.941},{0.941,0.878,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.941,0.878,0.000},{0.941,0.878,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.816,0.690,0.000},{0.816,0.690,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000}},
};

const DeadlineTrophy::FloatRgb fontLetterN[7][8] = {
	{{0.000,0.000,0.000},{0.941,0.941,0.941},{0.941,0.878,0.000},{0.816,0.690,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.941,0.878,0.000},{0.941,0.878,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.816,0.690,0.000},{0.816,0.690,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.627,0.439,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000}},
};

const DeadlineTrophy::FloatRgb fontLetterS[7][8] = {
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.941,0.878,0.000},{0.816,0.690,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.941,0.878,0.000},{0.941,0.878,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.816,0.690,0.000},{0.816,0.690,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.753,0.565,0.000},{0.627,0.439,0.000},{0.502,0.376,0.000},{0.000,0.000,0.000}},
	{{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.000,0.000,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.502,0.376,0.000},{0.439,0.251,0.000}},
	{{0.000,0.000,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.439,0.251,0.000},{0.000,0.000,0.000}},
};

std::string scrollText = "  DEADLINE ISLAND ";
uint scrollLetter = 0;
float scrollX = .0f;

void fillBuffer(DeadlineTrophy::FloatRgb rgb) {
	for(int y = 0; y < bufferH; y++) {
		for(int x = 0; x < bufferW; x++) {
			buffer[y][x] = rgb;
		}
	}
}

void letterToBuffer(char letter, uint dx, uint dy, uint colourSwitch) {
	const DeadlineTrophy::FloatRgb (*fontLetter)[8] = nullptr;
	colourSwitch = colourSwitch % 6;
	switch(letter) {
		case 'A':
			fontLetter = fontLetterA;
			break;
		case 'D':
			fontLetter = fontLetterD;
			break;
		case 'E':
			fontLetter = fontLetterE;
			break;
		case 'I':
			fontLetter = fontLetterI;
			break;
		case 'L':
			fontLetter = fontLetterL;
			break;
		case 'N':
			fontLetter = fontLetterN;
			break;
		case 'S':
			fontLetter = fontLetterS;
			break;
		default:
			return;
	}
	for(int y = 0; y < 8; y++) {
		uint drawY = y;
		if(y>=6) {
			drawY--;
		}
		for(int x = 0; x < 8; x++) {
			for(int ix = 0; ix < letterInterlace; ix++) {
				for(int iy = 0; iy < letterInterlace; iy++) {
					uint px = (dx+x)*letterInterlace + ix;
					uint py = (dy+y)*letterInterlace + iy;
					if(px>=0 && px<bufferW && py>=0 && py<bufferH) {
						DeadlineTrophy::FloatRgb rgb = fontLetter[drawY][x];

						switch(colourSwitch) {
							case 1:
								rgb = {rgb.g, rgb.b, rgb.r};
								break;
							case 2:
								rgb = {rgb.b, rgb.r, rgb.g};
								break;	
							case 3:
								rgb = {rgb.r, rgb.b, rgb.g};
								break;
							case 4:
								rgb = {rgb.b, rgb.g, rgb.r};
								break;	
							case 5:
								rgb = {rgb.g, rgb.r, rgb.b};
								break;
						}

						buffer[py][px] = rgb;
					}
				}
			}
		}
	}
}

float floorSkew(float xProp) {
	return xProp * .35f;
}

void backgroundToBuffer(uint32_t t) {
	const float sky = 0.5f;
	for(int x = 0; x < bufferW; x++) {
		float xProp = float(x)/bufferW;
		float shoreSkew = floorSkew(xProp);
		for(int y = 0; y < bufferH; y++) {
			float yProp = float(y)/bufferH;
			const float shore = .5 + shoreSkew + (sin(t/6.0f) + sin((xProp/2 - yProp)*10.f) + sin(x*.1 + t/8.3f)) * .01f;

			DeadlineTrophy::FloatRgb rgb;
			if(yProp < sky) {
				const float fade = 1 - ((sky - yProp) / sky);
				rgb = {.1f * fade, .02f * fade, .3f};
			} else if(yProp < shore) {
				float waterProp = 1.0f; //(yProp / shore) + .2 * sin((xProp/2 - yProp)*2.f + t/6.0f); 
				float fade = ((yProp - sky) / (1 - sky));
				rgb = {
					MIN(1.0f, MAX(0.0f, (.02f * fade) + (.025f * waterProp))),
					MIN(1.0f, MAX(0.0f, (.02f * fade) + (.025f * waterProp))),
					MIN(1.0f, MAX(0.04f, .02f + (.1f * fade) + (.1f * waterProp)))
				};
			} else {
				rgb = {.12f, .12f, .0f};
			}

			buffer[y][x] = rgb;
		}
	}
}

// y = 0->1
float treeSkew(float yProp, float wind) {
	float skew = 1 - yProp;
	skew = (skew * skew) * (.06 * (wind + 1));
	return skew;
}

void treeToBuffer(uint32_t t) {
	const float treeX = .15;
	const float wind = sin(t/20.0f) * .5+ sin(t/4.4f) * .4 + sin(t/2.2f) * .3;
	const uint y0 = .25*bufferH;
	const uint y1 = .72*bufferH;
	float yDiff = y1 - y0;
	for(int y=y0; y<y1; y++) {
		float yProp = (float)(y - y0) / yDiff;
		float treeW = .05 + yProp * .02;
		float skew = treeSkew(yProp, wind);
		uint x0 = (treeX + skew - treeW) * bufferW;
		uint x1 = (treeX + skew + treeW) * bufferW;
		for(int x=x0; x<x1; x++) {
			buffer[y][x] = {.15,.05,.05};
		}
	}

	uint x0 = (treeX - .1) + treeSkew(0, wind) * bufferW;
	uint x1 = x0 + .55 * bufferW;
	for(int x=x0; x<x1; x++) {
		float xProp = (float(x-x0)/float(x1-x0));
		float y0 = .20f * bufferH + xProp * .1 * bufferH;
		float y1 = y0 + .125f * bufferH;
		for(int y=y0; y<y1; y++) {
			buffer[y][x] = {.0,.1,.0};
		}
	}
}

const DeadlineTrophy::Vec2 dHalfPixel = {dPixelW/2, dPixelH/2};

// Describes how much of a pixel dimension to sample
struct Partial {
	int whole;
	float part;
};

// Takes a from and to, and returns partials
// to must not be less than from
std::vector<Partial> getSpread(float from, float to) {
	std::vector<Partial> result;

	int iFrom = (int) from;
	int iTo = (int) to;
	float fFromR = from - iFrom;
	float fToR = to - iTo;

	if(iFrom == iTo) {
		float diff = fToR - fFromR;
		if(diff > 0) {
			result.push_back({iFrom, diff});
		}
	} else {
		float diff = 1 - fFromR;
		if(diff > 0) {
			result.push_back({iFrom, diff});
		}
		for(int i = iFrom + 1; i < iTo; i++) {
			result.push_back({i, 1});
		}
		if(fToR > 0) {
			result.push_back({iTo, fToR});
		}
	}

	return result;
};

CRGB bufferToPixel(DeadlineTrophy::Vec2 uv) {
	using namespace DeadlineTrophy;

	// map x:-1->1 to 0->1 and y:-1->1 to 1->0
	Vec2 uvUnit = {(uv.x + 1) / 2, 1 - ((uv.y + 1) / 2)};

	// Attempt to straighten
//	uvUnit.x -= uvUnit.y * 1/3;

	Vec2 uvLow = uvUnit - dHalfPixel;
	Vec2 uvHigh = uvUnit + dHalfPixel;

	std::vector<Partial> ySpread = getSpread(uvLow.y * bufferH, uvHigh.y * bufferH);
	std::vector<Partial> xSpread = getSpread(uvLow.x * bufferW, uvHigh.x * bufferW);

	DeadlineTrophy::FloatRgb rgbPixel = {0,0,0};
	float totalArea = 0;
	for(const Partial& yPartial : ySpread) {
		int y = yPartial.whole;
		for(const Partial& xPartial : xSpread) {
			int x = xPartial.whole;
			float frac = xPartial.part * yPartial.part;

			if(y >= 0 && y < bufferW && x >= 0 && x < bufferW) {
				FloatRgb rgb = buffer[y][x];
				rgbPixel.r += rgb.r * frac;
				rgbPixel.g += rgb.g * frac;
				rgbPixel.b += rgb.b * frac;
			}

			totalArea += frac;
		}
	}

	if(totalArea > 0) {
		rgbPixel.r /= totalArea;
		rgbPixel.g /= totalArea;
		rgbPixel.b /= totalArea;
	}
	return rgbPixel.toCRGB();
}

void bufferToLogo() {
	using namespace DeadlineTrophy;
	using namespace DeadlineTrophy::FxHelpers;

	for (const auto& coord : logoCoordinates()) {
		setLogo(coord.x, coord.y, uint32_t(bufferToPixel(coord.uv)));
	}
}

DeadlineTrophy::FloatRgb pointSample(DeadlineTrophy::Vec2 uv, float time) {
	float a = atan2f(uv.y, uv.x);
	float d = sqrtf(uv.x * uv.x + uv.y * uv.y);

	return {double(0.5f + sin(a * 12.f + time) * 0.5f), 0, double(0.5f + sin(d) * 0.5f)};
}

CRGB superSample(DeadlineTrophy::Vec2 uv, float time) {	
	DeadlineTrophy::FloatRgb rgbAll = {0, 0, 0};

	DeadlineTrophy::Vec2 sampleUV = {uv.x - dPixelW/2 + dSampleW/2, uv.y - dPixelH/2 + dSampleH/2};
	for(int xSample = 0; xSample < samplesPerPixelDimension; xSample++) {
		for(int ySample = 0; ySample < samplesPerPixelDimension; ySample++) {
			DeadlineTrophy::FloatRgb rgbPoint = pointSample(sampleUV, time);
			rgbAll.r += rgbPoint.r;
			rgbAll.g += rgbPoint.g;
			rgbAll.b += rgbPoint.b;
			sampleUV.x += dSampleW;
			sampleUV.y += dSampleH;
		}
	}
	rgbAll.r /= samplesPerPixel;
	rgbAll.g /= samplesPerPixel;
	rgbAll.b /= samplesPerPixel;

	return rgbAll.toCRGB();
}

uint16_t mode_DeadlineTrophy(void) {
	using namespace DeadlineTrophy;
	using namespace DeadlineTrophy::FxHelpers;
	using namespace Logo;

	if (SEGENV.call == 0) {
		// Initialise anything
		state = 0;
		stateCounter = 0;
	}

	if (IS_BASE_SEGMENT) {
		for (const auto& coord : baseCoordinates()) {
			float fade = 0.5f + 0.5f * sin((coord.uv.x + coord.uv.y) + float(SEGENV.call)/5.0f);

			FloatRgb rgb = {0, 0.3f, fade};
			setBase(coord.x, coord.y, rgb);
		}
	} else if (IS_LOGO_SEGMENT) {
		if(state < 3) {
			scrollX -= 1.2f;
			if(scrollX < -10) {
				scrollX = 0.0f;
				scrollLetter = scrollLetter + 1;
				if(scrollLetter > scrollText.length()) {
					scrollLetter = 0;
					state ++;
					stateCounter = 0;
				};
			}

			fillBuffer({0,0,0});
			int drawScrollX = scrollX;
			int drawScrollLetter = scrollLetter;
			uint yScroll = 2;

			while(drawScrollX < 20 && drawScrollLetter < scrollText.length()) {
				int colourSwitch = state * 2;
				if(drawScrollLetter > 10) {
					colourSwitch ++;
				}
				letterToBuffer(scrollText[drawScrollLetter], drawScrollX, yScroll, colourSwitch);
				drawScrollX += 10;
				drawScrollLetter++;
			};
		} else {
			backgroundToBuffer(SEGENV.call);
			treeToBuffer(SEGENV.call);
			stateCounter ++;
			if(stateCounter > 150) {
				state = 0;
				stateCounter = 0;
			}
		}

		bufferToLogo();
	} else if (IS_BACK_LED) {
		SEGMENT.fill(YELLOW);
	} else if (IS_FLOOR_LED) {
		SEGMENT.fill(BLUE);
	}

	return FRAMETIME;
}
static const char _data_FX_MODE_DEADLINE_TROPHY[] PROGMEM = "DEADLINE Trophy@!,Fade time;;;01";

//static const char _data_FX_MODE_DEADLINE_TROPHY[] PROGMEM =
//    "DEADLINE Trophy@BPM,!,Contour Intensity,!,!;!,!;!;2;c1=0,sx=110,pal=59";
// <-- cf. https://kno.wled.ge/interfaces/json-api/#effect-metadata
// <EffectParameters>;<Colors>;<Palette>;<Flags>;<Defaults>
// <Colors> = !,! = Two Defaults
// <Palette> = ! = Default enabled (Empty would disable palette selection)
// <Flags> = 2 for "it needs the 2D matrix", add "v" and/or "f" for AudioReactive volume and frequency.
// <Defaults>: <ParameterCode>=<Value>
//             si=0 is "sound interaction" (for AudioReactive),
// <EffectParameters>: These are then read by (if defined, comma-separated)
//   sx = SEGMENT.speed (int 0-255)
//   ix = SEGMENT.intensity (int 0-255)
//   c1 = SEGMENT.custom1 (int 0-255)
//   c2 = SEGMENT.custom2 (int 0-255)
//   c3 = SEGMENT.custom3 (int 0-31)
//   o1 = SEGMENT.check1 (bool)
//   o2 = SEGMENT.check2 (bool)
//   o3 = SEGMENT.check3 (bool)
