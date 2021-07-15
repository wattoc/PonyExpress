#include "Manager.h"

#include <FindDirectory.h>

#include "DropboxSupport.h"
#include "LocalFilesystem.h"
#include "Globals.h"

#define KILL_THREAD 'UDIE'
#define START_THREAD 'STRT'

static const char * CloudPaths[] = {"", "Dropbox/"};

Manager::Manager(SupportedClouds cloud, int maxWorkerThreads)
{
	fErrorCount = 0;
	fMaxThreads = maxWorkerThreads;
	fRunningCloud = cloud;
	fFileSystem = new LocalFilesystem(this, CloudPaths[cloud]);
	fFileSystem->Run();
	fUploadCommits = BList();
	fUploadAsyncJobId = BString("");
	fUploadCommitLocker = new BLocker(true);
	fQueuedUploadsLocker = new BLocker(true);
	fActivityLocker = new BLocker(true);
	fQueuedActivities = new BObjectList<Manager::Activity>();
	fManagerThread = spawn_thread(ManagerThread_static, "manager", B_LOW_PRIORITY, (void *)this);
	fUploadManagerThread = -1;
	fCreateThread = -1;
	fDeleteThread = -1;
	fMoveThread = -1;
	fDownloadThreads = new thread_id[fMaxThreads];	
	for (int i=0; i < fMaxThreads; i++)
	{
		fDownloadThreads[i] = -1;
	}
	resume_thread(fManagerThread);
}

Manager::~Manager()
{
	//clean up running threads
	send_data(fManagerThread, KILL_THREAD, NULL, 0);	
	sleep(2);
	kill_thread(fManagerThread);
	delete[] fDownloadThreads;
	delete fUploadCommitLocker;
	delete fQueuedUploadsLocker;
	delete fFileSystem;
}

Manager::Activity::~Activity()
{

}

void Manager::PerformFullUpdate(bool forceFull)
{
		CloudSupport * cs;
		bool hasMore = true;
		
		cs = _GetCloudController();
		gGlobals.SetActivity(M_ACTIVITY_UPDOWN);
		while (hasMore) 
		{
			BList items = BList();
			BList updateItems = BList();
			BList localItems = BList();
			size_t cursorLength = 0;
			gSettings.Lock();
			cursorLength = gSettings.cursor.Length();
			gSettings.Unlock();
			
			if (cursorLength==0)
			{				
				cs->ListFiles("", true, items, hasMore);
			}
			else
			{
				cs->GetFolder(items, hasMore);
			}
			gGlobals.SetActivity(M_ACTIVITY_NONE);
			for(int i=0; i < items.CountItems(); i++)
			{
				if (fFileSystem->TestLocation((BMessage*)items.ItemAtFast(i)))
					updateItems.AddItem(items.ItemAtFast(i));
			}
			fFileSystem->ResolveUnreferencedLocals("", items, localItems, forceFull);
			PullMissing(updateItems);
			fFileSystem->SendMissing(localItems);
	
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
		delete cs;

}

void Manager::PerformPolledUpdate()
{
		CloudSupport * cs;
		int backoff = 0;
		bool changes = false;
		BString cursor;
		cs = _GetCloudController();
		gSettings.Lock();
		cursor = gSettings.cursor;
		gSettings.Unlock();
		if (cursor.Length()==0) 
		{
			changes = true; // do a get later
		} else {
			changes = cs->LongPollForChanges(backoff);
		}
		if (backoff < 0) {
			backoff = 30;
			NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());
		}
		else 
		{
			if (changes) 
			{
				bool hasMore = true;
				while (hasMore)
				{
					hasMore = false;
					BList items = BList();
					BList updateItems = BList();
					// list or longpoll
					gSettings.Lock();
					cursor = gSettings.cursor; //may have been reset during longpoll
					gSettings.Unlock();	
					if (cursor.Length() == 0)
					{		
						if (!cs->ListFiles("", true, items, hasMore))
						{
							NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());			
						}
					} else
					{
						if (!cs->GetFolder(items, hasMore))
						{
							NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());
						}
					}
					for(int i=0; i < items.CountItems(); i++)
					{
						if (fFileSystem->TestLocation((BMessage*)items.ItemAtFast(i)))
							updateItems.AddItem(items.ItemAtFast(i));
					}
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
				}
			}
		}
		delete cs;
		sleep(backoff);
		
}


