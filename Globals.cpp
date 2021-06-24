#include "Globals.h"

#include <AboutWindow.h>
#include <Notification.h>
#include <Message.h>

#include "HttpRequest.h"

BLooper *logRecipient;
bool recipientSet = FALSE;
volatile bool isRunning;
Manager * cloudManager;

void InitGlobals()
{
	gSettings.LoadSettings();
	cloudManager = new Manager(CLOUD_DROPBOX, gSettings.maxThreads);
	isRunning = true;
	HttpRequest::GlobalInit();
	cloudManager->StartCloud();
}

void CleanupGlobals()
{
	isRunning = false;
	cloudManager->StopCloud();
	delete cloudManager;
	HttpRequest::GlobalCleanup();
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
	BNotification * notification;
	if (error) {
		notification = new BNotification(B_ERROR_NOTIFICATION);				
	} else {
		notification = new BNotification(B_INFORMATION_NOTIFICATION);	
	}
	
	notification->SetGroup("PonyExpress");
	notification->SetTitle(title);
	notification->SetContent(content);
	notification->SetMessageID("ponyexpress_infonote");
	notification->Send();
	delete notification;
}


void SendProgressNotification(const char * title, const char * content, const char * identifier, float progress)
{
	
	BNotification * notification = new BNotification(B_PROGRESS_NOTIFICATION);	
	
	notification->SetGroup("PonyExpress");
	notification->SetTitle(title);
	notification->SetContent(content);
	notification->SetMessageID(identifier);
	notification->SetProgress(progress);
	notification->Send();
	delete notification;
}

void ShowAbout()
{
	BAboutWindow * aboutWindow = new BAboutWindow("PonyExpress",APP_SIGNATURE);
	aboutWindow->AddDescription("A native Haiku cloud folder synchronisation application");
	aboutWindow->SetVersion("0.1");	
	aboutWindow->AddCopyright(2021, "Craig Watson");
	aboutWindow->Show();
}
