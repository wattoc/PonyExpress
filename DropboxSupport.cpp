#include "DropboxSupport.h"

#include <FindDirectory.h>
#include <Directory.h>
#include <Path.h>

#include <mail_encoding.h>
#include <String.h>
#include <private/shared/Json.h>

#include "config.h"
#include "Globals.h"
#include "HttpRequest.h"
#include "sha-256.h"
#include "LocalFilesystem.h"

BString DropboxSupport::sAccessToken = NULL;
time_t DropboxSupport::sTokenExpiry = 0;

BString * DropboxSupport::GetClientAuth(const char * appkey, const char * verifier, int length)
{	
	BString *authUrl = new BString(DROPBOX_AUTH_URL);
	uint8_t challenge[32];
	char * encoded = new char[128];
	BString base64encodeddigest;

	SHA::calc_sha_256(challenge, verifier, length);
	ssize_t used = encode_base64(encoded, (char *)challenge, 32, 1);
		encoded[used] = '\0';
	base64encodeddigest = BString(encoded);
	delete encoded;
	
	base64encodeddigest.RemoveAll("=");
	base64encodeddigest.ReplaceAll('+', '-');
	base64encodeddigest.ReplaceAll('/', '_');

	authUrl->Append("?client_id=");
	authUrl->Append(appkey);
	authUrl->Append("&response_type=code&code_challenge=");
	authUrl->Append(base64encodeddigest);
	authUrl->Append("&code_challenge_method=S256&token_access_type=offline");
	return authUrl;
}

BString * DropboxSupport::GetCodeVerifier()
{
	BString *verifier = NULL;
	char random[32];
	_FillWithRandomData(&random[0], 32);	
	char * encoded = new char[128];
	ssize_t used = encode_base64(encoded, random, 32, 1);
	encoded[used] = '\0';
	verifier = new BString(encoded);
	delete encoded;
	
	verifier->RemoveAll("=");
	verifier->ReplaceAll('+', '-');
	verifier->ReplaceAll('/', '_');
	
	return verifier;
}

bool DropboxSupport::_FillWithRandomData(const char* randomBytes, int length)
{
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) {
		fd = open("/dev/random", O_RDONLY);
		if (fd < 0)
		{
			return FALSE; 
		}
	}
	
	ssize_t bytesRead = read(fd, (void*)randomBytes, length);
	close(fd);
	
	return bytesRead == (ssize_t)sizeof(randomBytes);	
}

bool DropboxSupport::GetToken()
{
	const char * postChars = NULL;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString authToken = BString("");
	HttpRequest * req = new HttpRequest();
	bool result;

	if (time(NULL) < sTokenExpiry) return true;

	gSettings.Lock();
	if (gSettings.refreshToken.Length() > 0) {
		// use refresh token to get a new access token	
		postData.Append("&grant_type=refresh_token");
		postData.Append("&refresh_token=");
		postData.Append(gSettings.refreshToken.String());
	
	} else {
		postData.Append("code=");
		postData.Append(gSettings.authKey);
		postData.Append("&grant_type=authorization_code");	
		postData.Append("&code_verifier=");
		postData.Append(gSettings.authVerifier);
		}
	gSettings.Unlock();
	
	postData.Append("&client_id=");
	postData.Append(DROPBOX_APP_KEY);

	postChars = postData.String();
	result = req->Post(DROPBOX_TOKEN_URL, postChars, postData.Length(), response, &authToken, false);
	delete req;
	if (result) {
		status_t status = BJson::Parse(response.String(),jsonContent);
		if (status == B_OK)
		{
			fLastError = jsonContent.GetString("error");
			fLastErrorSummary = jsonContent.GetString("error_description");
			if (fLastError.Length() == 0) {
				sAccessToken = jsonContent.GetString("access_token");
				gSettings.Lock();
				if (gSettings.refreshToken.Length() == 0) {
					gSettings.refreshToken = BString(jsonContent.GetString("refresh_token"));
					gSettings.SaveSettings();	
				}
				gSettings.Unlock();
				sTokenExpiry = time(NULL) + jsonContent.GetInt32("expires_in",0);
			} else {
				return false;	
			}
		}
  		else {
  			return false;	
  		}
	}
	else {
		return false;	
	}
	
	return true;
}

