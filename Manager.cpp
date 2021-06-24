#include "Manager.h"

#include <FindDirectory.h>

#include "DropboxSupport.h"
#include "Globals.h"

#define KILL_THREAD 'UDIE'
#define START_THREAD 'STRT'

static const char * CloudPaths[] = {"", "Dropbox/"};


BObjectList<Manager::Activity> * Manager::queuedActivities = new BObjectList<Manager::Activity>[END];

BLocker * Manager::activityLocker = new BLocker(true);

Manager::Manager(SupportedClouds cloud, int maxWorkerThreads)
{
	maxThreads = maxWorkerThreads;
	runningCloud = cloud;
	fileSystem = new LocalFilesystem(cloud, CloudPaths[cloud]);
	fileSystem->Run();
	uploadCommits = BList();
	uploadasyncjobid = BString("");
	uploadCommitLocker = new BLocker(true);
	queuedUploadsLocker = new BLocker(true);
	managerThread = spawn_thread(ManagerThread_static, "manager", B_LOW_PRIORITY, (void *)this);
	resume_thread(managerThread);
}

Manager::~Manager()
{
	//clean up running threads
	send_data(managerThread, KILL_THREAD, NULL, 0);	
	sleep(2);
	kill_thread(managerThread);
	delete uploadCommitLocker;
	delete queuedUploadsLocker;
	delete fileSystem;
}

Manager::Activity::~Activity()
{
	delete sourcePath;
	delete destPath;
}

void Manager::PerformFullUpdate(bool forceFull)
{
		CloudSupport * cs;
		BList items = BList();
		BList updateItems = BList();
		BList localItems = BList();
		
		cs = GetCloudController(runningCloud);
		
		cs->GetChanges(items, forceFull);

		for(int i=0; i < items.CountItems(); i++)
		{
			if (fileSystem->TestLocation((BMessage*)items.ItemAtFast(i)))
				updateItems.AddItem(items.ItemAtFast(i));
		}
		char itemstoupdate[40];
		sprintf(itemstoupdate, "%d remote items to update\n", updateItems.CountItems());
    	LogInfo(itemstoupdate);
		fileSystem->ResolveUnreferencedLocals("", items, localItems, forceFull);
		sprintf(itemstoupdate, "%d local items to update\n", localItems.CountItems());
    	LogInfo(itemstoupdate);
		PullMissing(updateItems);
		fileSystem->SendMissing(localItems);

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
		
		delete cs;

}

void Manager::PerformPolledUpdate()
{
		CloudSupport * cs;

		BList items = BList();
		BList updateItems = BList();
	
		cs = GetCloudController(runningCloud);
	
		int backoff = cs->LongPollForChanges(items);

		for(int i=0; i < items.CountItems(); i++)
		{
			if (fileSystem->TestLocation((BMessage*)items.ItemAtFast(i)))
				updateItems.AddItem(items.ItemAtFast(i));
		}
		char itemstoupdate[40];
		sprintf(itemstoupdate, "%d remote items to update\n", updateItems.CountItems());
    	LogInfo(itemstoupdate);
		PullMissing(updateItems);

		for(int i=0; i < items.CountItems(); i++)
		{
			delete (BMessage*)items.ItemAtFast(i);
		}
		//update the lastLocalSync time
		gSettings.Lock();
		gSettings.lastLocalSync = time(NULL);
		gSettings.SaveSettings();
		gSettings.Unlock();
		delete cs;
		sleep(backoff);
}


