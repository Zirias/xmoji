#include "xrdb.h"

#include <poser/core.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define XRDB_INVALID ((unsigned)-1)
#define XRDB_WILDCARD ((unsigned)-2)
#define XRDB_FLEX ((unsigned)-3)

typedef struct XRdbEntry {
    char *value;
    unsigned keylen;
    unsigned key[];
} XRdbEntry;

typedef struct XRdbQualifiedEntry {
    XRdbEntry *entry;
    int quality;
} XRdbQualifiedEntry;

struct XRdb {
    PSC_HashTable *ids;
    PSC_HashTable *instances;
    PSC_List *keys;
    PSC_List *entries;
    char *root;
};

static unsigned XRdb_id(XRdb *self, const char *str)
{
    uintptr_t pid = (uintptr_t)PSC_HashTable_get(self->ids, str);
    if (pid) return pid - 1;
    pid = PSC_List_size(self->keys);
    PSC_List_append(self->keys, PSC_copystr(str), free);
    PSC_HashTable_set(self->ids, str, (void *)(pid + 1), 0);
    return pid;
}

static void skipws(const char *str, size_t slen, size_t *pos)
{
    while (*pos < slen && str[*pos] &&
	    (str[*pos] == ' ' || str[*pos] == '\t')) ++(*pos);
}

static size_t complen(const char *str, size_t slen, size_t pos)
{
    size_t len = 0;
    if (pos == slen || !str[pos]) return 0;
    if (str[pos] == '.')
    {
	++len;
	if (pos+len == slen || !str[pos+len]) return 0;
    }
    if (str[pos+len] == '*') { ++len; goto done; }
    if (str[pos+len] == '?'
	    && pos+len < slen && str[pos+len+1] == '.') { ++len; goto done; }
    while (pos+len < slen && str[pos+len] && str[pos+len] != '*'
	    && str[pos+len] != '.' && str[pos+len] != ':') ++len;
done:
    if (pos+len == slen || !str[pos+len]) return 0;
    return len;
}

static void destroyentry(void *obj)
{
    if (!obj) return;
    XRdbEntry *entry = obj;
    free(entry->value);
    free(entry);
}

static size_t XRdb_parseEntry(XRdb *self, const char *str, size_t slen)
{
    size_t pos = 0;
    while (pos < slen && str[pos])
    {
	skipws(str, slen, &pos);
	if (pos == slen || !str[pos]) return slen;
	if (str[pos] == '\n')
	{
	    ++pos;
	    continue;
	}
	if (str[pos] == '!')
	{
	    do
	    {
		if (++pos == slen || !str[pos]) return slen;
	    } while (str[pos] != '\n');
	    ++pos;
	    continue;
	}
	break;
    }
    if (pos == slen || !str[pos]) return slen;
    unsigned keylen = 0;
    unsigned key[10];
    char keystr[64];
    size_t len;
    while (keylen < 10 && str[pos] != ':' && (len = complen(str, slen, pos)))
    {
	if (str[pos] == '.')
	{
	    if (!keylen) key[keylen++] = XRDB_WILDCARD;
	    ++pos;
	    if (!--len) return slen;
	}
	if (len >= sizeof keystr) return slen;
	if (str[pos] == '?') key[keylen++] = XRDB_WILDCARD;
	else if (str[pos] == '*') key[keylen++] = XRDB_FLEX;
	else
	{
	    memcpy(keystr, str+pos, len);
	    keystr[len] = 0;
	    key[keylen++] = XRdb_id(self, keystr);
	}
	pos += len;
    }
    if (pos == slen || str[pos] != ':') return slen;
    ++pos;
    if (pos == slen || !str[pos]) return slen;
    skipws(str, slen, &pos);
    if (pos == slen || !str[pos]) return slen;
    unsigned vallen = 0;
    char valstr[4096];
    while (vallen < sizeof valstr && pos < slen && str[pos])
    {
	if (str[pos] == '\\')
	{
	    if (++pos < slen && str[pos] == '\n')
	    {
		++pos;
		skipws(str, slen, &pos);
		continue;
	    }
	}
	else if (str[pos] == '\n') break;
	valstr[vallen++] = str[pos++];
    }
    if (vallen == sizeof valstr) return slen;
    valstr[vallen] = 0;
    XRdbEntry *entry = PSC_malloc(sizeof *entry + keylen * sizeof *entry->key);
    entry->value = PSC_copystr(valstr);
    entry->keylen = keylen;
    memcpy(entry->key, key, keylen * sizeof *entry->key);
    PSC_List_append(self->entries, entry, destroyentry);
    return pos;
}

XRdb *XRdb_create(const char *str, size_t slen,
	const char *className, const char *instanceName)
{
    XRdb *self = PSC_malloc(sizeof *self);
    self->ids = PSC_HashTable_create(8);
    self->instances = PSC_HashTable_create(8);
    self->keys = PSC_List_create();
    self->entries = PSC_List_create();

    if (className && instanceName)
    {
	XRdb_register(self, className, instanceName);
	self->root = PSC_copystr(instanceName);
    }
    else self->root = 0;

    while (slen)
    {
	size_t elen = XRdb_parseEntry(self, str, slen);
	str += elen;
	slen -= elen;
    }

    return self;
}

