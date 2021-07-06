#include "DeskbarIcon.h"

#include <Application.h>
#include <AppFileInfo.h>
#include <Bitmap.h>
#include <Deskbar.h>
#include <MenuItem.h>
#include <Mime.h>
#include <Resources.h>
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

BArchivable * DeskbarIcon::Instantiate(BMessage * msg)
{
	if (!(validate_instantiation(msg, VIEW_NAME))) return NULL;
	if (!be_roster->IsRunning(APP_SIGNATURE)) return NULL;
	return new DeskbarIcon(msg);	
}

DeskbarIcon::DeskbarIcon() : 
				BView(BRect(0, 0, B_MINI_ICON - 1, B_MINI_ICON -1), VIEW_NAME, B_FOLLOW_ALL, B_WILL_DRAW)
{
	Init();
}
	
DeskbarIcon::DeskbarIcon(BMessage *msg)	:
				BView(msg)
{
	Init();	
}

DeskbarIcon::~DeskbarIcon()
{
	if (icon) delete icon;
	if (iconup) delete iconup;
	if (icondown) delete icondown;
	if (iconexclamation) delete iconexclamation;
	icon = NULL;
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
		SetDrawingMode(B_OP_ALPHA);
		if (activityError) {
			DrawBitmap(iconexclamation, BPoint(0,0));			
		} else {
			if (activityUpDown) {
				DrawBitmap(iconcloud, BPoint(0,0));	
			}
			if (activityUp) {
				DrawBitmap(iconup, BPoint(0,0));	
			}
			if (activityDown) {
				DrawBitmap(icondown, BPoint(0,0));	
			}
		}
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
	SetViewColor(Parent()->ViewColor());
	popUp = new BPopUpMenu("popUpMenu", false, false);
	popUp->AddItem(new BMenuItem("Configure", new BMessage(M_CONFIGURE)));
	popUp->AddItem(new BMenuItem("About", new BMessage(M_ABOUT)));
	popUp->AddSeparatorItem();
	popUp->AddItem(new BMenuItem("Quit", new BMessage(M_QUIT)));
	popUp->SetTargetForItems(this);
	ConnectToParent();
}

void DeskbarIcon::DetachedFromWindow()
{
	delete popUp;
}

void DeskbarIcon::Init()
{
	entry_ref ref;
	BResources * resources;
	BFile file;
	image_info info;

	if (our_image(info) != B_OK)
		return;

	file.SetTo(info.name, B_READ_ONLY);
	
	if (file.InitCheck() < B_OK)
		return;
	
	resources = new BResources(&file);
	if (resources->InitCheck() < B_OK) 
		return;
	
	icon = GetIconFromResources(resources, 0, B_MINI_ICON);
	iconexclamation = GetIconFromResources(resources, 1, B_MINI_ICON);
	iconup = GetIconFromResources(resources, 2, B_MINI_ICON);
	icondown = GetIconFromResources(resources, 3, B_MINI_ICON);
	iconcloud = GetIconFromResources(resources, 4, B_MINI_ICON);

	delete resources;
	popUp = NULL;
	counter = 0;
}

void DeskbarIcon::Pulse()
{
	
}


void DeskbarIcon::ConnectToParent()
{
	BMessage registration = BMessage(M_REGISTER);
	BMessenger msgr = BMessenger(APP_SIGNATURE);
	registration.AddMessenger("deskbar",BMessenger(this));
	msgr.SendMessage(&registration, this, 0);
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
		case M_ACTIVITY_NONE:
			activityError = activityUp = activityDown = activityUpDown = false;
			Invalidate();
			break;
		case M_ACTIVITY_ERROR:
			activityError = true;
			activityUp = activityDown = activityUpDown = false;
			Invalidate();
			break;
		case M_ACTIVITY_UPDOWN:
			activityError = activityUp = activityDown = false;
			activityUpDown = true;
			Invalidate();
			break;
		case M_ACTIVITY_DOWN:
			activityError = activityUp = activityUpDown = false;
			activityDown = true; 
			Invalidate();
			break;
		case M_ACTIVITY_UP:
			activityError = activityDown = activityUpDown = false;
			activityUp = true;
			Invalidate();
			break;
		case M_CONFIGURE:
		{
			if (configureWindow == NULL)
				configureWindow = new ConfigureWindow();
			configureWindow->Show();
			break;
		}	
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
