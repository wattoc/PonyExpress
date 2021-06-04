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
		M_AUTH_CODE_CHANGED = 'accd',
		M_REQ_AUTH_CODE 	= 'rqac',
		M_MAX_THREADS_CHANGED = 'mtrc'
};

void 
ConfigureWindow::RequestAuthCode() {
	entry_ref ref;
	BString *url, *codeVerifier;
	const char * args[] = { NULL , NULL};
	if (get_ref_for_path("/bin/open", &ref))
		return;
	
	codeVerifier = DropboxSupport::GetCodeVerifier();
	gSettings.authVerifier = BString(codeVerifier->String());
	authorizationCode->SetText("");	
	url = DropboxSupport::GetClientAuth(DROPBOX_APP_KEY, codeVerifier->String(), codeVerifier->Length());
	
	args[0] = url->String();
	// launch browser with client auth	
	be_roster->Launch(&ref, 1, args);
	
	delete codeVerifier;
	delete url;
}

ConfigureWindow::ConfigureWindow(void)
	:	BWindow(BRect(200,100,900,400),"Configuration",B_TITLED_WINDOW, B_NOT_V_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
{
	generalTab = new BView("DropBox", B_WILL_DRAW);
	generalTab->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	authorizationCode = new BTextControl("Authorization Code:", gSettings.authKey ,new BMessage(M_AUTH_CODE_CHANGED));
	reqAuthorization = new BButton("reqAuthorization", "Request Code", new BMessage(M_REQ_AUTH_CODE));
	reqAuthorization->ResizeToPreferred();
	BLayoutItem * authTextView = authorizationCode->CreateTextViewLayoutItem();
	authTextView->SetExplicitMinSize(BSize(300,authTextView->PreferredSize().Height()));
	maxThreadsLabel = new BStringView("maxthreadslabel","Maximum Threads:");
	maxThreadsCountLabel = new BStringView("maxthreadscountlabel","");
	maxThreads = new BSlider("maxThreads", NULL, new BMessage(M_MAX_THREADS_CHANGED), 1, 10, B_HORIZONTAL);
	maxThreads->SetValue(gSettings.maxThreads);
	char val[2];
	sprintf(val,"%d",gSettings.maxThreads);
	maxThreadsCountLabel->SetText(val);

	BLayoutBuilder::Group<>(generalTab, B_VERTICAL,0)
		.AddGrid(B_USE_DEFAULT_SPACING< B_USE_SMALL_SPACING)
		.Add(authorizationCode->CreateLabelLayoutItem(), 0, 0)
		.Add(authTextView, 1, 0, 2)
		.Add(reqAuthorization,1,1, 2)
		.Add(maxThreadsLabel,0,2)
		.Add(maxThreads,1,2)
		.Add(maxThreadsCountLabel,2,2)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.End();
	
	
	tabView = new BTabView("tabview", B_WIDTH_FROM_LABEL);
	tabView->SetBorder(B_NO_BORDER);
	tabView->AddTab(generalTab);
	tabView->Select(0L);
	
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddStrut(B_USE_SMALL_SPACING)
		.Add(tabView)
		.End();
	
	CenterOnScreen();
	
}

void
ConfigureWindow::MessageReceived(BMessage *msg)
{
	switch (msg->what)
	{
		case M_REQ_AUTH_CODE:
			RequestAuthCode();
			break;
		case M_MAX_THREADS_CHANGED:
			char val[2];
			sprintf(val,"%d",maxThreads->Value());
			maxThreadsCountLabel->SetText(val);
			break;
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
	gSettings.authKey=BString(authorizationCode->TextView()->Text());
	gSettings.maxThreads=maxThreads->Position();
	gSettings.SaveSettings();
	Hide();
	return false;
}
