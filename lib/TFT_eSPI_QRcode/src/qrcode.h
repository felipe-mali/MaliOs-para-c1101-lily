#pragma once

#include <display/tft.h>
#include <cstddef>
#include <cstdint>

class QRcode
{
	private:
		tft_display *tft;
		void render(int x, int y, int color);

	public:
		QRcode(tft_display *display);
		void init();
		bool create(const String &message);

		// Encodes with the same QR implementation used by create(), but without
		// drawing. Bits are packed row-major, most-significant bit first.
		static bool encode(const String &message, uint8_t *packed, size_t capacity, uint8_t &matrixSize);
		static uint8_t matrixSize();
		static size_t packedSize();
};
