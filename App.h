#ifndef APP_H
#define APP_H

#include <Application.h>
#include <Notification.h>

#include "DropboxSupport.h"

class App : public BApplication
{
public:
	App(void);
	void MessageReceived(BMessage *msg);
	void SendNotification(const char * title, const char * content, bool error);
	void SendProgressNotification(const char * title, const char * content, const char * identifier, float progress);
private:
	thread_id	DBCheckerThread;

	static volatile bool isRunning;
	static int DBCheckerThread_static(void *app);
	static int DBCheckerThread_func();
};


#endif
