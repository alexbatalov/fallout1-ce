#include "plib/db/db.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <fpattern.h>

#include "platform_compat.h"
#include "plib/assoc/assoc.h"
#include "plib/db/lzss.h"

namespace fallout {

#define DB_DATABASE_LIST_CAPACITY 10
#define DB_DATABASE_FILE_LIST_CAPACITY 32
#define DB_HASH_TABLE_SIZE 4095

#if defined(_WIN32)
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

typedef struct DB_FILE {
    DB_DATABASE* database;
    unsigned int flags;
    int field_8;
    union {
        int field_C;
        FILE* uncompressed_file_stream;
    };
    int field_10;
    int field_14;
    int field_18;
    unsigned char* field_1C;
    unsigned char* field_20;
} DB_FILE;

typedef struct DB_DATABASE {
    char* datafile;
    FILE* stream;
    char* datafile_path;
    char* patches_path;
    unsigned char should_free_patches_path;
    assoc_array root;
    assoc_array* entries;
    int files_length;
    DB_FILE files[DB_DATABASE_FILE_LIST_CAPACITY];
    unsigned char* hash_table;
} DB_DATABASE;

typedef struct DB_FIND_DATA {
#if defined(_WIN32)
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
#else
    DIR* dir;
    struct dirent* entry;
    char path[COMPAT_MAX_PATH];
#endif
} DB_FIND_DATA;

static int db_read_long(FILE* stream, int* value_ptr);
static int db_write_long(FILE* stream, int value);
static int db_assoc_load_dir_entry(FILE* stream, void* buffer, size_t size, int flags);
static int db_assoc_save_dir_entry(FILE* stream, void* buffer, size_t size, int flags);
static int db_assoc_load_db_dir_entry(DB_FILE* stream, void* buffer, size_t size, int flags);
static int db_assoc_save_db_dir_entry(DB_FILE* stream, void* buffer, size_t size, int flags);
static int db_create_database(DB_DATABASE** database_ptr);
static int db_destroy_database(DB_DATABASE** database_ptr);
static int db_init_database(DB_DATABASE* database, const char* datafile, const char* datafile_path);
static void db_exit_database(DB_DATABASE* database);
static int db_init_patches(DB_DATABASE* database, const char* path);
static void db_exit_patches(DB_DATABASE* database);
static int db_init_hash_table(DB_DATABASE* database);
static int db_reset_hash_table(DB_DATABASE* database);
static int db_fill_hash_table(DB_DATABASE* database, const char* path);
static int db_add_hash_entry_to_database(DB_DATABASE* database, const char* path, int sep);
static int db_set_hash_value(DB_DATABASE* database, unsigned int key, unsigned char enabled);
static int db_get_hash_value(DB_DATABASE* database, const char* path, int sep, int* value_ptr);
static int db_hash_string_to_key(const char* path, int sep, unsigned int* key_ptr);
static void db_exit_hash_table(DB_DATABASE* database);
static DB_FILE* db_add_fp_rec(FILE* stream, unsigned char* a2, int a3, int flags);
static int db_delete_fp_rec(DB_FILE* stream);
static int db_find_empty_position(int* position_ptr);
static int db_find_dir_entry(char* path, dir_entry* de);
static int db_findfirst(const char* path, DB_FIND_DATA* find_data);
static int db_findnext(DB_FIND_DATA* find_data);
static int db_findclose(DB_FIND_DATA* find_data);
static void* internal_malloc(size_t size);
static char* internal_strdup(const char* string);
static void internal_free(void* ptr);
static void* db_default_malloc(size_t size);
static char* db_default_strdup(const char* string);
static void db_default_free(void* ptr);
static void db_preload_buffer(DB_FILE* stream);
static int fread_short(FILE* stream, unsigned short* s);

static inline bool fileFindIsDirectory(DB_FIND_DATA* find_data);
static inline char* fileFindGetName(DB_FIND_DATA* find_data);

// 0x4FE058
static char empty_patches_path[] = "";

// 0x539D34
static DB_DATABASE* current_database = NULL;

// 0x539D38
static bool db_used_malloc = false;

// 0x539D3C
static db_malloc_func* db_malloc = db_default_malloc;

// 0x539D40
static db_strdup_func* db_strdup = db_default_strdup;

// 0x539D44
static db_free_func* db_free = db_default_free;

// 0x539D48
static bool hash_is_on = false;

// NOTE: Original type is `unsigned long`.
//
// 0x539D4C
static size_t read_count = 0;

// NOTE: Original type is `unsigned long`.
//
// 0x539D50
static size_t read_threshold = 16384;

// 0x539D54
static db_read_callback* read_callback = NULL;

// 0x6713C8
static DB_DATABASE* database_list[DB_DATABASE_LIST_CAPACITY];

// 0x4AEE90
DB_DATABASE* db_init(const char* datafile, const char* datafile_path, const char* patches_path, int show_cursor)
{
    DB_DATABASE* database;

    if (db_create_database(&database) != 0) {
        return INVALID_DATABASE_HANDLE;
    }

    if (db_init_database(database, datafile, datafile_path) != 0) {
        db_close(database);
        return INVALID_DATABASE_HANDLE;
    }

    if (db_init_patches(database, patches_path) != 0) {
        db_close(database);
        return INVALID_DATABASE_HANDLE;
    }

    if (current_database == NULL) {
        current_database = database;
    }

    if (hash_is_on) {
        if (db_init_hash_table(database) != 0) {
            database->hash_table = NULL;
        }
    }

    return database;
}

// 0x4AEF10
int db_select(DB_DATABASE* db_handle)
{
    int index;

    if (db_handle == INVALID_DATABASE_HANDLE) {
        return -1;
    }

    for (index = 0; index < DB_DATABASE_LIST_CAPACITY; index++) {
        if (database_list[index] == db_handle) {
            current_database = database_list[index];
            return 0;
        }
    }

    return -1;
}

// 0x4AEF54
DB_DATABASE* db_current()
{
    if (current_database != NULL) {
        return current_database;
    }

    return INVALID_DATABASE_HANDLE;
}

// 0x4AEF6C
int db_total()
{
    int index;
    int count;

    count = 0;
    for (index = 0; index < DB_DATABASE_LIST_CAPACITY; index++) {
        if (database_list[index] != NULL) {
            count++;
        }
    }

    return count;
}

// 0x4AEF88
int db_close(DB_DATABASE* db_handle)
{
    int index;

    if (db_handle == NULL || db_handle == INVALID_DATABASE_HANDLE) {
        return -1;
    }

    for (index = 0; index < DB_DATABASE_LIST_CAPACITY; index++) {
        if (database_list[index] == (DB_DATABASE*)db_handle) {
            if (database_list[index] == current_database) {
                current_database = NULL;
            }

            db_exit_database(database_list[index]);
            db_exit_patches(database_list[index]);
            db_exit_hash_table(database_list[index]);
            db_destroy_database(&(database_list[index]));

            return 0;
        }
    }

    return -1;
}

// 0x4AF048
void db_exit()
{
    int index;

    for (index = 0; index < DB_DATABASE_LIST_CAPACITY; index++) {
        if (database_list[index] != NULL) {
            db_close(database_list[index]);
        }
    }
}

// 0x4AF068
int db_dir_entry(const char* name, dir_entry* de)
{
    char path[COMPAT_MAX_PATH];
    bool v2;
    bool v3;
    int value;
    FILE* stream;

    if (current_database == NULL) {
        return -1;
    }

    if (name == NULL) {
        return -1;
    }

    if (de == NULL) {
        return -1;
    }

    v2 = true;
    if (name[0] == '@') {
        strcpy(path, name + 1);
        v2 = false;
    }

    if (current_database->patches_path != NULL) {
        stream = NULL;
        v3 = false;

        if (v2) {
            snprintf(path, sizeof(path), "%s%s", current_database->patches_path, name);
        }

        compat_windows_path_to_native(path);

        if (db_get_hash_value(current_database, path, PATH_SEP, &value) != 0 || value == 1) {
            v3 = true;
        }

        if (v3) {
            stream = compat_fopen(path, "rb");
        }

        if (stream != NULL) {
            de->flags = 4;
            de->offset = 0;
            de->length = getFileSize(stream);
            de->field_C = 0;
            fclose(stream);
            return 0;
        }
    }

    if (current_database->datafile == NULL) {
        return -1;
    }

    if (v2) {
        snprintf(path, sizeof(path), "%s%s", current_database->datafile_path, name);
    }

    compat_strupr(path);

    if (db_find_dir_entry(path, de) != 0) {
        return -1;
    }

    if (de->flags == 0) {
        de->flags = 16;
    }

    de->flags |= 8;

    return 0;
}

// 0x4AF4F8
int db_read_to_buf(const char* filename, unsigned char* buf)
{
    bool v1;
    char path[COMPAT_MAX_PATH];
    bool v3;
    FILE* stream;
    int hash_value;
    int size;
    size_t bytes_read;
    int remaining_size;
    int chunk_size;
    dir_entry de;
    unsigned char* end;
    unsigned short v4;

    if (current_database == NULL) {
        return -1;
    }

    if (filename == NULL) {
        return -1;
    }

    if (buf == NULL) {
        return -1;
    }

    v1 = true;
    if (filename[0] == '@') {
        strcpy(path, filename + 1);
        v1 = false;
    }

    if (current_database->patches_path != NULL) {
        stream = NULL;
        v3 = false;

        if (v1) {
            snprintf(path, sizeof(path), "%s%s", current_database->patches_path, filename);
        }

        compat_windows_path_to_native(path);

        if (db_get_hash_value(current_database, path, PATH_SEP, &hash_value) != 0 || hash_value == 1) {
            v3 = true;
        }

        if (v3) {
            stream = compat_fopen(path, "rb");
        }

        if (stream != NULL) {
            size = getFileSize(stream);
            if (read_callback != NULL) {
                remaining_size = size;
                chunk_size = read_threshold - read_count;

                while (remaining_size >= chunk_size) {
                    bytes_read = fread(buf, 1, chunk_size, stream);
                    buf += bytes_read;
                    remaining_size -= bytes_read;

                    read_count = 0;
                    read_callback();

                    chunk_size = read_threshold;
                }

                if (remaining_size != 0) {
                    fread(buf, 1, remaining_size, stream);
                    read_count += remaining_size;
                }
            } else {
                fread(buf, 1, size, stream);
            }

            fclose(stream);

            return 0;
        }
    }

    if (current_database->datafile == NULL) {
        return -1;
    }

    if (v1) {
        snprintf(path, sizeof(path), "%s%s", current_database->datafile_path, filename);
    }

    compat_strupr(path);

    if (db_find_dir_entry(path, &de) == -1) {
        return -1;
    }

    if (current_database->stream == NULL) {
        return -1;
    }

    if (fseek(current_database->stream, de.offset, SEEK_SET) != 0) {
        return -1;
    }

    if (de.flags == 0) {
        de.flags = 16;
    }

    switch (de.flags & 0xF0) {
    case 16:
        lzss_decode_to_buf(current_database->stream, buf, de.field_C);
        break;
    case 32:
        if (read_callback != NULL) {
            remaining_size = de.length;
            chunk_size = read_threshold - read_count;

            while (remaining_size >= chunk_size) {
                bytes_read = fread(buf, 1, chunk_size, current_database->stream);
                buf += bytes_read;
                remaining_size -= bytes_read;

                read_count = 0;
                read_callback();

                chunk_size = read_threshold;
            }

            if (remaining_size != 0) {
                fread(buf, 1, remaining_size, current_database->stream);
                read_count += remaining_size;
            }
        } else {
            fread(buf, 1, de.length, current_database->stream);
        }
        break;
    case 64:
        end = buf + de.length;
        if (read_callback != NULL) {
            while (buf < end) {
                if (fread_short(current_database->stream, &v4) == 0) {
                    if ((v4 & 0x8000) != 0) {
                        v4 &= ~0x8000;
                        bytes_read = fread(buf, 1, v4, current_database->stream);

                        buf += bytes_read;
                        read_count += bytes_read;

                        while (read_count >= read_threshold) {
                            read_count -= read_threshold;
                            read_callback();
                        }
                    } else {
                        read_count += lzss_decode_to_buf(current_database->stream, buf, v4);
                        while (read_count >= read_threshold) {
                            read_count -= read_threshold;
                            read_callback();
                        }
                    }
                }
            }
        } else {
            while (buf < end) {
                if (fread_short(current_database->stream, &v4) == 0) {
                    if ((v4 & 0x8000) != 0) {
                        v4 &= ~0x8000;
                        fread(buf, 1, v4, current_database->stream);
                        buf += v4;
                    } else {
                        buf += lzss_decode_to_buf(current_database->stream, buf, v4);
                    }
                }
            }
        }
    }

    return 0;
}

// 0x4AF9C4
DB_FILE* db_fopen(const char* filename, const char* mode)
{
    bool v1;
    char path[COMPAT_MAX_PATH];
    FILE* stream;
    bool v2;
    int hash_value;
    int mode_value;
    bool mode_is_text;
    int flags;
    int k;
    dir_entry de;
    unsigned char* buf;

    if (current_database == NULL) {
        return NULL;
    }

    if (filename == NULL) {
        return NULL;
    }

    if (mode == NULL) {
        return NULL;
    }

    if (current_database->files_length >= DB_DATABASE_FILE_LIST_CAPACITY) {
        return NULL;
    }

    mode_value = -1;
    mode_is_text = true;
    for (k = 0; mode[k] != '\0'; k++) {
        switch (mode[k]) {
        case 'b':
            mode_is_text = false;
            break;
        case '+':
        case 'a':
        case 'w':
            mode_value = 0;
            break;
        case 'r':
            mode_value = 1;
            break;
        }
    }

    if (mode_value == -1) {
        return NULL;
    }

    stream = NULL;
    flags = 1;
    if (mode_is_text) {
        flags = 2;
    }

    v1 = true;
    if (filename[0] == '@') {
        strcpy(path, filename + 1);
        v1 = false;
    }

    if (current_database->patches_path != NULL) {
        v2 = false;

        if (v1) {
            snprintf(path, sizeof(path), "%s%s", current_database->patches_path, filename);
        }

        compat_windows_path_to_native(path);

        if (mode_value == 0) {
            db_add_hash_entry_to_database(current_database, path, PATH_SEP);
            v2 = true;
        } else {
            if (db_get_hash_value(current_database, path, PATH_SEP, &hash_value) != 0 || hash_value == 1) {
                v2 = true;
            }
        }

        if (v2) {
            stream = compat_fopen(path, mode);
        }

        if (stream != NULL) {
            return db_add_fp_rec(stream, NULL, 0, flags | 0x4);
        }
    }

    if (mode_value == 0) {
        return NULL;
    }

    if (current_database->datafile == NULL) {
        return NULL;
    }

    if (v1) {
        snprintf(path, sizeof(path), "%s%s", current_database->datafile_path, filename);
    }

    compat_strupr(path);

    if (db_find_dir_entry(path, &de) == -1) {
        return NULL;
    }

    if (current_database->stream == NULL) {
        return NULL;
    }

    if (fseek(current_database->stream, de.offset, SEEK_SET) != 0) {
        return NULL;
    }

    if (de.flags == 0) {
        de.flags = 16;
    }

    switch (de.flags & 0xF0) {
    case 16:
        buf = (unsigned char*)internal_malloc(de.length);
        if (buf != NULL) {
            lzss_decode_to_buf(current_database->stream, buf, de.field_C);
            return db_add_fp_rec(NULL, buf, de.length, flags | 0x10 | 0x8);
        }
        break;
    case 32:
        return db_add_fp_rec(current_database->stream, NULL, de.length, flags | 0x20 | 0x8);
    case 64:
        buf = (unsigned char*)internal_malloc(0x4000);
        if (buf != NULL) {
            return db_add_fp_rec(current_database->stream, buf, de.length, flags | 0x40 | 0x8);
        }
        break;
    }

    return NULL;
}

// 0x4B2664
int db_fclose(DB_FILE* stream)
{
    return db_delete_fp_rec(stream);
}

// 0x4AFD50
size_t db_fread(void* ptr, size_t size, size_t count, DB_FILE* stream)
{
    int remaining_size;
    int chunk_size;
    size_t bytes_read;
    unsigned char* buf;
    size_t elements_read;
    size_t v1;

    buf = (unsigned char*)ptr;
    elements_read = 0;

    if (stream != NULL) {
        if ((stream->flags & 0x4) != 0) {
            if (read_callback != NULL) {
                remaining_size = size * count;
                chunk_size = read_threshold - read_count;

                while (remaining_size >= chunk_size) {
                    bytes_read = fread(buf, 1, chunk_size, stream->uncompressed_file_stream);
                    buf += bytes_read;
                    remaining_size -= bytes_read;
                    elements_read += bytes_read;

                    read_count = 0;
                    read_callback();

                    chunk_size = read_threshold;
                }

                if (remaining_size != 0) {
                    elements_read += fread(buf, 1, remaining_size, stream->uncompressed_file_stream);
                    read_count += remaining_size;
                }

                elements_read /= size;
            } else {
                elements_read = fread(buf, size, count, stream->uncompressed_file_stream);
            }
        } else {
            if (ptr != NULL) {
                switch (stream->flags & 0xF0) {
                case 16:
                    if (stream->field_10 != 0) {
                        elements_read = stream->field_10 / size;
                        if (elements_read > count) {
                            elements_read = count;
                        }

                        if (elements_read != 0) {
                            remaining_size = elements_read * size;
                            if (read_callback != NULL) {
                                chunk_size = read_threshold - read_count;
                                while (remaining_size >= chunk_size) {
                                    remaining_size -= chunk_size;
                                    memcpy(buf, stream->field_20, chunk_size);

                                    buf += chunk_size;
                                    stream->field_20 += chunk_size;
                                    stream->field_10 -= chunk_size;

                                    read_count = 0;
                                    read_callback();

                                    chunk_size = read_threshold;
                                }

                                if (remaining_size != 0) {
                                    memcpy(buf, stream->field_20, remaining_size);
                                    stream->field_20 += remaining_size;
                                    stream->field_10 -= remaining_size;
                                    read_count += remaining_size;
                                }
                            } else {
                                memcpy(ptr, stream->field_20, remaining_size);
                                stream->field_20 += remaining_size;
                                stream->field_10 -= remaining_size;
                            }
                        }
                    }
                    break;
                case 32:
                    if (stream->field_10 != 0) {
                        elements_read = stream->field_10 / size;
                        if (elements_read > count) {
                            elements_read = count;
                        }

                        if (elements_read != 0) {
                            if (fseek(stream->database->stream, stream->field_18, SEEK_SET) == 0) {
                                if (read_callback != NULL) {
                                    remaining_size = elements_read * size;
                                    chunk_size = read_threshold - read_count;

                                    // CE: Reuse `elements_read` to represent
                                    // number of bytes read.
                                    elements_read = 0;

                                    while (remaining_size >= chunk_size) {
                                        bytes_read = fread(buf, 1, chunk_size, stream->database->stream);
                                        buf += bytes_read;
                                        remaining_size -= bytes_read;
                                        elements_read += bytes_read;

                                        read_count = 0;
                                        read_callback();

                                        chunk_size = read_threshold;
                                    }

                                    if (remaining_size != 0) {
                                        elements_read += fread(buf, 1, remaining_size, stream->database->stream);
                                        read_count += remaining_size;
                                    }

                                    stream->field_18 = ftell(stream->database->stream);
                                    stream->field_10 -= elements_read * size;

                                    elements_read /= size;
                                } else {
                                    elements_read = fread(buf, size, elements_read, stream->database->stream);
                                    stream->field_18 = ftell(stream->database->stream);
                                    stream->field_10 -= elements_read * size;
                                }
                            }
                        }
                    }
                    break;
                case 64:
                    if (stream->field_10 != 0) {
                        elements_read = stream->field_10 / size;
                        if (elements_read > count) {
                            elements_read = count;
                        }

                        if (elements_read != 0) {
                            remaining_size = elements_read * size;
                            if (read_callback != NULL) {
                                chunk_size = read_threshold - read_count;
                                while (remaining_size > chunk_size) {
                                    db_preload_buffer(stream);

                                    v1 = stream->field_1C - (stream->field_20 - 0x4000);
                                    if (v1 > chunk_size) {
                                        v1 = chunk_size;
                                    }

                                    // FIXME: Copying same data twice.
                                    memcpy(buf, stream->field_20, v1);
                                    memcpy(buf, stream->field_20, v1);

                                    stream->field_20 += v1;
                                    stream->field_10 -= v1;

                                    buf += v1;
                                    remaining_size -= v1;

                                    read_count += v1;
                                    if (read_count >= read_threshold) {
                                        read_count = 0;
                                        read_callback();
                                    }

                                    chunk_size = read_threshold - read_count;
                                }

                                while (remaining_size != 0) {
                                    db_preload_buffer(stream);

                                    v1 = stream->field_1C - (stream->field_20 - 0x4000);
                                    if (v1 > remaining_size) {
                                        v1 = remaining_size;
                                    }

                                    memcpy(buf, stream->field_20, v1);

                                    buf += v1;
                                    remaining_size -= v1;

                                    stream->field_20 += v1;
                                    stream->field_10 -= v1;

                                    read_count += v1;
                                }
                            } else {
                                while (remaining_size != 0) {
                                    db_preload_buffer(stream);

                                    v1 = stream->field_1C - (stream->field_20 - 0x4000);
                                    if (v1 > remaining_size) {
                                        v1 = remaining_size;
                                    }

                                    memcpy(buf, stream->field_20, v1);

                                    buf += v1;
                                    remaining_size -= v1;

                                    stream->field_20 += v1;
                                    stream->field_10 -= v1;
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    return elements_read;
}

// 0x4B02A0
int db_fgetc(DB_FILE* stream)
{
    int ch = -1;
    int next_ch;

    if (stream != NULL) {
        if ((stream->flags & 0x4) != 0) {
            ch = fgetc(stream->uncompressed_file_stream);
        } else {
            switch (stream->flags & 0xF0) {
            case 16:
                if (stream->field_10 != 0) {
                    ch = *stream->field_20;
                    stream->field_20++;
                    stream->field_10--;

                    if (stream->field_10 != 0 && (stream->flags & 0x2) != 0 && ch == '\r') {
                        next_ch = *stream->field_20;
                        if (next_ch == '\n') {
                            stream->field_20++;
                            stream->field_10--;
                            ch = '\n';
                        }
                    }
                }
                break;
            case 32:
                if (stream->field_10 != 0) {
                    if (fseek(stream->database->stream, stream->field_18, SEEK_SET) == 0) {
                        ch = fgetc(stream->database->stream);
                        stream->field_10 -= 1;

                        if (stream->field_10 != 0 && (stream->flags & 0x2) != 0 && ch == '\r') {
                            next_ch = fgetc(stream->database->stream);
                            if (next_ch == '\n') {
                                stream->field_10--;
                                ch = '\n';
                            } else {
                                ungetc(next_ch, stream->database->stream);
                            }
                        }
                        stream->field_18 = ftell(stream->database->stream);
                    }
                }
                break;
            case 64:
                db_preload_buffer(stream);

                if (stream->field_10 != 0) {
                    ch = *stream->field_20;
                    stream->field_20++;
                    stream->field_10--;

                    if (stream->field_10 != 0 && (stream->flags & 0x2) != 0 && ch == '\r') {
                        next_ch = *stream->field_20;
                        if (next_ch == '\n') {
                            stream->field_20++;
                            stream->field_10--;
                            ch = '\n';
                        }
                    }
                }
                break;
            }
        }
    }

    if (read_callback != NULL) {
        read_count++;
        if (read_count >= read_threshold) {
            read_callback();
            read_count = 0;
        }
    }

    return ch;
}

// 0x4B03F0
int db_ungetc(int ch, DB_FILE* stream)
{
    if (stream != NULL) {
        if ((stream->flags & 0x4) != 0) {
            return ungetc(ch, stream->uncompressed_file_stream);
        } else {
            // NOTE: Original implementation looks broken, it does not return
            // `ch` into stream, but steps back in read stream.
            switch (stream->flags & 0xF0) {
            case 16:
                if (stream->field_20 != stream->field_1C) {
                    stream->field_20--;
                    stream->field_10++;
                }
                break;
            case 32:
                if (stream->field_18 != stream->field_14) {
                    if (fseek(stream->database->stream, stream->field_18, SEEK_SET) == 0) {
                        if (fseek(stream->database->stream, -1, SEEK_CUR) == 0) {
                            stream->field_18 = ftell(stream->database->stream);
                            stream->field_10++;
                        }
                    }
                }
                break;
            case 64:
                if (stream->field_20 != stream->field_1C) {
                    stream->field_20--;
                    stream->field_10++;
                }
                break;
            }
        }
    }

    return ch;
}

// 0x4B04A4
char* db_fgets(char* string, size_t size, DB_FILE* stream)
{
    char* res = NULL;
    size_t index;
    int ch;

    if (stream != NULL) {
        if ((stream->flags & 0x4) != 0) {
            res = fgets(string, size, stream->uncompressed_file_stream);
        } else {
            if (string != NULL) {
                for (index = 0; index < size - 1; index++) {
                    ch = db_fgetc(stream);
                    if (ch == -1) {
                        break;
                    }

                    string[index] = ch;

                    if (ch == '\n') {
                        index++;
                        break;
                    }
                }

                string[index] = '\0';

                if (index != 0) {
                    res = string;
                }
            }
        }
    }

    return res;
}

// 0x4B051C
int db_fseek(DB_FILE* stream, long offset, int origin)
{
    int rc = -1;
    long current_offset;
    unsigned char* v1;
    int chunks;

    if (stream != NULL) {
        if ((stream->flags & 0x4) != 0) {
            rc = fseek(stream->uncompressed_file_stream, offset, origin);
        } else {
            current_offset = db_ftell(stream);

            switch (origin) {
            case SEEK_SET:
                break;
            case SEEK_CUR:
                offset += current_offset;
                break;
            case SEEK_END:
                offset += stream->field_C;
                break;
            default:
                offset = -1;
                break;
            }

            if (offset < 0 || offset > stream->field_C) {
                return -1;
            }

            switch (stream->flags & 0xF0) {
            case 16:
                stream->field_20 = stream->field_1C + offset;
                stream->field_10 = stream->field_C - offset;
                rc = 0;
                break;
            case 32:
                if (fseek(stream->database->stream, stream->field_14 + offset, SEEK_SET) == 0) {
                    stream->field_18 = ftell(stream->database->stream);
                    stream->field_10 = stream->field_C - offset;
                    rc = 0;
                }
                break;
            case 64:
                v1 = stream->field_20 + offset - current_offset;
                if (v1 >= stream->field_1C && v1 < stream->field_1C + 0x4000) {
                    stream->field_20 = v1;
                    stream->field_10 = current_offset - offset;
                    rc = 0;
                } else {
                    if (offset < current_offset) {
                        db_rewind(stream);
                        chunks = offset / 0x4000;
                    } else {
                        stream->field_10 -= stream->field_1C - (stream->field_20 - 0x4000);
                        stream->field_20 = stream->field_1C + 0x4000;
                        db_preload_buffer(stream);
                        chunks = (offset - db_ftell(stream)) / 0x4000;
                    }

                    while (chunks > 0) {
                        stream->field_10 -= 0x4000;
                        stream->field_20 = stream->field_1C + 0x4000;
                        db_preload_buffer(stream);
                        chunks--;
                    }

                    if (offset % 0x4000 != 0) {
                        stream->field_10 -= offset % 0x4000;
                        stream->field_20 += offset % 0x4000;
                    }

                    stream->field_10 = stream->field_C - offset;
                }
            }
        }
    }

    return rc;
}

// 0x4B06A8
long db_ftell(DB_FILE* stream)
{
    if (stream != NULL) {
        if ((stream->flags & 0x4) != 0) {
            return ftell(stream->uncompressed_file_stream);
        } else {
            switch (stream->flags & 0xF0) {
            case 16:
                return stream->field_C - stream->field_10;
            case 32:
            case 64:
                return stream->field_C - stream->field_10;
            }
        }
    }

    return -1;
}

// 0x4B06F4
void db_rewind(DB_FILE* stream)
{
    if (stream != NULL) {
        if ((stream->flags & 0x4) != 0) {
            rewind(stream->uncompressed_file_stream);
        } else {
            switch (stream->flags & 0xF0) {
            case 16:
                stream->field_10 = stream->field_C;
                stream->field_20 = stream->field_1C;
                break;
            case 32:
                stream->field_18 = stream->field_14;
                stream->field_10 = stream->field_C;
                break;
            case 64:
                stream->field_10 = stream->field_C;
                stream->field_20 = stream->field_1C + 16384;
                stream->field_18 = stream->field_14;
                db_preload_buffer(stream);
                break;
            }
        }
    }
}

// 0x4B0764
size_t db_fwrite(const void* buf, size_t size, size_t count, DB_FILE* stream)
{
    if (stream != NULL && (stream->flags & 0x4) != 0) {
        return fwrite(buf, size, count, stream->uncompressed_file_stream);
    }

    return count - 1;
}

// 0x4B077C
int db_fputc(int ch, DB_FILE* stream)
{
    if (stream != NULL && (stream->flags & 0x4) != 0) {
        return fputc(ch, stream->uncompressed_file_stream);
    }

    return -1;
}

// 0x4B0794
int db_fputs(const char* string, DB_FILE* stream)
{
    if (stream != NULL && (stream->flags & 0x4) != 0) {
        return fputs(string, stream->uncompressed_file_stream);
    }

    return -1;
}

// 0x4B07AC
int db_freadByte(DB_FILE* stream, unsigned char* c)
{
    int value = db_fgetc(stream);
    if (value == -1) {
        return -1;
    }

    *c = value & 0xFF;

    return 0;
}

// 0x4B07C0
int db_freadShort(DB_FILE* stream, unsigned short* s)
{
    unsigned char high;
    unsigned char low;

    // NOTE: Uninline.
    if (db_freadByte(stream, &high) == -1) {
        return -1;
    }

    // NOTE: Uninline.
    if (db_freadByte(stream, &low) == -1) {
        return -1;
    }

    *s = (high << 8) | low;

    return 0;
}

// 0x4B0820
int db_freadInt(DB_FILE* stream, int* i)
{
    unsigned short high;
    unsigned short low;

    if (db_freadShort(stream, &high) == -1) {
        return -1;
    }

    if (db_freadShort(stream, &low) == -1) {
        return -1;
    }

    *i = (high << 16) | low;

    return 0;
}

// 0x4B0820
int db_freadLong(DB_FILE* stream, unsigned long* l)
{
    int i;

    if (db_freadInt(stream, &i) == -1) {
        return -1;
    }

    *l = (unsigned long)i;

    return 0;
}

// 0x4B0820
int db_freadFloat(DB_FILE* stream, float* q)
{
    unsigned long l;

    if (db_freadLong(stream, &l) == -1) {
        return -1;
    }

    *q = *(float*)&l;

    return 0;
}

// 0x4B0870
int db_fwriteByte(DB_FILE* stream, unsigned char c)
{
    // NOTE: Uninline.
    if (db_fputc(c, stream) == -1) {
        return -1;
    }

    return 0;
};

// 0x4B08A0
int db_fwriteShort(DB_FILE* stream, unsigned short s)
{
    // NOTE: Uninline.
    if (db_fwriteByte(stream, s >> 8) == -1) {
        return -1;
    }

    // NOTE: Uninline.
    if (db_fwriteByte(stream, s & 0xFF) == -1) {
        return -1;
    }

    return 0;
}

// 0x4B08EC
int db_fwriteInt(DB_FILE* stream, int i)
{
    if (db_fwriteShort(stream, i >> 16) == -1) {
        return -1;
    }

    if (db_fwriteShort(stream, i & 0xFFFF) == -1) {
        return -1;
    }

    return 0;
}

// 0x4C6244
int db_fwriteLong(DB_FILE* stream, unsigned long l)
{
    // NOTE: Uninline.
    return db_fwriteInt(stream, l);
}

// 0x4B099C
int db_fwriteFloat(DB_FILE* stream, float q)
{
    // NOTE: Uninline.
    return db_fwriteLong(stream, *(unsigned long*)&q);
}

// 0x4B09D4
int db_freadByteCount(DB_FILE* stream, unsigned char* c, int count)
{
    int index;
    unsigned char value;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_freadByte(stream, &value) == -1) {
            return -1;
        }

        c[index] = value;
    }

    return 0;
}

// 0x4B0A14
int db_freadShortCount(DB_FILE* stream, unsigned short* s, int count)
{
    int index;
    unsigned short value;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_freadShort(stream, &value) == -1) {
            return -1;
        }

        s[index] = value;
    }

    return 0;
}

// 0x4B0AB0
int db_freadIntCount(DB_FILE* stream, int* i, int count)
{
    int index;
    int value;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_freadInt(stream, &value) == -1) {
            return -1;
        }

        i[index] = value;
    }

    return 0;
}

// 0x4B0AB0
int db_freadLongCount(DB_FILE* stream, unsigned long* l, int count)
{
    int index;
    unsigned long value;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_freadLong(stream, &value) == -1) {
            return -1;
        }

        l[index] = value;
    }

    return 0;
}

// 0x4B0AB0
int db_freadFloatCount(DB_FILE* stream, float* q, int count)
{
    int index;
    float value;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_freadFloat(stream, &value) == -1) {
            return -1;
        }