void XRdb_register(XRdb *self, const char *className, const char *instanceName)
{
    uintptr_t classid = XRdb_id(self, className);
    PSC_HashTable_set(self->instances, instanceName, (void *)(classid + 1), 0);
}

static int lowerqual(void *obj, const void *arg)
{
    XRdbQualifiedEntry *qe = obj;
    const int *bestqual = arg;
    return qe->quality < *bestqual;
}

const char *XRdb_value(const XRdb *self, XRdbKey key)
{
    unsigned instid[XRDB_KEYLEN];
    unsigned classid[XRDB_KEYLEN];
    unsigned keylen = 0;
    unsigned arglen = 0;
    if (self->root)
    {
	uintptr_t id = (uintptr_t)PSC_HashTable_get(self->ids, self->root);
	instid[keylen] = id - 1;
	id = (uintptr_t)PSC_HashTable_get(self->instances, self->root);
	classid[keylen] = id - 1;
	++keylen;
    }
    while (keylen < XRDB_KEYLEN && key[arglen])
    {
	uintptr_t id = (uintptr_t)PSC_HashTable_get(self->ids, key[arglen]);
	instid[keylen] = id - 1;
	id = (uintptr_t)PSC_HashTable_get(self->instances, key[arglen]);
	classid[keylen] = id - 1;
	if (instid[keylen] == (unsigned)-1
		&& classid[keylen] == (unsigned)-1) return 0;
	++keylen;
	++arglen;
    }
    if (!keylen) return 0;
    PSC_List *matches = PSC_List_create();
    PSC_ListIterator *i = PSC_List_iterator(self->entries);
    while (PSC_ListIterator_moveNext(i))
    {
	XRdbEntry *entry = PSC_ListIterator_current(i);
	unsigned ekpos = 0;
	unsigned kpos = 0;
	int match = 1;
	while (match && kpos < keylen)
	{
	    if (ekpos == entry->keylen) match = 0;
	    else if (entry->key[ekpos] == instid[kpos]
		    || entry->key[ekpos] == classid[kpos]
		    || entry->key[ekpos] == XRDB_WILDCARD) ++ekpos;
	    else if (entry->key[ekpos] == XRDB_FLEX)
	    {
		if (ekpos + 1 < entry->keylen &&
			(entry->key[ekpos+1] == instid[kpos] ||
			 entry->key[ekpos+1] == classid[kpos])) ekpos += 2;
	    }
	    else match = 0;
	    ++kpos;
	}
	if (match && ekpos == entry->keylen)
	{
	    XRdbQualifiedEntry *qe = PSC_malloc(sizeof *qe);
	    qe->entry = entry;
	    qe->quality = 0;
	    PSC_List_append(matches, qe, free);
	}
    }
    PSC_ListIterator_destroy(i);
    char *result = 0;
    if (!PSC_List_size(matches)) goto done;
    unsigned checkpos = 0;
    while (checkpos < keylen && PSC_List_size(matches) > 1)
    {
	int bestqual = 0;
	for (size_t idx = 0; idx < PSC_List_size(matches); ++idx)
	{
	    XRdbQualifiedEntry *qe = PSC_List_at(matches, idx);
	    XRdbEntry *entry = qe->entry;
	    unsigned ekpos = 0;
	    for (unsigned kpos = 0; kpos < checkpos; ++kpos)
	    {
		if (entry->key[ekpos] == instid[kpos]
			|| entry->key[ekpos] == classid[kpos]
			|| entry->key[ekpos] == XRDB_WILDCARD) ++ekpos;
		else if (entry->key[ekpos] == XRDB_FLEX &&
			ekpos + 1 < entry->keylen &&
			( entry->key[ekpos+1] == instid[kpos]
			  || entry->key[ekpos+1] == classid[kpos])) ekpos += 2;
	    }
	    int quality = 0;
	    if (ekpos < entry->keylen && entry->key[ekpos] == XRDB_FLEX &&
		    (entry->key[ekpos+1] == instid[checkpos] ||
		     entry->key[ekpos+1] == classid[checkpos])) ++ekpos;
	    if (ekpos > 0 && entry->key[ekpos-1] != XRDB_FLEX) quality = 1;
	    if (entry->key[ekpos] != XRDB_FLEX)
	    {
		quality += 2;
		if (entry->key[ekpos] != XRDB_WILDCARD)
		{
		    quality += 2;
		    if (entry->key[ekpos] == instid[checkpos]) quality += 2;
		}
	    }
	    qe->quality = quality;
	    if (quality > bestqual) bestqual = quality;
	}
	PSC_List_removeAll(matches, lowerqual, &bestqual);
	++checkpos;
    }
    XRdbQualifiedEntry *qe = PSC_List_at(matches, 0);
    result = qe->entry->value;

done:
    PSC_List_destroy(matches);
    return result;
}

void XRdb_destroy(XRdb *self)
{
    if (!self) return;
    free(self->root);
    PSC_List_destroy(self->entries);
    PSC_List_destroy(self->keys);
    PSC_HashTable_destroy(self->instances);
    PSC_HashTable_destroy(self->ids);
    free(self);
}