bool Manager::PullMissing(BList & items)
{
	bool result = true;
	BPath userpath;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		gGlobals.SetActivity(M_ACTIVITY_UPDOWN);

		userpath.Append(CloudPaths[fRunningCloud]);
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
			fFileSystem->AddToIgnoreList(fullPath.String());			
			if (entryType=="file") 
			{
				//TODO: ewww
				time_t sModified = DropboxSupport::ConvertTimestampToSystem(item->GetString("client_modified"));
				QueueDownload(entryPath.String(), fullPath.String(), sModified);
			}
			else
			{
				//start watching folder immediately
				fFileSystem->WatchEntry(&fsentry, WATCH_FLAGS);
				fFileSystem->RemoveFromIgnoreList(fullPath.String());	
			}
		}
	}
	gGlobals.SetActivity(M_ACTIVITY_NONE);

	return result;	
}

status_t Manager::ManagerThread_static(void *manager)
{
	return ((Manager *)manager)->ManagerThread_func();
}

void Manager::TrySpawnWorker()
{
	thread_info info;
	//wake up manager if it's asleep
	if (get_thread_info(fManagerThread, &info) == B_OK && info.state == B_THREAD_RECEIVING)
	{
		send_data(fManagerThread, START_THREAD, NULL, 0);		
	}
}

Manager::Activity * Manager::Dequeue(SupportedActivities type)
{
	Activity * activity = NULL;
	bool match = false;
	fActivityLocker->Lock();
	for (int i = 0; i< fQueuedActivities->CountItems(); i++)
	{
		activity = fQueuedActivities->ItemAt(i);
		if (activity->action == type)
		{
			match = true;
			fQueuedActivities->RemoveItemAt(i);
			break;
		}
	}
	fActivityLocker->Unlock();
	if (match)
		return activity;
	return NULL;
}

Manager::Activity * Manager::DequeueUpload()
{
	Activity * activity = NULL;
	fQueuedUploadsLocker->Lock();
	if (fQueuedUploads->CountItems() > 0) 
	{
		activity = fQueuedUploads->ItemAt(0);
		fQueuedUploads->RemoveItemAt(0);
	}
	fQueuedUploadsLocker->Unlock();	
	return activity;
}

int Manager::UploadTotal()
{
	int count = 0;
	fQueuedUploadsLocker->Lock();	
	count = fQueuedUploads->CountItems();
	fQueuedUploadsLocker->Unlock();	
	return count;
}

int Manager::ActivityTotal()
{
	int count = 0;
	fActivityLocker->Lock();	
	count = fQueuedActivities->CountItems();
	fActivityLocker->Unlock();	
	return count;
}

int Manager::ActivityTotalByType(SupportedActivities type)
{
	int count = 0;
	fActivityLocker->Lock();
	for (int i=0; i < fQueuedActivities->CountItems(); i++)
	{
		if (fQueuedActivities->ItemAt(i)->action==type)
			count++;
	}
	fActivityLocker->Unlock();	
	return count;
}


