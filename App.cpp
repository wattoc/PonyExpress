#include "App.h"

#include <AboutWindow.h>
#include <Deskbar.h>
#include <NodeMonitor.h>
#include <Notification.h>
#include <Roster.h>

#include "config.h"
#include "CloudSupport.h"
#include "DeskbarIcon.h"
#include "Globals.h"

enum 
{
	M_ABOUT = 'abot',
	M_CONFIGURE = 'conf',
	M_QUIT = 'quit'
};

App::App(void)
	:	BApplication(APP_SIGNATURE)
{
	
	BDeskbar deskbar;
	entry_ref ref;
	be_roster->FindApp(APP_SIGNATURE, &ref);
	
	deskbar.AddItem(&ref);
	
		
	InitGlobals();
}

App::~App()
{
	CleanupGlobals();
}

void App::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case M_QUIT:
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
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

int main(void)
{
	App *app = new App();
	app->Run();
	delete app;
	return 0;
}
