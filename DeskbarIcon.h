#ifndef DESKBARICON_H
#define DESKBARICON_H

#include <PopUpMenu.h>
#include <View.h>

#include "ConfigureWindow.h"

extern "C" _EXPORT BView *instantiate_deskbar_item();

#define VIEW_NAME "PonyExpress Deskbar Icon"
#define M_REGISTER 'rgst'

class DeskbarIcon : public BView
{
	public:
		static BArchivable * Instantiate(BMessage *msg);
		
		DeskbarIcon(void);
		DeskbarIcon(BMessage *msg);
		~DeskbarIcon();
		
		static void RemoveFromDeskbar(void);
		
		status_t Archive(BMessage *msg, bool deep = true) const;
		void Draw(BRect update);
		void MouseDown(BPoint pt);
		void AttachedToWindow(void);
		void DetachedFromWindow(void);
		void Pulse(void);
		
		void MessageReceived(BMessage *msg);
		
	private:
		void Init(void);
		void ConnectToParent(void);
		BBitmap *icon;
		BBitmap *iconexclamation;
		BBitmap *iconup;
		BBitmap *icondown;
		BBitmap *iconcloud;
		ConfigureWindow *configureWindow;
		BPopUpMenu *popUp;

		int counter;
		bool parentConnected;
		bool activityError;
		bool activityUp;
		bool activityDown;
		bool activityUpDown;
};

#endif
