// NOTE: This module is completely standalone. It does not have external
// dependencies and uses __cdecl calling convention, which probably means it
// was implemented as a separate library and linked statically.

#include "movie_lib.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio_engine.h"
#include "platform_compat.h"

namespace fallout {

typedef struct STRUCT_6B3690 {
    void* field_0;
    unsigned int field_4;
    int field_8;
} STRUCT_6B3690;

#pragma pack(2)
typedef struct Mve {
    char sig[20];
    short field_14;
    short field_16;
    short field_18;
    int field_1A;
} Mve;
#pragma pack()

typedef struct STRUCT_4F6930 {
    int field_0;
    MovieReadProc* readProc;
    STRUCT_6B3690 field_8;
    void* fileHandle;
    int field_18;
    SDL_Surface* field_24;
    SDL_Surface* field_28;
    int field_2C;
    unsigned char* field_30;
    unsigned char* field_34;
    unsigned char field_38;
    unsigned char field_39;
    unsigned char field_3A;
    unsigned char field_3B;
    int field_3C;
    int field_40;
    int field_44;
    int field_48;
    int field_4C;
    int field_50;
} STRUCT_4F6930;

static void _MVE_MemInit(STRUCT_6B3690* a1, int a2, void* a3);
static void _MVE_MemFree(STRUCT_6B3690* a1);
static void _do_nothing_2(SDL_Surface* a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
static int _sub_4F4B5();
static int _ioReset(void* handle);
static void* _ioRead(int size);
static void* _MVE_MemAlloc(STRUCT_6B3690* a1, unsigned int a2);
static unsigned char* _ioNextRecord();
static void _sub_4F4DD();
static int _MVE_rmHoldMovie();
static int _syncWait();
static void _MVE_sndPause();
static int _syncInit(int a1, int a2);
static void _syncReset(int a1);
static int _MVE_sndConfigure(int a1, int a2, int a3, int a4, int a5, int a6);
static void _MVE_syncSync();
static void _MVE_sndReset();
static void _MVE_sndSync();
static int _syncWaitLevel(int a1);
static void _CallsSndBuff_Loc(unsigned char* a1, int a2);
static int _MVE_sndAdd(unsigned char* dest, unsigned char** src_ptr, int a3, int a4, int a5);
static void _MVE_sndResume();
static int _nfConfig(int a1, int a2, int a3, int a4);
static bool movieLockSurfaces();
static void movieUnlockSurfaces();
static void movieSwapSurfaces();
static void _sfShowFrame(int a1, int a2, int a3);
static void _do_nothing_(int a1, int a2, unsigned short* a3);
static void _SetPalette_1(int a1, int a2);
static void _SetPalette_(int a1, int a2);
static void _palMakeSynthPalette(int a1, int a2, int a3, int a4, int a5, int a6);
static void _palLoadPalette(unsigned char* palette, int a2, int a3);
static void _syncRelease();
static void _ioRelease();
static void _MVE_sndRelease();
static void _nfRelease();
static void _frLoad(STRUCT_4F6930* a1);
static void _frSave(STRUCT_4F6930* a1);
static void _MVE_frClose(STRUCT_4F6930* a1);
static int _MVE_sndDecompM16(unsigned short* a1, unsigned char* a2, int a3, int a4);
static int _MVE_sndDecompS16(unsigned short* a1, unsigned char* a2, int a3, int a4);
static void _nfPkConfig();
static void _nfPkDecomp(unsigned char* buf, unsigned char* a2, int a3, int a4, int a5, int a6);

static constexpr uint16_t loadUInt16LE(const uint8_t* b);
static constexpr uint32_t loadUInt32LE(const uint8_t* b);
static int getOffset(uint16_t v);

// 0x51EBD8
static int dword_51EBD8 = 0;

// 0x51EBDC
static int dword_51EBDC = 4;

// 0x51EBE0
static unsigned short word_51EBE0[256] = {
    // clang-format off
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002F, 0x0033, 0x0038, 0x003D,
    0x0042, 0x0048, 0x004F, 0x0056, 0x005E, 0x0066, 0x0070, 0x007A,
    0x0085, 0x0091, 0x009E, 0x00AD, 0x00BD, 0x00CE, 0x00E1, 0x00F5,
    0x010B, 0x0124, 0x013E, 0x015C, 0x017B, 0x019E, 0x01C4, 0x01ED,
    0x021A, 0x024B, 0x0280, 0x02BB, 0x02FB, 0x0340, 0x038C, 0x03DF,
    0x0439, 0x049C, 0x0508, 0x057D, 0x05FE, 0x0689, 0x0722, 0x07C9,
    0x087F, 0x0945, 0x0A1E, 0x0B0A, 0x0C0C, 0x0D25, 0x0E58, 0x0FA8,
    0x1115, 0x12A4, 0x1458, 0x1633, 0x183A, 0x1A6F, 0x1CD9, 0x1F7B,
    0x225A, 0x257D, 0x28E8, 0x2CA4, 0x30B7, 0x3529, 0x3A03, 0x3F4E,
    0x4515, 0x4B62, 0x5244, 0x59C5, 0x61F6, 0x6AE7, 0x74A8, 0x7F4D,
    0x8AEB, 0x9798, 0xA56E, 0xB486, 0xC4FF, 0xD6F9, 0xEA97, 0xFFFF,
    0x0001, 0x0001, 0x1569, 0x2907, 0x3B01, 0x4B7A, 0x5A92, 0x6868,
    0x7515, 0x80B3, 0x8B58, 0x9519, 0x9E0A, 0xA63B, 0xADBC, 0xB49E,
    0xBAEB, 0xC0B2, 0xC5FD, 0xCAD7, 0xCF49, 0xD35C, 0xD718, 0xDA83,
    0xDDA6, 0xE085, 0xE327, 0xE591, 0xE7C6, 0xE9CD, 0xEBA8, 0xED5C,
    0xEEEB, 0xF058, 0xF1A8, 0xF2DB, 0xF3F4, 0xF4F6, 0xF5E2, 0xF6BB,
    0xF781, 0xF837, 0xF8DE, 0xF977, 0xFA02, 0xFA83, 0xFAF8, 0xFB64,
    0xFBC7, 0xFC21, 0xFC74, 0xFCC0, 0xFD05, 0xFD45, 0xFD80, 0xFDB5,
    0xFDE6, 0xFE13, 0xFE3C, 0xFE62, 0xFE85, 0xFEA4, 0xFEC2, 0xFEDC,
    0xFEF5, 0xFF0B, 0xFF1F, 0xFF32, 0xFF43, 0xFF53, 0xFF62, 0xFF6F,
    0xFF7B, 0xFF86, 0xFF90, 0xFF9A, 0xFFA2, 0xFFAA, 0xFFB1, 0xFFB8,
    0xFFBE, 0xFFC3, 0xFFC8, 0xFFCD, 0xFFD1, 0xFFD5, 0xFFD6, 0xFFD7,
    0xFFD8, 0xFFD9, 0xFFDA, 0xFFDB, 0xFFDC, 0xFFDD, 0xFFDE, 0xFFDF,
    0xFFE0, 0xFFE1, 0xFFE2, 0xFFE3, 0xFFE4, 0xFFE5, 0xFFE6, 0xFFE7,
    0xFFE8, 0xFFE9, 0xFFEA, 0xFFEB, 0xFFEC, 0xFFEE, 0xFFED, 0xFFEF,
    0xFFF0, 0xFFF1, 0xFFF2, 0xFFF3, 0xFFF4, 0xFFF5, 0xFFF6, 0xFFF7,
    0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF,
    // clang-format on
};

// 0x51EDE4
static int _sync_active = 0;

// 0x51EDE8
static int _sync_late = 0;

// 0x51EDEC
static int _sync_FrameDropped = 0;

// 0x51EDF8
static int gMovieLibVolume = 0;

// 0x51EDFC
static int gMovieLibPan = 0;

// 0x51EE08
static MovieShowFrameProc* _sf_ShowFrame = _do_nothing_2;

// 0x51EE0C
static int dword_51EE0C = 1;

// TODO: There is a default function (not yet implemented).
//
// 0x51EE14
static void (*_pal_SetPalette)(unsigned char*, int, int) = NULL;

// 0x51EE18
static int _rm_hold = 0;

// 0x51EE1C
static int _rm_active = 0;

// 0x51EE20
static bool dword_51EE20 = false;

// 0x51F018
static int dword_51F018[256];

// 0x51F418
static unsigned short word_51F418[256] = {
    // clang-format off
    0xF8F8, 0xF8F9, 0xF8FA, 0xF8FB, 0xF8FC, 0xF8FD, 0xF8FE, 0xF8FF,
    0xF800, 0xF801, 0xF802, 0xF803, 0xF804, 0xF805, 0xF806, 0xF807,
    0xF9F8, 0xF9F9, 0xF9FA, 0xF9FB, 0xF9FC, 0xF9FD, 0xF9FE, 0xF9FF,
    0xF900, 0xF901, 0xF902, 0xF903, 0xF904, 0xF905, 0xF906, 0xF907,
    0xFAF8, 0xFAF9, 0xFAFA, 0xFAFB, 0xFAFC, 0xFAFD, 0xFAFE, 0xFAFF,
    0xFA00, 0xFA01, 0xFA02, 0xFA03, 0xFA04, 0xFA05, 0xFA06, 0xFA07,
    0xFBF8, 0xFBF9, 0xFBFA, 0xFBFB, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF,
    0xFB00, 0xFB01, 0xFB02, 0xFB03, 0xFB04, 0xFB05, 0xFB06, 0xFB07,
    0xFCF8, 0xFCF9, 0xFCFA, 0xFCFB, 0xFCFC, 0xFCFD, 0xFCFE, 0xFCFF,
    0xFC00, 0xFC01, 0xFC02, 0xFC03, 0xFC04, 0xFC05, 0xFC06, 0xFC07,
    0xFDF8, 0xFDF9, 0xFDFA, 0xFDFB, 0xFDFC, 0xFDFD, 0xFDFE, 0xFDFF,
    0xFD00, 0xFD01, 0xFD02, 0xFD03, 0xFD04, 0xFD05, 0xFD06, 0xFD07,
    0xFEF8, 0xFEF9, 0xFEFA, 0xFEFB, 0xFEFC, 0xFEFD, 0xFEFE, 0xFEFF,
    0xFE00, 0xFE01, 0xFE02, 0xFE03, 0xFE04, 0xFE05, 0xFE06, 0xFE07,
    0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF,
    0xFF00, 0xFF01, 0xFF02, 0xFF03, 0xFF04, 0xFF05, 0xFF06, 0xFF07,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF,
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x01F8, 0x01F9, 0x01FA, 0x01FB, 0x01FC, 0x01FD, 0x01FE, 0x01FF,
    0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107,
    0x02F8, 0x02F9, 0x02FA, 0x02FB, 0x02FC, 0x02FD, 0x02FE, 0x02FF,
    0x0200, 0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207,
    0x03F8, 0x03F9, 0x03FA, 0x03FB, 0x03FC, 0x03FD, 0x03FE, 0x03FF,
    0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306, 0x0307,
    0x04F8, 0x04F9, 0x04FA, 0x04FB, 0x04FC, 0x04FD, 0x04FE, 0x04FF,
    0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
    0x05F8, 0x05F9, 0x05FA, 0x05FB, 0x05FC, 0x05FD, 0x05FE, 0x05FF,
    0x0500, 0x0501, 0x0502, 0x0503, 0x0504, 0x0505, 0x0506, 0x0507,
    0x06F8, 0x06F9, 0x06FA, 0x06FB, 0x06FC, 0x06FD, 0x06FE, 0x06FF,
    0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x0606, 0x0607,
    0x07F8, 0x07F9, 0x07FA, 0x07FB, 0x07FC, 0x07FD, 0x07FE, 0x07FF,
    0x0700, 0x0701, 0x0702, 0x0703, 0x0704, 0x0705, 0x0706, 0x0707,
    // clang-format on
};

// 0x51F618
static unsigned short word_51F618[256] = {
    // clang-format off
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0108,
    0x0109, 0x010A, 0x010B, 0x010C, 0x010D, 0x010E, 0x0208, 0x0209,
    0x020A, 0x020B, 0x020C, 0x020D, 0x020E, 0x0308, 0x0309, 0x030A,
    0x030B, 0x030C, 0x030D, 0x030E, 0x0408, 0x0409, 0x040A, 0x040B,
    0x040C, 0x040D, 0x040E, 0x0508, 0x0509, 0x050A, 0x050B, 0x050C,
    0x050D, 0x050E, 0x0608, 0x0609, 0x060A, 0x060B, 0x060C, 0x060D,
    0x060E, 0x0708, 0x0709, 0x070A, 0x070B, 0x070C, 0x070D, 0x070E,
    0x08F2, 0x08F3, 0x08F4, 0x08F5, 0x08F6, 0x08F7, 0x08F8, 0x08F9,
    0x08FA, 0x08FB, 0x08FC, 0x08FD, 0x08FE, 0x08FF, 0x0800, 0x0801,
    0x0802, 0x0803, 0x0804, 0x0805, 0x0806, 0x0807, 0x0808, 0x0809,
    0x080A, 0x080B, 0x080C, 0x080D, 0x080E, 0x09F2, 0x09F3, 0x09F4,
    0x09F5, 0x09F6, 0x09F7, 0x09F8, 0x09F9, 0x09FA, 0x09FB, 0x09FC,
    0x09FD, 0x09FE, 0x09FF, 0x0900, 0x0901, 0x0902, 0x0903, 0x0904,
    0x0905, 0x0906, 0x0907, 0x0908, 0x0909, 0x090A, 0x090B, 0x090C,
    0x090D, 0x090E, 0x0AF2, 0x0AF3, 0x0AF4, 0x0AF5, 0x0AF6, 0x0AF7,
    0x0AF8, 0x0AF9, 0x0AFA, 0x0AFB, 0x0AFC, 0x0AFD, 0x0AFE, 0x0AFF,
    0x0A00, 0x0A01, 0x0A02, 0x0A03, 0x0A04, 0x0A05, 0x0A06, 0x0A07,
    0x0A08, 0x0A09, 0x0A0A, 0x0A0B, 0x0A0C, 0x0A0D, 0x0A0E, 0x0BF2,
    0x0BF3, 0x0BF4, 0x0BF5, 0x0BF6, 0x0BF7, 0x0BF8, 0x0BF9, 0x0BFA,
    0x0BFB, 0x0BFC, 0x0BFD, 0x0BFE, 0x0BFF, 0x0B00, 0x0B01, 0x0B02,
    0x0B03, 0x0B04, 0x0B05, 0x0B06, 0x0B07, 0x0B08, 0x0B09, 0x0B0A,
    0x0B0B, 0x0B0C, 0x0B0D, 0x0B0E, 0x0CF2, 0x0CF3, 0x0CF4, 0x0CF5,
    0x0CF6, 0x0CF7, 0x0CF8, 0x0CF9, 0x0CFA, 0x0CFB, 0x0CFC, 0x0CFD,
    0x0CFE, 0x0CFF, 0x0C00, 0x0C01, 0x0C02, 0x0C03, 0x0C04, 0x0C05,
    0x0C06, 0x0C07, 0x0C08, 0x0C09, 0x0C0A, 0x0C0B, 0x0C0C, 0x0C0D,
    0x0C0E, 0x0DF2, 0x0DF3, 0x0DF4, 0x0DF5, 0x0DF6, 0x0DF7, 0x0DF8,
    0x0DF9, 0x0DFA, 0x0DFB, 0x0DFC, 0x0DFD, 0x0DFE, 0x0DFF, 0x0D00,
    0x0D01, 0x0D02, 0x0D03, 0x0D04, 0x0D05, 0x0D06, 0x0D07, 0x0D08,
    0x0D09, 0x0D0A, 0x0D0B, 0x0D0C, 0x0D0D, 0x0D0E, 0x0EF2, 0x0EF3,
    0x0EF4, 0x0EF5, 0x0EF6, 0x0EF7, 0x0EF8, 0x0EF9, 0x0EFA, 0x0EFB,
    0x0EFC, 0x0EFD, 0x0EFE, 0x0EFF, 0x0E00, 0x0E01, 0x0E02, 0x0E03,
    0x0E04, 0x0E05, 0x0E06, 0x0E07, 0x0E08, 0x0E09, 0x0E0A, 0x0E0B,
    // clang-format on
};

// 0x51F818
static unsigned int _$$R0053[16] = {
    // clang-format off
    0xC3C3C3C3, 0xC3C3C1C3, 0xC3C3C3C1, 0xC3C3C1C1, 0xC1C3C3C3, 0xC1C3C1C3, 0xC1C3C3C1, 0xC1C3C1C1,
    0xC3C1C3C3, 0xC3C1C1C3, 0xC3C1C3C1, 0xC3C1C1C1, 0xC1C1C3C3, 0xC1C1C1C3, 0xC1C1C3C1, 0xC1C1C1C1,
    // clang-format on
};

// 0x51F858
static unsigned int _$$R0004[256] = {
    // clang-format off
    0xC3C3C3C3, 0xC3C3C2C3, 0xC3C3C1C3, 0xC3C3C5C3, 0xC3C3C3C2, 0xC3C3C2C2, 0xC3C3C1C2, 0xC3C3C5C2,
    0xC3C3C3C1, 0xC3C3C2C1, 0xC3C3C1C1, 0xC3C3C5C1, 0xC3C3C3C5, 0xC3C3C2C5, 0xC3C3C1C5, 0xC3C3C5C5,
    0xC2C3C3C3, 0xC2C3C2C3, 0xC2C3C1C3, 0xC2C3C5C3, 0xC2C3C3C2, 0xC2C3C2C2, 0xC2C3C1C2, 0xC2C3C5C2,
    0xC2C3C3C1, 0xC2C3C2C1, 0xC2C3C1C1, 0xC2C3C5C1, 0xC2C3C3C5, 0xC2C3C2C5, 0xC2C3C1C5, 0xC2C3C5C5,
    0xC1C3C3C3, 0xC1C3C2C3, 0xC1C3C1C3, 0xC1C3C5C3, 0xC1C3C3C2, 0xC1C3C2C2, 0xC1C3C1C2, 0xC1C3C5C2,
    0xC1C3C3C1, 0xC1C3C2C1, 0xC1C3C1C1, 0xC1C3C5C1, 0xC1C3C3C5, 0xC1C3C2C5, 0xC1C3C1C5, 0xC1C3C5C5,
    0xC5C3C3C3, 0xC5C3C2C3, 0xC5C3C1C3, 0xC5C3C5C3, 0xC5C3C3C2, 0xC5C3C2C2, 0xC5C3C1C2, 0xC5C3C5C2,
    0xC5C3C3C1, 0xC5C3C2C1, 0xC5C3C1C1, 0xC5C3C5C1, 0xC5C3C3C5, 0xC5C3C2C5, 0xC5C3C1C5, 0xC5C3C5C5,
    0xC3C2C3C3, 0xC3C2C2C3, 0xC3C2C1C3, 0xC3C2C5C3, 0xC3C2C3C2, 0xC3C2C2C2, 0xC3C2C1C2, 0xC3C2C5C2,
    0xC3C2C3C1, 0xC3C2C2C1, 0xC3C2C1C1, 0xC3C2C5C1, 0xC3C2C3C5, 0xC3C2C2C5, 0xC3C2C1C5, 0xC3C2C5C5,
    0xC2C2C3C3, 0xC2C2C2C3, 0xC2C2C1C3, 0xC2C2C5C3, 0xC2C2C3C2, 0xC2C2C2C2, 0xC2C2C1C2, 0xC2C2C5C2,
    0xC2C2C3C1, 0xC2C2C2C1, 0xC2C2C1C1, 0xC2C2C5C1, 0xC2C2C3C5, 0xC2C2C2C5, 0xC2C2C1C5, 0xC2C2C5C5,
    0xC1C2C3C3, 0xC1C2C2C3, 0xC1C2C1C3, 0xC1C2C5C3, 0xC1C2C3C2, 0xC1C2C2C2, 0xC1C2C1C2, 0xC1C2C5C2,
    0xC1C2C3C1, 0xC1C2C2C1, 0xC1C2C1C1, 0xC1C2C5C1, 0xC1C2C3C5, 0xC1C2C2C5, 0xC1C2C1C5, 0xC1C2C5C5,
    0xC5C2C3C3, 0xC5C2C2C3, 0xC5C2C1C3, 0xC5C2C5C3, 0xC5C2C3C2, 0xC5C2C2C2, 0xC5C2C1C2, 0xC5C2C5C2,
    0xC5C2C3C1, 0xC5C2C2C1, 0xC5C2C1C1, 0xC5C2C5C1, 0xC5C2C3C5, 0xC5C2C2C5, 0xC5C2C1C5, 0xC5C2C5C5,
    0xC3C1C3C3, 0xC3C1C2C3, 0xC3C1C1C3, 0xC3C1C5C3, 0xC3C1C3C2, 0xC3C1C2C2, 0xC3C1C1C2, 0xC3C1C5C2,
    0xC3C1C3C1, 0xC3C1C2C1, 0xC3C1C1C1, 0xC3C1C5C1, 0xC3C1C3C5, 0xC3C1C2C5, 0xC3C1C1C5, 0xC3C1C5C5,
    0xC2C1C3C3, 0xC2C1C2C3, 0xC2C1C1C3, 0xC2C1C5C3, 0xC2C1C3C2, 0xC2C1C2C2, 0xC2C1C1C2, 0xC2C1C5C2,
    0xC2C1C3C1, 0xC2C1C2C1, 0xC2C1C1C1, 0xC2C1C5C1, 0xC2C1C3C5, 0xC2C1C2C5, 0xC2C1C1C5, 0xC2C1C5C5,
    0xC1C1C3C3, 0xC1C1C2C3, 0xC1C1C1C3, 0xC1C1C5C3, 0xC1C1C3C2, 0xC1C1C2C2, 0xC1C1C1C2, 0xC1C1C5C2,
    0xC1C1C3C1, 0xC1C1C2C1, 0xC1C1C1C1, 0xC1C1C5C1, 0xC1C1C3C5, 0xC1C1C2C5, 0xC1C1C1C5, 0xC1C1C5C5,
    0xC5C1C3C3, 0xC5C1C2C3, 0xC5C1C1C3, 0xC5C1C5C3, 0xC5C1C3C2, 0xC5C1C2C2, 0xC5C1C1C2, 0xC5C1C5C2,
    0xC5C1C3C1, 0xC5C1C2C1, 0xC5C1C1C1, 0xC5C1C5C1, 0xC5C1C3C5, 0xC5C1C2C5, 0xC5C1C1C5, 0xC5C1C5C5,
    0xC3C5C3C3, 0xC3C5C2C3, 0xC3C5C1C3, 0xC3C5C5C3, 0xC3C5C3C2, 0xC3C5C2C2, 0xC3C5C1C2, 0xC3C5C5C2,
    0xC3C5C3C1, 0xC3C5C2C1, 0xC3C5C1C1, 0xC3C5C5C1, 0xC3C5C3C5, 0xC3C5C2C5, 0xC3C5C1C5, 0xC3C5C5C5,
    0xC2C5C3C3, 0xC2C5C2C3, 0xC2C5C1C3, 0xC2C5C5C3, 0xC2C5C3C2, 0xC2C5C2C2, 0xC2C5C1C2, 0xC2C5C5C2,
    0xC2C5C3C1, 0xC2C5C2C1, 0xC2C5C1C1, 0xC2C5C5C1, 0xC2C5C3C5, 0xC2C5C2C5, 0xC2C5C1C5, 0xC2C5C5C5,
    0xC1C5C3C3, 0xC1C5C2C3, 0xC1C5C1C3, 0xC1C5C5C3, 0xC1C5C3C2, 0xC1C5C2C2, 0xC1C5C1C2, 0xC1C5C5C2,
    0xC1C5C3C1, 0xC1C5C2C1, 0xC1C5C1C1, 0xC1C5C5C1, 0xC1C5C3C5, 0xC1C5C2C5, 0xC1C5C1C5, 0xC1C5C5C5,
    0xC5C5C3C3, 0xC5C5C2C3, 0xC5C5C1C3, 0xC5C5C5C3, 0xC5C5C3C2, 0xC5C5C2C2, 0xC5C5C1C2, 0xC5C5C5C2,
    0xC5C5C3C1, 0xC5C5C2C1, 0xC5C5C1C1, 0xC5C5C5C1, 0xC5C5C3C5, 0xC5C5C2C5, 0xC5C5C1C5, 0xC5C5C5C5,
    // clang-format on
};

// 0x51FC58
static unsigned int _$$R0063[256] = {
    // clang-format off
    0xE3C3E3C3, 0xE3C7E3C3, 0xE3C1E3C3, 0xE3C5E3C3, 0xE7C3E3C3, 0xE7C7E3C3, 0xE7C1E3C3, 0xE7C5E3C3,
    0xE1C3E3C3, 0xE1C7E3C3, 0xE1C1E3C3, 0xE1C5E3C3, 0xE5C3E3C3, 0xE5C7E3C3, 0xE5C1E3C3, 0xE5C5E3C3,
    0xE3C3E3C7, 0xE3C7E3C7, 0xE3C1E3C7, 0xE3C5E3C7, 0xE7C3E3C7, 0xE7C7E3C7, 0xE7C1E3C7, 0xE7C5E3C7,
    0xE1C3E3C7, 0xE1C7E3C7, 0xE1C1E3C7, 0xE1C5E3C7, 0xE5C3E3C7, 0xE5C7E3C7, 0xE5C1E3C7, 0xE5C5E3C7,
    0xE3C3E3C1, 0xE3C7E3C1, 0xE3C1E3C1, 0xE3C5E3C1, 0xE7C3E3C1, 0xE7C7E3C1, 0xE7C1E3C1, 0xE7C5E3C1,
    0xE1C3E3C1, 0xE1C7E3C1, 0xE1C1E3C1, 0xE1C5E3C1, 0xE5C3E3C1, 0xE5C7E3C1, 0xE5C1E3C1, 0xE5C5E3C1,
    0xE3C3E3C5, 0xE3C7E3C5, 0xE3C1E3C5, 0xE3C5E3C5, 0xE7C3E3C5, 0xE7C7E3C5, 0xE7C1E3C5, 0xE7C5E3C5,
    0xE1C3E3C5, 0xE1C7E3C5, 0xE1C1E3C5, 0xE1C5E3C5, 0xE5C3E3C5, 0xE5C7E3C5, 0xE5C1E3C5, 0xE5C5E3C5,
    0xE3C3E7C3, 0xE3C7E7C3, 0xE3C1E7C3, 0xE3C5E7C3, 0xE7C3E7C3, 0xE7C7E7C3, 0xE7C1E7C3, 0xE7C5E7C3,
    0xE1C3E7C3, 0xE1C7E7C3, 0xE1C1E7C3, 0xE1C5E7C3, 0xE5C3E7C3, 0xE5C7E7C3, 0xE5C1E7C3, 0xE5C5E7C3,
    0xE3C3E7C7, 0xE3C7E7C7, 0xE3C1E7C7, 0xE3C5E7C7, 0xE7C3E7C7, 0xE7C7E7C7, 0xE7C1E7C7, 0xE7C5E7C7,
    0xE1C3E7C7, 0xE1C7E7C7, 0xE1C1E7C7, 0xE1C5E7C7, 0xE5C3E7C7, 0xE5C7E7C7, 0xE5C1E7C7, 0xE5C5E7C7,
    0xE3C3E7C1, 0xE3C7E7C1, 0xE3C1E7C1, 0xE3C5E7C1, 0xE7C3E7C1, 0xE7C7E7C1, 0xE7C1E7C1, 0xE7C5E7C1,
    0xE1C3E7C1, 0xE1C7E7C1, 0xE1C1E7C1, 0xE1C5E7C1, 0xE5C3E7C1, 0xE5C7E7C1, 0xE5C1E7C1, 0xE5C5E7C1,
    0xE3C3E7C5, 0xE3C7E7C5, 0xE3C1E7C5, 0xE3C5E7C5, 0xE7C3E7C5, 0xE7C7E7C5, 0xE7C1E7C5, 0xE7C5E7C5,
    0xE1C3E7C5, 0xE1C7E7C5, 0xE1C1E7C5, 0xE1C5E7C5, 0xE5C3E7C5, 0xE5C7E7C5, 0xE5C1E7C5, 0xE5C5E7C5,
    0xE3C3E1C3, 0xE3C7E1C3, 0xE3C1E1C3, 0xE3C5E1C3, 0xE7C3E1C3, 0xE7C7E1C3, 0xE7C1E1C3, 0xE7C5E1C3,
    0xE1C3E1C3, 0xE1C7E1C3, 0xE1C1E1C3, 0xE1C5E1C3, 0xE5C3E1C3, 0xE5C7E1C3, 0xE5C1E1C3, 0xE5C5E1C3,
    0xE3C3E1C7, 0xE3C7E1C7, 0xE3C1E1C7, 0xE3C5E1C7, 0xE7C3E1C7, 0xE7C7E1C7, 0xE7C1E1C7, 0xE7C5E1C7,
    0xE1C3E1C7, 0xE1C7E1C7, 0xE1C1E1C7, 0xE1C5E1C7, 0xE5C3E1C7, 0xE5C7E1C7, 0xE5C1E1C7, 0xE5C5E1C7,
    0xE3C3E1C1, 0xE3C7E1C1, 0xE3C1E1C1, 0xE3C5E1C1, 0xE7C3E1C1, 0xE7C7E1C1, 0xE7C1E1C1, 0xE7C5E1C1,
    0xE1C3E1C1, 0xE1C7E1C1, 0xE1C1E1C1, 0xE1C5E1C1, 0xE5C3E1C1, 0xE5C7E1C1, 0xE5C1E1C1, 0xE5C5E1C1,
    0xE3C3E1C5, 0xE3C7E1C5, 0xE3C1E1C5, 0xE3C5E1C5, 0xE7C3E1C5, 0xE7C7E1C5, 0xE7C1E1C5, 0xE7C5E1C5,
    0xE1C3E1C5, 0xE1C7E1C5, 0xE1C1E1C5, 0xE1C5E1C5, 0xE5C3E1C5, 0xE5C7E1C5, 0xE5C1E1C5, 0xE5C5E1C5,
    0xE3C3E5C3, 0xE3C7E5C3, 0xE3C1E5C3, 0xE3C5E5C3, 0xE7C3E5C3, 0xE7C7E5C3, 0xE7C1E5C3, 0xE7C5E5C3,
    0xE1C3E5C3, 0xE1C7E5C3, 0xE1C1E5C3, 0xE1C5E5C3, 0xE5C3E5C3, 0xE5C7E5C3, 0xE5C1E5C3, 0xE5C5E5C3,
    0xE3C3E5C7, 0xE3C7E5C7, 0xE3C1E5C7, 0xE3C5E5C7, 0xE7C3E5C7, 0xE7C7E5C7, 0xE7C1E5C7, 0xE7C5E5C7,
    0xE1C3E5C7, 0xE1C7E5C7, 0xE1C1E5C7, 0xE1C5E5C7, 0xE5C3E5C7, 0xE5C7E5C7, 0xE5C1E5C7, 0xE5C5E5C7,
    0xE3C3E5C1, 0xE3C7E5C1, 0xE3C1E5C1, 0xE3C5E5C1, 0xE7C3E5C1, 0xE7C7E5C1, 0xE7C1E5C1, 0xE7C5E5C1,
    0xE1C3E5C1, 0xE1C7E5C1, 0xE1C1E5C1, 0xE1C5E5C1, 0xE5C3E5C1, 0xE5C7E5C1, 0xE5C1E5C1, 0xE5C5E5C1,
    0xE3C3E5C5, 0xE3C7E5C5, 0xE3C1E5C5, 0xE3C5E5C5, 0xE7C3E5C5, 0xE7C7E5C5, 0xE7C1E5C5, 0xE7C5E5C5,
    0xE1C3E5C5, 0xE1C7E5C5, 0xE1C1E5C5, 0xE1C5E5C5, 0xE5C3E5C5, 0xE5C7E5C5, 0xE5C1E5C5, 0xE5C5E5C5,
    // clang-format on
};

// 0x6B3660
static int dword_6B3660;

// 0x6B367C
static int _sf_ScreenWidth;

// 0x6B3680
static int dword_6B3680;

// 0x6B3684
static int _rm_FrameDropCount;

// 0x6B3688
static int _snd_buf;

// 0x6B3690
static STRUCT_6B3690 _io_mem_buf;

// 0x6B369C
static int _io_next_hdr;

// 0x6B36A0
static int dword_6B36A0;

// 0x6B36A4
static unsigned int dword_6B36A4;

// 0x6B36A8
static int _rm_FrameCount;

// 0x6B36AC
static int _sf_ScreenHeight;

// 0x6B36B0
static int dword_6B36B0;

// 0x6B36B8
static unsigned char _palette_entries1[768];

// 0x6B39B8
static MveMallocFunc* gMovieLibMallocProc;

// 0x6B39BC
static int (*_rm_ctl)();

// 0x6B39C0
static int _rm_dx;

// 0x6B39C4
static int _rm_dy;

// 0x6B39C8
static int _gSoundTimeBase;

// 0x6B39CC
static void* _io_handle;

// 0x6B39D0
static int _rm_len;

// 0x6B39D4
static MveFreeFunc* gMovieLibFreeProc;

// 0x6B39D8
static int _snd_comp;

// 0x6B39DC
static unsigned char* _rm_p;

// 0x6B39E0
static int dword_6B39E0[60];

// 0x6B3AD0
static int _sync_wait_quanta;

// 0x6B3AD4
static int dword_6B3AD4;

// 0x6B3AD8
static int _rm_track_bit;

// 0x6B3ADC
static int _sync_time;

// 0x6B3AE0
static MovieReadProc* gMovieLibReadProc;

// 0x6B3AE4
static int dword_6B3AE4;

// 0x6B3AE8
static int dword_6B3AE8;

// 0x6B3CEC
static int dword_6B3CEC;

// 0x6B3CF0
static int dword_6B3CF0;

// 0x6B3CF4
static int dword_6B3CF4;

// 0x6B3CF8
static int dword_6B3CF8;

// 0x6B3CFC
static int _mveBW;

// 0x6B3D00
static int dword_6B3D00;

// 0x6B3D04
static int dword_6B3D04;

// 0x6B3D08
static int dword_6B3D08;

// 0x6B3D0C
static unsigned char _pal_tbl[768];

// 0x6B400C
static unsigned char byte_6B400C;

// 0x6B400D
static unsigned char byte_6B400D;

// 0x6B400E
static int dword_6B400E;

// 0x6B4012
static int dword_6B4012;

// 0x6B4016
static unsigned char byte_6B4016;

// 0x6B4017
static int dword_6B4017;

// 0x6B401B
static int dword_6B401B;

// 0x6B401F
static int dword_6B401F;

// 0x6B4023
static int dword_6B4023;

// 0x6B4027
static int dword_6B4027;

// 0x6B402B
static int dword_6B402B;

// 0x6B402F
static int _mveBH;

// 0x6B4033
static unsigned char* gMovieDirectDrawSurfaceBuffer1;

// 0x6B4037
static unsigned char* gMovieDirectDrawSurfaceBuffer2;

// 0x6B403B
static int dword_6B403B;

// 0x6B403F
static int dword_6B403F;

static SDL_Surface* gMovieSdlSurface1;
static SDL_Surface* gMovieSdlSurface2;
static int gMveSoundBuffer = -1;
static unsigned int gMveBufferBytes;

// 0x4F4800
void movieLibSetMemoryProcs(MveMallocFunc* mallocProc, MveFreeFunc* freeProc)
{
    gMovieLibMallocProc = mallocProc;
    gMovieLibFreeProc = freeProc;
}

// 0x4F4860
void movieLibSetReadProc(MovieReadProc* readProc)
{
    gMovieLibReadProc = readProc;
}

// 0x4F4890
static void _MVE_MemInit(STRUCT_6B3690* a1, int a2, void* a3)
{
    if (a3 == NULL) {
        return;
    }

    _MVE_MemFree(a1);

    a1->field_0 = a3;
    a1->field_4 = a2;
    a1->field_8 = 0;
}

// 0x4F48C0
static void _MVE_MemFree(STRUCT_6B3690* a1)
{
    if (a1->field_8 && gMovieLibFreeProc != NULL) {
        gMovieLibFreeProc(a1->field_0);
        a1->field_8 = 0;
    }
    a1->field_4 = 0;
}

// 0x4F4900
void movieLibSetVolume(int volume)
{
    gMovieLibVolume = volume;

    if (gMveSoundBuffer != -1) {
        audioEngineSoundBufferSetVolume(gMveSoundBuffer, volume);
    }
}

// 0x4F4920
void movieLibSetPan(int pan)
{
    gMovieLibPan = pan;

    if (gMveSoundBuffer != -1) {
        audioEngineSoundBufferSetPan(gMveSoundBuffer, pan);
    }
}

// 0x4F4940
void _MVE_sfSVGA(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
    _sf_ScreenWidth = a1;
    _sf_ScreenHeight = a2;
    dword_6B3AD4 = a1;
    dword_6B36B0 = a2;
    dword_6B3D04 = a3;
    if (dword_51EBD8 & 4)
        dword_6B3D04 = 2 * a3;
    dword_6B403F = a4;
    dword_6B3CF4 = a6;
    dword_6B400E = a5;
    dword_6B403B = a7;
    dword_6B3CF0 = a6 + a5;
    dword_6B3D08 = a8;
    if (a7)
        dword_6B4012 = a6 / a7;
    else
        dword_6B4012 = 1;
    dword_51EE0C = 0;
    dword_6B3680 = a9;
}

// 0x4F49F0
void _MVE_sfCallbacks(MovieShowFrameProc* proc)
{
    _sf_ShowFrame = proc;
}

// 0x4F4A00
static void _do_nothing_2(SDL_Surface* a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
}

// 0x4F4A10
void movieLibSetPaletteEntriesProc(void (*fn)(unsigned char*, int, int))
{
    _pal_SetPalette = fn;
}

// 0x4F4B50
static int _sub_4F4B5()
{
    return 0;
}

// 0x4F4B90
void _MVE_rmCallbacks(int (*fn)())
{
    _rm_ctl = fn;
}

// 0x4F4BB0
void _sub_4F4BB(int a1)
{
    if (a1 == 3) {
        dword_51EBDC = 3;
    } else {
        dword_51EBDC = 4;
    }
}

// 0x4F4BD0
void _MVE_rmFrameCounts(int* a1, int* a2)
{
    *a1 = _rm_FrameCount;
    *a2 = _rm_FrameDropCount;
}

// 0x4F4BF0
int _MVE_rmPrepMovie(void* handle, int a2, int a3, char a4)
{
    _sub_4F4DD();

    _rm_dx = a2;
    _rm_dy = a3;
    _rm_track_bit = 1 << a4;

    if (_rm_track_bit == 0) {
        _rm_track_bit = 1;
    }

    if (!_ioReset(handle)) {
        _MVE_rmEndMovie();
        return -8;
    }

    _rm_p = _ioNextRecord();
    _rm_len = 0;

    if (!_rm_p) {
        _MVE_rmEndMovie();
        return -2;
    }

    _rm_active = 1;
    _rm_hold = 0;
    _rm_FrameCount = 0;
    _rm_FrameDropCount = 0;

    return 0;
}

// 0x4F4C90
static int _ioReset(void* handle)
{
    Mve* mve;

    _io_handle = handle;

    mve = (Mve*)_ioRead(sizeof(Mve));
    if (mve == NULL) {
        return 0;
    }

    if (strncmp(mve->sig, "Interplay MVE File\x1A\x00", 20) != 0) {
        return 0;
    }

    if (~mve->field_16 - mve->field_18 != 0xFFFFEDCC) {
        return 0;
    }

    if (mve->field_16 != 256) {
        return 0;
    }

    if (mve->field_14 != 26) {
        return 0;
    }

    _io_next_hdr = mve->field_1A;

    return 1;
}

// Reads data from movie file.
//
// 0x4F4D00
static void* _ioRead(int size)
{
    void* buf;

    buf = _MVE_MemAlloc(&_io_mem_buf, size);
    if (buf == NULL) {
        return NULL;
    }

    return gMovieLibReadProc(_io_handle, buf, size) < 1 ? NULL : buf;
}

// 0x4F4D40
static void* _MVE_MemAlloc(STRUCT_6B3690* a1, unsigned int a2)
{
    void* ptr;

    if (a1->field_4 >= a2) {
        return a1->field_0;
    }

    if (gMovieLibMallocProc == NULL) {
        return NULL;
    }

    _MVE_MemFree(a1);

    ptr = gMovieLibMallocProc(a2 + 100);
    if (ptr == NULL) {
        return NULL;
    }

    _MVE_MemInit(a1, a2 + 100, ptr);

    a1->field_8 = 1;

    return a1->field_0;
}

// 0x4F4DA0
static unsigned char* _ioNextRecord()
{
    unsigned char* buf;

    buf = (unsigned char*)_ioRead((_io_next_hdr & 0xFFFF) + 4);
    if (buf == NULL) {
        return NULL;
    }

    _io_next_hdr = loadUInt32LE(buf + (_io_next_hdr & 0xFFFF));

    return buf;
}

// 0x4F4DD0
static void _sub_4F4DD()
{
    if (dword_51EE20) {
        return;
    }

    // TODO: Incomplete.

    dword_51EE20 = true;
}

// 0x4F4E20
static int _MVE_rmHoldMovie()
{
    if (!_rm_hold) {
        _MVE_sndPause();
        _rm_hold = 1;
    }
    _syncWait();
    return 0;
}

// 0x4F4E40
static int _syncWait()
{
    int result;

    result = 0;
    if (_sync_active) {
        if (((_sync_time + 1000 * compat_timeGetTime()) & 0x80000000) != 0) {
            result = 1;
            while (((_sync_time + 1000 * compat_timeGetTime()) & 0x80000000) != 0)
                ;
        }
        _sync_time += _sync_wait_quanta;
    }

    return result;
}

// 0x4F4EA0
static void _MVE_sndPause()
{
    if (gMveSoundBuffer != -1) {
        audioEngineSoundBufferStop(gMveSoundBuffer);
    }
}

// 0x4F4EC0
int _MVE_rmStepMovie()
{
    int v0;
    unsigned short* v1;
    unsigned int v5;
    int v6;
    int v7;
    int v8;
    int v9;
    int v10;
    int v11;
    int v12;
    int v13;
    unsigned short* v3;
    unsigned short* v21;
    int v18;
    int v19;
    int v20;
    unsigned char* v14;

    v0 = _rm_len;
    v1 = (unsigned short*)_rm_p;

    if (!_rm_active) {
        return -10;
    }

    if (_rm_hold) {
        _MVE_sndResume();
        _rm_hold = 0;
    }

LABEL_5:
    v21 = NULL;
    v3 = NULL;
    if (!v1) {
        v6 = -2;
        _MVE_rmEndMovie();
        return v6;
    }

    while (1) {
        v5 = loadUInt32LE((unsigned char*)v1 + v0);
        v1 = (unsigned short*)((unsigned char*)v1 + v0 + 4);
        v0 = v5 & 0xFFFF;

        switch ((v5 >> 16) & 0xFF) {
        case 0:
            return -1;
        case 1:
            v0 = 0;
            v1 = (unsigned short*)_ioNextRecord();
            goto LABEL_5;
        case 2:
            if (!_syncInit(v1[0], v1[2])) {
                v6 = -3;
                break;
            }
            continue;
        case 3:
            if ((v5 >> 24) < 1) {
                v7 = 0;
            } else {
                v7 = (v1[1] & 0x04) >> 2;
            }
            v8 = loadUInt32LE((unsigned char*)v1 + 6);
            if ((v5 >> 24) == 0) {
                v8 &= 0xFFFF;
            }

            if (_MVE_sndConfigure(v1[0], v8, v1[1] & 0x01, v1[2], (v1[1] & 0x02) >> 1, v7)) {
                continue;
            }

            v6 = -4;
            break;
        case 4:
            // initialize audio buffers
            _MVE_sndSync();
            continue;
        case 5:
            v9 = 0;
            if ((v5 >> 24) >= 2) {
                v9 = v1[3];
            }

            v10 = 1;
            if ((v5 >> 24) >= 1) {
                v10 = v1[2];
            }

            if (!_nfConfig(v1[0], v1[1], v10, v9)) {
                v6 = -5;
                break;
            }

            v11 = 4 * _mveBW / dword_51EBDC & 0xFFFFFFF0;
            if (dword_6B4027) {
                v11 >>= 1;
            }

            v12 = _rm_dx;
            if (v12 < 0) {
                v12 = 0;
            }

            if (v11 + v12 > _sf_ScreenWidth) {
                v6 = -6;
                break;
            }

            v13 = _rm_dy;
            if (v13 < 0) {
                v13 = 0;
            }

            if (_mveBH + v13 > _sf_ScreenHeight) {
                v6 = -6;
                break;
            }

            if (dword_6B4027 && !dword_6B3680) {
                v6 = -6;
                break;
            }

            continue;
        case 7:
            ++_rm_FrameCount;

            v18 = 0;
            if ((v5 >> 24) >= 1) {
                v18 = v1[2];
            }

            v19 = v1[1];
            if (v19 == 0 || v21 || dword_6B3680) {
                _SetPalette_1(v1[0], v19);
            } else {
                _SetPalette_(v1[0], v19);
            }

            if (v21) {
                _do_nothing_(_rm_dx, _rm_dy, v21);
            } else if (!_sync_late || v1[1]) {
                _sfShowFrame(_rm_dx, _rm_dy, v18);
            } else {
                _sync_FrameDropped = 1;
                ++_rm_FrameDropCount;
            }

            v20 = v1[1];
            if (v20 && !v21 && !dword_6B3680) {
                _SetPalette_1(v1[0], v20);
            }

            _rm_p = (unsigned char*)v1;
            _rm_len = v0;

            return 0;
        case 8:
        case 9:
            // push data to audio buffers?
            if (v1[1] & _rm_track_bit) {
                v14 = (unsigned char*)v1 + 6;
                if ((v5 >> 16) != 8) {
                    v14 = NULL;
                }
                _CallsSndBuff_Loc(v14, v1[2]);
            }
            continue;
        case 10:
            if (!dword_51EE0C) {
                continue;
            }

            // TODO: Probably never reached.

            continue;
        case 11:
            // some kind of palette rotation
            _palMakeSynthPalette(v1[0], v1[1], v1[2], v1[3], v1[4], v1[5]);
            continue;
        case 12:
            // palette
            _palLoadPalette((unsigned char*)v1 + 4, v1[0], v1[1]);
            continue;
        case 14:
            // save current position
            v21 = v1;
            continue;
        case 15:
            // save current position
            v3 = v1;
            continue;
        case 17:
            // decode video chunk
            if ((v5 >> 24) < 3) {
                v6 = -8;
                break;
            }

            // swap movie surfaces
            if (v1[6] & 0x01) {
                movieSwapSurfaces();
            }

            if (dword_6B4027) {
                if (dword_51EBD8) {
                    v6 = -8;
                    break;
                }

                // lock
                if (!movieLockSurfaces()) {
                    v6 = -12;
                    break;
                }

                // TODO: Incomplete.
                assert(false);
                // _nfHPkDecomp(v3, v1[7], v1[2], v1[3], v1[4], v1[5]);

                // unlock
                movieUnlockSurfaces();
                continue;
            }

            if ((dword_51EBD8 & 3) == 1) {
                // lock
                if (!movieLockSurfaces()) {
                    v6 = -12;
                    break;
                }

                // TODO: Incomplete.
                assert(false);
                // _nfPkDecompH(v3, v1[7], v1[2], v1[3], v1[4], v1[5]);

                // unlock
                movieUnlockSurfaces();
                continue;
            }

            if ((dword_51EBD8 & 3) == 2) {
                // lock
                if (!movieLockSurfaces()) {
                    v6 = -12;
                    break;
                }

                // TODO: Incomplete.
                assert(false);
                // _nfPkDecompH(v3, v1[7], v1[2], v1[3], v1[4], v1[5]);

                // unlock
                movieUnlockSurfaces();
                continue;
            }

            // lock
            if (!movieLockSurfaces()) {
                v6 = -12;
                break;
            }

            _nfPkDecomp((unsigned char*)v3, (unsigned char*)&v1[7], v1[2], v1[3], v1[4], v1[5]);

            // unlock
            movieUnlockSurfaces();
            continue;
        default:
            // unknown chunk
            continue;
        }
    }

    _MVE_rmEndMovie();
    return v6;
}

// 0x4F54F0
static int _syncInit(int a1, int a2)
{
    int v2;

    v2 = -((a2 >> 1) + a1 * a2);

    if (_sync_active && _sync_wait_quanta == v2) {
        return 1;
    }

    _syncWait();

    _sync_wait_quanta = v2;

    _syncReset(v2);

    return 1;
}

// 0x4F5540
static void _syncReset(int a1)
{
    _sync_active = 1;
    _sync_time = -1000 * compat_timeGetTime() + a1;
}

// 0x4F5570
static int _MVE_sndConfigure(int a1, int a2, int a3, int a4, int a5, int a6)
{
    _MVE_sndReset();

    _snd_comp = a3;
    dword_6B36A0 = a5;
    _snd_buf = a6;

    gMveBufferBytes = (a2 + (a2 >> 1)) & 0xFFFFFFFC;

    dword_6B3AE4 = 0;
    dword_6B3660 = 0;

    gMveSoundBuffer = audioEngineCreateSoundBuffer(gMveBufferBytes, a5 < 1 ? 8 : 16, 2 - (a3 < 1), a4);
    if (gMveSoundBuffer == -1) {
        return 0;
    }

    audioEngineSoundBufferSetVolume(gMveSoundBuffer, gMovieLibVolume);
    audioEngineSoundBufferSetPan(gMveSoundBuffer, gMovieLibPan);

    dword_6B36A4 = 0;

    return 1;
}

// 0x4F56C0
static void _MVE_syncSync()
{
    if (_sync_active) {
        while (((_sync_time + 1000 * compat_timeGetTime()) & 0x80000000) != 0) {
        }
    }
}

// 0x4F56F0
static void _MVE_sndReset()
{
    if (gMveSoundBuffer != -1) {
        audioEngineSoundBufferStop(gMveSoundBuffer);
        audioEngineSoundBufferRelease(gMveSoundBuffer);
        gMveSoundBuffer = -1;
    }
}

// 0x4F5720
static void _MVE_sndSync()
{
    unsigned int dwCurrentPlayCursor;
    unsigned int dwCurrentWriteCursor;
    bool v10;
    unsigned int dwStatus;
    unsigned int v1;
    bool v2;
    int v3;
    unsigned int v4;
    bool v5;
    bool v0;
    unsigned int v6;
    int v7;
    int v8;
    unsigned int v9;

    v0 = false;

    _sync_late = _syncWaitLevel(_sync_wait_quanta >> 2) > -_sync_wait_quanta >> 1 && !_sync_FrameDropped;
    _sync_FrameDropped = 0;

    if (gMveSoundBuffer == -1) {
        return;
    }

    while (1) {
        if (!audioEngineSoundBufferGetStatus(gMveSoundBuffer, &dwStatus)) {
            return;
        }

        if (!audioEngineSoundBufferGetCurrentPosition(gMveSoundBuffer, &dwCurrentPlayCursor, &dwCurrentWriteCursor)) {
            return;
        }

        dwCurrentWriteCursor = dword_6B36A4;

        v1 = (gMveBufferBytes + dword_6B39E0[dword_6B3660] - _gSoundTimeBase)
            % gMveBufferBytes;

        if (dwCurrentPlayCursor <= dword_6B36A4) {
            if (v1 < dwCurrentPlayCursor || v1 >= dword_6B36A4) {
                v2 = false;
            } else {
                v2 = true;
            }
        } else {
            if (v1 < dwCurrentPlayCursor && v1 >= dword_6B36A4) {
                v2 = false;
            } else {
                v2 = true;
            }
        }

        if (!v2 || !(dwStatus & AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING)) {
            if (v0) {
                _syncReset(_sync_wait_quanta + (_sync_wait_quanta >> 2));
            }

            v3 = dword_6B39E0[dword_6B3660];

            if (!(dwStatus & AUDIO_ENGINE_SOUND_BUFFER_STATUS_PLAYING)) {
                v4 = (gMveBufferBytes + v3) % gMveBufferBytes;

                if (dwCurrentWriteCursor >= dwCurrentPlayCursor) {
                    if (v4 >= dwCurrentPlayCursor && v4 < dwCurrentWriteCursor) {
                        v5 = true;
                    } else {
                        v5 = false;
                    }
                } else if (v4 >= dwCurrentPlayCursor || v4 < dwCurrentWriteCursor) {
                    v5 = true;
                } else {
                    v5 = false;
                }

                if (v5) {
                    if (!audioEngineSoundBufferSetCurrentPosition(gMveSoundBuffer, v4)) {
                        return;
                    }

                    if (!audioEngineSoundBufferPlay(gMveSoundBuffer, 1)) {
                        return;
                    }
                }

                break;
            }

            v6 = (gMveBufferBytes + _gSoundTimeBase + v3) % gMveBufferBytes;
            v7 = dwCurrentWriteCursor - dwCurrentPlayCursor;

            if (((dwCurrentWriteCursor - dwCurrentPlayCursor) & 0x80000000) != 0) {
                v7 += gMveBufferBytes;
            }

            v8 = gMveBufferBytes - v7 - 1;
            // NOTE: Original code uses signed comparison.
            if ((int)gMveBufferBytes / 2 < v8) {
                v8 = gMveBufferBytes >> 1;
            }

            v9 = (gMveBufferBytes + dwCurrentPlayCursor - v8) % gMveBufferBytes;

            dwCurrentPlayCursor = v9;

            if (dwCurrentWriteCursor >= v9) {
                if (v6 < dwCurrentPlayCursor || v6 >= dwCurrentWriteCursor) {
                    v10 = false;
                } else {
                    v10 = true;
                }
            } else {
                if (v6 >= v9 || v6 < dwCurrentWriteCursor) {
                    v10 = true;
                } else {
                    v10 = false;
                }
            }

            if (!v10) {
                audioEngineSoundBufferStop(gMveSoundBuffer);
            }

            break;
        }
        v0 = true;
    }

    if (dword_6B3660 != dword_6B3AE4) {
        if (dword_6B3660 == 59) {
            dword_6B3660 = 0;
        } else {
            ++dword_6B3660;
        }
    }
}

// 0x4F59B0
static int _syncWaitLevel(int a1)
{
    int v2;
    int result;

    if (!_sync_active) {
        return 0;
    }

    v2 = _sync_time + a1;
    do {
        result = v2 + 1000 * compat_timeGetTime();
    } while (result < 0);

    _sync_time += _sync_wait_quanta;

    return result;
}

// 0x4F5A00
static void _CallsSndBuff_Loc(unsigned char* a1, int a2)
{
    int v2;
    int v3;
    int v5;
    unsigned int dwCurrentPlayCursor;
    unsigned int dwCurrentWriteCursor;
    void* lpvAudioPtr1;
    unsigned int dwAudioBytes1;
    void* lpvAudioPtr2;
    unsigned int dwAudioBytes2;

    _gSoundTimeBase = a2;

    if (gMveSoundBuffer == -1) {
        return;
    }

    v5 = 60;
    if (dword_6B3660) {
        v5 = dword_6B3660;
    }

    if (dword_6B3AE4 - v5 == -1) {
        return;
    }

    if (!audioEngineSoundBufferGetCurrentPosition(gMveSoundBuffer, &dwCurrentPlayCursor, &dwCurrentWriteCursor)) {
        return;
    }

    dwCurrentWriteCursor = dword_6B36A4;

    if (!audioEngineSoundBufferLock(gMveSoundBuffer, dword_6B36A4, a2, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2, 0)) {
        return;
    }

    v2 = 0;
    v3 = 1;
    if (dwAudioBytes1 != 0) {
        v2 = _MVE_sndAdd((unsigned char*)lpvAudioPtr1, &a1, dwAudioBytes1, 0, 1);
        v3 = 0;
        dword_6B36A4 += dwAudioBytes1;
    }

    if (dwAudioBytes2 != 0) {
        _MVE_sndAdd((unsigned char*)lpvAudioPtr2, &a1, dwAudioBytes2, v2, v3);
        dword_6B36A4 = dwAudioBytes2;
    }

    if (dword_6B36A4 == gMveBufferBytes) {
        dword_6B36A4 = 0;
    }

    audioEngineSoundBufferUnlock(gMveSoundBuffer, lpvAudioPtr1, dwAudioBytes1, lpvAudioPtr2, dwAudioBytes2);

    dword_6B39E0[dword_6B3AE4] = dwCurrentWriteCursor;

    if (dword_6B3AE4 == 59) {
        dword_6B3AE4 = 0;
    } else {
        ++dword_6B3AE4;
    }
}

// 0x4F5B70
static int _MVE_sndAdd(unsigned char* dest, unsigned char** src_ptr, int a3, int a4, int a5)
{
    unsigned char* src;
    int v9;
    unsigned short* v10;
    int v11;
    int result;

    int v12;
    unsigned short* v13;
    int v14;

    src = *src_ptr;

    if (*src_ptr == NULL) {
        memset(dest, dword_6B36A0 < 1 ? 0x80 : 0, a3);
        *src_ptr = NULL;
        return a4;
    }

    if (!_snd_buf) {
        memcpy(dest, src_ptr, a3);
        *src_ptr += a3;
        return a4;
    }

    if (!_snd_comp) {
        if (a5) {
            v9 = *(unsigned short*)src;
            src += 2;

            *(unsigned short*)dest = v9;
            v10 = (unsigned short*)(dest + 2);
            v11 = a3 - 2;
        } else {
            v9 = a4;
            v10 = (unsigned short*)dest;
            v11 = a3;
        }

        result = _MVE_sndDecompM16(v10, src, v11 >> 1, v9);
        *src_ptr = src + (v11 >> 1);
        return result;
    }

    if (a5) {
        v12 = loadUInt32LE(src);
        src += 4;

        *(unsigned int*)dest = v12;
        v13 = (unsigned short*)(dest + 4);
        v14 = a3 - 4;
    } else {
        v13 = (unsigned short*)dest;
        v14 = a3;
        v12 = a4;
    }

    result = _MVE_sndDecompS16(v13, src, v14 >> 2, v12);
    *src_ptr = src + (v14 >> 1);

    return result;
}

// 0x4F5CA0
static void _MVE_sndResume()
{
}

// 0x4F5CB0
static int _nfConfig(int a1, int a2, int a3, int a4)
{
    if (gMovieSdlSurface1 != NULL) {
        SDL_FreeSurface(gMovieSdlSurface1);
        gMovieSdlSurface1 = NULL;
    }

    if (gMovieSdlSurface2 != NULL) {
        SDL_FreeSurface(gMovieSdlSurface2);
        gMovieSdlSurface2 = NULL;
    }

    byte_6B400D = a1;
    byte_6B400C = a2;
    byte_6B4016 = a3;
    _mveBW = 8 * a1;
    _mveBH = 8 * a2 * a3;

    if (dword_51EBD8) {
        _mveBH >>= 1;
    }

    int depth;
    int rmask;
    int gmask;
    int bmask;
    if (a4) {
        depth = 16;
        rmask = 0x7C00;
        gmask = 0x3E0;
        bmask = 0x1F;
    } else {
        depth = 8;
        rmask = 0;
        gmask = 0;
        bmask = 0;
    }

    gMovieSdlSurface1 = SDL_CreateRGBSurface(0, _mveBW, _mveBH, depth, rmask, gmask, bmask, 0);
    if (gMovieSdlSurface1 == NULL) {
        return 0;
    }

    gMovieSdlSurface2 = SDL_CreateRGBSurface(0, _mveBW, _mveBH, depth, rmask, gmask, bmask, 0);
    if (gMovieSdlSurface2 == NULL) {
        return 0;
    }

    dword_6B4027 = a4;
    dword_6B402B = a3 * _mveBW - 8;

    if (a4) {
        _mveBW *= 2;
        dword_6B402B *= 2;
    }

    dword_6B3D00 = 8 * a3 * _mveBW;
    dword_6B3CEC = 7 * a3 * _mveBW;

    _nfPkConfig();

    return 1;
}

// 0x4F5E60
static bool movieLockSurfaces()
{
    if (gMovieSdlSurface1 != NULL && gMovieSdlSurface2 != NULL) {
        if (SDL_LockSurface(gMovieSdlSurface1) != 0) {
            return false;
        }

        gMovieDirectDrawSurfaceBuffer1 = (unsigned char*)gMovieSdlSurface1->pixels;

        if (SDL_LockSurface(gMovieSdlSurface2) != 0) {
            return false;
        }

        gMovieDirectDrawSurfaceBuffer2 = (unsigned char*)gMovieSdlSurface2->pixels;
    }

    return true;
}

// 0x4F5EF0
static void movieUnlockSurfaces()
{
    SDL_UnlockSurface(gMovieSdlSurface1);
    SDL_UnlockSurface(gMovieSdlSurface2);
}

// 0x4F5F20
static void movieSwapSurfaces()
{
    SDL_Surface* tmp = gMovieSdlSurface2;
    gMovieSdlSurface2 = gMovieSdlSurface1;
    gMovieSdlSurface1 = tmp;
}

// 0x4F5F40
static void _sfShowFrame(int a1, int a2, int a3)
{
    int v3;
    int v4;
    int v5;
    int v6;
    int v7;

    v4 = ((4 * _mveBW / dword_51EBDC - 12) & 0xFFFFFFF0) + 12;

    dword_6B3CF8 = _mveBW - dword_51EBDC * (v4 >> 2);

    v3 = a1;
    if (a1 < 0) {
        if (dword_6B4027) {
            v3 = (_sf_ScreenWidth - (v4 >> 1)) >> 1;
        } else {
            v3 = (_sf_ScreenWidth - v4) >> 1;
        }
    }

    if (dword_6B4027) {
        v3 *= 2;
    }

    v5 = a2;
    if (a2 >= 0) {
        v6 = _mveBH;
    } else {
        v6 = _mveBH;
        if (dword_51EBD8 & 4) {
            v5 = (_sf_ScreenHeight - 2 * _mveBH) >> 1;
        } else {
            v5 = (_sf_ScreenHeight - _mveBH) >> 1;
        }
    }

    v7 = v3 & 0xFFFFFFFC;
    if (dword_51EBD8 & 4) {
        v5 >>= 1;
    }

    if (a3) {
        // TODO: Incomplete.
        // _mve_ShowFrameField(off_6B4033, _mveBW, v6, dword_6B401B, dword_6B401F, dword_6B4017, dword_6B4023, v7, v5, a3);
    } else if (dword_51EBDC == 4) {
        _sf_ShowFrame(gMovieSdlSurface1, _mveBW, v6, dword_6B401B, dword_6B401F, dword_6B4017, dword_6B4023, v7, v5);
    } else {
        _sf_ShowFrame(gMovieSdlSurface1, _mveBW, v6, 0, dword_6B401F, ((4 * _mveBW / dword_51EBDC - 12) & 0xFFFFFFF0) + 12, dword_6B4023, v7, v5);
    }
}

// 0x4F6080
static void _do_nothing_(int a1, int a2, unsigned short* a3)
{
}

// 0x4F6090
static void _SetPalette_1(int a1, int a2)
{
    if (!dword_6B4027) {
        _pal_SetPalette(_pal_tbl, a1, a2);
    }
}

// 0x4F60C0
static void _SetPalette_(int a1, int a2)
{
    if (!dword_6B4027) {
        _pal_SetPalette(_palette_entries1, a1, a2);
    }
}

// 0x4F60F0
static void _palMakeSynthPalette(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int i;
    int j;

    for (i = 0; i < a2; i++) {
        for (j = 0; j < a3; j++) {
            _pal_tbl[3 * a1 + 3 * j] = (63 * i) / (a2 - 1);
            _pal_tbl[3 * a1 + 3 * j + 1] = 0;
            _pal_tbl[3 * a1 + 3 * j + 2] = 5 * ((63 * j) / (a3 - 1)) / 8;
        }
    }

    for (i = 0; i < a5; i++) {
        for (j = 0; j < a6; j++) {
            _pal_tbl[3 * a4 + 3 * j] = 0;
            _pal_tbl[3 * a4 + 3 * j + 1] = (63 * i) / (a5 - 1);
            _pal_tbl[3 * a1 + 3 * j + 2] = 5 * ((63 * j) / (a6 - 1)) / 8;
        }
    }
}

// 0x4F6210
static void _palLoadPalette(unsigned char* palette, int a2, int a3)
{
    memcpy(_pal_tbl + 3 * a2, palette, 3 * a3);
}

// 0x4F6240
void _MVE_rmEndMovie()
{
    if (_rm_active) {
        _syncWait();
        _syncRelease();
        _MVE_sndReset();
        _rm_active = 0;
    }
}

// 0x4F6270
static void _syncRelease()
{
    _sync_active = 0;
}

// 0x4F6350
void _MVE_ReleaseMem()
{
    _MVE_rmEndMovie();
    _ioRelease();
    _MVE_sndRelease();
    _nfRelease();
}

// 0x4F6370
static void _ioRelease()
{
    _MVE_MemFree(&_io_mem_buf);
}

// 0x4F6380
static void _MVE_sndRelease()
{
}

// 0x4F6390
static void _nfRelease()
{
    if (gMovieSdlSurface1 != NULL) {
        SDL_FreeSurface(gMovieSdlSurface1);
        gMovieSdlSurface1 = NULL;
    }

    if (gMovieSdlSurface2 != NULL) {
        SDL_FreeSurface(gMovieSdlSurface2);
        gMovieSdlSurface2 = NULL;
    }
}

// 0x4F6550
static void _frLoad(STRUCT_4F6930* a1)
{
    gMovieLibReadProc = a1->readProc;
    _io_mem_buf.field_0 = a1->field_8.field_0;
    _io_mem_buf.field_4 = a1->field_8.field_4;
    _io_mem_buf.field_8 = a1->field_8.field_8;
    _io_handle = a1->fileHandle;
    _io_next_hdr = a1->field_18;
    gMovieSdlSurface1 = a1->field_24;
    gMovieSdlSurface2 = a1->field_28;
    dword_6B3AE8 = a1->field_2C;
    gMovieDirectDrawSurfaceBuffer1 = a1->field_30;
    gMovieDirectDrawSurfaceBuffer2 = a1->field_34;
    byte_6B400D = a1->field_38;
    byte_6B400C = a1->field_39;
    byte_6B4016 = a1->field_3A;
    dword_6B4027 = a1->field_3C;
    _mveBW = a1->field_40;
    _mveBH = a1->field_44;
    dword_6B402B = a1->field_48;
    dword_6B3D00 = a1->field_4C;
    dword_6B3CEC = a1->field_50;
}

// 0x4F6610
static void _frSave(STRUCT_4F6930* a1)
{
    STRUCT_6B3690* ptr;

    ptr = &(a1->field_8);
    a1->readProc = gMovieLibReadProc;
    ptr->field_0 = _io_mem_buf.field_0;
    ptr->field_4 = _io_mem_buf.field_4;
    ptr->field_8 = _io_mem_buf.field_8;
    a1->fileHandle = _io_handle;
    a1->field_18 = _io_next_hdr;
    a1->field_24 = gMovieSdlSurface1;
    a1->field_28 = gMovieSdlSurface2;
    a1->field_2C = dword_6B3AE8;
    a1->field_30 = gMovieDirectDrawSurfaceBuffer1;
    a1->field_34 = gMovieDirectDrawSurfaceBuffer2;
    a1->field_38 = byte_6B400D;
    a1->field_39 = byte_6B400C;
    a1->field_3A = byte_6B4016;
    a1->field_3C = dword_6B4027;
    a1->field_40 = _mveBW;
    a1->field_44 = _mveBH;
    a1->field_48 = dword_6B402B;
    a1->field_4C = dword_6B3D00;
    a1->field_50 = dword_6B3CEC;
}

// 0x4F6930
static void _MVE_frClose(STRUCT_4F6930* a1)
{
    STRUCT_4F6930 v1;

    _frSave(&v1);
    _frLoad(a1);
    _ioRelease();
    _nfRelease();
    _frLoad(&v1);

    if (gMovieLibFreeProc != NULL) {
        gMovieLibFreeProc(a1);
    }
}

// 0x4F697C
static int _MVE_sndDecompM16(unsigned short* a1, unsigned char* a2, int a3, int a4)
{
    int i;
    int v8;
    unsigned short result;

    result = a4;

    v8 = 0;
    for (i = 0; i < a3; i++) {
        v8 = *a2++;
        result += word_51EBE0[v8];
        *a1++ = result;
    }

    return result;
}

// 0x4F69AD
static int _MVE_sndDecompS16(unsigned short* a1, unsigned char* a2, int a3, int a4)
{
    int i;
    unsigned short v4;
    unsigned short v5;
    unsigned short v9;

    v4 = a4 & 0xFFFF;
    v5 = (a4 >> 16) & 0xFFFF;

    v9 = 0;
    for (i = 0; i < a3; i++) {
        v9 = *a2++;
        v4 = (word_51EBE0[v9] + v4) & 0xFFFF;
        *a1++ = v4;

        v9 = *a2++;
        v5 = (word_51EBE0[v9] + v5) & 0xFFFF;
        *a1++ = v5;
    }

    return (v5 << 16) | v4;
}

// 0x4F731D
static void _nfPkConfig()
{
    int* ptr;
    int v1;
    int v2;
    int v3;
    int v4;
    int v5;

    ptr = dword_51F018;
    v1 = _mveBW;
    v2 = 0;

    v3 = 128;
    do {
        *ptr++ = v2;
        v2 += v1;
        --v3;
    } while (v3);

    v4 = -128 * v1;
    v5 = 128;
    do {
        *ptr++ = v4;
        v4 += v1;
        --v5;
    } while (v5);
}

// 0x4F7359
static void _nfPkDecomp(unsigned char* a1, unsigned char* a2, int a3, int a4, int a5, int a6)
{
    int v49;
    unsigned char* dest;
    int v8;
    int v7;
    int i;
    int j;
    ptrdiff_t v10;
    int byte;
    unsigned int value1;
    unsigned int value2;
    int var_10;
    unsigned char map1[512];
    unsigned int map2[256];
    int var_8;
    unsigned int* src_ptr;
    unsigned int* dest_ptr;
    unsigned int nibbles[2];

    dword_6B401B = 8 * a3;
    dword_6B4017 = 8 * a5;
    dword_6B401F = 8 * a4 * byte_6B4016;
    dword_6B4023 = 8 * a6 * byte_6B4016;

    var_8 = dword_6B3D00 - dword_6B4017;
    dest = gMovieDirectDrawSurfaceBuffer1;

    var_10 = dword_6B3CEC - 8;

    if (a3 || a4) {
        dest = gMovieDirectDrawSurfaceBuffer1 + dword_6B401B + _mveBW * dword_6B401F;
    }

    while (a6--) {
        v49 = a5 >> 1;
        while (v49--) {
            v8 = *a1++;
            nibbles[0] = v8 & 0xF;
            nibbles[1] = v8 >> 4;
            for (j = 0; j < 2; j++) {
                v7 = nibbles[j];

                switch (v7) {
                case 1:
                    dest += 8;
                    break;
                case 0:
                case 2:
                case 3:
                case 4:
                case 5:
                    switch (v7) {
                    case 0:
                        v10 = gMovieDirectDrawSurfaceBuffer2 - gMovieDirectDrawSurfaceBuffer1;
                        break;
                    case 2:
                    case 3:
                        if (1) {
                            byte = *a2++;
                            uint16_t offset = word_51F618[byte];
                            if (v7 == 3) {
                                offset = ((-(offset & 0xFF)) & 0xFF) | ((-(offset >> 8) & 0xFF) << 8);
                            }
                            v10 = getOffset(offset);
                        }
                        break;
                    case 4:
                    case 5:
                        if (1) {
                            uint16_t offset;
                            if (v7 == 4) {
                                byte = *a2++;
                                offset = word_51F418[byte];
                            } else {
                                offset = loadUInt16LE(a2);
                                a2 += 2;
                            }

                            v10 = getOffset(offset) + (gMovieDirectDrawSurfaceBuffer2 - gMovieDirectDrawSurfaceBuffer1);
                        }
                        break;
                    }

                    value2 = _mveBW;

                    for (i = 0; i < 8; ++i) {
                        memcpy(dest, dest + v10, 8);
                        dest += value2;
                    }

                    dest -= value2;

                    dest -= var_10;

                    break;
                case 6:
                    nibbles[0] += 2;
                    while (nibbles[0]--) {
                        dest += 16;

                        if (v49--) {
                            continue;
                        }

                        dest += var_8;

                        a6--;
                        v49 = (a5 >> 1) - 1;
                    }
                    break;
                case 7:
                    if (a2[0] > a2[1]) {
                        // 7/1
                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0053[a2[2 + i] & 0xF];
                            map1[i * 8] = value1 & 0xFF;
                            map1[i * 8 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 8 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 8 + 3] = (value1 >> 24) & 0xFF;

                            value1 = _$$R0053[a2[2 + i] >> 4];
                            map1[i * 8 + 4] = value1 & 0xFF;
                            map1[i * 8 + 5] = (value1 >> 8) & 0xFF;
                            map1[i * 8 + 6] = (value1 >> 16) & 0xFF;
                            map1[i * 8 + 7] = (value1 >> 24) & 0xFF;
                        }

                        map2[0xC1] = (a2[1] << 8) | a2[1]; // cx
                        map2[0xC3] = (a2[0] << 8) | a2[0]; // bx

                        value2 = _mveBW;

                        for (i = 0; i < 4; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | (map2[map1[i * 4 + 1]]);

                            dest_ptr = (unsigned int*)(dest + value2);
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | (map2[map1[i * 4 + 1]]);

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]]);

                            dest_ptr = (unsigned int*)(dest + value2);
                            dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]]);

                            dest += value2 * 2;
                        }

