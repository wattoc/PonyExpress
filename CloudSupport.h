#ifndef CLOUDSUPPORT_H
#define CLOUDSUPPORT_H

#include <List.h>

class CloudSupport {
	public:
		CloudSupport() {}
		virtual ~CloudSupport() {}
		
		virtual bool GetToken() = 0;
		virtual bool ListFiles(const char * path, bool recurse, BList & items) = 0;
		virtual int LongPollForChanges(BList & items) = 0;
		virtual bool GetChanges(BList & items, bool fullupdate) = 0;
		
	//file get/put/delete
		virtual bool Upload(const char * file, const char * destfullpath, time_t modified, off_t size, BString & commitentry) = 0;
		virtual bool Download(const char * file, const char * destfullpath) = 0;
		virtual bool CreatePaths(BList & paths) = 0;
		virtual bool DownloadPath(const char * path) = 0;
		virtual bool DeletePaths(BList & paths) = 0;
		virtual bool MovePaths(BList & from, BList & to) = 0;
		
		virtual bool UploadBatch(BList & commitdata, BString & asyncjobid) = 0;
		virtual bool UploadBatchCheck(const char * asyncjobid, BString & jobstatus) = 0;
	private:
};

#endif