bool DropboxSupport::ListFiles(const char * path, bool recurse, BList & items) 
{
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	BString localCursor = BString("");
	bool hasMore = true;
	bool listingStatus = true;
	HttpRequest * req = new HttpRequest();
	
	url.Append("2/files/list_folder");

	postData.Append("\{\"path\": \"");
	postData.Append(path);
	postData.Append("\", \"include_deleted\": ");
	postData.Append("false"); // may want to make this configurable later
	postData.Append(", \"recursive\": ");
	postData.Append(recurse ? "true" : "false");
	postData.Append("}");
	
	while (hasMore) {
		hasMore = false;
		if (GetToken()) 
		{
			if (req->Post(url.String(), postData.String(), postData.Length(), response, &sAccessToken, true)) 
			{
				status_t status = BJson::Parse(response.String(),jsonContent);
			
				if (status == B_OK) 
				{
					int index = 0;
					
					BMessage array;
					status = jsonContent.FindMessage("entries", 0, &array);
					while (status == B_OK) 
					{
						BMessage * entry = new BMessage();
						char msgname[10];
						sprintf(msgname, "%d", index);
						status = array.FindMessage(msgname, 0, entry);
						if (status == B_OK) 
						{
							items.AddItem(entry);
						}
						index++;
					} 			
					//request the next set of listings
					hasMore = jsonContent.GetBool("has_more");
					if (hasMore) {
						localCursor = jsonContent.GetString("cursor");
						url = BString(DROPBOX_API_URL);
						url.Append("2/files/list_folder/continue");
						postData = BString("\{\"cursor\": \"");
						postData.Append(localCursor);
						postData.Append("\"}");
					}			
				}
				else {
					fLastError = BString("API");
					fLastErrorSummary = response.String();	
				}			
			}
		}
		else {
			listingStatus = false;
		}
	}
	delete req;
	gSettings.Lock();
	gSettings.cursor = jsonContent.GetString("cursor");
	gSettings.SaveSettings();
	gSettings.Unlock();
	return listingStatus;
}

int DropboxSupport::LongPollForChanges(BList & items)
{
	BString response = NULL;
	BMessage jsonContent;
	int backoff = 0;
	BString postData = BString("");
	BString url = BString(DROPBOX_NOTIFY_URL);
	BString localCursor = BString("");
	BString token = BString("");
	HttpRequest * req = new HttpRequest();
	bool hasMore = true;
	bool reset = false;
	url.Append("2/files/list_folder/longpoll");

	postData.Append("\{\"cursor\": \"");
	gSettings.Lock();
	postData.Append(gSettings.cursor);
	gSettings.Unlock();
	postData.Append("\", \"timeout\": ");
	postData.Append("30"); // may want to make this configurable later
	postData.Append("}");
	
	//long poll request
	if (req->Post(url.String(), postData.String(), postData.Length(), response, &token, true)) {
		status_t status = BJson::Parse(response.String(),jsonContent);
	
		if (status == B_OK) {
			BMessage error;
			bool changes = jsonContent.GetBool("changes", false);
			backoff = jsonContent.GetDouble("backoff", 0);
			if (jsonContent.FindMessage("error", 0, &error) == B_OK)
			{
				fLastError = jsonContent.GetString(".tag");
				fLastErrorSummary = jsonContent.GetString("error_summary");

				if (strcmp(fLastError.String(), "reset")==0)
				{
					//cursor has reset, need to do a full update
					reset = true;
				} else {
					backoff = -1;	
				}
			}
			if (!changes) 
			{
				delete req;
				return backoff;
			}
				
		} else {
			fLastError = BString("API");
			fLastErrorSummary = response.String();

			delete req;
			return -1;	
		}
	}
	//we can handle a reset cursor here
	//by doing a full list instead
	url = BString(DROPBOX_API_URL);
	gSettings.Lock();
	localCursor = BString(gSettings.cursor);
	gSettings.Unlock();

	if (reset) 
	{
		url.Append("2/files/list_folder");
	
		postData.Append("\{\"path\": \"");
		postData.Append("\", \"include_deleted\": ");
		postData.Append("false"); // may want to make this configurable later
		postData.Append(", \"recursive\": ");
		postData.Append("true");
		postData.Append("}");	
	} 
	else 
	{
		url.Append("2/files/list_folder/continue");
		postData = BString("\{\"cursor\": \"");
		postData.Append(localCursor);
		postData.Append("\"}");
	}
	while (hasMore) {
		hasMore = false;
		if (GetToken())
		{
			if (req->Post(url.String(), postData.String(), postData.Length(), response, &sAccessToken, true)) 
			{
				status_t status = BJson::Parse(response.String(),jsonContent);		
				if (status == B_OK) 
				{
					int index = 0;
					
					BMessage array;
					status = jsonContent.FindMessage("entries", 0, &array);
					while (status == B_OK) 
					{
						BMessage * entry = new BMessage();
						char msgname[10];
						sprintf(msgname, "%d", index);
						status = array.FindMessage(msgname, 0, entry);
						if (status == B_OK) 
						{
							items.AddItem(entry);
						}
						index++;
					} 			
					//request the next set of listings
					hasMore = jsonContent.GetBool("has_more");
					if (hasMore) {
						localCursor = jsonContent.GetString("cursor");
						url = BString(DROPBOX_API_URL);
						url.Append("2/files/list_folder/continue");
						postData = BString("\{\"cursor\": \"");
						postData.Append(localCursor);
						postData.Append("\"}");
					}			
				} else {
					fLastError = BString("API");
					fLastErrorSummary = response.String();
					backoff = -1;
				}			
			}
		}
	}
	delete req;
	gSettings.Lock();
	gSettings.cursor = jsonContent.GetString("cursor");
	gSettings.SaveSettings();
	gSettings.Unlock();
	return backoff;	
}

