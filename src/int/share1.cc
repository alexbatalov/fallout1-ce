#include "int/share1.h"

#include <stdlib.h>
#include <string.h>

#include "plib/db/db.h"

namespace fallout {

static int compare(const void* a1, const void* a2);

// 0x4980C0
static int compare(const void* a1, const void* a2)
{
    const char* v1 = *(const char**)a1;
    const char* v2 = *(const char**)a2;
    return strcmp(v1, v2);
}

// 0x498114
char** getFileList(const char* pattern, int* fileNameListLengthPtr)
{
    char** fileNameList;
    int fileNameListLength = db_get_file_list(pattern, &fileNameList, NULL, 0);
    *fileNameListLengthPtr = fileNameListLength;
    if (fileNameListLength == 0) {
        return NULL;
    }

    qsort(fileNameList, fileNameListLength, sizeof(*fileNameList), compare);

    return fileNameList;
}

// 0x49814C
void freeFileList(char** fileList)
{
    db_free_file_list(&fileList, NULL);
}

} // namespace fallout
