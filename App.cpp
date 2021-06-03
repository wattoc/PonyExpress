#include "App.h"


#include <NodeMonitor.h>
#include <Notification.h>

#include "config.h"
#include "Globals.h"
#include "MainWindow.h"
#include "LocalFilesystem.h"

volatile bool App::isRunning = false; 

App::App(void)
	:	BApplication(APP_SIGNATURE)
{
	InitGlobals(this);
	MainWindow * mainwin = new MainWindow();
	mainwin->Show();
	SetLogRecipient(mainwin);
	LogInfo("App started\n");
	LocalFilesystem::CheckOrCreateRootFolder("Dropbox/");
	if (gSettings.authKey == NULL || gSettings.authVerifier == NULL) {
		LogInfo("Please configure Dropbox\n");
		SendNotification("Error", "Please configure Dropbox", true);

	}
	else {
		LogInfo("DropBox configured, performing sync check\n");
		DropboxSupport db = DropboxSupport();	
		db.PerformFullUpdate(false);

		LogInfo("Polling for updates\n");
		// run updater in thread
		isRunning = true;
		DBCheckerThread = spawn_thread(DBCheckerThread_static, "Check for remote Dropbox updates", B_LOW_PRIORITY, (void*)this);
		if ((DBCheckerThread) < B_OK) {
			LogInfoLine("Failed to start Dropbox Checker Thread");	
		} else {
			resume_thread(DBCheckerThread);	
		}
		LocalFilesystem::WatchDirectories();
		
		SendNotification(" Ready", "Watching for updates", false);
	}
	
}

int App::DBCheckerThread_static(void *app)
{
	return ((App *)app)->DBCheckerThread_func();
}

int App::DBCheckerThread_func()
{
	DropboxSupport * db = new DropboxSupport();
	while (isRunning)
	{
		db->PerformPolledUpdate();
	}
	delete db;
	return B_OK;
}

void App::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case B_NODE_MONITOR:
		{
			LocalFilesystem::HandleNodeEvent(msg);
			break;	
		}
		default:
			BApplication::MessageReceived(msg);
			break;
	}
}

void App::SendNotification(const char * title, const char * content, bool error)
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


void App::SendProgressNotification(const char * title, const char * content, const char * identifier, float progress)
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


int main(void)
{
	App *app = new App();
	app->Run();
	delete app;
	return 0;
}
