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
			void 		RequestAuthCode(void);
			
			BTabView	*tabView;
			BView		*generalTab;
			BTextControl *authorizationCode;
			BButton		*reqAuthorization;
			BStringView *maxThreadsLabel;
			BStringView *maxThreadsCountLabel;
			BSlider *maxThreads;
									
};


#endif
