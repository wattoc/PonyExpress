#ifndef GLOBALS_H
#define GLOBALS_H

#include <Bitmap.h>
#include <IconUtils.h>
#include <Looper.h>
#include <Resources.h>

#include "App.h"
#include "Manager.h"
#include "Settings.h"

#define M_LOG_MESSAGE 'logm'

#define M_ACTIVITY_NONE 'anon'
#define M_ACTIVITY_ERROR 'aerr'
#define M_ACTIVITY_UPDOWN 'abth'
#define M_ACTIVITY_DOWN 'adwn'
#define M_ACTIVITY_UP 'aupp'

#define SETTINGS_UPDATE 'supd'

class Globals 
{
public:
	void InitGlobals();
	void CleanupGlobals();

	void SetLogRecipient(BLooper *recipient);
	void SetActivityRecipient(BMessenger &recipient);
	void SetActivity(int32 activity);
	void LogInfo(const char * info);
	void LogInfoLine(const char * info);
	void SendNotification(const char * title, const char * content, bool error);
	void SendProgressNotification(const char * title, const char * content, const char * identifier, float progress);
	static void ShowAbout(void);

	static BBitmap *GetIconFromResources(BResources * resources, int32 num, icon_size size);

	volatile bool gIsRunning;
private:	
	Manager * gCloudManager;
	BLooper *logRecipient;
	bool recipientSet;
	volatile BMessenger *activityRecipient;
};

extern Globals gGlobals;

	status_t our_image(image_info & image);

#endif
