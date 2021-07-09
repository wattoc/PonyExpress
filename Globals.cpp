#include "Globals.h"

#include <AboutWindow.h>
#include <Notification.h>
#include <Message.h>

#include "HttpRequest.h"

Globals gGlobals;

void Globals::InitGlobals()
{
	recipientSet = false;
	activityRecipient = NULL;
	gSettings.LoadSettings();
	gCloudManager = new Manager(CLOUD_DROPBOX, gSettings.maxThreads);
	gIsRunning = true;
	HttpRequest::GlobalInit();
	gCloudManager->StartCloud();
}

void Globals::CleanupGlobals()
{
	gIsRunning = false;
	gCloudManager->StopCloud();
	delete gCloudManager;
	HttpRequest::GlobalCleanup();
}

void Globals::SetLogRecipient(BLooper *recipient)
{
	logRecipient = recipient;
	recipientSet = TRUE;
}

void Globals::SetActivityRecipient(BMessenger &recipient)
{
	activityRecipient = new BMessenger(recipient);
}

void Globals::SetActivity(int32 activity) 
{
	BMessenger * rec = (BMessenger*)activityRecipient;
	if (rec) {
		if (rec->SendMessage(activity) != B_OK) {
			SendNotification("Error","Can't reach Deskbar Icon", true);	
		}
	}
}
void Globals::LogInfo(const char * info) {
	if (!recipientSet) return;
	
	BMessage log = BMessage(M_LOG_MESSAGE);
	log.AddString("log",info);
	logRecipient->PostMessage(&log);
}

void Globals::LogInfoLine(const char * info) {
	LogInfo(info);
	LogInfo("\n");
}

void Globals::SendNotification(const char * title, const char * content, bool error)
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


void Globals::SendProgressNotification(const char * title, const char * content, const char * identifier, float progress)
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

void Globals::ShowAbout()
{
	BAboutWindow * aboutWindow = new BAboutWindow("PonyExpress",APP_SIGNATURE);
	aboutWindow->AddDescription("A native Haiku cloud folder synchronisation application");
	aboutWindow->SetVersion("0.1.1");	
	aboutWindow->AddCopyright(2021, "Craig Watson");
	aboutWindow->Show();
}

BBitmap * Globals::GetIconFromResources(BResources * resources, int32 num, icon_size size)
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

