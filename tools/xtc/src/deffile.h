#ifndef XTC_DEFFILE_H
#define XTC_DEFFILE_H

#include <stddef.h>

typedef enum DefType
{
    DT_CHAR,
    DT_CHAR32
} DefType;

typedef struct DefEntry DefEntry;
typedef struct DefFile DefFile;

DefFile *DefFile_create(const char *filename);
size_t DefFile_len(const DefFile *self);
const DefEntry *DefFile_byId(const DefFile *self, unsigned id);
const DefEntry *DefFile_byKey(const DefFile *self, const char *key);
const char *DefEntry_from(const DefEntry *self);
const char *DefEntry_to(const DefEntry *self);
DefType DefEntry_type(const DefEntry *self);
unsigned DefEntry_id(const DefEntry *self);
void DefFile_destroy(DefFile *self);

#endif