bool DropboxSupport::GetChanges(BList & items, bool fullupdate)
{
	size_t cursorLength = 0;
	
	gSettings.Lock();
	cursorLength = gSettings.cursor.Length();
	gSettings.Unlock();
	
	if (cursorLength == 0 || fullupdate) {
		LogInfo("No cursor available, performing full sync\n");
		return ListFiles("", true, items);
	}
	else {
		LogInfo("Cursor found, updating on poll\n");	
	}
		
	return true;
}

bool DropboxSupport::Upload(const char * file, const char * destfullpath, time_t modified, off_t size, BString & commitentry)
{
	BString response;
	BMessage jsonContent;
	BString url = BString(DROPBOX_CONTENT_URL);
	BString headerdata = BString("Dropbox-API-Arg: ");
	BString commitdata = BString("\{\"path\": \"");
	BString clientmodified = BString(ConvertSystemToTimestamp(modified));
	BString sessionid = BString("");
	HttpRequest * req = new HttpRequest();
	commitdata.Append(destfullpath);
	commitdata.Append("\", \"mode\": \"overwrite\", \"autorename\": true, \"mute\": false, \"strict_conflict\": false, \"client_modified\": \"");
	commitdata.Append(clientmodified);
	commitdata.Append("\"");
	commitdata.Append("}");
	//everything is done as a chunked upload so we can get
	//session id and do a single commit on bulk
    bool result = true;
	off_t remainingsize = size;
	off_t offset = 0;
	if (remainingsize - DROPBOX_UPLOAD_CHUNK > 0) {
		headerdata.Append("\{\"close\": false }");
	} else {
		headerdata.Append("\{\"close\": true }");
	}
	url.Append("2/files/upload_session/start");

	while (remainingsize >= 0 && result)
	{
		if (GetToken()) 
		{
			status_t status;
			result &= req->UploadChunked(url.String(), headerdata.String(), file, &sAccessToken, DROPBOX_UPLOAD_CHUNK, offset, response);
			status = BJson::Parse(response.String(),jsonContent);
			if (status == B_OK) 
			{
				BMessage error;
				if (jsonContent.FindMessage("error", 0, &error) == B_OK)
				{
					fLastError = error.GetString(".tag");
					fLastErrorSummary = error.GetString("error_summary");
					result = false;
				}
			} else {
				fLastError = BString("API");
				fLastErrorSummary = response.String();
				result = false;
			}
			if (sessionid.Length() == 0)
			{
				if (status == B_OK)
					sessionid = jsonContent.GetString("session_id");
			}
			offset += DROPBOX_UPLOAD_CHUNK;
			remainingsize = size - offset;
			if (remainingsize > 0)
			{
				headerdata = BString("Dropbox-API-Arg: ");
				headerdata.Append("\{\"cursor\": \{\"session_id\": \"");
				headerdata.Append(sessionid);
				headerdata.Append("\", \"offset\": ");
				headerdata << offset;
				headerdata.Append("}, ");
				url = BString(DROPBOX_CONTENT_URL);
				url.Append("2/files/upload_session/append_v2");
				if (remainingsize <= DROPBOX_UPLOAD_CHUNK) {
					headerdata.Append(" \"close\": true }");
				}
				else 
				{
					headerdata.Append(" \"close\": false }");
				}
			}
		} else {
			result = false;	
		}
	}
	
	commitentry.Append("\{\"cursor\": \{\"session_id\": \"");		
	commitentry.Append(sessionid);
	commitentry.Append("\", \"offset\": ");
	commitentry << size;
	commitentry.Append(" }, \"commit\": ");
	commitentry.Append(commitdata);
	commitentry.Append("}");
	delete req;
	return result;

}

