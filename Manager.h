#ifndef MANAGER_H
#define MANAGER_H

#include <ObjectList.h>
#include <Locker.h>
#include <String.h>
#include <stdio.h>

#include "CloudSupport.h"

class Manager
{
	public:
		enum SupportedClouds {
			DROPBOX = 1	
		};

		Manager(SupportedClouds cloud, int maxWorkerThreads);
		~Manager(void);


	
		void QueueUpload(SupportedClouds cloud, const char * file, const char * destfullpath, time_t modified, off_t size);
		void QueueDownload(SupportedClouds cloud, const char * file, const char * destfullpath, time_t modified);
		void QueueCreate(SupportedClouds cloud, const char * destfullpath);
		void QueueDownloadFolder(SupportedClouds cloud, const char * path);
		void QueueDelete(SupportedClouds cloud, const char * path);
		void QueueMove(SupportedClouds cloud, const char * from, const char * to);

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
		
		SupportedClouds runningCloud;
	
		static BObjectList<Activity> * queuedActivities;
		static BLocker * activityLocker;
		int maxThreads;
		thread_id managerThread;
		thread_id uploadManagerThread;
		BList uploadCommits;
		BObjectList<Activity> * queuedUploads;
		BLocker * queuedUploadsLocker;
		BLocker * uploadCommitLocker;
		BString uploadasyncjobid;
		
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
		static int ManagerThread_static(void *manager);
		int ManagerThread_func();

//bulk ops support		
		//create thread
		static int CreateWorkerThread_static(void *manager);
		int CreateWorkerThread_func();
		//move thread
		static int MoveWorkerThread_static(void *manager);
		int MoveWorkerThread_func();
		//delete thread
		static int DeleteWorkerThread_static(void *manager);
		int DeleteWorkerThread_func();
		//upload manager thread
		static int UploadThread_static(void *manager);
		int UploadThread_func();	

//other bits
		//download thread
		static int DownloadWorkerThread_static(void *manager);
		int DownloadWorkerThread_func();
		//upload thread
		static int UploadWorkerThread_static(void *manager);
		int UploadWorkerThread_func();
		
		void LogUploadCommit(BString * commit);
		
		static CloudSupport * GetCloudController(Manager::SupportedClouds cloud);
};

#endif
