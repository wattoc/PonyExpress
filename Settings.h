#ifndef SETTINGS_H
#define SETTINGS_H

#include <Locker.h>
#include <String.h>

class Settings : public BLocker
{
public:
	Settings(void);

	void LoadSettings(void);
	void SaveSettings(void);

	BString		authKey;
	BString		authVerifier;
	BString		refreshToken;
	int			maxThreads;
	//remote cursor, so dropbox can keep sync of what we got last
	BString 		cursor;
	//local time we last synced, so we can detect changes after that while detached
	time_t		lastLocalSync;
	//future expansion, so we can resume
	BString		bulkUploadCursor;
	BString		bulkUploadPath;
	size_t		bulkUploadIndex;

private:

};

extern Settings gSettings;

#endif
