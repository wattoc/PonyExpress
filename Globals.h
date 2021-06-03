#ifndef GLOBALS_H
#define GLOBALS_H

#include <Looper.h>

#include "App.h"
#include "Settings.h"

#define M_LOG_MESSAGE 'logm'
void InitGlobals(App *app);

void SetLogRecipient(BLooper *recipient);

void LogInfo(const char * info);
void LogInfoLine(const char * info);
void SendNotification(const char * title, const char * content, bool error);
void SendProgressNotification(const char * title, const char * content, const char * identifier, float progress);

#endif
