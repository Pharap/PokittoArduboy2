/**
 * @file Sprites.cpp
 * \brief
 * A class for drawing animated sprites from image and mask bitmaps.
 */

#include "Sprites.h"

#include <cstdlib>

void Sprites::drawExternalMask(int16_t x, int16_t y, const uint8_t * bitmap, const uint8_t * mask, uint8_t frame, uint8_t mask_frame)
{
	draw(x, y, bitmap, frame, mask, mask_frame, SPRITE_MASKED);
}

void Sprites::drawOverwrite(int16_t x, int16_t y, const uint8_t * bitmap, uint8_t frame)
{
	draw(x, y, bitmap, frame, NULL, 0, SPRITE_OVERWRITE);
}

void Sprites::drawErase(int16_t x, int16_t y, const uint8_t * bitmap, uint8_t frame)
{
	draw(x, y, bitmap, frame, NULL, 0, SPRITE_IS_MASK_ERASE);
}

void Sprites::drawSelfMasked(int16_t x, int16_t y, const uint8_t * bitmap, uint8_t frame)
{
	draw(x, y, bitmap, frame, NULL, 0, SPRITE_IS_MASK);
}

void Sprites::drawPlusMask(int16_t x, int16_t y, const uint8_t * bitmap, uint8_t frame)
{
	draw(x, y, bitmap, frame, NULL, 0, SPRITE_PLUS_MASK);
}


//common functions
void Sprites::draw(int16_t x, int16_t y, const uint8_t * bitmap, uint8_t frame, const uint8_t * mask, uint8_t sprite_frame, uint8_t drawMode)
{
	if(bitmap == NULL)
		return;

	const uint8_t width = pgm_read_byte(bitmap);
	++bitmap;

	const uint8_t height = pgm_read_byte(bitmap);
	++bitmap;

	if((frame > 0) || (sprite_frame > 0))
	{
		unsigned int frame_offset = (width * ((height / 8) + (((height % 8) == 0) ? 0 : 1)));

		// sprite plus mask uses twice as much space for each frame
		if(drawMode == SPRITE_PLUS_MASK)
			frame_offset *= 2;
		else if(mask != NULL)
			mask += (sprite_frame * frame_offset);

		bitmap += (frame * frame_offset);
	}

	// if we're detecting the draw mode then base it on whether a mask
	// was passed as a separate object
	if(drawMode == SPRITE_AUTO_MODE)
		drawMode = (mask == NULL) ? SPRITE_UNMASKED : SPRITE_MASKED;

	drawBitmap(x, y, bitmap, mask, width, height, drawMode);
}