                        dest -= value2;

                        a2 += 4;
                        dest -= var_10;
                    } else {
                        // 7/2
                        // VERIFIED
                        for (i = 0; i < 8; i++) {
                            value1 = _$$R0004[a2[2 + i]];
                            map1[i * 4] = value1 & 0xFF;
                            map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                        map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                        map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                        map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                        value2 = _mveBW;

                        for (i = 0; i < 8; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                            dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];

                            dest += value2;
                        }

                        dest -= value2;

                        a2 += 10;
                        dest -= var_10;
                    }

                    break;
                case 8:
                    if (a2[0] > a2[1]) {
                        if (a2[6] > a2[7]) {
                            // 8/1
                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[2 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[8 + i]];
                                map1[16 + i * 4] = value1 & 0xFF;
                                map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = _mveBW;

                            map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                            map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                            map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                            map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];

                                dest += value2;
                            }

                            map2[0xC1] = (a2[6 + 1] << 8) | a2[6 + 0]; // cx
                            map2[0xC3] = (a2[6 + 0] << 8) | a2[6 + 0]; // bx
                            map2[0xC2] = (a2[6 + 0] << 8) | a2[6 + 1]; // dx
                            map2[0xC5] = (a2[6 + 1] << 8) | a2[6 + 1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[16 + i * 4]] << 16) | map2[map1[16 + i * 4 + 1]];
                                dest_ptr[1] = (map2[map1[16 + i * 4 + 2]] << 16) | map2[map1[16 + i * 4 + 3]];

                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= var_10;
                        } else {
                            // 8/2
                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[2 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0004[a2[8 + i]];
                                map1[16 + i * 4] = value1 & 0xFF;
                                map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = _mveBW;

                            map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                            map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                            map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                            map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];
                                dest += value2;
                            }

                            dest -= value2 * 8 - 4;

                            map2[0xC1] = (a2[6 + 1] << 8) | a2[6 + 0]; // cx
                            map2[0xC3] = (a2[6 + 0] << 8) | a2[6 + 0]; // bx
                            map2[0xC2] = (a2[6 + 0] << 8) | a2[6 + 1]; // dx
                            map2[0xC5] = (a2[6 + 1] << 8) | a2[6 + 1]; // bp

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[16 + i * 4]] << 16) | map2[map1[16 + i * 4 + 1]];
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[16 + i * 4 + 2]] << 16) | map2[map1[16 + i * 4 + 3]];
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= 4;
                            dest -= var_10;
                        }
                    } else {
                        // 8/3
                        // VERIFIED
                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[2 + i]];
                            map1[i * 4] = value1 & 0xFF;
                            map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[6 + i]];
                            map1[8 + i * 4] = value1 & 0xFF;
                            map1[8 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[8 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[8 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[10 + i]];
                            map1[16 + i * 4] = value1 & 0xFF;
                            map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 2; i++) {
                            value1 = _$$R0004[a2[14 + i]];
                            map1[24 + i * 4] = value1 & 0xFF;
                            map1[24 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[24 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[24 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        value2 = _mveBW;

                        map2[0xC1] = (a2[1] << 8) | a2[0]; // cx
                        map2[0xC3] = (a2[0] << 8) | a2[0]; // bx
                        map2[0xC2] = (a2[0] << 8) | a2[1]; // dx
                        map2[0xC5] = (a2[1] << 8) | a2[1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4]] << 16) | map2[map1[i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 4 + 2]] << 16) | map2[map1[i * 4 + 3]];
                            dest += value2;
                        }

                        map2[0xC1] = (a2[4 + 1] << 8) | a2[4 + 0]; // cx
                        map2[0xC3] = (a2[4 + 0] << 8) | a2[4 + 0]; // bx
                        map2[0xC2] = (a2[4 + 0] << 8) | a2[4 + 1]; // dx
                        map2[0xC5] = (a2[4 + 1] << 8) | a2[4 + 1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[8 + i * 4]] << 16) | map2[map1[8 + i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[8 + i * 4 + 2]] << 16) | map2[map1[8 + i * 4 + 3]];
                            dest += value2;
                        }

                        dest -= value2 * 8 - 4;

                        map2[0xC1] = (a2[8 + 1] << 8) | a2[8 + 0]; // cx
                        map2[0xC3] = (a2[8 + 0] << 8) | a2[8 + 0]; // bx
                        map2[0xC2] = (a2[8 + 0] << 8) | a2[8 + 1]; // dx
                        map2[0xC5] = (a2[8 + 1] << 8) | a2[8 + 1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 4]] << 16) | map2[map1[16 + i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 4 + 2]] << 16) | map2[map1[16 + i * 4 + 3]];
                            dest += value2;
                        }

                        map2[0xC1] = (a2[12 + 1] << 8) | a2[12 + 0]; // cx
                        map2[0xC3] = (a2[12 + 0] << 8) | a2[12 + 0]; // bx
                        map2[0xC2] = (a2[12 + 0] << 8) | a2[12 + 1]; // dx
                        map2[0xC5] = (a2[12 + 1] << 8) | a2[12 + 1]; // bp

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[24 + i * 4]] << 16) | map2[map1[24 + i * 4 + 1]];
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[24 + i * 4 + 2]] << 16) | map2[map1[24 + i * 4 + 3]];
                            dest += value2;
                        }

                        dest -= value2;

                        a2 += 16;
                        dest -= 4;
                        dest -= var_10;
                    }

                    break;
                case 9:
                    if (a2[0] > a2[1]) {
                        if (a2[2] > a2[3]) {
                            // 9/1
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = _mveBW;

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);

                                dest_ptr = (unsigned int*)(dest + value2);
                                dest_ptr[0] = (map2[map1[i * 8]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);

                                dest += value2 * 2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= var_10;
                        } else {
                            // 9/2
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4 + 3] = value1 & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 16) & 0xFF;
                                map1[i * 4] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = _mveBW;

                            for (i = 0; i < 8; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4]] << 24) | (map2[map1[i * 4 + 0]] << 16) | (map2[map1[i * 4 + 1]] << 8) | (map2[map1[i * 4 + 1]]);
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 24) | (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]] << 8) | (map2[map1[i * 4 + 3]]);

                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 12;
                            dest -= var_10;
                        }
                    } else {
                        if (a2[2] > a2[3]) {
                            // 9/3
                            // VERIFIED
                            for (i = 0; i < 4; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4 + 3] = value1 & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 16) & 0xFF;
                                map1[i * 4] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = _mveBW;

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4 + 0]] << 24) | (map2[map1[i * 4 + 0]] << 16) | (map2[map1[i * 4 + 1]] << 8) | (map2[map1[i * 4 + 1]]);
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 24) | (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]] << 8) | (map2[map1[i * 4 + 3]]);

                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 4 + 0]] << 24) | (map2[map1[i * 4 + 0]] << 16) | (map2[map1[i * 4 + 1]] << 8) | (map2[map1[i * 4 + 1]]);
                                dest_ptr[1] = (map2[map1[i * 4 + 2]] << 24) | (map2[map1[i * 4 + 2]] << 16) | (map2[map1[i * 4 + 3]] << 8) | (map2[map1[i * 4 + 3]]);

                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 8;
                            dest -= var_10;
                        } else {
                            // 9/4
                            // VERIFIED
                            for (i = 0; i < 16; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            value2 = _mveBW;

                            for (i = 0; i < 8; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 20;
                            dest -= var_10;
                        }
                    }
                    break;
                case 10:
                    if (a2[0] > a2[1]) {
                        if (a2[12] > a2[13]) {
                            // 10/1
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[16 + i]];
                                map1[32 + i * 4] = value1 & 0xFF;
                                map1[32 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[32 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[32 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = _mveBW;

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            map2[0xC1] = a2[0x0C + 2]; // mov al, cl
                            map2[0xC3] = a2[0x0C + 0]; // mov al, bl
                            map2[0xC5] = a2[0x0C + 3]; // mov al, ch
                            map2[0xC7] = a2[0x0C + 1]; // mov al, bh
                            map2[0xE1] = a2[0x0C + 2]; // mov ah, cl
                            map2[0xE3] = a2[0x0C + 0]; // mov ah, bl
                            map2[0xE5] = a2[0x0C + 3]; // mov ah, ch
                            map2[0xE7] = a2[0x0C + 1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[32 + i * 8 + 0]] << 16) | (map2[map1[32 + i * 8 + 1]] << 24) | (map2[map1[32 + i * 8 + 2]]) | (map2[map1[32 + i * 8 + 3]] << 8);
                                dest_ptr[1] = (map2[map1[32 + i * 8 + 4]] << 16) | (map2[map1[32 + i * 8 + 5]] << 24) | (map2[map1[32 + i * 8 + 6]]) | (map2[map1[32 + i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 24;
                            dest -= var_10;
                        } else {
                            // 10/2
                            // VERIFIED
                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[4 + i]];
                                map1[i * 4] = value1 & 0xFF;
                                map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            for (i = 0; i < 8; i++) {
                                value1 = _$$R0063[a2[16 + i]];
                                map1[32 + i * 4] = value1 & 0xFF;
                                map1[32 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                                map1[32 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                                map1[32 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                            }

                            value2 = _mveBW;

                            map2[0xC1] = a2[2]; // mov al, cl
                            map2[0xC3] = a2[0]; // mov al, bl
                            map2[0xC5] = a2[3]; // mov al, ch
                            map2[0xC7] = a2[1]; // mov al, bh
                            map2[0xE1] = a2[2]; // mov ah, cl
                            map2[0xE3] = a2[0]; // mov ah, bl
                            map2[0xE5] = a2[3]; // mov ah, ch
                            map2[0xE7] = a2[1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2 * 8 - 4;

                            map2[0xC1] = a2[0x0C + 2]; // mov al, cl
                            map2[0xC3] = a2[0x0C + 0]; // mov al, bl
                            map2[0xC5] = a2[0x0C + 3]; // mov al, ch
                            map2[0xC7] = a2[0x0C + 1]; // mov al, bh
                            map2[0xE1] = a2[0x0C + 2]; // mov ah, cl
                            map2[0xE3] = a2[0x0C + 0]; // mov ah, bl
                            map2[0xE5] = a2[0x0C + 3]; // mov ah, ch
                            map2[0xE7] = a2[0x0C + 1]; // mov ah, bh

                            for (i = 0; i < 4; i++) {
                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[32 + i * 8 + 0]] << 16) | (map2[map1[32 + i * 8 + 1]] << 24) | (map2[map1[32 + i * 8 + 2]]) | (map2[map1[32 + i * 8 + 3]] << 8);
                                dest += value2;

                                dest_ptr = (unsigned int*)dest;
                                dest_ptr[0] = (map2[map1[32 + i * 8 + 4]] << 16) | (map2[map1[32 + i * 8 + 5]] << 24) | (map2[map1[32 + i * 8 + 6]]) | (map2[map1[32 + i * 8 + 7]] << 8);
                                dest += value2;
                            }

                            dest -= value2;

                            a2 += 24;
                            dest -= 4;
                            dest -= var_10;
                        }
                    } else {
                        // 10/3
                        // VERIFIED
                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[4 + i]];
                            map1[i * 4] = value1 & 0xFF;
                            map1[i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[12 + i]];
                            map1[16 + i * 4] = value1 & 0xFF;
                            map1[16 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[16 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[16 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[20 + i]];
                            map1[32 + i * 4] = value1 & 0xFF;
                            map1[32 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[32 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[32 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        for (i = 0; i < 4; i++) {
                            value1 = _$$R0063[a2[28 + i]];
                            map1[48 + i * 4] = value1 & 0xFF;
                            map1[48 + i * 4 + 1] = (value1 >> 8) & 0xFF;
                            map1[48 + i * 4 + 2] = (value1 >> 16) & 0xFF;
                            map1[48 + i * 4 + 3] = (value1 >> 24) & 0xFF;
                        }

                        value2 = _mveBW;

                        map2[0xC1] = a2[2]; // mov al, cl
                        map2[0xC3] = a2[0]; // mov al, bl
                        map2[0xC5] = a2[3]; // mov al, ch
                        map2[0xC7] = a2[1]; // mov al, bh
                        map2[0xE1] = a2[2]; // mov ah, cl
                        map2[0xE3] = a2[0]; // mov ah, bl
                        map2[0xE5] = a2[3]; // mov ah, ch
                        map2[0xE7] = a2[1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 8 + 0]] << 16) | (map2[map1[i * 8 + 1]] << 24) | (map2[map1[i * 8 + 2]]) | (map2[map1[i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[i * 8 + 4]] << 16) | (map2[map1[i * 8 + 5]] << 24) | (map2[map1[i * 8 + 6]]) | (map2[map1[i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        map2[0xC1] = a2[0x08 + 2]; // mov al, cl
                        map2[0xC3] = a2[0x08 + 0]; // mov al, bl
                        map2[0xC5] = a2[0x08 + 3]; // mov al, ch
                        map2[0xC7] = a2[0x08 + 1]; // mov al, bh
                        map2[0xE1] = a2[0x08 + 2]; // mov ah, cl
                        map2[0xE3] = a2[0x08 + 0]; // mov ah, bl
                        map2[0xE5] = a2[0x08 + 3]; // mov ah, ch
                        map2[0xE7] = a2[0x08 + 1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 8 + 0]] << 16) | (map2[map1[16 + i * 8 + 1]] << 24) | (map2[map1[16 + i * 8 + 2]]) | (map2[map1[16 + i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[16 + i * 8 + 4]] << 16) | (map2[map1[16 + i * 8 + 5]] << 24) | (map2[map1[16 + i * 8 + 6]]) | (map2[map1[16 + i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        dest -= value2 * 8 - 4;

                        map2[0xC1] = a2[0x10 + 2]; // mov al, cl
                        map2[0xC3] = a2[0x10 + 0]; // mov al, bl
                        map2[0xC5] = a2[0x10 + 3]; // mov al, ch
                        map2[0xC7] = a2[0x10 + 1]; // mov al, bh
                        map2[0xE1] = a2[0x10 + 2]; // mov ah, cl
                        map2[0xE3] = a2[0x10 + 0]; // mov ah, bl
                        map2[0xE5] = a2[0x10 + 3]; // mov ah, ch
                        map2[0xE7] = a2[0x10 + 1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[32 + i * 8 + 0]] << 16) | (map2[map1[32 + i * 8 + 1]] << 24) | (map2[map1[32 + i * 8 + 2]]) | (map2[map1[32 + i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[32 + i * 8 + 4]] << 16) | (map2[map1[32 + i * 8 + 5]] << 24) | (map2[map1[32 + i * 8 + 6]]) | (map2[map1[32 + i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        map2[0xC1] = a2[0x18 + 2]; // mov al, cl
                        map2[0xC3] = a2[0x18 + 0]; // mov al, bl
                        map2[0xC5] = a2[0x18 + 3]; // mov al, ch
                        map2[0xC7] = a2[0x18 + 1]; // mov al, bh
                        map2[0xE1] = a2[0x18 + 2]; // mov ah, cl
                        map2[0xE3] = a2[0x18 + 0]; // mov ah, bl
                        map2[0xE5] = a2[0x18 + 3]; // mov ah, ch
                        map2[0xE7] = a2[0x18 + 1]; // mov ah, bh

                        for (i = 0; i < 2; i++) {
                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[48 + i * 8 + 0]] << 16) | (map2[map1[48 + i * 8 + 1]] << 24) | (map2[map1[48 + i * 8 + 2]]) | (map2[map1[48 + i * 8 + 3]] << 8);
                            dest += value2;

                            dest_ptr = (unsigned int*)dest;
                            dest_ptr[0] = (map2[map1[48 + i * 8 + 4]] << 16) | (map2[map1[48 + i * 8 + 5]] << 24) | (map2[map1[48 + i * 8 + 6]]) | (map2[map1[48 + i * 8 + 7]] << 8);
                            dest += value2;
                        }

                        dest -= value2;

                        a2 += 32;
                        dest -= 4;
                        dest -= var_10;
                    }
                    break;
                case 11:
                    value2 = _mveBW;

                    for (i = 0; i < 32; i += 4) {
                        memcpy(dest, &a2[i * 2], 8);
                        dest += value2;
                    }

                    dest -= value2;

                    a2 += 64;
                    dest -= var_10;
                    break;
                case 12:
                    value2 = _mveBW;

                    for (i = 0; i < 4; i++) {
                        byte = a2[i * 4 + 0];
                        value1 = byte | (byte << 8);

                        byte = a2[i * 4 + 1];
                        value1 |= (byte << 16) | (byte << 24);

                        byte = a2[i * 4 + 2];
                        value2 = byte | (byte << 8);

                        byte = a2[i * 4 + 3];
                        value2 |= (byte << 16) | (byte << 24);

                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest_ptr = (unsigned int*)(dest + _mveBW);
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest += _mveBW * 2;
                    }

                    dest -= _mveBW;

                    a2 += 16;
                    dest -= var_10;
                    break;
                case 13:
                    byte = a2[0];
                    value1 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    byte = a2[1];
                    value2 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    for (i = 0; i < 2; i++) {
                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest_ptr = (unsigned int*)(dest + _mveBW);
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest += _mveBW * 2;
                    }

                    byte = a2[2];
                    value1 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    byte = a2[3];
                    value2 = byte | (byte << 8) | (byte << 16) | (byte << 24);

                    for (i = 0; i < 2; i++) {
                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest_ptr = (unsigned int*)(dest + _mveBW);
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value2;

                        dest += _mveBW * 2;
                    }

                    dest -= _mveBW;

                    a2 += 4;
                    dest -= var_10;
                    break;
                case 14:
                case 15:
                    if (v7 == 14) {
                        byte = *a2++;
                        value1 = byte | (byte << 8) | (byte << 16) | (byte << 24);
                        value2 = value1;
                    } else {
                        byte = loadUInt16LE(a2);
                        a2 += 2;
                        value1 = byte | (byte << 16);
                        value2 = value1;
                        value2 = (value2 << 8) | (value2 >> (32 - 8));
                    }

                    for (i = 0; i < 4; i++) {
                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value1;
                        dest_ptr[1] = value1;
                        dest += _mveBW;

                        dest_ptr = (unsigned int*)dest;
                        dest_ptr[0] = value2;
                        dest_ptr[1] = value2;
                        dest += _mveBW;
                    }

                    dest -= _mveBW;

                    dest -= var_10;
                    break;
                }
            }
        }

        dest += var_8;
    }
}

constexpr uint16_t loadUInt16LE(const uint8_t* b)
{
    return (b[1] << 8) | b[0];
}

constexpr uint32_t loadUInt32LE(const uint8_t* b)
{
    return (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
}

int getOffset(uint16_t v)
{
    return static_cast<int8_t>(v & 0xFF) + dword_51F018[v >> 8];
}

} // namespace fallout
