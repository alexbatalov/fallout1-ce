#include "platform_compat.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <timeapi.h>
#else
#include <chrono>
#endif

#include <SDL.h>

namespace fallout {

int compat_stricmp(const char* string1, const char* string2)
{
    return SDL_strcasecmp(string1, string2);
}

int compat_strnicmp(const char* string1, const char* string2, size_t size)
{
    return SDL_strncasecmp(string1, string2, size);
}

char* compat_strupr(char* string)
{
    return SDL_strupr(string);
}

char* compat_strlwr(char* string)
{
    return SDL_strlwr(string);
}

char* compat_itoa(int value, char* buffer, int radix)
{
    return SDL_itoa(value, buffer, radix);
}

void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
#ifdef _WIN32
    _splitpath(path, drive, dir, fname, ext);
#else
    const char* driveStart = path;
    if (path[0] == '/' && path[1] == '/') {
        path += 2;
        while (*path != '\0' && *path != '/' && *path != '.') {
            path++;
        }
    }

    if (drive != NULL) {
        size_t driveSize = path - driveStart;
        if (driveSize > COMPAT_MAX_DRIVE - 1) {
            driveSize = COMPAT_MAX_DRIVE - 1;
        }
        strncpy(drive, path, driveSize);
        drive[driveSize] = '\0';
    }

    const char* dirStart = path;
    const char* fnameStart = path;
    const char* extStart = NULL;

    const char* end = path;
    while (*end != '\0') {
        if (*end == '/') {
            fnameStart = end + 1;
        } else if (*end == '.') {
            extStart = end;
        }
        end++;
    }

    if (extStart == NULL) {
        extStart = end;
    }

    if (dir != NULL) {
        size_t dirSize = fnameStart - dirStart;
        if (dirSize > COMPAT_MAX_DIR - 1) {
            dirSize = COMPAT_MAX_DIR - 1;
        }
        strncpy(dir, path, dirSize);
        dir[dirSize] = '\0';
    }

    if (fname != NULL) {
        size_t fileNameSize = extStart - fnameStart;
        if (fileNameSize > COMPAT_MAX_FNAME - 1) {
            fileNameSize = COMPAT_MAX_FNAME - 1;
        }
        strncpy(fname, fnameStart, fileNameSize);
        fname[fileNameSize] = '\0';
    }

    if (ext != NULL) {
        size_t extSize = end - extStart;
        if (extSize > COMPAT_MAX_EXT - 1) {
            extSize = COMPAT_MAX_EXT - 1;
        }
        strncpy(ext, extStart, extSize);
        ext[extSize] = '\0';
    }
#endif
}

void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
#ifdef _WIN32
    _makepath(path, drive, dir, fname, ext);
#else
    path[0] = '\0';

    if (drive != NULL) {
        if (*drive != '\0') {
            strcpy(path, drive);
            path = strchr(path, '\0');

            if (path[-1] == '/') {
                path--;
            } else {
                *path = '/';
            }
        }
    }

    if (dir != NULL) {
        if (*dir != '\0') {
            if (*dir != '/' && *path == '/') {
                path++;
            }

            strcpy(path, dir);
            path = strchr(path, '\0');

            if (path[-1] == '/') {
                path--;
            } else {
                *path = '/';
            }
        }
    }

    if (fname != NULL && *fname != '\0') {
        if (*fname != '/' && *path == '/') {
            path++;
        }

        strcpy(path, fname);
        path = strchr(path, '\0');
    } else {
        if (*path == '/') {
            path++;
        }
    }

    if (ext != NULL) {
        if (*ext != '\0') {
            if (*ext != '.') {
                *path++ = '.';
            }

            strcpy(path, ext);
            path = strchr(path, '\0');
        }
    }

    *path = '\0';
#endif
}

int compat_read(int fileHandle, void* buf, unsigned int size)
{
    return read(fileHandle, buf, size);
}

int compat_write(int fileHandle, const void* buf, unsigned int size)
{
    return write(fileHandle, buf, size);
}

long compat_lseek(int fileHandle, long offset, int origin)
{
    return lseek(fileHandle, offset, origin);
}

long compat_tell(int fd)
{
    return lseek(fd, 0, SEEK_CUR);
}

long compat_filelength(int fd)
{
    long originalOffset = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);
    long filesize = lseek(fd, 0, SEEK_END);
    lseek(fd, originalOffset, SEEK_SET);
    return filesize;
}

int compat_mkdir(const char* path)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);

#ifdef _WIN32
    return mkdir(nativePath);
#else
    return mkdir(nativePath, 0755);
#endif
}

unsigned int compat_timeGetTime()
{
#ifdef _WIN32
    return timeGetTime();
#else
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
#endif
}

FILE* compat_fopen(const char* path, const char* mode)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    return fopen(nativePath, mode);
}

int compat_remove(const char* path)
{
    char nativePath[COMPAT_MAX_PATH];
    strcpy(nativePath, path);
    compat_windows_path_to_native(nativePath);
    compat_resolve_path(nativePath);
    return remove(nativePath);
}

int compat_rename(const char* oldFileName, const char* newFileName)
{
    char nativeOldFileName[COMPAT_MAX_PATH];
    strcpy(nativeOldFileName, oldFileName);
    compat_windows_path_to_native(nativeOldFileName);
    compat_resolve_path(nativeOldFileName);

    char nativeNewFileName[COMPAT_MAX_PATH];
    strcpy(nativeNewFileName, newFileName);
    compat_windows_path_to_native(nativeNewFileName);
    compat_resolve_path(nativeNewFileName);

    return rename(nativeOldFileName, nativeNewFileName);
}

void compat_windows_path_to_native(char* path)
{
#ifndef _WIN32
    char* pch = path;
    while (*pch != '\0') {
        if (*pch == '\\') {
            *pch = '/';
        }
        pch++;
    }
#endif
}

void compat_resolve_path(char* path)
{
#ifndef _WIN32
    char* pch = path;

    DIR* dir;
    if (pch[0] == '/') {
        dir = opendir("/");
        pch++;
    } else {
        dir = opendir(".");
    }

    while (dir != NULL) {
        char* sep = strchr(pch, '/');
        size_t length;
        if (sep != NULL) {
            length = sep - pch;
        } else {
            length = strlen(pch);
        }

        bool found = false;

        struct dirent* entry = readdir(dir);
        while (entry != NULL) {
            if (strlen(entry->d_name) == length && compat_strnicmp(pch, entry->d_name, length) == 0) {
                strncpy(pch, entry->d_name, length);
                found = true;
                break;
            }
            entry = readdir(dir);
        }

        closedir(dir);
        dir = NULL;

        if (!found) {
            break;
        }

        if (sep == NULL) {
            break;
        }

        *sep = '\0';
        dir = opendir(path);
        *sep = '/';

        pch = sep + 1;
    }
#endif
}

char* compat_strdup(const char* string)
{
    return SDL_strdup(string);
}

// It's a replacement for compat_filelength(fileno(stream)) on platforms without
// fileno defined.
long getFileSize(FILE* stream)
{
    long originalOffset = ftell(stream);
    fseek(stream, 0, SEEK_END);
    long filesize = ftell(stream);
    fseek(stream, originalOffset, SEEK_SET);
    return filesize;
}

} // namespace fallout