void Sprites::drawBitmap(int16_t x, int16_t y, const uint8_t * bitmap, const uint8_t * mask, uint8_t w, uint8_t h, uint8_t draw_mode)
{
	// no need to draw at all of we're offscreen
	if(((x + w) <= 0) || (x > (WIDTH - 1)) || ((y + h) <= 0) || (y > (HEIGHT - 1)))
		return;

	if(bitmap == NULL)
		return;

	// xOffset technically doesn't need to be 16 bit but the math operations
	// are measurably faster if it is
	// if the left side of the render is offscreen skip those loops
	const uint16_t xOffset = (x < 0) ? std::abs(x) : 0;

	// if the right side of the render is offscreen skip those loops
	const uint8_t rendered_width = ((x + w) > (WIDTH - 1)) ? ((WIDTH - x) - xOffset) : (w - xOffset);

	const int8_t yOffset = (y & 7);
	const int8_t tempSRow = (y / 8);
	int8_t sRow = ((y < 0) && (yOffset > 0)) ? (tempSRow - 1) : tempSRow;

	// if the top side of the render is offscreen skip those loops
	const uint8_t start_h = (sRow < -1) ? (std::abs(sRow) - 1) : 0;

	// divide, then round up
	const uint8_t rows = (h / 8);
	uint8_t loop_h = ((h % 8) > 0) ? (rows + 1) : rows;

	// if(sRow + loop_h - 1 > (HEIGHT/8)-1)
	if((sRow + loop_h) > (HEIGHT / 8))
		loop_h = ((HEIGHT / 8) - sRow);

	// prepare variables for loops later so we can compare with 0
	// instead of comparing two variables
	loop_h -= start_h;
	sRow += start_h;

	const uint8_t mul_amt = (1 << yOffset);
	uint16_t ofs = ((sRow * WIDTH) + x + xOffset);
	const uint8_t * bofs = &bitmap[(start_h * w) + xOffset];

	switch (draw_mode)
	{
		case SPRITE_UNMASKED:
		{
			// we only want to mask the 8 bits of our own sprite, so we can
			// calculate the mask before the start of the loop
			const uint16_t mask_data = ~(0xFF * mul_amt);
			// really if yOffset = 0 you have a faster case here that could be
			// optimized
			for(uint8_t a = 0; a < loop_h; ++a)
			{
				for(uint8_t iCol = 0; iCol < rendered_width; ++iCol)
				{
					const uint16_t bitmap_data = (pgm_read_byte(bofs) * mul_amt);

					if(sRow >= 0)
					{
						uint8_t data = Arduboy2Base::sBuffer[ofs];
						data &= static_cast<uint8_t>(mask_data);
						data |= static_cast<uint8_t>(bitmap_data);
						Arduboy2Base::sBuffer[ofs] = data;
					}

					if(yOffset != 0 && sRow < 7)
					{
						const std::size_t index = static_cast<uint16_t>(ofs + WIDTH);
						uint8_t data = Arduboy2Base::sBuffer[index];
						data &= (reinterpret_cast<const unsigned char *>(&mask_data)[1]);
						data |= (reinterpret_cast<const unsigned char *>(&bitmap_data)[1]);
						Arduboy2Base::sBuffer[index] = data;
					}

					++ofs;
					++bofs;
				}

				++sRow;
				bofs += (w - rendered_width);
				ofs += (WIDTH - rendered_width);
			}
			break;
		}

		case SPRITE_IS_MASK:
		{
			for(uint8_t a = 0; a < loop_h; ++a)
			{
				for(uint8_t iCol = 0; iCol < rendered_width; ++iCol)
				{
					const uint16_t bitmap_data = (pgm_read_byte(bofs) * mul_amt);

					if(sRow >= 0)
						Arduboy2Base::sBuffer[ofs] |= static_cast<uint8_t>(bitmap_data);

					if(yOffset != 0 && sRow < 7)
						Arduboy2Base::sBuffer[ofs + WIDTH] |= (reinterpret_cast<const unsigned char *>(&bitmap_data)[1]);

					++ofs;
					++bofs;
				}

				++sRow;
				bofs += (w - rendered_width);
				ofs += (WIDTH - rendered_width);
			}
			break;
		}

		case SPRITE_IS_MASK_ERASE:
		{
			for(uint8_t a = 0; a < loop_h; ++a)
			{
				for(uint8_t iCol = 0; iCol < rendered_width; ++iCol)
				{
					const uint16_t bitmap_data = (pgm_read_byte(bofs) * mul_amt);

					if(sRow >= 0)
						Arduboy2Base::sBuffer[ofs]  &= ~static_cast<uint8_t>(bitmap_data);

					if(yOffset != 0 && sRow < 7)
					{
						const std::size_t index = static_cast<uint16_t>(ofs + WIDTH);
						Arduboy2Base::sBuffer[index] &= ~(reinterpret_cast<const unsigned char *>(&bitmap_data)[1]);
					}

					++ofs;
					++bofs;
				}

				++sRow;
				bofs += (w - rendered_width);
				ofs += (WIDTH - rendered_width);
			}
			break;
		}

		case SPRITE_MASKED:
		{
			const uint8_t * mask_ofs = (mask + (start_h * w) + xOffset);

			for(uint8_t a = 0; a < loop_h; ++a)
			{
				for(uint8_t iCol = 0; iCol < rendered_width; ++iCol)
				{
					// NOTE: you might think in the yOffset==0 case that this results
					// in more effort, but in all my testing the compiler was forcing
					// 16-bit math to happen here anyways, so this isn't actually
					// compiling to more code than it otherwise would. If the offset
					// is 0 the high part of the word will just never be used.

					// load data and bit shift
					// mask needs to be bit flipped
					const uint16_t mask_data = ~(pgm_read_byte(mask_ofs) * mul_amt);
					const uint16_t bitmap_data = (pgm_read_byte(bofs) * mul_amt);

					if(sRow >= 0)
					{
						uint8_t data = Arduboy2Base::sBuffer[ofs];
						data &= static_cast<uint8_t>(mask_data);
						data |= static_cast<uint8_t>(bitmap_data);
						Arduboy2Base::sBuffer[ofs] = data;
					}

					if(yOffset != 0 && sRow < 7)
					{
						const std::size_t index = static_cast<uint16_t>(ofs + WIDTH);
						uint8_t data = Arduboy2Base::sBuffer[index];
						data &= (reinterpret_cast<const unsigned char *>(&mask_data)[1]);
						data |= (reinterpret_cast<const unsigned char *>(&bitmap_data)[1]);
						Arduboy2Base::sBuffer[index] = data;
					}

					++ofs;
					++mask_ofs;
					++bofs;
				}

				++sRow;
				bofs += (w - rendered_width);
				mask_ofs += (w - rendered_width);
				ofs += (WIDTH - rendered_width);
			}
			break;
		}

		case SPRITE_PLUS_MASK:
		{
			// TODO: Implement drawPlusMask mode functionality

			// // *2 because we use double the bits (mask + bitmap)
			// bofs = reinterpret_cast<const uint8_t *>(&bitmap[((start_h * w) + xOffset) * 2]);

			// // counter for x loop below
			// uint8_t xi = rendered_width;
			// uint8_t data;
			// uint16_t mask_data;
			// uint16_t bitmap_data;

			// asm volatile
			// (
				// // save Y
				// "push r28\n"
				// "push r29\n"
				// // Y = buffer_ofs_2
				// "movw r28, %[buffer_ofs]\n"
				// // buffer_ofs_2 = buffer_ofs + 128
				// "adiw r28, 63\n"
				// "adiw r28, 63\n"
				// "adiw r28, 2\n"
				// "loop_y:\n"
				// "loop_x:\n"
				// // load bitmap and mask data
				// "lpm %A[bitmap_data], Z+\n"
				// "lpm %A[mask_data], Z+\n"

				// // shift mask and buffer data
				// "tst %[yOffset]\n"
				// "breq skip_shifting\n"
				// "mul %A[bitmap_data], %[mul_amt]\n"
				// "movw %[bitmap_data], r0\n"
				// "mul %A[mask_data], %[mul_amt]\n"
				// "movw %[mask_data], r0\n"

				// // SECOND PAGE
				// // if yOffset != 0 && sRow < 7
				// "cpi %[sRow], 7\n"
				// "brge end_second_page\n"
				// // then
				// "ld %[data], Y\n"
				// // invert high byte of mask
				// "com %B[mask_data]\n"
				// "and %[data], %B[mask_data]\n"
				// "or %[data], %B[bitmap_data]\n"
				// // update buffer, increment
				// "st Y+, %[data]\n"

				// "end_second_page:\n"
				// "skip_shifting:\n"

				// // FIRST PAGE
				// // if sRow >= 0
				// "tst %[sRow]\n"
				// "brmi skip_first_page\n"
				// "ld %[data], %a[buffer_ofs]\n"
				// // then
				// "com %A[mask_data]\n"
				// "and %[data], %A[mask_data]\n"
				// "or %[data], %A[bitmap_data]\n"
				// // update buffer, increment
				// "st %a[buffer_ofs]+, %[data]\n"
				// "jmp end_first_page\n"

				// "skip_first_page:\n"
				// // since no ST Z+ when skipped we need to do this manually
				// "adiw %[buffer_ofs], 1\n"

				// "end_first_page:\n"

				// // "x_loop_next:\n"
				// "dec %[xi]\n"
				// "brne loop_x\n"

				// // increment y
				// "next_loop_y:\n"
				// "dec %[yi]\n"
				// "breq finished\n"
				// // reset x counter
				// "mov %[xi], %[x_count]\n"
				// // sRow++;
				// "inc %[sRow]\n"
				// "clr __zero_reg__\n"
				// // sprite_ofs += (w - rendered_width) * 2;
				// "add %A[sprite_ofs], %A[sprite_ofs_jump]\n"
				// "adc %B[sprite_ofs], __zero_reg__\n"
				// // buffer_ofs += WIDTH - rendered_width;
				// "add %A[buffer_ofs], %A[buffer_ofs_jump]\n"
				// "adc %B[buffer_ofs], __zero_reg__\n"
				// // buffer_ofs_page_2 += WIDTH - rendered_width;
				// "add r28, %A[buffer_ofs_jump]\n"
				// "adc r29, __zero_reg__\n"

				// "rjmp loop_y\n"
				// "finished:\n"
				// // put the Y register back in place
				// "pop r29\n"
				// "pop r28\n"
				// // just in case
				// "clr __zero_reg__\n"
				// :
				// [xi] "+&a" (xi),
				// [yi] "+&a" (loop_h),
				// // CPI requires an upper register (r16-r23)
				// [sRow] "+&a" (sRow),
				// [data] "=&l" (data),
				// [mask_data] "=&l" (mask_data),
				// [bitmap_data] "=&l" (bitmap_data)
				// :
				// [screen_width] "M" (WIDTH),
				// // lower register
				// [x_count] "l" (rendered_width),
				// [sprite_ofs] "z" (bofs),
				// [buffer_ofs] "x" (&Arduboy2Base::sBuffer[ofs]),
				// // upper reg (r16-r23)
				// [buffer_ofs_jump] "a" (WIDTH - rendered_width),
				// // upper reg (r16-r23)
				// [sprite_ofs_jump] "a" ((w - rendered_width) * 2),

				// // [sprite_ofs_jump] "r" (0),
				// // lower register
				// [yOffset] "l" (yOffset),
				// // lower register
				// [mul_amt] "l" (mul_amt)
				// // NOTE: We also clobber r28 and r29 (y) but sometimes the compiler
				// // won't allow us, so in order to make this work we don't tell it
				// // that we clobber them. Instead, we push/pop to preserve them.
				// // Then we need to guarantee that the the compiler doesn't put one of
				// // our own variables into r28/r29.
				// // We do that by specifying all the inputs and outputs use either
				// // lower registers (l) or simple (r16-r23) upper registers (a).
				// // pushes/clobbers/pops r28 and r29 (y)
				// :
			// );
			break;
		}
	}
}
