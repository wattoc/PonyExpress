#include "Globals.h"

#include <AboutWindow.h>
#include <Notification.h>
#include <Message.h>

#include "HttpRequest.h"

BLooper *logRecipient;
BMessenger *activityRecipient = NULL;
bool recipientSet = FALSE;
volatile bool gIsRunning;
Manager * gCloudManager;

void InitGlobals()
{
	gSettings.LoadSettings();
	gCloudManager = new Manager(CLOUD_DROPBOX, gSettings.maxThreads);
	gIsRunning = true;
	HttpRequest::GlobalInit();
	gCloudManager->StartCloud();
}

void CleanupGlobals()
{
	gIsRunning = false;
	gCloudManager->StopCloud();
	delete gCloudManager;
	HttpRequest::GlobalCleanup();
}

void SetLogRecipient(BLooper *recipient)
{
	logRecipient = recipient;
	recipientSet = TRUE;
}

void SetActivityRecipient(BMessenger *recipient)
{
	activityRecipient = recipient;
}

void SetActivity(int32 activity) 
{
	if (activityRecipient) {
		activityRecipient->SendMessage(activity);
	}
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
	aboutWindow->SetVersion("0.1.1");	
	aboutWindow->AddCopyright(2021, "Craig Watson");
	aboutWindow->Show();
}

BBitmap *GetIconFromResources(BResources * resources, int32 num, icon_size size)
{
	if (resources == NULL)
		return NULL;
	const uint8* data;
	size_t nbytes = 0;
	data = (const uint8*)resources->LoadResource(B_VECTOR_ICON_TYPE, num, &nbytes);

	BBitmap * icon = new BBitmap(BRect(0,0, size-1, size-1), B_RGBA32);
	if (icon->InitCheck() < B_OK)
		return NULL;
	
	if (BIconUtils::GetVectorIcon(data, nbytes, icon) < B_OK)
	{
		delete icon;
		return NULL;	
	}			
	return icon;
}


status_t our_image(image_info & image)
{
	team_id team = B_CURRENT_TEAM;
	int32 cookie = 0;
	
	while (get_next_image_info(team, &cookie, &image) == B_OK)
	{
		if ((char *)our_image >= (char *)image.text
			&& (char *)our_image <= (char *)image.text + image.text_size)
				return B_OK;	
	}
	return B_ERROR;
}

