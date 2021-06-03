#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <MenuBar.h>
#include <ScrollView.h>
#include <TextView.h>
#include <Window.h>

#include "ConfigureWindow.h"

class MainWindow : public BWindow
{
public:
						MainWindow(void);
			void		MessageReceived(BMessage *msg);
			bool		QuitRequested(void);
			void LogInfo(const char * msg);
			
private:
			BMenuBar	*menuBar;
			BScrollView	*listView;
			BTextView	*logView;
			
			ConfigureWindow *configureWindow;
			
			void ShowAbout(void);
};


#endif