bool DropboxSupport::Download(const char * file, const char * destfullpath)
{
	HttpRequest * req = new HttpRequest();
	bool result = false;
	BString url = BString(DROPBOX_CONTENT_URL);
	BString headerdata = BString("Dropbox-API-Arg: \{\"path\": \"");
	headerdata.Append(file);
	headerdata.Append("\"}");
	url.Append("2/files/download");
	if (GetToken()) 
	{
		result = req->Download(url.String(),headerdata.String(),destfullpath, &sAccessToken);
		if (result)
		{
// will need to read in created file...
//			status_t status = BJson::Parse(response.String(),jsonContent);	
//			if (status == B_OK) {
//				BMessage error;
//				if (jsonContent.FindMessage("error", 0, &error) == B_OK)
//				{
//					fLastError = error.GetString(".tag");
//					fLastErrorSummary = error.GetString("error_summary");
//					result = false;
//				}
//			}
		}
	} else {
		result = false;	
	}
	delete req;
	return result;
}

bool DropboxSupport::CreatePaths(BList & paths)
{
	HttpRequest * req = new HttpRequest();
	bool result = false;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/create_folder_batch");
	postData.Append("\{\"paths\": [");
	for (int i=0; i< paths.CountItems(); i++)
	{
		postData.Append("\"");
		postData.Append(((BString *)paths.ItemAt(i))->String());
		postData.Append("\"");
		if (i < (paths.CountItems()-1))
		{
			postData.Append(",");	
		}
	}
	postData.Append("], \"autorename\": false, \"force_async\": false }");
	if (GetToken()) 
	{
		result = req->Post(url.String(), postData.String(), postData.Length(), response, &sAccessToken, true);
		if (result)
		{
			status_t status = BJson::Parse(response.String(),jsonContent);	
			if (status == B_OK) {
				BMessage error;
				if (jsonContent.FindMessage("error", 0, &error) == B_OK)
				{
					fLastError = error.GetString(".tag");
					fLastErrorSummary = error.GetString("error_summary");
					result = false;
				}
			} else {
				fLastError = BString("API");
				fLastErrorSummary = response.String();	
			}
		}
	} else {
		result = false;	
	}
	delete req;
	return result;
}

bool DropboxSupport::DownloadPath(const char * path)
{
	//add zipped download support
	return false;
}

bool DropboxSupport::DeletePaths(BList & paths)
{
	HttpRequest * req = new HttpRequest();
	bool result = false;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/delete_batch");
	postData.Append("{\"entries\": [");
	
	for (int i=0; i< paths.CountItems(); i++)
	{
		BString * path;
		path = (BString *)paths.ItemAtFast(i);
		postData.Append("{ \"path\": \"");
		postData.Append(path->String());
		postData.Append("\"}");
		if (i < (paths.CountItems()-1))
		{
			postData.Append(",");	
		}
	}
	postData.Append("] }");

	if (GetToken())
	{
		result = req->Post(url.String(), postData.String(), postData.Length(), response, &sAccessToken, true); 
		if (result)
		{
			status_t status = BJson::Parse(response.String(),jsonContent);	
			if (status == B_OK) {
				BMessage error;
				if (jsonContent.FindMessage("error", 0, &error) == B_OK)
				{
					fLastError = error.GetString(".tag");
					fLastErrorSummary = error.GetString("error_summary");
					result = false;
				}
			}
		}	
	} else {
		result = false;	
	}
	delete req;
	return result;
}