bool Manager::PullMissing(BList & items)
{
	bool result = true;
	BPath userpath;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(CloudPaths[runningCloud]);
		if (items.CountItems() > 0) 
		for(int i=0; i < items.CountItems(); i++)
		{	
			BMessage * item = (BMessage*)items.ItemAtFast(i);
			BEntry fsentry;
			BString entryPath = item->GetString("path_display");
			BString entryType = item->GetString(".tag");
			BString fullPath = BString(userpath.Path());
			fullPath.Append(entryPath);
			fsentry = BEntry(fullPath.String());
			fileSystem->AddToIgnoreList(fullPath.String());			
			if (entryType=="file") 
			{
				//TODO: ewww
				time_t sModified = DropboxSupport::ConvertTimestampToSystem(item->GetString("client_modified"));
				QueueDownload(runningCloud, entryPath.String(), fullPath.String(), sModified);
			}
			else
			{
				//start watching folder immediately
				fileSystem->WatchEntry(&fsentry, WATCH_FLAGS);
				fileSystem->RemoveFromIgnoreList(fullPath.String());	
			}
		}
	}
	return result;	
}



int Manager::ManagerThread_static(void *manager)
{
	return ((Manager *)manager)->ManagerThread_func();
}

void Manager::TrySpawnWorker()
{
	thread_info info;
	//wake up manager if it's asleep
	if (get_thread_info(managerThread, &info) == B_OK && info.state == B_THREAD_RECEIVING)
	{
		send_data(managerThread, START_THREAD, NULL, 0);		
	}
}

void Manager::TryWakeUploadManager()
{
	thread_info info;
	//wake up manager if it's asleep
	if (get_thread_info(uploadManagerThread, &info) == B_OK && (info.state == B_THREAD_RECEIVING || info.state == B_THREAD_WAITING))
	{
		send_data(uploadManagerThread, START_THREAD, NULL, 0);		
	}
}

Manager::Activity & Manager::Dequeue(SupportedActivities type)
{
	Activity * activity;
	activityLocker->Lock();
	activity = cloudManager->queuedActivities[type].ItemAt(0);
	cloudManager->queuedActivities[type].RemoveItemAt(0);
	activityLocker->Unlock();	
	return *activity;
}

Manager::Activity & Manager::DequeueUpload()
{
	Activity * activity;
	queuedUploadsLocker->Lock();
	activity = queuedUploads->ItemAt(0);
	queuedUploads->RemoveItemAt(0);
	queuedUploadsLocker->Unlock();	
	return *activity;
}

int Manager::UploadTotal()
{
	int count = 0;
	queuedUploadsLocker->Lock();	
	count = queuedUploads->CountItems();
	queuedUploadsLocker->Unlock();	
	return count;
}

int Manager::ActivityTotal()
{
	int count = 0;
	activityLocker->Lock();	
	for (int i=0; i < END; i++)
	{
		count += queuedActivities[i].CountItems();
	}
	activityLocker->Unlock();	
	return count;
}

int Manager::ActivityTotalByType(SupportedActivities type)
{
	int count = 0;
	activityLocker->Lock();
	count = queuedActivities[type].CountItems();
	activityLocker->Unlock();	
	return count;
}


void Manager::LogUploadCommit(BString * commit)
{
	uploadCommitLocker->Lock();
	uploadCommits.AddItem(commit);
	uploadCommitLocker->Unlock();
}

