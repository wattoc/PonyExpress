#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <String.h>
#include <SupportDefs.h>

#include <curl/curl.h>


class HttpRequest {
public:
	
	HttpRequest(void);
	~HttpRequest();
	
	bool Post(const char * url, const char * postdata, int postlength, BString &response, BString * authToken, bool addExpectJson);
	bool Download(const char * url, const char * headerdata, const char * fullPath, BString * authToken);
	bool Upload(const char * url, const char * headerdata, const char * fullPath, BString &response, BString * authToken, off_t size);
	bool UploadChunked(const char * url, const char * headerdata, const char * fullPath, BString * authToken, size_t maxchunksize, off_t offset, BString &response);
	
	static bool GlobalInit(void);
	static void GlobalCleanup(void);
private:
	CURL *fCurlHandle;
	FILE * fReadFileCallbackFile;
	size_t fReadLimitCallbackCount;
	size_t fMaxChunkSize;
	static size_t _CallMemberReadFileCallback(void * buffer, size_t sz, size_t n, void *f);
	size_t _ReadFileCallbackLimit(char * buffer, size_t size, size_t nitems);
};


#endif // _H
