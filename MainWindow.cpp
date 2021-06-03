#include "MainWindow.h"

#include <AboutWindow.h>
#include <Application.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuItem.h>
#include <View.h>

#include "config.h"
#include "Globals.h"

enum 
{
	M_ABOUT = 'abot',
	M_CONFIGURE = 'conf'
};

MainWindow::MainWindow(void)
	:	BWindow(BRect(100,100,500,400),"PonyExpress",B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE),
	configureWindow(NULL)
{

	menuBar = new BMenuBar("menubar");
	BMenu *menu = new BMenu("File");
	menu->AddItem(new BMenuItem("Configure", new BMessage(M_CONFIGURE)));
	menu->AddItem(new BMenuItem("About", new BMessage(M_ABOUT)));
	menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED),'Q'));
	menuBar->AddItem(menu);

	logView = new BTextView("logoutput");
	logView->MakeEditable(false);
	listView = new BScrollView("scrolllogoutput",logView,0,false,true);
	listView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.Add(listView)
	.End();
	logView->Insert("Starting up PonyExpress\n");
}

void		
MainWindow::LogInfo(const char * msg)
{
	if (logView->CountLines() > 100) 
	{
		logView->Delete(0, logView->OffsetAt(1));
	}
	logView->GoToLine(logView->CountLines());
	logView->Insert(msg);
	logView->ScrollTo(0, logView->Bounds().bottom);
}


void
MainWindow::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case M_ABOUT:
			ShowAbout();
			break;
		case M_LOG_MESSAGE:
			LogInfo(msg->GetString("log"));
			break;
		case M_CONFIGURE:
		{
			if (configureWindow == NULL)
				configureWindow = new ConfigureWindow();
			configureWindow->Show();
			break;
		}
		default:
		{
			BWindow::MessageReceived(msg);
			break;
		}
	}
}


bool
MainWindow::QuitRequested(void)
{
	if (configureWindow != NULL)
		configureWindow->Quit();
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

void MainWindow::ShowAbout(void)
{
	BAboutWindow * aboutWindow = new BAboutWindow("PonyExpress",APP_SIGNATURE);
	aboutWindow->AddDescription("A native Haiku cloud folder synchronisation application");
	aboutWindow->SetVersion("0.1");	
	aboutWindow->AddCopyright(2021, "Craig Watson");
	aboutWindow->Show();
}