//Manager Thread should attempt to batch up calls
//otherwise it is not performant - Dropbox locking means most threads
//hang on retry due to namespace lock, so we need to use the batching functions
int Manager::ManagerThread_func()
{
	int32 code;
	thread_id sender;
	thread_info info;
	int sleepActivity = 2; // sleep initially for 2 seconds
	code = receive_data(&sender, NULL, 0);
	
	while (code != KILL_THREAD && code != B_INTERRUPTED)
	{
		// in terms of priority
		//DELETE -- shouldn't impact other instructions
		//CREATE, UPLOAD -- we need to create folders before we can upload to them
		//DOWNLOAD_FOLDER, DOWNLOAD -- downloaded folders may contain paths that we later download to
		//MOVE -- needs to happen AFTER UPLOAD
		
		//we were triggered by something, sleep to see if there have been additional changes before we activate
		//this is just to help facilitate batching, shouldn't matter if we don't get full batches
		
		int currentTotal;	
		int newTotal = ActivityTotal();
		do
		{	
			currentTotal = newTotal;
			sleep(sleepActivity);
			newTotal = ActivityTotal();
		} while(currentTotal != newTotal);
		
		thread_id deleteThread = spawn_thread(DeleteWorkerThread_static, "batch delete", B_LOW_PRIORITY, (void *)this);
		resume_thread(deleteThread);
		
		thread_id createThread = spawn_thread(CreateWorkerThread_static, "batch create", B_LOW_PRIORITY, (void *)this);
		resume_thread(createThread);
			
		//spin up some download workers
		for (int i=0; i < maxThreads; i++)
		{
			thread_id downloadThread = spawn_thread(DownloadWorkerThread_static, "download", B_LOW_PRIORITY, (void *)this);
			resume_thread(downloadThread);
		}
		
		//spin up the upload manager if we can
		if (get_thread_info(uploadManagerThread, &info) != B_OK)
		{
			uploadManagerThread = spawn_thread(UploadThread_static, "batch upload", B_LOW_PRIORITY, (void *)this);
			resume_thread(uploadManagerThread);
		}
		
		thread_id moveThread = spawn_thread(MoveWorkerThread_static, "batch move", B_LOW_PRIORITY, (void *)this);
		resume_thread(moveThread);


		//check if we have any new activities added after doing all this stuff
		if (ActivityTotal() == 0) {
			// wait again
			code = receive_data(&sender, NULL, 0);
		}
	}
	return B_OK;	
}


CloudSupport * Manager::GetCloudController(SupportedClouds cloud)
{
	CloudSupport * cs;
	switch (cloud) 
	{
		case CLOUD_DROPBOX:
			cs = new DropboxSupport();
			break;
		default:
			cs = NULL;
			break;
	}
	
	return cs;
}

int Manager::DeleteWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->DeleteWorkerThread_func();	
}

int Manager::DeleteWorkerThread_func()
{
	if (queuedActivities[DELETE].CountItems()>0)
	{
		CloudSupport *cs;
		BList deletePaths = BList();
		Activity * activity = &Dequeue(DELETE);
		while (activity != NULL)
		{
			BString * path =  new BString(activity->sourcePath->String());
			deletePaths.AddItem(path);
			// need a new path string added otherwise we lose the string when we delete activity
			delete activity;
			activity = &Dequeue(DELETE);
		}
		
		cs = GetCloudController(runningCloud);
		cs->DeletePaths(deletePaths);
		
		//clean up
		for (int i=0; i<deletePaths.CountItems(); i++)
		{
			delete (BString *)deletePaths.ItemAtFast(i);	
		}
		
		delete cs;
	}	
	return B_OK;	
}

int Manager::CreateWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->CreateWorkerThread_func();	
}

int Manager::CreateWorkerThread_func()
{
	if (queuedActivities[CREATE].CountItems()>0)
	{
		CloudSupport *cs;
		BList createPaths = BList();
		Activity * activity = &Dequeue(CREATE);
		while (activity != NULL)
		{
			BString * path =  new BString(activity->sourcePath->String());
			createPaths.AddItem(path);
			// need a new path string added otherwise we lose the string when we delete activity
			delete activity;
			activity = &Dequeue(CREATE);
		}
		
		cs = GetCloudController(runningCloud);
		cs->CreatePaths(createPaths);
		
		//clean up
		for (int i=0; i<createPaths.CountItems(); i++)
		{
			delete (BString *)createPaths.ItemAtFast(i);	
		}
		
		delete cs;
	}	
	return B_OK;		
}

int Manager::MoveWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->MoveWorkerThread_func();	
}

int Manager::MoveWorkerThread_func()
{
	if (queuedActivities[MOVE].CountItems()>0)
	{
		CloudSupport *cs;
		//TODO: yes this sucks
		BList toPaths = BList();
		BList fromPaths = BList();
		Activity * activity = &Dequeue(MOVE);
		while (activity != NULL)
		{
			BString * from =  new BString(activity->sourcePath->String());
			BString * to =  new BString(activity->destPath->String());
			fromPaths.AddItem(from);
			toPaths.AddItem(to);
			// need new path strings added otherwise we lose the string when we delete activity
			delete activity;
			activity = &Dequeue(MOVE);
		}
		
		cs = GetCloudController(runningCloud);
		cs->MovePaths(fromPaths, toPaths);
		
		//clean up
		for (int i=0; i<toPaths.CountItems(); i++)
		{
			delete (BString *)toPaths.ItemAtFast(i);
			delete (BString *)fromPaths.ItemAtFast(i);
		}
		
		delete cs;
	}	
	return B_OK;		
}

