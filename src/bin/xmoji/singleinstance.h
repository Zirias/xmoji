#ifndef XMOJI_SINGLEINSTANCE_H
#define XMOJI_SINGLEINSTANCE_H

#include <poser/decl.h>

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(SingleInstance);

SingleInstance *SingleInstance_create(void) ATTR_RETNONNULL;
int SingleInstance_start(SingleInstance *self) CMETHOD;
void SingleInstance_stop(SingleInstance *self) CMETHOD;
PSC_Event *SingleInstance_secondary(SingleInstance *self)
    CMETHOD ATTR_RETNONNULL;
void SingleInstance_destroy(SingleInstance *self);

#endif
