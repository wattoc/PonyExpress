#include "Settings.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include "config.h"

Settings gSettings;

Settings::Settings(void)
{
	maxThreads = 10;

	LoadSettings();	
}

void
Settings::LoadSettings(void)
{
	BPath configPath;
	BDirectory configDirectory;
	BFile settingsFile;
	BString readString;
	BMessage settings = BMessage();

	if(find_directory(B_USER_SETTINGS_DIRECTORY, &configPath) == B_OK) 
	{
		configDirectory.SetTo(configPath.Path());
		if(settingsFile.SetTo(&configDirectory, SETTINGS_FILE, B_READ_ONLY) == B_OK)
		{
			if (settingsFile.Lock() == B_OK) { 
				if (settings.Unflatten(&settingsFile) == B_OK) {
					authKey = settings.GetString("DropBox:AuthKey");
					authVerifier = settings.GetString("DropBox:AuthVerifier");
					refreshToken = settings.GetString("DropBox:RefreshToken");
					cursor = settings.GetString("DropBox:Cursor");
					bulkUploadCursor = settings.GetString("DropBox:BulkUploadCursor");
					bulkUploadPath = settings.GetString("DropBox:BulkUploadPath");
					bulkUploadIndex = settings.GetInt64("DropBox:BulkUploadIndex", 0);
					lastLocalSync = settings.GetInt32("DropBox:LastLocalSync", 0);
				}
				settingsFile.Unlock();		
			}
		}
		else
		{
			authKey.SetTo("");
			authVerifier.SetTo("");
			refreshToken.SetTo("");
			cursor.SetTo("");
		}
	}
}

void
Settings::SaveSettings(void)
{
	BPath configPath;
	BDirectory configDirectory;
	BFile settingsFile;
	BString readString;
	BMessage settings = BMessage();
	
	if(find_directory(B_USER_SETTINGS_DIRECTORY, &configPath) == B_OK) 
	{
		configDirectory.SetTo(configPath.Path());
		if(settingsFile.SetTo(&configDirectory, SETTINGS_FILE, B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE) == B_OK)
		{
			settings.AddString("DropBox:AuthKey", authKey);
			settings.AddString("DropBox:AuthVerifier", authVerifier);
			settings.AddString("DropBox:RefreshToken", refreshToken);
			settings.AddString("DropBox:Cursor", cursor);
			settings.AddString("DropBox:BulkUploadCursor", bulkUploadCursor);
			settings.AddString("DropBox:BulkUploadPath", bulkUploadPath);
			settings.AddInt64("DropBox:BulkUploadIndex", bulkUploadIndex);
			settings.AddInt32("DropBox:LastLocalSync", lastLocalSync);
			
			if (settingsFile.Lock() == B_OK) { 
				if (settings.Flatten(&settingsFile) == B_OK) {
					
				}
				settingsFile.Unlock();
			}
		}
	}
}
