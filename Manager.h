#ifndef MANAGER_H
#define MANAGER_H

#include <Message.h>
#include <ObjectList.h>
#include <Locker.h>
#include <String.h>
#include <stdio.h>

#include "config.h"
#include "CloudSupport.h"

#define MAX_ERRORS 3

class LocalFilesystem;
class Manager
{
	public:
		Manager(SupportedClouds cloud, int maxWorkerThreads);
		~Manager(void);

		void PerformFullUpdate(bool forceFull);
		void PerformPolledUpdate(void);
		bool PullMissing(BList & items);
	
		void QueueUpload(const char * file, const char * destfullpath, time_t modified, off_t size);
		void QueueDownload(const char * file, const char * destfullpath, time_t modified);
		void QueueCreate(const char * destfullpath);
		void QueueDownloadFolder(const char * path);
		void QueueDelete(const char * path);
		void QueueMove(const char * from, const char * to);

		void StartCloud(void);
		void StopCloud(void);	
		void HandleNodeEvent(BMessage *msg);
		
		enum SupportedActivities {
			UPLOAD = 0,
			DOWNLOAD,
			CREATE,
			DOWNLOAD_FOLDER,
			DELETE,
			MOVE,
			END
		};

		class Activity {
			public:
				Activity(SupportedClouds cloud, SupportedActivities perform, const char * source, const char * dest, time_t modified, off_t size)
				{
					actCloud = cloud;
					action = perform;
					sourcePath = new BString(source);
					destPath = new BString(dest);
					modifiedTime = modified;
					fileSize = size;
				};
				~Activity(void);
			
				SupportedClouds actCloud;
				SupportedActivities action;
				time_t modifiedTime;
				off_t fileSize;
				BString * sourcePath;
				BString * destPath;
		};

		
	private:
		
		LocalFilesystem * fFileSystem;
		SupportedClouds fRunningCloud;
	
		static BObjectList<Activity> * fQueuedActivities;
		static BLocker * fActivityLocker;
		int fMaxThreads;
		int fErrorCount;
		thread_id fManagerThread;
		thread_id fUploadManagerThread;
		BList fUploadCommits;
		BObjectList<Activity> * fQueuedUploads;
		BLocker * fQueuedUploadsLocker;
		BLocker * fUploadCommitLocker;
		BString fUploadAsyncJobId;
		
		void NotifyError(const char * error, const char * summary);
		
		//Activity Queue management
		static Activity & Dequeue(SupportedActivities type);
		Activity & DequeueUpload(void);
		int UploadTotal(void);
		static int ActivityTotalByType(SupportedActivities type);
		static int ActivityTotal(void);
		void TrySpawnWorker(void);
		void TryWakeUploadManager(void);
		void QueueActivity(Activity ** activity, SupportedActivities type);
		
		//manager thread
		static status_t ManagerThread_static(void *manager);
		int ManagerThread_func();
		
		//checker thread
		thread_id	fCheckerThread;
		static status_t CheckerThread_static(void *app);
		int CheckerThread_func();

//bulk ops support		
		//create thread
		static status_t CreateWorkerThread_static(void *manager);
		int CreateWorkerThread_func();
		//move thread
		static status_t MoveWorkerThread_static(void *manager);
		int MoveWorkerThread_func();
		//delete thread
		static status_t DeleteWorkerThread_static(void *manager);
		int DeleteWorkerThread_func();
		//upload manager thread
		static status_t UploadThread_static(void *manager);
		int UploadThread_func();	

//other bits
		//download thread
		static status_t DownloadWorkerThread_static(void *manager);
		int DownloadWorkerThread_func();
		//upload thread
		static status_t UploadWorkerThread_static(void *manager);
		int UploadWorkerThread_func();
		
		void LogUploadCommit(BString * commit);
		
		CloudSupport * _GetCloudController(void);

};

#endif