void Manager::LogUploadCommit(BString * commit)
{
	fUploadCommitLocker->Lock();
	fUploadCommits.AddItem(commit);
	fUploadCommitLocker->Unlock();
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
		
		if (ActivityTotalByType(DELETE)>0 && get_thread_info(fDeleteThread, &info) != B_OK)
		{
			fDeleteThread = spawn_thread(DeleteWorkerThread_static, "batch delete", B_LOW_PRIORITY, (void *)this);
			resume_thread(fDeleteThread);
		} 
		else 
		{
			if (ActivityTotalByType(CREATE)>0 && get_thread_info(fCreateThread, &info) != B_OK)
			{
				fCreateThread = spawn_thread(CreateWorkerThread_static, "batch create", B_LOW_PRIORITY, (void *)this);
				resume_thread(fCreateThread);
			}
			else
			{
				if (ActivityTotalByType(DOWNLOAD)>0) {
					//spin up some download workers if there aren't any already
					for (int i=0; i < fMaxThreads; i++)
					{
						if (fDownloadThreads[i] == -1 || get_thread_info(fDownloadThreads[i], &info) != B_OK)
						{
							fDownloadThreads[i] = spawn_thread(DownloadWorkerThread_static, "download", B_LOW_PRIORITY, (void *)this);
							resume_thread(fDownloadThreads[i]);
						}
					}
				}
				//spin up the upload manager if we can
				if (ActivityTotalByType(UPLOAD)>0 && get_thread_info(fUploadManagerThread, &info) != B_OK)
				{
					fUploadManagerThread = spawn_thread(UploadThread_static, "batch upload", B_LOW_PRIORITY, (void *)this);
					resume_thread(fUploadManagerThread);
				}
				else
				{
					if (ActivityTotalByType(MOVE)>0 && get_thread_info(fMoveThread, &info) != B_OK)
					{
						fMoveThread = spawn_thread(MoveWorkerThread_static, "batch move", B_LOW_PRIORITY, (void *)this);
						resume_thread(fMoveThread);
					}
				}
			}
		}
		if (fErrorCount >= MAX_ERRORS)
		{
			gGlobals.SetActivity(M_ACTIVITY_ERROR);
			gGlobals.SendNotification("Processing Suspended", "Too many errors, waiting before trying again", false);
			sleep(30);
		}

		//check if we have any new activities added after doing all this stuff
		if (ActivityTotal() == 0) {
			// wait again
			code = receive_data(&sender, NULL, 0);
		}
	}
	return B_OK;	
}


CloudSupport * Manager::_GetCloudController()
{
	CloudSupport * cs;
	switch (fRunningCloud) 
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

status_t Manager::DeleteWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->DeleteWorkerThread_func();	
}

int Manager::DeleteWorkerThread_func()
{
	if (ActivityTotalByType(DELETE)>0)
	{
		CloudSupport *cs;
		BList deletePaths = BList();
		Activity * activity = Dequeue(DELETE);
		while (activity != NULL)
		{
			BString * path =  new BString(activity->sourcePath.String());
			deletePaths.AddItem(path);
			// need a new path string added otherwise we lose the string when we delete activity		
			Activity * nextActivity = activity->next;
			delete activity;		
			if (nextActivity != NULL) QueueActivity(nextActivity, nextActivity->action, false);
			
			activity = Dequeue(DELETE);
		}
		
		cs = _GetCloudController();
		if (!cs->DeletePaths(deletePaths))
		{
			//re-add to queue
			for (int i=0; i<deletePaths.CountItems(); i++)
			{
				QueueDelete(((BString *)deletePaths.ItemAtFast(i))->String());
				delete (BString *)deletePaths.ItemAtFast(i);				
			}
			NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());
		} else {	
			//clean up
			for (int i=0; i<deletePaths.CountItems(); i++)
			{
				delete (BString *)deletePaths.ItemAtFast(i);	
			}
			fErrorCount = 0;			
		}
		delete cs;
	}	
	return B_OK;
}

status_t Manager::CreateWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->CreateWorkerThread_func();	
}

int Manager::CreateWorkerThread_func()
{
	if (ActivityTotalByType(CREATE)>0)
	{
		CloudSupport *cs;
		BList createPaths = BList();
		Activity * activity = Dequeue(CREATE);
		while (activity != NULL)
		{
			BString * path =  new BString(activity->sourcePath.String());
			createPaths.AddItem(path);
			// need a new path string added otherwise we lose the string when we delete activity
			Activity * nextActivity = activity->next;
			delete activity;		
			if (nextActivity != NULL) QueueActivity(nextActivity, nextActivity->action, false);
			activity = Dequeue(CREATE);
		}
		
		cs = _GetCloudController();
		if (!cs->CreatePaths(createPaths))
		{
			//re-add to queue
			for (int i=0; i<createPaths.CountItems(); i++)
			{
				QueueCreate(((BString *)createPaths.ItemAtFast(i))->String());
				delete (BString *)createPaths.ItemAtFast(i);				
			}
			NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());
		} else {			
			//clean up
			for (int i=0; i<createPaths.CountItems(); i++)
			{
				delete (BString *)createPaths.ItemAtFast(i);	
			}
			fErrorCount = 0;

		}
		delete cs;
	}	
	return B_OK;		
}

