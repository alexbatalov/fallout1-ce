#ifndef FALLOUT_PLIB_DB_DB_H_
#define FALLOUT_PLIB_DB_DB_H_

#include <stddef.h>

namespace fallout {

#define INVALID_DATABASE_HANDLE ((DB_DATABASE*)-1)

typedef struct DB_FILE DB_FILE;
typedef struct DB_DATABASE DB_DATABASE;

typedef struct dir_entry_s {
    int flags;
    int offset;
    int length;
    int field_C;
} dir_entry;

typedef void db_read_callback();
typedef void*(db_malloc_func)(size_t size);
typedef char*(db_strdup_func)(const char* string);
typedef void(db_free_func)(void* ptr);

DB_DATABASE* db_init(const char* datafile, const char* datafile_path, const char* patches_path, int show_cursor);
int db_select(DB_DATABASE* db_handle);
DB_DATABASE* db_current();
int db_total();
int db_close(DB_DATABASE* db_handle);
void db_exit();
int db_dir_entry(const char* filePath, dir_entry* de);
int db_read_to_buf(const char* filePath, unsigned char* ptr);
DB_FILE* db_fopen(const char* filename, const char* mode);
int db_fclose(DB_FILE* stream);
size_t db_fread(void* buf, size_t size, size_t count, DB_FILE* stream);
int db_fgetc(DB_FILE* stream);
int db_ungetc(int ch, DB_FILE* stream);
char* db_fgets(char* str, size_t size, DB_FILE* stream);
size_t db_fwrite(const void* buf, size_t size, size_t count, DB_FILE* stream);
int db_fputc(int ch, DB_FILE* stream);
int db_fputs(const char* s, DB_FILE* stream);
int db_fseek(DB_FILE* stream, long offset, int origin);
long db_ftell(DB_FILE* stream);
void db_rewind(DB_FILE* stream);
int db_freadByte(DB_FILE* stream, unsigned char* c);
int db_freadShort(DB_FILE* stream, unsigned short* s);
int db_freadInt(DB_FILE* stream, int* i);
int db_freadLong(DB_FILE* stream, unsigned long* l);
int db_freadFloat(DB_FILE* stream, float* q);
int db_fwriteByte(DB_FILE* stream, unsigned char c);
int db_fwriteShort(DB_FILE* stream, unsigned short s);
int db_fwriteInt(DB_FILE* stream, int i);
int db_fwriteLong(DB_FILE* stream, unsigned long l);
int db_fwriteFloat(DB_FILE* stream, float q);
int db_freadByteCount(DB_FILE* stream, unsigned char* c, int count);
int db_freadShortCount(DB_FILE* stream, unsigned short* s, int count);
int db_freadIntCount(DB_FILE* stream, int* i, int count);
int db_freadLongCount(DB_FILE* stream, unsigned long* l, int count);
int db_freadFloatCount(DB_FILE* stream, float* q, int count);
int db_fwriteByteCount(DB_FILE* stream, unsigned char* c, int count);
int db_fwriteShortCount(DB_FILE* stream, unsigned short* s, int count);
int db_fwriteIntCount(DB_FILE* stream, int* i, int count);
int db_fwriteLongCount(DB_FILE* stream, unsigned long* l, int count);
int db_fwriteFloatCount(DB_FILE* stream, float* q, int count);
int db_fprintf(DB_FILE* stream, const char* format, ...);
int db_feof(DB_FILE* stream);
int db_get_file_list(const char* filespec, char*** filelist, char*** desclist, int desclen);
void db_free_file_list(char*** file_list, char*** desclist);
long db_filelength(DB_FILE* stream);
void db_register_mem(db_malloc_func* malloc_func, db_strdup_func* strdup_func, db_free_func* free_func);
void db_register_callback(db_read_callback* callback, size_t threshold);
void db_enable_hash_table();
int db_reset_hash_tables();
int db_add_hash_entry(const char* path, int sep);

int db_freadUInt8(DB_FILE* stream, unsigned char* valuePtr);
int db_freadInt8(DB_FILE* stream, char* valuePtr);
int db_freadUInt16(DB_FILE* stream, unsigned short* valuePtr);
int db_freadInt16(DB_FILE* stream, short* valuePtr);
int db_freadUInt32(DB_FILE* stream, unsigned int* valuePtr);
int db_freadInt32(DB_FILE* stream, int* valuePtr);
int db_freadUInt8List(DB_FILE* stream, unsigned char* arr, int count);
int db_freadInt8List(DB_FILE* stream, char* arr, int count);
int db_freadInt16List(DB_FILE* stream, short* arr, int count);
int db_freadInt32List(DB_FILE* stream, int* arr, int count);
int db_freadBool(DB_FILE* stream, bool* valuePtr);

int db_fwriteUInt8(DB_FILE* stream, unsigned char value);
int db_fwriteInt8(DB_FILE* stream, char value);
int db_fwriteUInt16(DB_FILE* stream, unsigned short value);
int db_fwriteInt16(DB_FILE* stream, short value);
int db_fwriteUInt32(DB_FILE* stream, unsigned int value);
int db_fwriteInt32(DB_FILE* stream, int value);
int db_fwriteUInt8List(DB_FILE* stream, unsigned char* arr, int count);
int db_fwriteInt8List(DB_FILE* stream, char* arr, int count);
int db_fwriteInt16List(DB_FILE* stream, short* arr, int count);
int db_fwriteInt32List(DB_FILE* stream, int* arr, int count);
int db_fwriteBool(DB_FILE* stream, bool value);

} // namespace fallout

#endif /* FALLOUT_PLIB_DB_DB_H_ */
