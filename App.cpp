#include "App.h"

#include <AboutWindow.h>
#include <Deskbar.h>
#include <NodeMonitor.h>
#include <Notification.h>
#include <Path.h>
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
	gGlobals.InitGlobals();
	image_info info;
	entry_ref ref;
	
	if (our_image(info) == B_OK && get_ref_for_path(info.name, &ref) == B_OK) {
		BDeskbar deskbar;
		if (!deskbar.IsRunning()) return;
		deskbar.AddItem(&ref);
	}
}

App::~App()
{
	gGlobals.CleanupGlobals();
}

void App::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case M_QUIT:
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		case M_REGISTER:
		{
			BMessenger messenger;
			if (msg->FindMessenger("deskbar", &messenger) == B_OK)
				gGlobals.SetActivityRecipient(messenger);
			break;
		}
		case SETTINGS_UPDATE:
			gSettings.LoadSettings();
			break;
		case B_QUIT_REQUESTED:
		{
			gGlobals.gIsRunning = false;
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
