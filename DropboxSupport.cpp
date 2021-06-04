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

BString DropboxSupport::accessToken = NULL;
time_t DropboxSupport::tokenExpiry = 0;

BString * DropboxSupport::GetClientAuth(const char * appkey, const char * verifier, int length)
{	
	BString *authUrl = new BString(DROPBOX_AUTH_URL);
	uint8_t challenge[32];
	char * encoded = new char[128];
	BString base64encodeddigest;

	calc_sha_256(challenge, verifier, length);
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
	FillWithRandomData(&random[0], 32);	
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

bool DropboxSupport::FillWithRandomData(const char* randomBytes, int length)
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

	if (time(NULL) < tokenExpiry) return true;

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
			accessToken = BString(jsonContent.GetString("access_token"));
			gSettings.Lock();
			if (gSettings.refreshToken.Length() == 0) {
				gSettings.refreshToken = BString(jsonContent.GetString("refresh_token"));
				gSettings.SaveSettings();	
			}
			gSettings.Unlock();
			tokenExpiry = time(NULL) + jsonContent.GetInt32("expires_in",0);
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
	
	url.Append("/2/files/list_folder");

	postData.Append("\{\"path\": \"");
	postData.Append(path);
	postData.Append("\", \"include_deleted\": ");
	postData.Append("true"); // may want to make this configurable later
	postData.Append(", \"recursive\": ");
	postData.Append(recurse ? "true" : "false");
	postData.Append("}");
	
	while (hasMore) {
		hasMore = false;
		GetToken();
		if (req->Post(url.String(), postData.String(), postData.Length(), response, &accessToken, true)) {
			status_t status = BJson::Parse(response.String(),jsonContent);
		
			if (status == B_OK) {
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
					url.Append("/2/files/list_folder/continue");
					postData = BString("\{\"cursor\": \"");
					postData.Append(localCursor);
					postData.Append("\"}");
				}			
			}			
		
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
	url.Append("/2/files/list_folder/longpoll");

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
			bool changes = jsonContent.GetBool("changes", false);
			backoff = jsonContent.GetDouble("backoff", 0);
			// need to also check for "error" : { ".tag": "reset" }
			if (!changes) 
			{
				delete req;
				return backoff;
			}
				
		} else {
			delete req;
			return backoff;	
		}
	}
	//we can handle a reset cursor here
	//by doing a full list instead
	url = BString(DROPBOX_API_URL);
	gSettings.Lock();
	localCursor = BString(gSettings.cursor);
	gSettings.Unlock();
	url.Append("/2/files/list_folder/continue");
	postData = BString("\{\"cursor\": \"");
	postData.Append(localCursor);
	postData.Append("\"}");
	
	while (hasMore) {
		hasMore = false;
		GetToken();
		if (req->Post(url.String(), postData.String(), postData.Length(), response, &accessToken, true)) {
			status_t status = BJson::Parse(response.String(),jsonContent);		
			if (status == B_OK) {
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
					url.Append("/2/files/list_folder/continue");
					postData = BString("\{\"cursor\": \"");
					postData.Append(localCursor);
					postData.Append("\"}");
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
		ListFiles("", true, items);
	}
	else {
		LogInfo("Cursor found, updating on poll\n");	
	}
		
	return true;
}

void DropboxSupport::PerformFullUpdate(bool forceFull)
{
		BList items = BList();
		BList updateItems = BList();
		BList localItems = BList();
		GetChanges(items, forceFull);

		for(int i=0; i < items.CountItems(); i++)
		{
			if (LocalFilesystem::TestLocation(DROPBOX_FOLDER, (BMessage*)items.ItemAtFast(i)))
				updateItems.AddItem(items.ItemAtFast(i));
		}
		char itemstoupdate[40];
		sprintf(itemstoupdate, "%d remote items to update\n", updateItems.CountItems());
    	LogInfo(itemstoupdate);
		LocalFilesystem::ResolveUnreferencedLocals(DROPBOX_FOLDER, "", items, localItems, forceFull);
		sprintf(itemstoupdate, "%d local items to update\n", localItems.CountItems());
    	LogInfo(itemstoupdate);
		PullMissing(DROPBOX_FOLDER, updateItems);
		SendMissing(DROPBOX_FOLDER, localItems);

		for(int i=0; i < localItems.CountItems(); i++)
		{
			delete (BMessage*)localItems.ItemAtFast(i);
		}

		for(int i=0; i < items.CountItems(); i++)
		{
			delete (BMessage*)items.ItemAtFast(i);
		}
				//update the lastLocalSync time
		gSettings.Lock();
		gSettings.lastLocalSync = time(NULL);
		gSettings.SaveSettings();
		gSettings.Unlock();

}

void DropboxSupport::PerformPolledUpdate()
{
		BList items = BList();
		BList updateItems = BList();
		int backoff = LongPollForChanges(items);

		for(int i=0; i < items.CountItems(); i++)
		{
			if (LocalFilesystem::TestLocation(DROPBOX_FOLDER, (BMessage*)items.ItemAtFast(i)))
				updateItems.AddItem(items.ItemAtFast(i));
		}
		char itemstoupdate[40];
		sprintf(itemstoupdate, "%d remote items to update\n", updateItems.CountItems());
    	LogInfo(itemstoupdate);
		PullMissing(DROPBOX_FOLDER, updateItems);

		for(int i=0; i < items.CountItems(); i++)
		{
			delete (BMessage*)items.ItemAtFast(i);
		}
		//update the lastLocalSync time
		gSettings.Lock();
		gSettings.lastLocalSync = time(NULL);
		gSettings.SaveSettings();
		gSettings.Unlock();
		sleep(backoff);
}

bool DropboxSupport::PullMissing(const char * rootpath, BList & items)
{
	bool result = true;
	BPath userpath;
	float progress = 0;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(rootpath);
		if (items.CountItems() > 0) 
		for(int i=0; i < items.CountItems(); i++)
		{	
			progress = ((float)i) / items.CountItems();
			SendProgressNotification("Remote update", "retrieving updates", "remote_update", progress);
			BMessage * item = (BMessage*)items.ItemAtFast(i);
			BEntry fsentry;
			BString entryPath = item->GetString("path_display");
			BString entryType = item->GetString(".tag");
			BString fullPath = BString(userpath.Path());
			fullPath.Append(entryPath);
			LocalFilesystem::AddToIgnoreList(fullPath.String());			
			if (entryType=="file") {
				time_t sModified = ConvertTimestampToSystem(item->GetString("client_modified"));
				result = result & Download(entryPath.String(), fullPath.String());
				//set modified date
				fsentry = BEntry(fullPath.String());
				fsentry.SetModificationTime(sModified);
			}
			// we don't support DIRs by Zip yet
		}
		sleep(1);
		for(int i=0; i < items.CountItems(); i++)
		{		
			BMessage * item = (BMessage*)items.ItemAtFast(i);
			BEntry fsentry;
			BString entryPath = item->GetString("path_display");
			BString entryType = item->GetString(".tag");
			BString fullPath = BString(userpath.Path());
			fullPath.Append(entryPath);
			fsentry = BEntry(fullPath.String());
			if (fsentry.IsDirectory()) {
				LogInfo("Watching directory: ");
				LogInfoLine(fullPath);
			}
			LocalFilesystem::WatchEntry(&fsentry, WATCH_FLAGS);
			LocalFilesystem::RemoveFromIgnoreList(fullPath.String());		
		}
		
		if (items.CountItems() > 0)
			SendProgressNotification("Remote update", "completing updates", "remote_update", 1);

	}
	return result;	
}

