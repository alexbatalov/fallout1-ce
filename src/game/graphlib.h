#ifndef FALLOUT_GAME_GRAPHLIB_H_
#define FALLOUT_GAME_GRAPHLIB_H_

namespace fallout {

extern int* dad;
extern int match_length;
extern int textsize;
extern int* rson;
extern int* lson;
extern unsigned char* text_buf;
extern int codesize;
extern int match_position;

int HighRGB(int a1);
void bit1exbit8(int ulx, int uly, int lrx, int lry, int offset_x, int offset_y, unsigned char* src, unsigned char* dest, int src_pitch, int dest_pitch, unsigned char color);
int CompLZS(unsigned char* a1, unsigned char* a2, int a3);
int DecodeLZS(unsigned char* a1, unsigned char* a2, int a3);
void InitGreyTable(int a1, int a2);
void grey_buf(unsigned char* surface, int width, int height, int pitch);

} // namespace fallout

#endif /* FALLOUT_GAME_GRAPHLIB_H_ */