        q[index] = value;
    }

    return 0;
}

// 0x4B0B80
int db_fwriteByteCount(DB_FILE* stream, unsigned char* c, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_fwriteByte(stream, c[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4B0BC8
int db_fwriteShortCount(DB_FILE* stream, unsigned short* s, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_fwriteShort(stream, s[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4B0C3C
int db_fwriteIntCount(DB_FILE* stream, int* i, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_fwriteInt(stream, i[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4B0C9C
int db_fwriteLongCount(DB_FILE* stream, unsigned long* l, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_fwriteLong(stream, l[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4B0D54
int db_fwriteFloatCount(DB_FILE* stream, float* q, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        // NOTE: Uninline.
        if (db_fwriteFloat(stream, q[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4B0D94
static int db_read_long(FILE* stream, int* value_ptr)
{
    int c;
    int value;

    c = fgetc(stream);
    if (c == -1) return -1;
    value = c;

    c = fgetc(stream);
    if (c == -1) return -1;
    value <<= 8;
    value |= c;

    c = fgetc(stream);
    if (c == -1) return -1;
    value <<= 8;
    value |= c;

    c = fgetc(stream);
    if (c == -1) return -1;
    value <<= 8;
    value |= c;

    *value_ptr = value;

    return 0;
}

// 0x4B0DF0
static int db_write_long(FILE* stream, int value)
{
    if (fputc(value >> 24, stream) == -1) return -1;
    if (fputc((value >> 16) & 0xFF, stream) == -1) return -1;
    if (fputc((value >> 8) & 0xFF, stream) == -1) return -1;
    if (fputc(value & 0xFF, stream) == -1) return -1;
    return 0;
}

// 0x4C5ED0
int db_fprintf(DB_FILE* stream, const char* format, ...)
{
    int rc;
    va_list args;

    va_start(args, format);
    if (stream != NULL && (stream->flags & 0x4) != 0) {
        rc = vfprintf(stream->uncompressed_file_stream, format, args);
    } else {
        rc = -1;
    }
    va_end(args);

    return rc;
}

// 0x4B0E98
int db_feof(DB_FILE* stream)
{
    if (stream == NULL) {
        return -1;
    }

    if ((stream->flags & 0x4) != 0) {
        return feof(stream->uncompressed_file_stream);
    } else {
        switch (stream->flags & 0xF0) {
        case 16:
            return stream->field_10 == 0;
        case 32:
        case 64:
            return stream->field_10 == 0;
        }
    }

    return -1;
}

// 0x4B0EF0
int db_get_file_list(const char* filespec, char*** filelist, char*** desclist, int desclen)
{
    bool v1;
    char path[COMPAT_MAX_PATH];
    char* sep;
    char* filename;
    char* temp;
    assoc_array ary;
    int pos;
    int index;
    int count = 0;

    if (current_database == NULL) {
        return 0;
    }

    if (filespec == NULL) {
        return 0;
    }

    if (filelist == NULL) {
        return 0;
    }

    char filespec_copy_buffer[COMPAT_MAX_PATH];
    char* filespec_copy = filespec_copy_buffer;
    strcpy(filespec_copy, filespec);

    temp = NULL;

    v1 = true;
    if (filespec_copy[0] == '@') {
        filespec_copy++;
        v1 = false;
    }

    *filelist = NULL;

    sep = strrchr(filespec_copy, '\\');
    filename = sep != NULL ? sep + 1 : filespec_copy;

    if (strlen(filename) == 5 && filename[0] == '*' && filename[1] == '.') {
        if (desclist != NULL) {
            temp = (char*)internal_malloc(desclen);
            if (temp == NULL) {
                return 0;
            }
        }

        if (assoc_init(&ary, 10, desclen, NULL) == -1) {
            if (temp != NULL) {
                internal_free(temp);
            }
            return 0;
        }

        if (!v1) {
            strcpy(path, filespec_copy);
        }

        if (current_database->datafile != NULL) {
            pos = 0;

            if (v1) {
                snprintf(path, sizeof(path), "%s%s", current_database->datafile_path, filespec_copy);
            }

            compat_strupr(path);

            sep = strrchr(path, '\\');
            if (sep != NULL) {
                char* v3;

                *sep = '\0';

                v3 = path;
                if (path[0] == '.') {
                    v3 = path + 1;
                    if (path[1] == '\\') {
                        v3 = path + 2;
                    }
                }

                if (strlen(v3) != 0) {
                    pos = assoc_search(&(current_database->root), v3);
                } else {
                    pos = 0;
                }

                *sep = '\\';

                filename = sep + 1;
            } else {
                filename = path;
            }

            if (pos != -1) {
                char* name;
                size_t name_len;
                for (index = 0; index < current_database->entries[pos].size; index++) {
                    name = current_database->entries[pos].list[index].name;
                    name_len = strlen(name);
                    if (name_len > 4) {
                        if (name[name_len - 3] == filename[2] && name[name_len - 2] == filename[3] && name[name_len - 1] == filename[4]) {
                            if (temp != NULL) {
                                DB_FILE* stream = db_fopen(name, "rb");
                                if (stream != NULL) {
                                    if (db_fgets(temp, desclen, stream) != NULL) {
                                        temp[strlen(temp) - 1] = '\0';
                                    }
                                    db_fclose(stream);
                                }
                            }
                            assoc_insert(&ary, name, temp);
                        }
                    }
                }
            }
        }

        if (current_database->patches_path != NULL) {
            DB_FIND_DATA find_data;

            if (v1) {
                snprintf(path, sizeof(path), "%s%s", current_database->patches_path, filespec_copy);
            }

            compat_windows_path_to_native(path);

            if (db_findfirst(path, &find_data) == 0) {
                do {
                    if (temp != NULL) {
                        FILE* stream = compat_fopen(fileFindGetName(&find_data), "rb");
                        if (stream != NULL) {
                            if (fgets(temp, desclen, stream) != NULL) {
                                temp[strlen(temp - 1)] = '\0';
                            }
                            fclose(stream);
                        }
                    }
                    assoc_insert(&ary, fileFindGetName(&find_data), temp);
                } while (db_findnext(&find_data) != -1);

                db_findclose(&find_data);
            }
        }

        count = ary.size;
        if (ary.size > 0) {
            // Allocate one continous chunk of memory which is split into two
            // parts. The first part contains pointers (packed end-to-end and
            // thus allows indexed access) to the second part (which are actual
            // storage for strings 13 bytes each).
            //
            // NOTE: The size of storage is 33 bytes in Mac OS binary.
            *filelist = (char**)internal_malloc((sizeof(char*) + 13) * ary.size);
            if (*filelist != NULL) {
                for (index = 0; index < ary.size; index++) {
                    (*filelist)[index] = (char*)*filelist + sizeof(char*) * ary.size + 13 * index;
                    strcpy((*filelist)[index], ary.list[index].name);
                }
            }

            // TODO: Incomplete.
        }

        if (temp != NULL) {
            internal_free(temp);
        }
    }

    return count;
}

// 0x4B1518
void db_free_file_list(char*** file_list, char*** desclist)
{
    if (file_list != NULL) {
        if (*file_list != NULL) {
            internal_free(*file_list);
            *file_list = NULL;
        }
    }

    if (desclist != NULL) {
        if (*desclist != NULL) {
            internal_free(*desclist);
            *desclist = NULL;
        }
    }
}

// NOTE: Originally not static.
//
// 0x4B1554
static int db_assoc_load_dir_entry(FILE* stream, void* buffer, size_t size, int flags)
{
    dir_entry* de;
    if (size != sizeof(*de)) return -1;

    de = (dir_entry*)buffer;
    if (db_read_long(stream, &(de->flags)) != 0) return -1;
    if (db_read_long(stream, &(de->offset)) != 0) return -1;
    if (db_read_long(stream, &(de->length)) != 0) return -1;
    if (db_read_long(stream, &(de->field_C)) != 0) return -1;

    return 0;
}

// NOTE: Originally not static.
//
// 0x4B159C
static int db_assoc_save_dir_entry(FILE* stream, void* buffer, size_t size, int flags)
{
    dir_entry* de;

    if (size != sizeof(*de)) return -1;

    de = (dir_entry*)buffer;
    if (db_write_long(stream, de->flags) != 0) return -1;
    if (db_write_long(stream, de->offset) != 0) return -1;
    if (db_write_long(stream, de->length) != 0) return -1;
    if (db_write_long(stream, de->field_C) != 0) return -1;

    return 0;
}

// NOTE: Originally not static.
//
// 0x4B15E4
static int db_assoc_load_db_dir_entry(DB_FILE* stream, void* buffer, size_t size, int flags)
{
    dir_entry* de;
    if (size != sizeof(*de)) return -1;

    de = (dir_entry*)buffer;
    if (db_freadInt32(stream, &(de->flags)) != 0) return -1;
    if (db_freadInt32(stream, &(de->offset)) != 0) return -1;
    if (db_freadInt32(stream, &(de->length)) != 0) return -1;
    if (db_freadInt32(stream, &(de->field_C)) != 0) return -1;

    return 0;
}

// NOTE: Originally not static.
//
// 0x4B162C
static int db_assoc_save_db_dir_entry(DB_FILE* stream, void* buffer, size_t size, int flags)
{
    dir_entry* de;

    if (size != sizeof(*de)) return -1;

    de = (dir_entry*)buffer;
    if (db_fwriteInt32(stream, de->flags) != 0) return -1;
    if (db_fwriteInt32(stream, de->offset) != 0) return -1;
    if (db_fwriteInt32(stream, de->length) != 0) return -1;
    if (db_fwriteInt32(stream, de->field_C) != 0) return -1;

    return 0;
}

// 0x4B1A98
long db_filelength(DB_FILE* stream)
{
    if (stream == NULL) {
        return -1;
    }

    if ((stream->flags & 0x4) != 0) {
        return getFileSize(stream->uncompressed_file_stream);
    } else {
        return stream->field_C;
    }
}

// 0x4B1AC0
void db_register_mem(db_malloc_func* malloc_func, db_strdup_func* strdup_func, db_free_func* free_func)
{
    if (!db_used_malloc) {
        if (malloc_func != NULL && strdup_func != NULL && free_func != NULL) {
            db_malloc = malloc_func;
            db_strdup = strdup_func;
            db_free = free_func;
        } else {
            db_malloc = db_default_malloc;
            db_strdup = db_default_strdup;
            db_free = db_default_free;
        }
    }
}

// 0x4B1B14
void db_register_callback(db_read_callback* callback, size_t threshold)
{
    if (callback != NULL && threshold != 0) {
        read_callback = callback;
        read_threshold = threshold;
    } else {
        read_callback = NULL;
        read_threshold = 0;
    }
}

// 0x4B1B2C
static int db_create_database(DB_DATABASE** database_ptr)
{
    int index;

    for (index = 0; index < DB_DATABASE_LIST_CAPACITY; index++) {
        if (database_list[index] == NULL) {
            database_list[index] = (DB_DATABASE*)internal_malloc(sizeof(DB_DATABASE));
            if (database_list[index] == NULL) {
                return -1;
            }

            memset(database_list[index], 0, sizeof(DB_DATABASE));
            *database_ptr = database_list[index];

            return 0;
        }
    }

    return -1;
}

// 0x4B1B98
static int db_destroy_database(DB_DATABASE** database_ptr)
{
    if (database_ptr == NULL) {
        return -1;
    }

    if (*database_ptr == NULL) {
        return -1;
    }

    db_free(*database_ptr);
    *database_ptr = NULL;

    return 0;
}

// 0x4B1BC4
static int db_init_database(DB_DATABASE* database, const char* datafile, const char* datafile_path)
{
    assoc_func_list funcs;
    int index;
    const char* v1;
    size_t v2;

    if (database == NULL) {
        return -1;
    }

    if (datafile == NULL) {
        return 0;
    }

    database->datafile = internal_strdup(datafile);
    if (database->datafile == NULL) {
        return -1;
    }

    database->stream = compat_fopen(database->datafile, "rb");
    if (database->stream == NULL) {
        internal_free(database->datafile);
        database->datafile = NULL;
        return -1;
    }

    if (assoc_init(&(database->root), 0, sizeof(*database->entries), NULL) != 0) {
        fclose(database->stream);
        internal_free(database->datafile);
        database->datafile = NULL;
        return -1;
    }

    if (assoc_load(database->stream, &(database->root), 0) != 0) {
        fclose(database->stream);
        internal_free(database->datafile);
        database->datafile = NULL;
        return -1;
    }

    database->entries = (assoc_array*)internal_malloc(sizeof(*database->entries) * database->root.size);
    if (database->entries == NULL) {
        assoc_free(&(database->root));
        fclose(database->stream);
        internal_free(database->datafile);
        database->datafile = NULL;
        return -1;
    }

    funcs.loadFunc = db_assoc_load_dir_entry;
    funcs.saveFunc = db_assoc_save_dir_entry;
    funcs.loadFuncDB = NULL;
    funcs.saveFuncDB = NULL;

    for (index = 0; index < database->root.size; index++) {
        if (assoc_init(&(database->entries[index]), 0, sizeof(dir_entry), &funcs) != 0) {
            break;
        }

        if (assoc_load(database->stream, &(database->entries[index]), 0) != 0) {
            break;
        }
    }

    if (index < database->root.size) {
        while (--index >= 0) {
            assoc_free(&(database->entries[index]));
        }

        internal_free(database->entries);
        assoc_free(&(database->root));
        fclose(database->stream);
        internal_free(database->datafile);
        database->datafile = NULL;
        return -1;
    }

    if (datafile_path != NULL && strlen(datafile_path) != 0) {
        v1 = datafile_path;
        if (datafile_path[0] == PATH_SEP) {
            v1 = datafile_path + 1;
        }
    } else {
        v1 = ".\\";
    }

    v2 = strlen(v1);
    database->datafile_path = (char*)internal_malloc(v2 + 2);
    if (database->datafile_path == NULL) {
        internal_free(database->entries);
        assoc_free(&(database->root));
        fclose(database->stream);
        internal_free(database->datafile);
        database->datafile = NULL;
        return -1;
    }

    strcpy(database->datafile_path, v1);

    if (database->datafile_path[v2 - 1] != '\\') {
        database->datafile_path[v2] = '\\';
        database->datafile_path[v2 + 1] = '\0';
    }

    return 0;
}

// 0x4B1DE0
static void db_exit_database(DB_DATABASE* database)
{
    int index;

    if (database == NULL) {
        return;
    }

    if (database->stream != NULL) {
        fclose(database->stream);
        database->stream = NULL;
    }

    if (database->datafile != NULL) {
        internal_free(database->datafile);
        database->datafile = NULL;
    }

    if (database->entries != NULL) {
        for (index = 0; index < database->root.size; index++) {
            assoc_free(&(database->entries[index]));
        }
        internal_free(database->entries);
        database->entries = NULL;
    }

    assoc_free(&(database->root));

    if (database->datafile_path != NULL) {
        internal_free(database->datafile_path);
        database->datafile_path = NULL;
    }
}

// 0x4B1E70
static int db_init_patches(DB_DATABASE* database, const char* path)
{
    size_t path_len;

    if (database == NULL) {
        return -1;
    }

    if (path == NULL) {
        database->patches_path = NULL;
        return 0;
    }

    path_len = strlen(path);
    if (path_len == 0) {
        database->patches_path = empty_patches_path;
        return 0;
    }

    database->patches_path = (char*)internal_malloc(path_len + 2);
    if (database->patches_path == NULL) {
        return -1;
    }

    database->should_free_patches_path = true;
    strcpy(database->patches_path, path);

    if (database->patches_path[path_len - 1] != '\\') {
        database->patches_path[path_len] = PATH_SEP;
        database->patches_path[path_len + 1] = '\0';
    }

    return 0;
}

// 0x4B1F10
static void db_exit_patches(DB_DATABASE* database)
{
    if (database == NULL) {
        return;
    }

    if (database->patches_path != NULL) {
        if (database->should_free_patches_path == true) {
            internal_free(database->patches_path);
        }
    }

    database->patches_path = empty_patches_path;
    database->should_free_patches_path = false;
}

// 0x4B1F3C
static int db_init_hash_table(DB_DATABASE* database)
{
    if (!hash_is_on) {
        return -1;
    }

    if (database == NULL) {
        return -1;
    }

    database->hash_table = (unsigned char*)internal_malloc(DB_HASH_TABLE_SIZE);
    if (database->hash_table == NULL) {
        return -1;
    }

    return db_reset_hash_table(database);
}

// 0x4B1F90
void db_enable_hash_table()
{
    hash_is_on = true;
}

// 0x4B1F9C
static int db_reset_hash_table(DB_DATABASE* database)
{
    if (!hash_is_on) {
        return -1;
    }

    if (database == NULL) {
        return -1;
    }

    if (database->patches_path == NULL) {
        return -1;
    }

    if (database->hash_table == NULL) {
        database->hash_table = (unsigned char*)internal_malloc(DB_HASH_TABLE_SIZE);
        if (database->hash_table == NULL) {
            return -1;
        }
    }

    memset(database->hash_table, 0, DB_HASH_TABLE_SIZE);

    return db_fill_hash_table(database, database->patches_path);
}

// NOTE: Originally not static, but that would require exposing `DB_DATABASE`
// which is most likely considered implementation detail.
//
// 0x4B2028
static int db_fill_hash_table(DB_DATABASE* database, const char* path)
{
    char pattern[COMPAT_MAX_PATH];
    DB_FIND_DATA find_data;
    bool is_directory;
    char* filename;

    if (!hash_is_on) {
        return -1;
    }

    if (database == NULL) {
        return -1;
    }

    if (database->hash_table == NULL) {
        return -1;
    }

#if defined(_WIN32)
    snprintf(pattern, sizeof(pattern), "%s%s", path, "*.*");
#else
    snprintf(pattern, sizeof(pattern), "%s%s", path, "*");
#endif
    compat_windows_path_to_native(pattern);

    if (db_findfirst(pattern, &find_data) != -1) {
        do {
            is_directory = fileFindIsDirectory(&find_data);
            filename = fileFindGetName(&find_data);

            if (is_directory) {
                if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
                    snprintf(pattern, sizeof(pattern), "%s%s%c", path, filename, PATH_SEP);
                    db_fill_hash_table(database, pattern);
                }
            } else {
                db_add_hash_entry_to_database(database, filename, PATH_SEP);
            }
        } while (db_findnext(&find_data) != -1);

        db_findclose(&find_data);
    }

    return 0;
}

// 0x4B2154
int db_reset_hash_tables()
{
    int index;

    if (!hash_is_on) {
        return -1;
    }

    for (index = 0; index < DB_DATABASE_LIST_CAPACITY; index++) {
        if (database_list[index] != NULL) {
            db_reset_hash_table(database_list[index]);
        }
    }

    return 0;
}

// 0x4B218C
int db_add_hash_entry(const char* path, int sep)
{
    if (!hash_is_on) {
        return -1;
    }

    if (current_database == NULL) {
        return -1;
    }

    if (current_database->hash_table == NULL) {
        return -1;
    }

    if (path == NULL) {
        return -1;
    }

    return db_add_hash_entry_to_database(current_database, path, sep);
}

// 0x4B21E0
static int db_add_hash_entry_to_database(DB_DATABASE* database, const char* path, int sep)
{
    unsigned int key;

    if (!hash_is_on) {
        return -1;
    }

    if (database == NULL) {
        return -1;
    }

    if (database->hash_table == NULL) {
        return -1;
    }

    if (db_hash_string_to_key(path, sep, &key) != 0) {
        return -1;
    }

    return db_set_hash_value(database, key, 1);
}

// 0x4B2258
static int db_set_hash_value(DB_DATABASE* database, unsigned int key, unsigned char enabled)
{
    if (!hash_is_on) {
        return -1;
    }

    if (database->hash_table == NULL) {
        return -1;
    }

    if (key >= 0x7FFF) {
        return -1;
    }

    if (enabled == true) {
        database->hash_table[key / 8] |= 1 << (key % 8);
    } else {
        database->hash_table[key / 8] = 0;
    }

    return 0;
}

// 0x4B2304
static int db_get_hash_value(DB_DATABASE* database, const char* path, int sep, int* value_ptr)
{
    unsigned int key;

    if (!hash_is_on) {
        return -1;
    }

    if (database->hash_table == NULL) {
        return -1;
    }

    if (path == NULL) {
        return -1;
    }

    if (db_hash_string_to_key(path, sep, &key) != 0) {
        return -1;
    }

    if (key >= 0x7FFF) {
        return -1;
    }

    *value_ptr = (database->hash_table[key / 8] >> (key % 8)) & 1;

    return 0;
}

// 0x4B2394
static int db_hash_string_to_key(const char* path, int sep, unsigned int* key_ptr)
{
    char* copy;
    char* pch;
    char* filename;
    size_t index;
    unsigned int key;

    key = 1;

    if (path == NULL) {
        return -1;
    }

    copy = (char*)internal_strdup(path);
    compat_strupr(copy);

    pch = strrchr(copy, sep);
    if (pch != NULL) {
        filename = pch + 1;
    } else {
        filename = copy;
    }

    for (index = 0; index < strlen(filename); index++) {
        key *= *filename++;
        key &= 0x7FFFFFFF;
    }

    *key_ptr = key & 0x7FFF;

    internal_free(copy);

    return 0;
}

// 0x4B2420
static void db_exit_hash_table(DB_DATABASE* database)
{
    if (database->hash_table != NULL) {
        internal_free(database->hash_table);
    }
    database->hash_table = NULL;
}

// 0x4B2444
static DB_FILE* db_add_fp_rec(FILE* stream, unsigned char* a2, int a3, int flags)
{
    DB_FILE* ptr;
    int pos;

    ptr = NULL;
    if (current_database->files_length < DB_DATABASE_FILE_LIST_CAPACITY) {
        if (db_find_empty_position(&pos) == 0) {
            memset(&(current_database->files[pos]), 0, sizeof(*current_database->files));
            current_database->files[pos].database = current_database;

            if ((flags & 0x4) != 0) {
                current_database->files[pos].uncompressed_file_stream = stream;
                ptr = &(current_database->files[pos]);
            } else {
                current_database->files[pos].field_C = a3;
                current_database->files[pos].field_10 = a3;

                switch (flags & 0xF0) {
                case 16:
                    current_database->files[pos].field_1C = a2;
                    current_database->files[pos].field_20 = a2;
                    ptr = &(current_database->files[pos]);
                    break;
                case 32:
                    current_database->files[pos].field_14 = ftell(stream);
                    current_database->files[pos].field_18 = ftell(stream);
                    ptr = &(current_database->files[pos]);
                    break;
                case 64:
                    current_database->files[pos].field_14 = ftell(stream);
                    current_database->files[pos].field_18 = ftell(stream);
                    current_database->files[pos].field_1C = a2;
                    current_database->files[pos].field_20 = a2 + 0x4000;
                    ptr = &(current_database->files[pos]);
                    break;
                }
            }
        }
    }

    if (ptr != NULL) {
        current_database->files[pos].flags = flags;
        current_database->files[pos].field_8 = 1;
        current_database->files_length++;
    }

    return ptr;
}

// 0x4B2664
static int db_delete_fp_rec(DB_FILE* stream)
{
    if (stream == NULL) {
        return -1;
    }

    if ((stream->flags & 0x4) != 0) {
        fclose(stream->uncompressed_file_stream);
    } else {
        switch (stream->flags & 0xF0) {
        case 16:
            if (stream->field_1C != NULL) {
                internal_free(stream->field_1C);
            }
            break;
        case 32:
            break;
        case 64:
            if (stream->field_1C != NULL) {
                internal_free(stream->field_1C);
            }
            break;
        }
    }

    stream->database->files_length -= 1;
    memset(stream, 0, sizeof(*stream));

    return 0;
}

// 0x4B26D0
static int db_find_empty_position(int* position_ptr)
{
    int index;

    if (position_ptr == NULL) {
        return -1;
    }

    if (current_database->files_length >= DB_DATABASE_FILE_LIST_CAPACITY) {
        return -1;
    }

    for (index = 0; index < DB_DATABASE_FILE_LIST_CAPACITY; index++) {
        if (current_database->files[index].field_8 == 0) {
            *position_ptr = index;
            return 0;
        }
    }

    return -1;
}

// 0x4B2714
static int db_find_dir_entry(char* path, dir_entry* de)
{
    char* normalized_path;
    int pos;
    int dir_index;
    int entry_index;

    normalized_path = path;

    if (current_database->datafile == NULL) {
        return -1;
    }

    if (path == NULL) {
        return -1;
    }

    if (de == NULL) {
        return -1;
    }

    if (path[0] == '.') {
        normalized_path = path + 1;
        if (path[1] == '\\') {
            normalized_path = path + 2;
        }
    }

    pos = strlen(normalized_path) - 1;
    while (pos >= 0) {
        if (normalized_path[pos] == '\\') {
            break;
        }
        pos--;
    }

    if (pos >= 0) {
        normalized_path[pos] = '\0';
        dir_index = assoc_search(&(current_database->root), normalized_path);
    } else {
        dir_index = 0;
    }

    if (dir_index == -1) {
        if (pos >= 0) {
            normalized_path[pos] = '\\';
        }
        return -1;
    }

    entry_index = assoc_search(&(current_database->entries[dir_index]), normalized_path + pos + 1);
    if (entry_index == -1) {
        if (pos >= 0) {
            normalized_path[pos] = '\\';
        }
        return -1;
    }

    if (pos >= 0) {
        normalized_path[pos] = '\\';
    }

    *de = *((dir_entry*)current_database->entries[dir_index].list[entry_index].data);

    return 0;
}

// 0x4B2810
static int db_findfirst(const char* path, DB_FIND_DATA* findData)
{
#if defined(_WIN32)
    findData->hFind = FindFirstFileA(path, &(findData->ffd));
    if (findData->hFind == INVALID_HANDLE_VALUE) {
        return -1;
    }
#else
    strcpy(findData->path, path);

    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(path, drive, dir, NULL, NULL);

    char basePath[COMPAT_MAX_PATH];
    compat_makepath(basePath, drive, dir, NULL, NULL);

    findData->dir = opendir(basePath);
    if (findData->dir == NULL) {
        return -1;
    }

    findData->entry = readdir(findData->dir);
    while (findData->entry != NULL) {
        char entryPath[COMPAT_MAX_PATH];
        compat_makepath(entryPath, drive, dir, fileFindGetName(findData), NULL);
        if (fpattern_match(findData->path, entryPath)) {
            break;
        }
        findData->entry = readdir(findData->dir);
    }

    if (findData->entry == NULL) {
        closedir(findData->dir);
        findData->dir = NULL;
        return -1;
    }
#endif

    return 0;
}

// 0x4B2838
static int db_findnext(DB_FIND_DATA* findData)
{
#if defined(_WIN32)
    if (!FindNextFileA(findData->hFind, &(findData->ffd))) {
        return -1;
    }
#else
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(findData->path, drive, dir, NULL, NULL);

    findData->entry = readdir(findData->dir);
    while (findData->entry != NULL) {
        char entryPath[COMPAT_MAX_PATH];
        compat_makepath(entryPath, drive, dir, fileFindGetName(findData), NULL);
        if (fpattern_match(findData->path, entryPath)) {
            break;
        }
        findData->entry = readdir(findData->dir);
    }

    if (findData->entry == NULL) {
        closedir(findData->dir);
        findData->dir = NULL;
        return -1;
    }
#endif

    return 0;
}

// 0x4B2854
static int db_findclose(DB_FIND_DATA* findData)
{
#if defined(_WIN32)
    if (!FindClose(findData->hFind)) {
        return -1;
    }
#else
    if (findData->dir != NULL) {
        if (closedir(findData->dir) != 0) {
            return -1;
        }
    }
#endif

    return 0;
}

// 0x4B2868
static void* internal_malloc(size_t size)
{
    db_used_malloc = true;
    return db_malloc(size);
}

// 0x4B287C
static char* internal_strdup(const char* string)
{
    db_used_malloc = true;
    return db_strdup(string);
}

// 0x4B2890
static void internal_free(void* ptr)
{
    db_free(ptr);
}

// 0x4B2898
static void* db_default_malloc(size_t size)
{
    return malloc(size);
}

// 0x4B28A0
static char* db_default_strdup(const char* string)
{
    return compat_strdup(string);
}

// 0x4B28A8
static void db_default_free(void* ptr)
{
    free(ptr);
}

// 0x4B28B0
static void db_preload_buffer(DB_FILE* stream)
{
    unsigned short v1;

    if ((stream->flags & 0x8) != 0 && (stream->flags & 0xF0) == 64) {
        if (stream->field_10 != 0) {
            if (stream->field_20 >= stream->field_1C + 0x4000) {
                if (fseek(stream->database->stream, stream->field_18, SEEK_SET) == 0) {
                    if (fread_short(stream->database->stream, &v1) == 0) {
                        if ((v1 & 0x8000) != 0) {
                            v1 &= ~0x8000;
                            fread(stream->field_1C, 1, v1, stream->database->stream);
                        } else {
                            lzss_decode_to_buf(stream->database->stream, stream->field_1C, v1);
                        }

                        stream->field_20 = stream->field_1C;
                        stream->field_18 = ftell(stream->database->stream);
                    }
                }
            }
        }
    }
}

// 0x4B2970
static int fread_short(FILE* stream, unsigned short* s)
{
    int high;
    int low;

    high = fgetc(stream);
    if (high == -1) {
        return -1;
    }

    low = fgetc(stream);
    if (low == -1) {
        return -1;
    }

    *s = (low & 0xFF) | (high << 8);

    return 0;
}

static inline bool fileFindIsDirectory(DB_FIND_DATA* findData)
{
#if defined(_WIN32)
    return (findData->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif defined(__WATCOMC__)
    return (findData->entry->d_attr & _A_SUBDIR) != 0;
#else
    return findData->entry->d_type == DT_DIR;
#endif
}

static inline char* fileFindGetName(DB_FIND_DATA* findData)
{
#if defined(_WIN32)
    return findData->ffd.cFileName;
#else
    return findData->entry->d_name;
#endif
}

int db_freadUInt8(DB_FILE* stream, unsigned char* valuePtr)
{
    int value = db_fgetc(stream);
    if (value == -1) {
        return -1;
    }

    *valuePtr = static_cast<unsigned char>(value);

    return 0;
}

int db_freadInt8(DB_FILE* stream, char* valuePtr)
{
    unsigned char value;
    if (db_freadUInt8(stream, &value) == -1) {
        return -1;
    }

    *valuePtr = static_cast<char>(value);

    return 0;
}

int db_freadUInt16(DB_FILE* stream, unsigned short* valuePtr)
{
    return db_freadShort(stream, valuePtr);
}

int db_freadInt16(DB_FILE* stream, short* valuePtr)
{
    unsigned short value;
    if (db_freadUInt16(stream, &value) == -1) {
        return -1;
    }

    *valuePtr = static_cast<short>(value);

    return 0;
}

int db_freadUInt32(DB_FILE* stream, unsigned int* valuePtr)
{
    int value;
    if (db_freadInt(stream, &value) == -1) {
        return -1;
    }

    *valuePtr = static_cast<unsigned int>(value);

    return 0;
}

int db_freadInt32(DB_FILE* stream, int* valuePtr)
{
    unsigned int value;
    if (db_freadUInt32(stream, &value) == -1) {
        return -1;
    }

    *valuePtr = static_cast<int>(value);

    return 0;
}

int db_freadUInt8List(DB_FILE* stream, unsigned char* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_freadUInt8(stream, &(arr[index])) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_freadInt8List(DB_FILE* stream, char* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_freadInt8(stream, &(arr[index])) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_freadInt16List(DB_FILE* stream, short* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_freadInt16(stream, &(arr[index])) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_freadInt32List(DB_FILE* stream, int* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_freadInt32(stream, &(arr[index])) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_freadBool(DB_FILE* stream, bool* valuePtr)
{
    int value;
    if (db_freadInt32(stream, &value) == -1) {
        return -1;
    }

    *valuePtr = (value != 0);

    return 0;
}

int db_fwriteUInt8(DB_FILE* stream, unsigned char value)
{
    return db_fputc(static_cast<int>(value), stream);
}

int db_fwriteInt8(DB_FILE* stream, char value)
{
    return db_fwriteUInt8(stream, static_cast<unsigned char>(value));
}

int db_fwriteUInt16(DB_FILE* stream, unsigned short value)
{
    return db_fwriteShort(stream, value);
}

int db_fwriteInt16(DB_FILE* stream, short value)
{
    return db_fwriteUInt16(stream, static_cast<unsigned short>(value));
}

int db_fwriteUInt32(DB_FILE* stream, unsigned int value)
{
    return db_fwriteInt(stream, static_cast<int>(value));
}

int db_fwriteInt32(DB_FILE* stream, int value)
{
    return db_fwriteUInt32(stream, static_cast<unsigned int>(value));
}

int db_fwriteUInt8List(DB_FILE* stream, unsigned char* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_fwriteUInt8(stream, arr[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_fwriteInt8List(DB_FILE* stream, char* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_fwriteInt8(stream, arr[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_fwriteInt16List(DB_FILE* stream, short* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_fwriteInt16(stream, arr[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_fwriteInt32List(DB_FILE* stream, int* arr, int count)
{
    for (int index = 0; index < count; index++) {
        if (db_fwriteInt32(stream, arr[index]) == -1) {
            return -1;
        }
    }

    return 0;
}

int db_fwriteBool(DB_FILE* stream, bool value)
{
    return db_fwriteInt32(stream, value ? 1 : 0);
}

} // namespace fallout
