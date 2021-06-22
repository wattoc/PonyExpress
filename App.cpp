#include "App.h"

#include <NodeMonitor.h>
#include <Notification.h>

#include "config.h"
#include "CloudSupport.h"
#include "Globals.h"
#include "HttpRequest.h"
#include "MainWindow.h"

volatile bool App::isRunning = false; 

App::App(void)
	:	BApplication(APP_SIGNATURE)
{
	InitGlobals(this);
	HttpRequest::GlobalInit();
	MainWindow * mainwin = new MainWindow();
	mainwin->Show();
	SetLogRecipient(mainwin);
	LogInfo("App started\n");
	cloudManager = new Manager(CLOUD_DROPBOX, gSettings.maxThreads);
	isRunning = true;
	cloudManager->StartCloud();
}

App::~App()
{
	delete cloudManager;
	HttpRequest::GlobalCleanup();
}



void App::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case B_NODE_MONITOR:
		{
			cloudManager->HandleNodeEvent(msg);
			break;	
		}
		case B_QUIT_REQUESTED:
		{
			isRunning = false;
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