int Manager::DownloadWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->DownloadWorkerThread_func();	
}

int Manager::DownloadWorkerThread_func()
{
	if (queuedActivities[DOWNLOAD].CountItems()>0)
	{
		CloudSupport *cs;
		cs = GetCloudController(runningCloud);
		Activity * activity = &Dequeue(DOWNLOAD);
		while (activity != NULL)
		{
			BEntry fsentry;
			cs->Download(activity->sourcePath->String(), activity->destPath->String());		
			fsentry = BEntry(activity->destPath->String());
			fsentry.SetModificationTime(activity->modifiedTime);
			sleep(1);
			fileSystem->WatchEntry(&fsentry, WATCH_FLAGS);
			fileSystem->RemoveFromIgnoreList(activity->destPath->String());	
			delete activity;
			activity = &Dequeue(DOWNLOAD);
		}
		delete cs;
	}	
	return B_OK;		
}

int Manager::UploadWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->UploadWorkerThread_func();	
}

int Manager::UploadWorkerThread_func()
{
	bool processed = false;
	if (UploadTotal()>0)
	{
		CloudSupport *cs;
		cs = GetCloudController(runningCloud);
		Activity * activity = &DequeueUpload();
		while (activity != NULL)
		{
			processed = true;
			BString * commitentry = new BString("");
			cs->Upload(activity->sourcePath->String(), activity->destPath->String(), activity->modifiedTime, activity->fileSize, *commitentry);
			LogUploadCommit(commitentry);
			delete activity;
			activity = &DequeueUpload();
		}	
		delete cs;
		if (processed) TryWakeUploadManager();
	}	
	return B_OK;		
}


int Manager::UploadThread_static(void *manager)
{
	return ((Manager *)manager)->UploadThread_func();
}

// this thread manages the individual upload threads
// doing a bulk upload we need to collect all the session ids
// then send a finish message with all the entries - total mess
int Manager::UploadThread_func()
{
	thread_info info;
	int itemcount = ActivityTotalByType(UPLOAD);
	if (itemcount>0)
	{
		thread_id sender;
		CloudSupport *cs;
		//pull up to 1000 items (Dropbox limit) to a separate list
		if (itemcount > 1000) itemcount = 1000;

		queuedUploadsLocker->Lock();
		queuedUploads = new BObjectList<Activity>(itemcount);
		for (int i=0; i< itemcount; i++)
		{
			queuedUploads->AddItem(&Dequeue(UPLOAD));
		}
		queuedUploadsLocker->Unlock();
		
		// create upload workers
		thread_id * threads = new thread_id[maxThreads];	
		for (int i=0; i< maxThreads; i++)
		{
			threads[i] = spawn_thread(UploadWorkerThread_static, "upload worker", B_LOW_PRIORITY, (void *)this);
			resume_thread(threads[i]);
		}
		
		//TODO: possible hang/race here, needs more work
		do
		{
			// wait until we run out of stuff to do
			receive_data(&sender, NULL, 0);
		}
		while (UploadTotal()>0);
		
		//wait for all threads to terminate
		for (int i=0; i< maxThreads; i++)
		{
			while (get_thread_info(threads[i], &info) == B_OK)
			{
				sleep(2);
			}
		}
		
		//dunno why but seems like we still don't get all the commits in
		while(uploadCommits.CountItems() < itemcount)
		{
			sleep(2);	
		}
		
		cs = GetCloudController(runningCloud);
		uploadCommitLocker->Lock();
		
		//if we've got an existing batch we need to wait for it to complete before we can
		//complete a new one
		while (uploadasyncjobid.Length()!=0)
		{
			BString jobstatus = BString("");
			sleep(2);
			cs->UploadBatchCheck(uploadasyncjobid, jobstatus);
			if (strcmp(jobstatus.String(), "in_progress") != 0)
			{
				uploadasyncjobid = BString("");			
			}
		}
		
		cs->UploadBatch(uploadCommits, uploadasyncjobid);
		for (int i=0; i<uploadCommits.CountItems(); i++)
		{
			delete (BString *)uploadCommits.ItemAtFast(i);	
		}
		uploadCommits.MakeEmpty();
		uploadCommitLocker->Unlock();
		queuedUploadsLocker->Lock();
		delete queuedUploads;
		queuedUploadsLocker->Unlock();
		//kick off manager again as it may have new uploads to deal with it couldn't pick up while we were running
		TrySpawnWorker();
	}	
	return B_OK;
}

