#include "ConfigureWindow.h"

#include <Application.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Roster.h>
#include <View.h>
#include <stdio.h>

#include "config.h"
#include "DropboxSupport.h"
#include "Globals.h"

enum {
		M_AUTH_CODE_CHANGED = 'aucd',
		M_REQ_AUTH_CODE 	= 'rqac',
		M_MAX_THREADS_CHANGED = 'mtrc'
};

void 
ConfigureWindow::_RequestAuthCode() {
	entry_ref ref;
	BString *url, *codeVerifier;
	const char * args[] = { NULL , NULL};
	if (get_ref_for_path("/bin/open", &ref))
		return;
	
	codeVerifier = DropboxSupport::GetCodeVerifier();
	gSettings.authVerifier = BString(codeVerifier->String());
	fAuthorizationCode->SetText("");
	url = DropboxSupport::GetClientAuth(DROPBOX_APP_KEY, codeVerifier->String(), codeVerifier->Length());
	
	args[0] = url->String();
	// launch browser with client auth	
	be_roster->Launch(&ref, 1, args);
	
	delete codeVerifier;
	delete url;
}

ConfigureWindow::ConfigureWindow(void)
	:	BWindow(BRect(200,100,900,400),"Configuration",B_TITLED_WINDOW, B_NOT_V_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE)
{
	fGeneralTab = new BView("DropBox", B_WILL_DRAW);
	fGeneralTab->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	fAuthorizationCode = new BTextControl("Authorization Code:", gSettings.authKey ,new BMessage(M_AUTH_CODE_CHANGED));
	fReqAuthorization = new BButton("reqAuthorization", "Request Code", new BMessage(M_REQ_AUTH_CODE));
	fReqAuthorization->ResizeToPreferred();
	fAuthTextView = fAuthorizationCode->CreateTextViewLayoutItem();
	fAuthTextView->SetExplicitMinSize(BSize(300,fAuthTextView->PreferredSize().Height()));
	fMaxThreadsLabel = new BStringView("maxthreadslabel","Maximum Threads:");
	fMaxThreadsCountLabel = new BStringView("maxthreadscountlabel","");
	fMaxThreads = new BSlider("maxThreads", NULL, new BMessage(M_MAX_THREADS_CHANGED), 1, 10, B_HORIZONTAL);
	fMaxThreads->SetValue(gSettings.maxThreads);
	char val[2];
	sprintf(val,"%d",gSettings.maxThreads);
	fMaxThreadsCountLabel->SetText(val);

	BLayoutBuilder::Group<>(fGeneralTab, B_VERTICAL,0)
		.AddGrid(B_USE_DEFAULT_SPACING< B_USE_SMALL_SPACING)
		.Add(fAuthorizationCode->CreateLabelLayoutItem(), 0, 0)
		.Add(fAuthTextView, 1, 0, 2)
		.Add(fReqAuthorization,1,1, 2)
		.Add(fMaxThreadsLabel,0,2)
		.Add(fMaxThreads,1,2)
		.Add(fMaxThreadsCountLabel,2,2)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.End();
	
	
	fTabView = new BTabView("tabview", B_WIDTH_FROM_LABEL);
	fTabView->SetBorder(B_NO_BORDER);
	fTabView->AddTab(fGeneralTab);
	fTabView->Select(0L);
	
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddStrut(B_USE_SMALL_SPACING)
		.Add(fTabView)
		.End();
	
	CenterOnScreen();
	
}

void
ConfigureWindow::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case M_REQ_AUTH_CODE:
			_RequestAuthCode();
			break;
		case M_MAX_THREADS_CHANGED:
			{
				BString val = BString("");
				val << fMaxThreads->Value();
				fMaxThreadsCountLabel->SetText(val);
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
ConfigureWindow::QuitRequested(void)
{
	gSettings.authKey=BString(fAuthorizationCode->TextView()->Text());
	gSettings.maxThreads=(int)fMaxThreads->Position();
	gSettings.SaveSettings();
	BMessenger msgr = BMessenger(APP_SIGNATURE);
	msgr.SendMessage(SETTINGS_UPDATE);
	Hide();
	return false;
}
