#ifndef APP_H
#define APP_H

#include <Application.h>
#include <Notification.h>

#include "DropboxSupport.h"
#include "Manager.h"
#include "LocalFilesystem.h"

class App : public BApplication
{
public:
	Manager * cloudManager;
	LocalFilesystem * fileSystem;
	
	App(void);
	~App(void);
	
	void MessageReceived(BMessage *msg);
	void SendNotification(const char * title, const char * content, bool error);
	void SendProgressNotification(const char * title, const char * content, const char * identifier, float progress);
private:
	thread_id	DBCheckerThread;

	static volatile bool isRunning;
	static int DBCheckerThread_static(void *app);
	int DBCheckerThread_func();
	
	void StartDropbox(void);
};


#endif
