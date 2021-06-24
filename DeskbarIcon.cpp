#include "DeskbarIcon.h"

#include <Application.h>
#include <AppFileInfo.h>
#include <Bitmap.h>
#include <Deskbar.h>
#include <MenuItem.h>
#include <Mime.h>
#include <Roster.h>

#include "config.h"
#include "Globals.h"

enum 
{
	M_ABOUT = 'abot',
	M_CONFIGURE = 'conf',
	M_QUIT = 'quit'
};

BView * instantiate_deskbar_item()
{
	return new DeskbarIcon();	
}

DeskbarIcon * DeskbarIcon::Instantiate(BMessage * msg)
{
	if (!(validate_instantiation(msg, VIEW_NAME))) return NULL;
	return new DeskbarIcon(msg);	
}

DeskbarIcon::DeskbarIcon() : 
				BView(BRect(0, 0, B_MINI_ICON - 1, B_MINI_ICON -1), VIEW_NAME, B_FOLLOW_ALL, B_WILL_DRAW)
{
}
	
DeskbarIcon::DeskbarIcon(BMessage *msg)	:
				BView(msg)
{
	Init();	
}

DeskbarIcon::~DeskbarIcon()
{
	
}

status_t DeskbarIcon::Archive(BMessage *msg, bool deep) const
{
	status_t err;
	err = BView::Archive(msg, deep);
	
	msg->AddString("add_on", APP_SIGNATURE);
	msg->AddString("class", VIEW_NAME);
	return err;	
}

void DeskbarIcon::Draw(BRect update)
{
	if (icon)
	{
		SetDrawingMode(B_OP_OVER);
		DrawBitmap(icon, BPoint(0, 0));	
	}	
}

void DeskbarIcon::MouseDown(BPoint pt)
{
	BPoint point;
	uint32 buttons;
	GetMouse(&point, &buttons);
	
	if (buttons != B_SECONDARY_MOUSE_BUTTON)
	{
		ConvertToScreen(&pt);
		popUp->Go(pt, true, false, false);
	}
}

void DeskbarIcon::AttachedToWindow()
{
	entry_ref ref;
	be_roster->FindApp(APP_SIGNATURE, &ref);

	SetViewColor(Parent()->ViewColor());
	BFile file;
	BAppFileInfo appInfo;

	file.SetTo(&ref, B_READ_ONLY);
	
	appInfo.SetTo(&file);
	
	icon = new BBitmap(BRect(0,0, B_MINI_ICON - 1, B_MINI_ICON - 1), B_CMAP8, false, false);
	
	if (appInfo.GetIcon(icon, B_MINI_ICON) != B_OK)
	{
		delete icon;
		icon = NULL;	
	}	
	popUp = new BPopUpMenu("popUpMenu", false, false);
	popUp->AddItem(new BMenuItem("Configure", new BMessage(M_CONFIGURE)));
	popUp->AddItem(new BMenuItem("About", new BMessage(M_ABOUT)));
	popUp->AddSeparatorItem();
	popUp->AddItem(new BMenuItem("Quit", new BMessage(M_QUIT)));
	popUp->SetTargetForItems(this);
}

void DeskbarIcon::DetachedFromWindow()
{
	delete icon;
	icon = NULL;

}

void DeskbarIcon::Init()
{
	
}

void DeskbarIcon::Pulse()
{
	
}


void DeskbarIcon::RemoveFromDeskbar()
{
	BDeskbar * deskbar = new BDeskbar();
	deskbar->RemoveItem(VIEW_NAME);
	delete deskbar;
}

void DeskbarIcon::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case M_CONFIGURE:
			if (configureWindow == NULL)
				configureWindow = new ConfigureWindow();
			configureWindow->Show();
			break;
			break;
		case M_ABOUT:
			ShowAbout();
			break;
		case M_QUIT:
			{
				if (configureWindow != NULL)
					configureWindow->Quit();
				BMessenger msgr = BMessenger(APP_SIGNATURE);
				msgr.SendMessage(B_QUIT_REQUESTED);
				RemoveFromDeskbar();
				
				break;
			}
		case B_QUIT_REQUESTED:
			RemoveFromDeskbar();
			break;
		default:
			BView::MessageReceived(msg);
			break;
	}	
}
