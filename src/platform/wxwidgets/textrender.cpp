#include "platform/wxwidgets/textrender.hpp"
#include "fonts/wrapper.hpp"
#include "library/utf8.hpp"

extern const uint32_t text_framebuffer::element::white = 0xFFFFFF;
extern const uint32_t text_framebuffer::element::black = 0x000000;

text_framebuffer::text_framebuffer()
{
	width = 0;
	height = 0;
}

text_framebuffer::text_framebuffer(size_t w, size_t h)
{
	width = 0;
	height = 0;
	set_size(w, h);
}

void text_framebuffer::set_size(size_t w, size_t h)
{
	if(w == width && h == height)
		return;
	std::vector<element> newfb;
	newfb.resize(w * h);
	for(size_t i = 0; i < h && i < height; i++)
			for(size_t j = 0; j < w && j < width; j++)
				newfb[i * w + j] = buffer[i * width + j];
	buffer.swap(newfb);
	width = w;
	height = h;
}

std::pair<size_t, size_t> text_framebuffer::get_cell()
{
	return std::make_pair(8, 16);
}

std::pair<size_t, size_t> text_framebuffer::get_pixels()
{
	auto x = get_cell();
	return std::make_pair(x.first * width, x.second * height);
}

void text_framebuffer::render(char* tbuffer)
{
	uint32_t dummy[8] = {0};
	size_t stride = 24 * width;
	size_t cellstride = 24;
	for(size_t y = 0; y < height; y++) {
		size_t xp = 0;
		for(size_t x = 0; x < width; x++) {
			if(xp >= width)
				break;	//No more space in row.
			char* cellbase = tbuffer + y * (16 * stride) + xp * cellstride;
			const element& e = buffer[y * width + x];
			const bitmap_font::glyph& g = main_font.get_glyph(e.ch);
			char bgb = (e.bg >> 16);
			char bgg = (e.bg >> 8);
			char bgr = (e.bg >> 0);
			char fgb = (e.fg >> 16);
			char fgg = (e.fg >> 8);
			char fgr = (e.fg >> 0);
			const uint32_t* data = (g.data ? g.data : dummy);
			if(g.wide && xp < width - 1) {
				//Wide character, can draw full width.
				for(size_t y2 = 0; y2 < 16; y2++) {
					uint32_t d = data[y2 >> 1];
					d >>= 16 - ((y2 & 1) << 4);
					for(size_t j = 0; j < 16; j++) {
						uint32_t b = 15 - j;
						if(((d >> b) & 1) != 0) {
							cellbase[3 * j + 0] = fgr;
							cellbase[3 * j + 1] = fgg;
							cellbase[3 * j + 2] = fgb;
						} else {
							cellbase[3 * j + 0] = bgr;
							cellbase[3 * j + 1] = bgg;
							cellbase[3 * j + 2] = bgb;
						}
					}
					cellbase += stride;
				}
				xp += 2;
			} else if(g.wide) {
				//Wide character, can only draw half.
				for(size_t y2 = 0; y2 < 16; y2++) {
					uint32_t d = data[y2 >> 1];
					d >>= 16 - ((y2 & 1) << 4);
					for(size_t j = 0; j < 8; j++) {
						uint32_t b = 15 - j;
						if(((d >> b) & 1) != 0) {
							cellbase[3 * j + 0] = fgr;
							cellbase[3 * j + 1] = fgg;
							cellbase[3 * j + 2] = fgb;
						} else {
							cellbase[3 * j + 0] = bgr;
							cellbase[3 * j + 1] = bgg;
							cellbase[3 * j + 2] = bgb;
						}
					}
					cellbase += stride;
				}
				xp += 2;
			} else {
				//Narrow character.
				for(size_t y2 = 0; y2 < 16; y2++) {
					uint32_t d = data[y2 >> 2];
					d >>= 24 - ((y2 & 3) << 3);
					for(size_t j = 0; j < 8; j++) {
						uint32_t b = 7 - j;
						if(((d >> b) & 1) != 0) {
							cellbase[3 * j + 0] = fgr;
							cellbase[3 * j + 1] = fgg;
							cellbase[3 * j + 2] = fgb;
						} else {
							cellbase[3 * j + 0] = bgr;
							cellbase[3 * j + 1] = bgg;
							cellbase[3 * j + 2] = bgb;
						}
					}
					cellbase += stride;
				}
				xp += 1;
			}
		}
	}
}

size_t text_framebuffer::text_width(const std::string& text)
{
	auto x = main_font.get_metrics(text);
	return x.first / 8;
}

size_t text_framebuffer::write(const std::string& str, size_t w, size_t x, size_t y, uint32_t fg, uint32_t bg)
{
	size_t spos = 0;
	size_t slen = str.length();
	size_t pused = 0;
	uint16_t state = utf8_initial_state;
	while(true) {
		int ch = (spos < slen) ? (unsigned char)str[spos] : - 1;
		int32_t u = utf8_parse_byte(ch, state);
		if(u < 0) {
			if(ch < 0)
				break;
			spos++;
			continue;
		}
		//Okay, got u to write...
		const bitmap_font::glyph& g = main_font.get_glyph(u);
		if(x < width) {
			element& e = buffer[y * width + x];
			e.ch = u;
			e.fg = fg;
			e.bg = bg;
		}
		x++;
		pused += (g.wide ? 2 : 1);
		spos++;
	}
	while(pused < w) {
		//Pad with spaces.
		if(x < width) {
			element& e = buffer[y * width + x];
			e.ch = 32;
			e.fg = fg;
			e.bg = bg;
		}
		pused++;
		x++;
	}
	return x;
}