status_t Manager::MoveWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->MoveWorkerThread_func();	
}

int Manager::MoveWorkerThread_func()
{
	if (ActivityTotalByType(MOVE)>0)
	{
		CloudSupport *cs;
		//TODO: yes this sucks
		BList toPaths = BList();
		BList fromPaths = BList();
		Activity * activity = Dequeue(MOVE);
		while (activity != NULL)
		{
			BString * from =  new BString(activity->sourcePath.String());
			BString * to =  new BString(activity->destPath.String());
			fromPaths.AddItem(from);
			toPaths.AddItem(to);
			// need new path strings added otherwise we lose the string when we delete activity
			Activity * nextActivity = activity->next;
			delete activity;		
			if (nextActivity != NULL) QueueActivity(nextActivity, nextActivity->action, false);
			activity = Dequeue(MOVE);
		}
		
		cs = _GetCloudController();
		if (!cs->MovePaths(fromPaths, toPaths)) 
		{
			for (int i=0; i<toPaths.CountItems(); i++)
			{
				BString * from = (BString *)fromPaths.ItemAtFast(i);
				BString * to = (BString *)toPaths.ItemAtFast(i);
				QueueMove(from->String(), to->String());				
				delete from;
				delete to;
			}
			NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());
		} else {	
			//clean up
			for (int i=0; i<toPaths.CountItems(); i++)
			{
				delete (BString *)toPaths.ItemAtFast(i);
				delete (BString *)fromPaths.ItemAtFast(i);
			}
			fErrorCount = 0;
		}
		delete cs;
	}	
	return B_OK;		
}

status_t Manager::DownloadWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->DownloadWorkerThread_func();	
}

int Manager::DownloadWorkerThread_func()
{
	if (ActivityTotalByType(DOWNLOAD)>0)
	{
		CloudSupport *cs;
		cs = _GetCloudController();
		Activity * activity = Dequeue(DOWNLOAD);
		gGlobals.SetActivity(M_ACTIVITY_DOWN);

		while (activity != NULL && fErrorCount < MAX_ERRORS)
		{
			BEntry fsentry;
			if (cs->Download(activity->sourcePath.String(), activity->destPath.String()))
			{
				fsentry = BEntry(activity->destPath.String());
				fsentry.SetModificationTime(activity->modifiedTime);
				sleep(1);
				fFileSystem->WatchEntry(&fsentry, WATCH_FLAGS);
				fFileSystem->RemoveFromIgnoreList(activity->destPath.String());	
				Activity * nextActivity = activity->next;
				delete activity;		
				if (nextActivity != NULL) QueueActivity(nextActivity, nextActivity->action, false);
				activity = Dequeue(DOWNLOAD);
				fErrorCount = 0;
			} else {
				NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());
				sleep(5); //sleep a bit before retry
			}
		}
		if (fErrorCount >= MAX_ERRORS)
		{
			//requeue failed activity
			QueueActivity(activity, DOWNLOAD);
		}	
		delete cs;
	}
	gGlobals.SetActivity(M_ACTIVITY_NONE);

	return B_OK;		
}

status_t Manager::UploadWorkerThread_static(void *manager)
{
	return ((Manager *)manager)->UploadWorkerThread_func();	
}

