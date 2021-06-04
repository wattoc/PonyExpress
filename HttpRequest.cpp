#include "HttpRequest.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

HttpRequest::HttpRequest()
{
	curl_handle = curl_easy_init();
}

HttpRequest::~HttpRequest()
{
	if (curl_handle)
	{
		curl_easy_cleanup(curl_handle);
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
	struct MemoryStruct chunk;
	struct curl_slist *headers = NULL;
	
	chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
  	chunk.size = 0;    /* no data at this point */

	if (curl_handle == NULL) 
	{
		curl_handle = curl_easy_init();
	} else {
		curl_easy_reset(curl_handle);
	}
	if (curl_handle) 
	{
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		if (postlength>0) {
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, postlength);
			curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postdata);
		}
		
		if (authToken->Length() > 0) {
			curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, authToken->String());
			curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		}
		
		if (addExpectJson) {
			headers = curl_slist_append(headers, "Expect:");
			headers = curl_slist_append(headers, "Content-Type: application/json");
			curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);			
		}		
		/* send all data to this function  */
  		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

	   	/* we pass our 'chunk' struct to the callback function */
  		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

  		/* some servers don't like requests that are made without a user-agent
     		field, so we provide one */
  		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		
		/* get it! */
		res = curl_easy_perform(curl_handle);

		/* check for errors */
  		if(res == CURLE_OK) {
  			response = BString(chunk.memory, chunk.size);
			result = true;
		}
		curl_slist_free_all(headers);
		free(chunk.memory);
	}
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

	if (curl_handle) 
	{
		curl_easy_reset(curl_handle);
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		
		curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, authToken->String());
		curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		//all bearer requests send json
		headers = curl_slist_append(headers, "Expect:");
		headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
		headers = curl_slist_append(headers, headerdata);

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);		
		
		
		/* send all data to this function  */
  		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeFileCallback);

	   	/* we pass our 'chunk' struct to the callback function */
  		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)file);

  		/* some servers don't like requests that are made without a user-agent
     		field, so we provide one */
  		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		
		/* get it! */
		res = curl_easy_perform(curl_handle);

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

size_t readLimitCallbackCount;
size_t maxchunkSize;

size_t readFileCallbackLimit(char * buffer, size_t size, size_t nitems, void *instream)
{
	size_t bytes_read;
	size_t to_read;
	
	if (readLimitCallbackCount > maxchunkSize)
		return 0;
	
	to_read = size * nitems;
	
	if (to_read > maxchunkSize)
	{
		to_read = maxchunkSize;	
	} else {
		if ((to_read + readLimitCallbackCount) > maxchunkSize)
		{
			to_read = maxchunkSize - readLimitCallbackCount;	
		}
	}
	
	bytes_read = fread(buffer, 1, to_read, (FILE *)instream);

	readLimitCallbackCount += bytes_read;

	return bytes_read;	
}

bool HttpRequest::Upload(const char * url, const char * headerdata, const char * fullPath, BString * authToken, off_t size)
{
	FILE * file;
	CURLcode res;
	struct curl_slist *headers = NULL;

	bool result = false;
	file = fopen(fullPath, "rb");
	
	if (!file) {
		return false;	
	}

	if (curl_handle) 
	{
		curl_easy_reset(curl_handle);
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		
		curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, authToken->String());
		curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		//all bearer requests send json
		headers = curl_slist_append(headers, "Expect:");
		headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
		headers = curl_slist_append(headers, headerdata);

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);		
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
		
  		curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void *)file);
  		curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, readFileCallback);
  		
		//curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) size);
  		/* some servers don't like requests that are made without a user-agent
     		field, so we provide one */
  		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		
		/* get it! */
		res = curl_easy_perform(curl_handle);

		/* check for errors */
  		if(res == CURLE_OK) {
			result = true;
		}
		fclose(file);
		curl_slist_free_all(headers);
	}
	return result;
}

bool HttpRequest::UploadChunked(const char * url, const char * headerdata, const char * fullPath, BString * authToken, size_t maxchunksize, off_t offset, BString & response)
{
	FILE * file;
	CURLcode res;
	struct MemoryStruct chunk;
	struct curl_slist *headers = NULL;
	readLimitCallbackCount = 0;
	chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */
  	chunk.size = 0;    /* no data at this point */
  	maxchunkSize = maxchunksize;
	bool result = false;
	file = fopen(fullPath, "rb");
	
	if (!file) {
		return false;	
	}
	
	// bump file forward to offset
	fseek(file, offset, SEEK_SET);

	if (curl_handle) 
	{
		curl_easy_reset(curl_handle);
		curl_easy_setopt(curl_handle, CURLOPT_URL, url);
		
		curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, authToken->String());
		curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		//all bearer requests send json
		headers = curl_slist_append(headers, "Expect:");
		headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
		headers = curl_slist_append(headers, headerdata);

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);		
		curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
		
  		curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void *)file);
  		curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, readFileCallbackLimit);
  				/* send all data to this function  */
  		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

	   	/* we pass our 'chunk' struct to the callback function */
  		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

		//curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) size);
  		/* some servers don't like requests that are made without a user-agent
     		field, so we provide one */
  		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
		
		/* get it! */
		res = curl_easy_perform(curl_handle);

		/* check for errors */
  		if(res == CURLE_OK) {
			result = true;
  			response = BString(chunk.memory, chunk.size);
		}
		fclose(file);
		free(chunk.memory);
		curl_slist_free_all(headers);
	}
	return result;
	
}
