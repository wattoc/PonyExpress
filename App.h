#ifndef APP_H
#define APP_H

#include <Application.h>
#include <Messenger.h>

#include "DeskbarIcon.h"

class App : public BApplication
{
public:
	App(void);
	~App(void);
	
	void MessageReceived(BMessage *msg);
	
private:

};


#endif
