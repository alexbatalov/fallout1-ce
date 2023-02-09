#ifndef FALLOUT_INT_WIDGET_H_
#define FALLOUT_INT_WIDGET_H_

namespace fallout {

typedef void(UpdateRegionShowFunc)(void* value);
typedef void(UpdateRegionDrawFunc)(void* value);
typedef void(TextInputRegionDeleteFunc)(char* text, void* userData);

int win_add_text_input_region(int textRegionId, char* text, int a3, int a4);
void windowSelectTextInputRegion(int textInputRegionId);
int win_delete_all_text_input_regions(int win);
int win_delete_text_input_region(int textInputRegionId);
int win_set_text_input_delete_func(int textInputRegionId, TextInputRegionDeleteFunc* deleteFunc, void* userData);
int win_add_text_region(int win, int x, int y, int width, int font, int textAlignment, int textFlags, int backgroundColor);
int win_print_text_region(int textRegionId, char* string);
int win_print_substr_region(int textRegionId, char* string, int stringLength);
int win_update_text_region(int textRegionId);
int win_delete_text_region(int textRegionId);
int win_delete_all_update_regions(int a1);
int win_text_region_style(int textRegionId, int font, int textAlignment, int textFlags, int backgroundColor);
void win_delete_widgets(int win);
int widgetDoInput();
int win_center_str(int win, char* string, int y, int a4);
int draw_widgets();
int update_widgets();
int win_register_update(int win, int x, int y, UpdateRegionShowFunc* showFunc, UpdateRegionDrawFunc* drawFunc, void* value, unsigned int type, int a8);
int win_delete_update_region(int updateRegionIndex);
void win_do_updateregions();
void initWidgets();
void widgetsClose();
void real_win_set_status_bar(int a1, int a2, int a3);
void real_win_update_status_bar(float a1, float a2);
void real_win_increment_status_bar(float a1);
void real_win_add_status_bar(int win, int a2, char* a3, char* a4, int x, int y);
void real_win_get_status_info(int a1, int* a2, int* a3, int* a4);
void real_win_modify_status_info(int a1, int a2, int a3, int a4);

} // namespace fallout

#endif /* FALLOUT_INT_WIDGET_H_ */