int Manager::UploadWorkerThread_func()
{
	if (UploadTotal()>0)
	{
		int localErrors = 0;
		CloudSupport *cs;
		cs = _GetCloudController();
		Activity * activity = DequeueUpload();
		while (activity != NULL && fErrorCount < MAX_ERRORS && localErrors < MAX_ERRORS)
		{
			BEntry entry = BEntry(activity->sourcePath.String());
			BString * commitentry = new BString("");
			if (!entry.Exists() || cs->Upload(activity->sourcePath.String(), activity->destPath.String(), activity->modifiedTime, activity->fileSize, *commitentry))
			{
				LogUploadCommit(commitentry);
				Activity * nextActivity = activity->next;
				delete activity;	
				if (nextActivity != NULL) QueueActivity(nextActivity, nextActivity->action);
				activity = DequeueUpload();
				fErrorCount = 0;
				localErrors = 0;
			} else {
				localErrors++;
				NotifyError(cs->GetLastError(), cs->GetLastErrorMessage());
				sleep(5); //sleep a bit before retry
			}
		}
		if (fErrorCount >= MAX_ERRORS || localErrors >= MAX_ERRORS)
		{
			//check if file exists - if so requeue failed activity
			BEntry entry = BEntry(activity->sourcePath.String());
			if (entry.Exists()) {
				QueueActivity(activity, activity->action, false);	
			}
		}		
		delete cs;
	}	
	return B_OK;		
}


status_t Manager::UploadThread_static(void *manager)
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
		gGlobals.SetActivity(M_ACTIVITY_UP);

		CloudSupport *cs;
		//pull up to 1000 items (Dropbox limit) to a separate list
		if (itemcount > 1000) itemcount = 1000;

		fQueuedUploadsLocker->Lock();
		fQueuedUploads = new BObjectList<Activity>(itemcount);
		for (int i=0; i< itemcount; i++)
		{
			fQueuedUploads->AddItem(Dequeue(UPLOAD));
		}
		fQueuedUploadsLocker->Unlock();
		
		// create upload workers
		thread_id * threads = new thread_id[fMaxThreads];	
		for (int i=0; i< fMaxThreads; i++)
		{
			threads[i] = spawn_thread(UploadWorkerThread_static, "upload worker", B_LOW_PRIORITY, (void *)this);
			resume_thread(threads[i]);
		}
		
		//TODO: possible hang/race here, needs more work
		//do
		//{
			// wait until we run out of stuff to do
		//	receive_data(&sender, NULL, 0);
		//}
		//while (UploadTotal()>0 && fErrorCount < MAX_ERRORS);
		
		//wait for all threads to terminate
		for (int i=0; i< fMaxThreads; i++)
		{
			while (get_thread_info(threads[i], &info) == B_OK)
			{
				sleep(2);
			}
		}
		

		cs = _GetCloudController();
		fUploadCommitLocker->Lock();
		
		//if we've got an existing batch we need to wait for it to complete before we can
		//complete a new one
		while (fUploadAsyncJobId.Length()!=0)
		{
			BString jobstatus = BString("");
			sleep(2);
			cs->UploadBatchCheck(fUploadAsyncJobId, jobstatus);
			if (strcmp(jobstatus.String(), "in_progress") != 0)
			{
				fUploadAsyncJobId = BString("");	
			}
		}
		
		cs->UploadBatch(fUploadCommits, fUploadAsyncJobId);
		for (int i=0; i<fUploadCommits.CountItems(); i++)
		{
			delete (BString *)fUploadCommits.ItemAtFast(i);	
		}
		fUploadCommits.MakeEmpty();
		fUploadCommitLocker->Unlock();
		fQueuedUploadsLocker->Lock();

		//re-add remaining queuedUploads to ActivityQueue
		for (int i=0; i< fQueuedUploads->CountItems(); i++)
		{
			QueueActivity(fQueuedUploads->ItemAt(i), UPLOAD, false);
		}

		delete fQueuedUploads;
		delete[] threads;
		fQueuedUploadsLocker->Unlock();
		gGlobals.SetActivity(M_ACTIVITY_NONE);

		//kick off manager again as it may have new uploads to deal with it couldn't pick up while we were running
		TrySpawnWorker();
	}	
	return B_OK;
}

void Manager::QueueActivity(Activity *activity, SupportedActivities type, bool triggerWorker)
{
	Activity * curactivity;
	bool found = false;
	fActivityLocker->Lock();
	for (int i=0; i< fQueuedActivities->CountItems(); i++)
	{
		curactivity = fQueuedActivities->ItemAt(i);
		if (strcmp(curactivity->sourcePath, activity->sourcePath) == 0)
		{
			curactivity->AddNext(activity);	
			found = true;
			break;
		}
	}
	if (!found)
		fQueuedActivities->AddItem(activity);
	fActivityLocker->Unlock();
	TrySpawnWorker();
}

