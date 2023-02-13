#ifndef FALLOUT_PLIB_GNW_GRBUF_H_
#define FALLOUT_PLIB_GNW_GRBUF_H_

namespace fallout {

void draw_line(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color);
void draw_box(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7);
void draw_shaded_box(unsigned char* buf, int a2, int a3, int a4, int a5, int a6, int a7, int a8);
void cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void trans_cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch);
void buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void trans_buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch);
void mask_buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* mask, int maskPitch, unsigned char* dest, int destPitch);
void buf_fill(unsigned char* buf, int width, int height, int pitch, int a5);
void buf_texture(unsigned char* buf, int width, int height, int pitch, void* a5, int a6, int a7);
void lighten_buf(unsigned char* buf, int width, int height, int pitch);
void swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2);
void buf_outline(unsigned char* buf, int width, int height, int pitch, int a5);
void srcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);
void transSrcCopy(unsigned char* dest, int destPitch, unsigned char* src, int srcPitch, int width, int height);

} // namespace fallout

#endif /* FALLOUT_PLIB_GNW_GRBUF_H_ */
