#ifndef CLOUDSUPPORT_H
#define CLOUDSUPPORT_H

class CloudSupport {
	public:
		CloudSupport() {}
		virtual ~CloudSupport() {}
		
		virtual bool GetToken() = 0;
		virtual bool ListFiles(const char * path, bool recurse, BList & items) = 0;
		virtual bool GetChanges(BList & items, bool fullupdate) = 0;
		virtual void PerformFullUpdate(bool forceFull) = 0;
		virtual void PerformPolledUpdate(void) = 0;
	
		virtual bool PullMissing(const char * rootpath, BList & items) = 0;
		virtual bool SendMissing(const char * rootpath, BList & items) = 0;
		
	//file get/put/delete
		virtual bool Upload(const char * file, const char * destfullpath, time_t modified, off_t size) = 0;
		virtual bool Download(const char * file, const char * destfullpath) = 0;
		virtual bool CreatePath(const char * destfullpath) = 0;
		virtual bool DownloadPath(const char * path) = 0;
		virtual bool DeletePath(const char * path) = 0;
		virtual bool Move(const char * from, const char * to) = 0;
};

#endif
