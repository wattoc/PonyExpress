#include "Globals.h"

#include <Message.h>

BLooper *logRecipient;
bool recipientSet = FALSE;
App * globalApp;

void InitGlobals(App *app)
{
	globalApp = app;
	gSettings.LoadSettings();
}

void SetLogRecipient(BLooper *recipient)
{
	logRecipient = recipient;
	recipientSet = TRUE;
}

void LogInfo(const char * info) {
	if (!recipientSet) return;
	
	BMessage log = BMessage(M_LOG_MESSAGE);
	log.AddString("log",info);
	logRecipient->PostMessage(&log);
}

void LogInfoLine(const char * info) {
	LogInfo(info);
	LogInfo("\n");
}

void SendNotification(const char * title, const char * content, bool error)
{
	globalApp->SendNotification(title, content, error);
}

void SendProgressNotification(const char * title, const char * content, const char * identifier, float progress)
{
	globalApp->SendProgressNotification(title, content, identifier, progress);	
}