bool DropboxSupport::SendMissing(const char * rootpath, BList & items)
{
	bool result = true;
	BPath userpath;
	float progress = 0;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(rootpath);
		for(int i=0; i < items.CountItems(); i++)
		{	
			progress = ((float)i) / items.CountItems();
			SendProgressNotification("Local update", "sending updates", "local_update", progress);
			BEntry fsentry;
			time_t sModified;
			off_t sSize = 0;

			BMessage * item = (BMessage*)items.ItemAtFast(i);
			BString entryPath = item->GetString("path_display");
			BString entryType = item->GetString(".tag");
			BString fullPath = BString(userpath.Path());
			
			fsentry = BEntry(fullPath.String());
			fsentry.GetModificationTime(&sModified);
			fsentry.GetSize(&sSize);
			fullPath.Append(entryPath);
			
			const char * modified = ConvertSystemToTimestamp(sModified);
			
			if (entryType=="file") {
				result &= Upload(fullPath.String(), entryPath.String(), modified, sSize);
			} else if (entryType=="folder") {
				result &= CreatePath(entryPath.String());
			}

			delete modified;
			// we don't support DIRs by Zip yet
		}
		if (items.CountItems() > 0) 
			SendProgressNotification("Local update", "sending updates", "local_update", progress);
		//update the lastLocalSync time
		gSettings.Lock();
		gSettings.lastLocalSync = time(NULL);
		gSettings.SaveSettings();
		gSettings.Unlock();
	}
	return result;	
}

