#ifndef CONFIGUREWINDOW_H
#define CONFIGUREWINDOW_H

#include <Button.h>
#include <Slider.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <Window.h>

class ConfigureWindow : public BWindow
{
public:
			ConfigureWindow(void);
			void		MessageReceived(BMessage *msg);
			bool		QuitRequested(void);		
private:
			void 		_RequestAuthCode(void);
			
			BTabView	*fTabView;
			BView		*fGeneralTab;
			BTextControl *fAuthorizationCode;
			BButton		*fReqAuthorization;
			BStringView *fMaxThreadsLabel;
			BStringView *fMaxThreadsCountLabel;
			BSlider 	*fMaxThreads;
									
};


#endif
