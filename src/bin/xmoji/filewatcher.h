#ifndef XMOJI_FILEWATCHER_H
#define XMOJI_FILEWATCHER_H

#include <poser/decl.h>

C_CLASS_DECL(FileWatcher);
C_CLASS_DECL(PSC_Event);

typedef enum FileChange
{
    FC_ERRORED,
    FC_MODIFIED,
    FC_CREATED,
    FC_DELETED
} FileChange;

FileWatcher *FileWatcher_create(const char *path) ATTR_NONNULL((1));
PSC_Event *FileWatcher_changed(FileWatcher *self) CMETHOD ATTR_RETNONNULL;
int FileWatcher_watch(FileWatcher *self) CMETHOD;
void FileWatcher_unwatch(FileWatcher *self) CMETHOD;
void FileWatcher_destroy(FileWatcher *self);

#endif