bool DropboxSupport::Upload(const char * file, const char * destfullpath, const char * clientmodified, off_t size)
{
	BString url = BString(DROPBOX_CONTENT_URL);
	BString headerdata = BString("Dropbox-API-Arg: ");
	BString commitdata = BString("\{\"path\": \"");
	HttpRequest * req = new HttpRequest();
	commitdata.Append(destfullpath);
	commitdata.Append("\", \"mode\": \"overwrite\", \"autorename\": true, \"mute\": false, \"strict_conflict\": false, \"client_modified\": \"");
	commitdata.Append(clientmodified);
	commitdata.Append("\"");
	commitdata.Append("}");
	if (size <= 157286400) {
		bool result; 
		headerdata.Append(commitdata);
		url.Append("2/files/upload");
		GetToken();
		SendNotification("Upload", destfullpath, false);
		result = req->Upload(url.String(),headerdata.String(), file, &accessToken, size);
		delete req;
		return result;
	} else 
	{
		// need to use sessions to do > 150MB
		BString response;
		BMessage jsonContent;
	    bool result = true;
		off_t remainingsize = size;
		off_t offset = 0;
		float progress = 0;
		headerdata.Append("\{\"close\": false }");
		url.Append("2/files/upload_session/start");
		BString sessionid = BString("");
		while (remainingsize > 0 && result)
		{
			GetToken();
			result &= req->UploadChunked(url.String(), headerdata.String(), file, &accessToken, DROPBOX_UPLOAD_CHUNK, offset, response);
			if (sessionid.Length() == 0)
			{
				status_t status = BJson::Parse(response.String(),jsonContent);
				if (status == B_OK)
					sessionid = jsonContent.GetString("session_id");
			}
			progress = offset/(double)size;
			SendProgressNotification("Bulk Upload", destfullpath, destfullpath, progress);

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
				url.Append("2/files/upload_session/");
				if (remainingsize <= DROPBOX_UPLOAD_CHUNK) {				
					url.Append("finish");
					headerdata.Append(" \"commit\": ");
					headerdata.Append(commitdata);
					headerdata.Append("}");
				}
				else 
				{
					
					url.Append("append_v2");	
					headerdata.Append(" \"close\": false }");
					
				}
			}
		}
		if (result)
		{
			SendProgressNotification("Bulk Upload", destfullpath, destfullpath, 1);
		}
		delete req;
		return result;
	}
}

bool DropboxSupport::Download(const char * file, const char * destfullpath)
{
	HttpRequest * req = new HttpRequest();
	bool result;
	BString url = BString(DROPBOX_CONTENT_URL);
	BString headerdata = BString("Dropbox-API-Arg: \{\"path\": \"");
	headerdata.Append(file);
	headerdata.Append("\"}");
	url.Append("2/files/download");
	GetToken();
	result = req->Download(url.String(),headerdata.String(),destfullpath, &accessToken);
	delete req;
	return result;
}

bool DropboxSupport::CreatePath(const char * destfullpath)
{
	HttpRequest * req = new HttpRequest();
	bool result;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/create_folder_v2");
	postData.Append("\{\"path\": \"");
	postData.Append(destfullpath);
	postData.Append("\", \"autorename\": false }");
	GetToken();
	result = req->Post(url.String(), postData.String(), postData.Length(), response, &accessToken, true);
	delete req;
	return result;
}

bool DropboxSupport::DeletePath(const char * path)
{
	HttpRequest * req = new HttpRequest();
	bool result;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/delete_v2");
	postData.Append("\{\"path\": \"");
	postData.Append(path);
	postData.Append("\" }");
	GetToken();

	result = req->Post(url.String(), postData.String(), postData.Length(), response, &accessToken, true); 
	delete req;
	return result;
}

bool DropboxSupport::Move(const char * from, const char * to)
{
	HttpRequest * req = new HttpRequest();
	bool result;
	BString response;
	BMessage jsonContent;
	BString postData = BString("");
	BString url = BString(DROPBOX_API_URL);
	url.Append("2/files/move_v2");
	postData.Append("\{\"from_path\": \"");
	postData.Append(from);
	postData.Append("\", \"to_path\": \"");
	postData.Append(to);	
	postData.Append("\" }");
	
	GetToken();
	result = req->Post(url.String(), postData.String(), postData.Length(), response, &accessToken, true);
	delete req;
	return result;
}

time_t DropboxSupport::ConvertTimestampToSystem(const char * timestamp)
{
	struct tm tm;
	strptime(timestamp, "%Y-%m-%dT%H:%M:%SZ", &tm);
	return mktime(&tm);
}

const char * DropboxSupport::ConvertSystemToTimestamp(time_t system)
{
	struct tm * tm;
	char * buffer = new char[80];
	tm = gmtime(&system);
	strftime(buffer, 80, "%Y-%m-%dT%H:%M:%SZ", tm);
	return buffer;
}