void Manager::QueueUpload(const char * file, const char * destfullpath, time_t modified, off_t size)
{
	Activity * activity = new Activity(UPLOAD, file, destfullpath, modified, size);
	QueueActivity(activity, UPLOAD);
}

void Manager::QueueDownload(const char * file, const char * destfullpath, time_t modified)
{
	Activity * activity = new Activity(DOWNLOAD, file, destfullpath, modified, 0);
	QueueActivity(activity, DOWNLOAD);
}

void Manager::QueueCreate(const char * destfullpath)
{
	Activity * activity = new Activity(CREATE, destfullpath, "", 0, 0);
	QueueActivity(activity, CREATE);
}

void Manager::QueueDownloadFolder(const char * path)
{
	Activity * activity = new Activity(DOWNLOAD_FOLDER, path, "", 0, 0);
	QueueActivity(activity, DOWNLOAD_FOLDER);
}

void Manager::QueueDelete(const char * path)
{
	Activity * activity = new Activity(DELETE, path, "", 0, 0);
	QueueActivity(activity, DELETE);
}

void Manager::QueueMove(const char * from, const char * to)
{
	Activity * activity = new Activity(MOVE, from, to, 0, 0);
	QueueActivity(activity, MOVE);
}

status_t Manager::CheckerThread_static(void *manager)
{
	return ((Manager *)manager)->CheckerThread_func();
}

int Manager::CheckerThread_func()
{
	while (gGlobals.gIsRunning && (gSettings.authKey.Length() == 0 || gSettings.authVerifier.Length() == 0))
	{
		gGlobals.SetActivity(M_ACTIVITY_ERROR);
		//wait for setup to be completed
		sleep(5);
	}
	gGlobals.SetActivity(M_ACTIVITY_NONE);
	if (gGlobals.gIsRunning)
	{
		PerformFullUpdate(false);
		fFileSystem->WatchDirectories();	
		gGlobals.SendNotification("Ready", "Watching for updates", false);
	}	
	
	while (gGlobals.gIsRunning)
	{
		PerformPolledUpdate();
		if (fErrorCount >= MAX_ERRORS)
		{
			gGlobals.SendNotification("Remote Checker", "Too many errors, temporarily suspending polling", false);
			gGlobals.SetActivity(M_ACTIVITY_ERROR);
			sleep(30);
			//clear errors
			fErrorCount = 0;
		}
	}
	return B_OK;
}

void Manager::StartCloud()
{
	fErrorCount = 0;
	fFileSystem->CheckOrCreateRootFolder();
	if (gSettings.authKey == NULL || gSettings.authVerifier == NULL) {
		gGlobals.SendNotification("Error", "Please configure Dropbox", true);
		gGlobals.SetActivity(M_ACTIVITY_ERROR);
	}
	// run updater in thread
	fCheckerThread = spawn_thread(CheckerThread_static, "Check for remote Dropbox updates", B_LOW_PRIORITY, (void*)this);
	if ((fCheckerThread) < B_OK) {
		gGlobals.SendNotification("Error", "Failed to start Remote Checker thread", true);
		gGlobals.SetActivity(M_ACTIVITY_ERROR);
	} else {
		resume_thread(fCheckerThread);	
	}
}

void Manager::StopCloud()
{
	exit_thread(fCheckerThread);
}


void Manager::HandleNodeEvent(BMessage *msg)
{
	fFileSystem->HandleNodeEvent(msg);
}

void Manager::NotifyError(const char * error, const char * summary)
{
	BString errormessage = BString("Error: ");
	errormessage.Append(error);
	errormessage.Append("\n");
	errormessage.Append(summary);
	gGlobals.SetActivity(M_ACTIVITY_ERROR);
	gGlobals.SendNotification("Error", errormessage.String(), true);
	fErrorCount++;
}