void Manager::QueueActivity(Activity ** activity, SupportedActivities type)
{
	activityLocker->Lock();
	cloudManager->queuedActivities[type].AddItem(*activity);
	activityLocker->Unlock();
	TrySpawnWorker();
}

void Manager::QueueUpload(SupportedClouds cloud, const char * file, const char * destfullpath, time_t modified, off_t size)
{
	Activity * activity = new Activity(cloud, UPLOAD, file, destfullpath, modified, size);
	QueueActivity(&activity, UPLOAD);
}

void Manager::QueueDownload(SupportedClouds cloud, const char * file, const char * destfullpath, time_t modified)
{
	Activity * activity = new Activity(cloud, DOWNLOAD, file, destfullpath, modified, 0);
	QueueActivity(&activity, DOWNLOAD);
}

void Manager::QueueCreate(SupportedClouds cloud, const char * destfullpath)
{
	Activity * activity = new Activity(cloud, CREATE, destfullpath, "", 0, 0);
	QueueActivity(&activity, CREATE);
}

void Manager::QueueDownloadFolder(SupportedClouds cloud, const char * path)
{
	Activity * activity = new Activity(cloud, DOWNLOAD_FOLDER, path, "", 0, 0);
	QueueActivity(&activity, DOWNLOAD_FOLDER);
}

void Manager::QueueDelete(SupportedClouds cloud, const char * path)
{
	Activity * activity = new Activity(cloud, DELETE, path, "", 0, 0);
	QueueActivity(&activity, DELETE);
}

void Manager::QueueMove(SupportedClouds cloud, const char * from, const char * to)
{
	Activity * activity = new Activity(cloud, MOVE, from, to, 0, 0);
	QueueActivity(&activity, MOVE);
}

int Manager::DBCheckerThread_static(void *manager)
{
	return ((Manager *)manager)->DBCheckerThread_func();
}

int Manager::DBCheckerThread_func()
{
	while (isRunning)
	{
		PerformPolledUpdate();
	}
	return B_OK;
}

void Manager::StartCloud()
{
	fileSystem->CheckOrCreateRootFolder();
	if (gSettings.authKey == NULL || gSettings.authVerifier == NULL) {
		LogInfo("Please configure Dropbox\n");
		SendNotification("Error", "Please configure Dropbox", true);

	}
	else {
		LogInfo("DropBox configured, performing sync check\n");
		PerformFullUpdate(false);

		LogInfo("Polling for updates\n");
		// run updater in thread
		DBCheckerThread = spawn_thread(DBCheckerThread_static, "Check for remote Dropbox updates", B_LOW_PRIORITY, (void*)this);
		if ((DBCheckerThread) < B_OK) {
			LogInfoLine("Failed to start Dropbox Checker Thread");	
		} else {
			resume_thread(DBCheckerThread);	
		}
		fileSystem->WatchDirectories();	
		SendNotification(" Ready", "Watching for updates", false);
	}
}

void Manager::StopCloud()
{
	exit_thread(DBCheckerThread);
}


void Manager::HandleNodeEvent(BMessage *msg)
{
	fileSystem->HandleNodeEvent(msg);
}
