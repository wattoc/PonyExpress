#ifndef APP_H
#define APP_H

#include <Application.h>
#include <Notification.h>

#include "DropboxSupport.h"
#include "Manager.h"

class App : public BApplication
{
public:
	Manager * cloudManager;
	
	App(void);
	~App(void);
	
	void MessageReceived(BMessage *msg);
	void SendNotification(const char * title, const char * content, bool error);
	void SendProgressNotification(const char * title, const char * content, const char * identifier, float progress);
	bool IsRunning(void) { return isRunning; }
private:

	static volatile bool isRunning;
};


#endif