bool DropboxSupport::MovePaths(BList & from, BList & to)
{
	HttpRequest * req = new HttpRequest();
	bool result = false;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/move_batch_v2");
	postData.Append("{\"entries\": [");
	
	for (int i=0; i< from.CountItems(); i++)
	{
		postData.Append("\{\"from_path\": \"");
		postData.Append(((BString *)from.ItemAt(i))->String());
		postData.Append("\", \"to_path\": \"");
		postData.Append(((BString *)to.ItemAt(i))->String());	
		postData.Append("\" }");
		if (i < (from.CountItems()-1))
		{
			postData.Append(",");	
		}
	}
	
	postData.Append("] }");
	
	if (GetToken()) 
	{
		result = req->Post(url.String(), postData.String(), postData.Length(), response, &sAccessToken, true);
		if (result)
		{
			status_t status = BJson::Parse(response.String(),jsonContent);	
			if (status == B_OK) {
				BMessage error;
				if (jsonContent.FindMessage("error", 0, &error) == B_OK)
				{
					fLastError = error.GetString(".tag");
					fLastErrorSummary = error.GetString("error_summary");
					result = false;
				}
			}
		}	
	} else {
		result = false;	
	}
	delete req;
	return result;
}

bool DropboxSupport::UploadBatch(BList & commitdata, BString & asyncjobid)
{
	HttpRequest * req = new HttpRequest();
	bool result;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/upload_session/finish_batch");
	postData.Append("{\"entries\": [");
	
	for (int i=0; i< commitdata.CountItems(); i++)
	{
		BString * commit = (BString *)commitdata.ItemAtFast(i);
		postData.Append(commit->String());
		if (i < (commitdata.CountItems()-1))
		{
			postData.Append(",");
		}
	}
	postData.Append("] }");

	if (GetToken())
	{
		result = req->Post(url.String(), postData.String(), postData.Length(), response, &sAccessToken, true); 
		if (result)
		{
			status_t status = BJson::Parse(response.String(),jsonContent);	
			if (status == B_OK) {
				BMessage error;
				if (jsonContent.FindMessage("error", 0, &error) == B_OK)
				{
					fLastError = error.GetString(".tag");
					fLastErrorSummary = error.GetString("error_summary");
					result = false;
				}
				asyncjobid.Append(jsonContent.GetString("async_job_id"));
			} else {
				fLastError = BString("API");
				fLastErrorSummary = response.String();
			}
		}	
	} else {
		result = false;	
	}
	delete req;
	return result;
}

bool DropboxSupport::UploadBatchCheck(const char * asyncjobid, BString & jobstatus)
{
	HttpRequest * req = new HttpRequest();
	bool result;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/upload_session/finish_batch/check");
	postData.Append("{\"async_job_id\": \"");
	postData.Append(asyncjobid);
	postData.Append("\"}");

	if (GetToken())
	{
		result = req->Post(url.String(), postData.String(), postData.Length(), response, &sAccessToken, true); 
		if (result)
		{
			status_t status = BJson::Parse(response.String(),jsonContent);	
			if (status == B_OK) {
				BMessage error;
				if (jsonContent.FindMessage("error", 0, &error) == B_OK)
				{
					fLastError = error.GetString(".tag");
					fLastErrorSummary = error.GetString("error_summary");
					result = false;
					jobstatus.Append(fLastError);
				} else {
					jobstatus.Append(jsonContent.GetString(".tag"));
				}
			}else {
				fLastError = BString("API");
				fLastErrorSummary = response.String();
			}
		}	
	} else {
		result = false;	
	}
	delete req;
	return result;
}


time_t DropboxSupport::ConvertTimestampToSystem(const char * timestamp)
{
	time_t utctime;
	struct tm tm{};
	strptime(timestamp, DROPBOX_TIMESTAMP_STRING, &tm);
	utctime = mktime(&tm);
	localtime_r(&utctime, &tm);
	return mktime(&tm);
}

const char * DropboxSupport::ConvertSystemToTimestamp(time_t system)
{
	struct tm * tm;
	char * buffer = new char[80];
	tm = localtime(&system);
	strftime(buffer, 80, DROPBOX_TIMESTAMP_STRING, tm);
	return buffer;
}

