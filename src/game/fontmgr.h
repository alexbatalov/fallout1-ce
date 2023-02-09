#ifndef FALLOUT_GAME_FONTMGR_H_
#define FALLOUT_GAME_FONTMGR_H_

namespace fallout {

int FMInit();
void FMExit();
void FMtext_font(int font);
int FMtext_height();
int FMtext_width(const char* string);
int FMtext_char_width(char c);
int FMtext_mono_width(const char* string);
int FMtext_spacing();
int FMtext_size(const char* string);
int FMtext_max();
int FMtext_curr();
void FMtext_to_buf(unsigned char* buf, const char* string, int length, int pitch, int color);

} // namespace fallout

#endif /* FALLOUT_GAME_FONTMGR_H_ */
