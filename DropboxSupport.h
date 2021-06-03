#ifndef DROPBOXSUPPORT_H
#define DROPBOXSUPPORT_H

#include <File.h>
#include <List.h>
#include <SupportDefs.h>
#include <String.h>

#include <curl/curl.h>

#define DROPBOX_API_URL "https://api.dropbox.com/"
#define DROPBOX_NOTIFY_URL "https://notify.dropboxapi.com/"
#define DROPBOX_CONTENT_URL "https://content.dropboxapi.com/"
#define DROPBOX_TOKEN_URL "https://api.dropbox.com/oauth2/token"
#define DROPBOX_AUTH_URL "https://www.dropbox.com/oauth2/authorize"

#define DROPBOX_UPLOAD_CHUNK 4194304
#define DROPBOX_FOLDER "Dropbox/"

class DropboxSupport
{
public:
	
	DropboxSupport(void) {};
	~DropboxSupport();
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
	bool SendMissing(const char * rootpath, BList & items);
		
	//file get/put/delete
	bool Upload(const char * file, const char * destfullpath, const char * clientmodified, off_t size);
	bool Download(const char * file, const char * destfullpath);
	bool CreatePath(const char * destfullpath);
	bool DownloadPath(const char * path);
	bool DeletePath(const char * path);
	bool Move(const char * from, const char * to);
	
	static time_t ConvertTimestampToSystem(const char * timestamp);
	static const char * ConvertSystemToTimestamp(time_t system);
	
private:
	static bool FillWithRandomData(const char* randomBytes, int length);
	bool HttpRequest(const char * url, const char * postdata, int postlength, BString *&response, bool addAuthHeader, bool addExpectJson);
	bool HttpRequestDownload(const char * url, const char * headerdata, const char * fullPath);
	bool HttpRequestUpload(const char * url, const char * headerdata, const char * fullPath, off_t size);
	bool HttpRequestUploadBulk(const char * url, const char * headerdata, const char * fullPath, off_t maxchunksize, off_t offset, BString & response);
	
	
	CURL *curl_handle = NULL;
	static BString accessToken;
	static time_t tokenExpiry;
};

#endif // _H
