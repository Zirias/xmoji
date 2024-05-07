#ifndef XMOJI_MAINWINDOW_H
#define XMOJI_MAINWINDOW_H

#include <poser/decl.h>

C_CLASS_DECL(MainWindow);

C_CLASS_DECL(PSC_Event);
C_CLASS_DECL(X11Adapter);

MainWindow *MainWindow_create(X11Adapter *x11)
    ATTR_NONNULL((1));

PSC_Event *MainWindow_closed(MainWindow *self)
    CMETHOD ATTR_RETNONNULL;

void MainWindow_show(MainWindow *self)
    CMETHOD;

void MainWindow_destroy(MainWindow *self);

#endif
