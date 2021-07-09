#include "HttpRequest.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

bool HttpRequest::GlobalInit()
{
	return curl_global_init(CURL_GLOBAL_ALL) != 0;
}

void HttpRequest::GlobalCleanup()
{
	curl_global_cleanup();
}

HttpRequest::HttpRequest()
{
	fCurlHandle = curl_easy_init();
}

HttpRequest::~HttpRequest()
{
	if (fCurlHandle)
	{
		curl_easy_cleanup(fCurlHandle);
	}	
}

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static size_t writeFileCallback(void * contents, size_t size, size_t nitems, FILE *file)
{
	return fwrite(contents, size, nitems, file);	
}

bool HttpRequest::Post(const char * url, const char * postdata, int postlength, BString &response, BString * authToken, bool addExpectJson)
{
	CURLcode res;
	bool result = false;
	bool retry = false;
	struct MemoryStruct chunk;
	struct curl_slist *headers = NULL;
	

	do
	{
		if (fCurlHandle) 
		{
			chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
  			chunk.size = 0;    /* no data at this point */

			curl_easy_reset(fCurlHandle);
			curl_easy_setopt(fCurlHandle, CURLOPT_URL, url);
			if (postlength>0) {
				curl_easy_setopt(fCurlHandle, CURLOPT_POSTFIELDSIZE, postlength);
				curl_easy_setopt(fCurlHandle, CURLOPT_POSTFIELDS, postdata);
			}
			
			if (authToken->Length() > 0) {
				curl_easy_setopt(fCurlHandle, CURLOPT_XOAUTH2_BEARER, authToken->String());
				curl_easy_setopt(fCurlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
			}
			
			if (addExpectJson) {
				headers = curl_slist_append(headers, "Expect:");
				headers = curl_slist_append(headers, "Content-Type: application/json");
				curl_easy_setopt(fCurlHandle, CURLOPT_HTTPHEADER, headers);			
			}		
			/* send all data to this function  */
	  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	
		   	/* we pass our 'chunk' struct to the callback function */
	  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEDATA, (void *)&chunk);
	
	  		/* some servers don't like requests that are made without a user-agent
	     		field, so we provide one */
	  		curl_easy_setopt(fCurlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
			
			/* get it! */
			res = curl_easy_perform(fCurlHandle);
	
			/* check for errors */
	  		if(res == CURLE_OK) {
	  			response = BString(chunk.memory, chunk.size);
	  			curl_off_t wait = 0;
				curl_easy_getinfo(fCurlHandle, CURLINFO_RETRY_AFTER, &wait);
				if (wait>0)
				{
					retry = true;
					sleep(wait);	
				}
				result = true;
			}
			curl_slist_free_all(headers);
			free(chunk.memory);
		}
	}
	while (retry);
	
	return result;
}

bool HttpRequest::Download(const char * url, const char * headerdata, const char * fullPath, BString * authToken)
{
	FILE * file;
	CURLcode res;
	struct curl_slist *headers = NULL;

	bool result = false;
	file = fopen(fullPath, "w");
	
	if (!file) {
		return false;	
	}

	if (fCurlHandle) 
	{
		curl_easy_reset(fCurlHandle);
		curl_easy_setopt(fCurlHandle, CURLOPT_URL, url);
		
		curl_easy_setopt(fCurlHandle, CURLOPT_XOAUTH2_BEARER, authToken->String());
		curl_easy_setopt(fCurlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		//all bearer requests send json
		headers = curl_slist_append(headers, "Expect:");
		headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
		headers = curl_slist_append(headers, headerdata);

		curl_easy_setopt(fCurlHandle, CURLOPT_HTTPHEADER, headers);		
		
		
		/* send all data to this function  */
  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEFUNCTION, writeFileCallback);

	   	/* we pass our 'chunk' struct to the callback function */
  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEDATA, (void *)file);

  		/* some servers don't like requests that are made without a user-agent
     		field, so we provide one */
  		curl_easy_setopt(fCurlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		
		/* get it! */
		res = curl_easy_perform(fCurlHandle);

		/* check for errors */
  		if(res == CURLE_OK) {
			result = true;
		}
		fclose(file);
		curl_slist_free_all(headers);
	}
	return result;
}

size_t readFileCallback(char * buffer, size_t size, size_t nitems, void *instream)
{
	size_t bytes_read;
	bytes_read = fread(buffer, 1, (size * nitems), (FILE *)instream);
	
	return bytes_read;	
}

size_t HttpRequest::_ReadFileCallbackLimit(char * buffer, size_t size, size_t nitems)
{
	size_t bytes_read;
	size_t to_read;
	
	if (fReadLimitCallbackCount > fMaxChunkSize)
		return 0;
	
	to_read = size * nitems;
	
	if (to_read > fMaxChunkSize)
	{
		to_read = fMaxChunkSize;	
	}
	
	if ((to_read + fReadLimitCallbackCount) > fMaxChunkSize)
	{
		to_read = fMaxChunkSize - fReadLimitCallbackCount;	
	}
		
	bytes_read = fread(buffer, 1, to_read, fReadFileCallbackFile);

	fReadLimitCallbackCount += bytes_read;

	return bytes_read;	
}

bool HttpRequest::Upload(const char * url, const char * headerdata, const char * fullPath, BString &response, BString * authToken, off_t size)
{
	FILE * file;
	CURLcode res;
	struct MemoryStruct chunk;
	struct curl_slist *headers = NULL;
	bool result = false;
	bool retry = false;

	do 
	{
		file = fopen(fullPath, "rb");
		
		if (!file) {
			return false;	
		}
	
		if (fCurlHandle) 
		{
			chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
  			chunk.size = 0;    /* no data at this point */

			curl_easy_reset(fCurlHandle);
			curl_easy_setopt(fCurlHandle, CURLOPT_URL, url);
			
			curl_easy_setopt(fCurlHandle, CURLOPT_XOAUTH2_BEARER, authToken->String());
			curl_easy_setopt(fCurlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
			//all bearer requests send json
			headers = curl_slist_append(headers, "Expect:");
			headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
			headers = curl_slist_append(headers, headerdata);
	
			curl_easy_setopt(fCurlHandle, CURLOPT_HTTPHEADER, headers);		
			curl_easy_setopt(fCurlHandle, CURLOPT_POST, 1L);
			
	  		curl_easy_setopt(fCurlHandle, CURLOPT_READDATA, (void *)file);
	  		curl_easy_setopt(fCurlHandle, CURLOPT_READFUNCTION, readFileCallback);
	  		
	  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEDATA, (void *)&chunk);
	
			//curl_easy_setopt(fCurlHandle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) size);
	  		/* some servers don't like requests that are made without a user-agent
	     		field, so we provide one */
	  		curl_easy_setopt(fCurlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
			
			/* get it! */
			res = curl_easy_perform(fCurlHandle);
	
			/* check for errors */
	  		if(res == CURLE_OK) {
				response = BString(chunk.memory, chunk.size);
				curl_off_t wait = 0;
				curl_easy_getinfo(fCurlHandle, CURLINFO_RETRY_AFTER, &wait);
				if (wait>0)
				{
					retry = true;
					sleep(wait);	
				}
				result = true;
			}
			fclose(file);
			free(chunk.memory);
			curl_slist_free_all(headers);
		}
	}
	while (retry);
	
	return result;
}

size_t HttpRequest::_CallMemberReadFileCallback(void * buffer, size_t sz, size_t n, void *f)
{
	return static_cast<HttpRequest*>(f)->_ReadFileCallbackLimit((char *)buffer, sz, n);
}

bool HttpRequest::UploadChunked(const char * url, const char * headerdata, const char * fullPath, BString * authToken, size_t maxchunksize, off_t offset, BString & response)
{
	CURLcode res;
	struct MemoryStruct chunk;
	struct curl_slist *headers = NULL;
	fReadLimitCallbackCount = 0;
	chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
  	chunk.size = 0;    /* no data at this point */
  	fMaxChunkSize = maxchunksize;
	bool result = false;
	fReadFileCallbackFile = fopen(fullPath, "rb");
	
	if (!fReadFileCallbackFile) {
		response = BString("Could not open file: ");
		response << fullPath;
		return false;
	}
	
	// bump file forward to offset
	fseek(fReadFileCallbackFile, offset, SEEK_SET);

	if (fCurlHandle) 
	{
		curl_easy_reset(fCurlHandle);
		curl_easy_setopt(fCurlHandle, CURLOPT_URL, url);
		
		curl_easy_setopt(fCurlHandle, CURLOPT_XOAUTH2_BEARER, authToken->String());
		curl_easy_setopt(fCurlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		//all bearer requests send json
		headers = curl_slist_append(headers, "Expect:");
		headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
		headers = curl_slist_append(headers, headerdata);

		curl_easy_setopt(fCurlHandle, CURLOPT_HTTPHEADER, headers);		
		curl_easy_setopt(fCurlHandle, CURLOPT_POST, 1L);
		
  		curl_easy_setopt(fCurlHandle, CURLOPT_READDATA, (void *)this);
  		curl_easy_setopt(fCurlHandle, CURLOPT_READFUNCTION, _CallMemberReadFileCallback);
  				/* send all data to this function  */
  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

	   	/* we pass our 'chunk' struct to the callback function */
  		curl_easy_setopt(fCurlHandle, CURLOPT_WRITEDATA, (void *)&chunk);

		//curl_easy_setopt(fCurlHandle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) size);
  		/* some servers don't like requests that are made without a user-agent
     		field, so we provide one */
  		curl_easy_setopt(fCurlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		
		/* get it! */
		res = curl_easy_perform(fCurlHandle);

		/* check for errors */
  		if(res == CURLE_OK) {
			result = true;
  			response = BString(chunk.memory, chunk.size);
		}
		fclose(fReadFileCallbackFile);
		free(chunk.memory);
		curl_slist_free_all(headers);
	}
	return result;
	
}
