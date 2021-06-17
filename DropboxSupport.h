#ifndef DROPBOXSUPPORT_H
#define DROPBOXSUPPORT_H

#include <File.h>
#include <List.h>
#include <SupportDefs.h>
#include <String.h>

#include <CloudSupport.h>

#define DROPBOX_API_URL "https://api.dropbox.com/"
#define DROPBOX_NOTIFY_URL "https://notify.dropboxapi.com/"
#define DROPBOX_CONTENT_URL "https://content.dropboxapi.com/"
#define DROPBOX_TOKEN_URL "https://api.dropbox.com/oauth2/token"
#define DROPBOX_AUTH_URL "https://www.dropbox.com/oauth2/authorize"

#define DROPBOX_UPLOAD_CHUNK 4194304
#define DROPBOX_FOLDER "Dropbox/"

#define DROPBOX_TIMESTAMP_STRING "%Y-%m-%dT%H:%M:%SZ"

class DropboxSupport : public CloudSupport
{
public:
	
	DropboxSupport(void) {};
	~DropboxSupport() {};
	//auth/token stuff
	static BString * GetClientAuth(const char * appkey, const char * verifier, int length);
	static BString * GetCodeVerifier(void);
	bool GetToken(void);
	
	//file listing/watching
	//returns true if there are more files to get
	bool ListFiles(const char * path, bool recurse, BList & items);
	int LongPollForChanges(BList & items);
	bool GetChanges(BList & items, bool fullupdate);
	void PerformFullUpdate(bool forceFull);
	void PerformPolledUpdate(void);
	
	bool PullMissing(const char * rootpath, BList & items);
		
	//file get/put/delete
	bool Upload(const char * file, const char * destfullpath, time_t modified, off_t size, BString & commitentry);
	bool Download(const char * file, const char * destfullpath);
	bool CreatePaths(BList & paths);
	bool DownloadPath(const char * path);
	bool DeletePaths(BList & paths);
	bool MovePaths(BList & from, BList & to);
	
	bool UploadBatch(BList & commitdata, BString & asyncjobid);
	bool UploadBatchCheck(const char * asyncjobid, BString & jobstatus);
	
	static time_t ConvertTimestampToSystem(const char * timestamp);
	static const char * ConvertSystemToTimestamp(time_t system);
	
private:
	static bool FillWithRandomData(const char* randomBytes, int length);
	static BString accessToken;
	static time_t tokenExpiry;
};

#endif // _H
