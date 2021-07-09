#include "LocalFilesystem.h"

#include <Application.h>
#include <Message.h>
#include <FindDirectory.h>
#include <NodeMonitor.h>
#include <String.h>

#include "DropboxSupport.h"
#include "Globals.h"

BList LocalFilesystem::sTrackedEntries = BList();
BList LocalFilesystem::sIgnoredEntries = BList();
BLocker* LocalFilesystem::sIgnoredEntriesLocker  = new BLocker(true);

void LocalFilesystem::CheckOrCreateRootFolder()
{
	BPath userpath;
	BDirectory directory;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(fCloudRootPath);
		directory.CreateDirectory(userpath.Path(), NULL);
	}
}

void LocalFilesystem::RecursiveDelete(const char *path)
{
	BPath userpath;
	BDirectory directory;
	BEntry fsentry;
	node_ref ref;
	directory.SetTo(path);
	while (directory.GetNextEntry(&fsentry) == B_OK)
	{	
		fsentry.GetNodeRef(&ref);
		StopWatchingNodeRef(&ref);
		if (fsentry.IsDirectory()) {
			fsentry.GetPath(&userpath);
			RecursiveDelete(userpath.Path());	
		}
		fsentry.Remove();		
	}
}

bool LocalFilesystem::TestLocation(BMessage * dbMessage) 
{
	BPath userpath;
	BDirectory directory;
	BEntry fsentry;
	bool needsUpdate = false;
	BString entryPath = NULL;
	BString entryType = NULL;
	
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(fCloudRootPath);
		directory.SetTo(userpath.Path());
		entryType=dbMessage->GetString(".tag");
		entryPath=dbMessage->GetString("path_display");
		entryPath.RemoveFirst("/");
		if (entryType=="file") {
			//look for file on filesystem
			fsentry = BEntry(&directory, entryPath.String(), true);
			if (fsentry.Exists()) {
				time_t modtime, sModified;
				off_t size;
				off_t sSize = 0;
				//compare size, modified date
				BNode node (&fsentry);
				node.GetModificationTime(&modtime);
				node.GetSize(&size);
				sSize=(off_t)dbMessage->GetDouble("size",0); //this shouldn't be a double?
				//TODO: eww
				sModified = DropboxSupport::ConvertTimestampToSystem(dbMessage->GetString("client_modified"));
				needsUpdate = (sSize != size || sModified > modtime);			
			} else {
				needsUpdate = true;	
			}
		}
		else if (entryType=="folder")
		{
			//can just create a folder now if it's missing
			fsentry = BEntry(&directory, entryPath.String(), true);
			if (!fsentry.Exists()) {
				fsentry.GetPath(&userpath);
				directory.CreateDirectory(userpath.Path(), NULL);
				// flag that we can potentially pull down this entire directory
				// in a zip file
				//also want to watch it after downloads are complete, so need this on
				needsUpdate = true;
			} else {
				needsUpdate = false;
			}
		}
		else if (entryType=="deleted")
		{
			//attempt to remove entry if it's present
			fsentry = BEntry(&directory, entryPath.String(), true);
			if (fsentry.Exists()) {
				if (fsentry.IsDirectory()) {
					node_ref ref;
					//directory requires recursive delete...	
					fsentry.GetPath(&userpath);
					fsentry.GetNodeRef(&ref);
					StopWatchingNodeRef(&ref);
					RecursiveDelete(userpath.Path());
				}
				fsentry.Remove();
			}
			needsUpdate = false;	
		}
		else {
			gGlobals.SendNotification("Filesystem","Unhandled entryType encountered", true);
		}	
	}
	return needsUpdate;	
}

//
bool LocalFilesystem::ResolveUnreferencedLocals(const char * leaf, BList & remote, BList & local, bool forceFull) 
{
	BPath userpath;
	BDirectory directory;
	BEntry fsentry;
	
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(fCloudRootPath);
		BString hardRoot = userpath.Path();
		userpath.Append(leaf);
		BString fullRoot = userpath.Path();
		directory.SetTo(userpath.Path());
		while (directory.GetNextEntry(&fsentry) == B_OK)
		{
			time_t localModified;
			time_t lastSync = gSettings.lastLocalSync;
			fsentry.GetPath(&userpath);
			fsentry.GetModificationTime(&localModified);
			BString strippedPath = BString(userpath.Path());
			
			strippedPath.RemoveFirst(hardRoot);
			
			if ((!IsInRemoteList(strippedPath.String(), localModified, remote) && (lastSync> 0 && localModified> lastSync)) || lastSync == 0 || forceFull)
			{
				// add pending upload
				BMessage * pending = new BMessage();
				pending->AddString(".tag", fsentry.IsDirectory()? "folder" : "file");
				pending->AddString("path_display", strippedPath);
				local.AddItem(pending);
			}
			if (fsentry.IsDirectory())
			{
				strippedPath.RemoveFirst("/");
				//recurse
				ResolveUnreferencedLocals(strippedPath.String(), remote, local, forceFull);
			}	
		}
		return true;
	}
	
	return false;
}

bool LocalFilesystem::SendMissing(BList & items)
{
	bool result = true;
	BPath userpath;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(fCloudRootPath);
		for(int i=0; i < items.CountItems(); i++)
		{	
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
			
			if (entryType=="file") {
				fManager->QueueUpload(fullPath.String(), entryPath.String(), sModified, sSize);
			} else if (entryType=="folder") {
				fManager->QueueCreate(entryPath.String());
			}
			// we don't support DIRs by Zip yet
		}
		//update the lastLocalSync time
		gSettings.Lock();
		gSettings.lastLocalSync = time(NULL);
		gSettings.SaveSettings();
		gSettings.Unlock();
	}
	return result;	
}

bool LocalFilesystem::IsInRemoteList(const char * path, time_t localModified, BList & remote)
{
		for(int i=0; i < remote.CountItems(); i++)
		{
			BMessage * ref = (BMessage*)remote.ItemAtFast(i);
			BString remotePath = ref->GetString("path_display");
			
			if (remotePath==path)
			{
				BString timestamp = ref->GetString("client_modified");
				if (timestamp == "")
					return true;
				time_t sModified = DropboxSupport::ConvertTimestampToSystem(timestamp);
				return sModified <= localModified;
			}
		}
		return false;
}

void LocalFilesystem::StopWatchingNodeRef(node_ref *nref)
{
	watch_node(nref, B_STOP_WATCHING, BMessenger(this));
	LocalFilesystem::RemoveTrackedEntry(nref);
}

void LocalFilesystem::WatchEntry(BEntry *entry, uint32 flags)
{
	node_ref nref;
	entry->GetNodeRef(&nref);
	if (flags == B_STOP_WATCHING) 
	{
		RemoveTrackedEntry(&nref);
	}
	else
	{
		trackeddata * td = new trackeddata();
		td->nref = nref;
		entry->GetPath(td->path);
		sTrackedEntries.AddItem(td);
	}
	watch_node(&nref, flags, BMessenger(this));
}

void LocalFilesystem::RecursivelyWatchDirectory(const char * fullPath, uint32 flags)
{
	BDirectory directory;
	BEntry entry;
	
	directory.SetTo(fullPath);
	//add node watcher to this dir
	directory.GetEntry(&entry);
	WatchEntry(&entry, flags);

	//add node watchers to any child dirs
	while (directory.GetNextEntry(&entry) == B_OK)
	{
		if (entry.IsDirectory())
		{
			BPath userpath;
			entry.GetPath(&userpath);
			RecursivelyWatchDirectory(userpath.Path(), flags);
		} else {
			WatchEntry(&entry, flags);	
		}
	}
}

void LocalFilesystem::RecursiveAddToCloud(const char *fullPath) 
{
	BDirectory directory;
	BEntry entry;
	BString dbpath;
	directory.SetTo(fullPath);
	directory.GetEntry(&entry);
	while (directory.GetNextEntry(&entry) == B_OK)
	{
		BPath userpath;
		entry.GetPath(&userpath);
		dbpath = BString(userpath.Path());
		ConvertFullPathToCloudRelativePath(dbpath);
		if (entry.IsDirectory())
		{
			fManager->QueueCreate(dbpath);
			RecursiveAddToCloud(userpath.Path());
		}
		else	
		{
			off_t size;
			time_t modified;
			entry.GetModificationTime(&modified);
			entry.GetSize(&size);
			
			fManager->QueueUpload(userpath.Path(), dbpath, modified, size);
		}
		WatchEntry(&entry, WATCH_FLAGS);	
	}
	
}

void LocalFilesystem::ConvertFullPathToCloudRelativePath(BString &full)
{
	BString dbpath = BString(fCloudRootPath);
	ApplyFullPathToRelativeBasePath(dbpath);
	full.RemoveFirst(dbpath);
}

void LocalFilesystem::ApplyFullPathToRelativeBasePath(BString &relative)
{
	BPath userpath;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(relative.String());
		relative = BString(userpath.Path());
	}
}

void LocalFilesystem::WatchDirectories()
{
		BString path = BString(fCloudRootPath);
		LocalFilesystem::ApplyFullPathToRelativeBasePath(path);
		LocalFilesystem::RecursivelyWatchDirectory(path, WATCH_FLAGS);	
}

LocalFilesystem::trackeddata * LocalFilesystem::FindTrackedEntry(node_ref find) 
{	
	for (int i=0; i<sTrackedEntries.CountItems(); i++)
	{
		trackeddata * file = (trackeddata *)sTrackedEntries.ItemAt(i);
		if (find.node == file->nref.node && find.device == file->nref.device)
			return file;
	}
	return NULL;
}

void LocalFilesystem::RemoveTrackedEntriesForPath(const char *fullPath)
{
	for (int i=0; i<sTrackedEntries.CountItems(); i++)
	{
		trackeddata *file = (trackeddata *)sTrackedEntries.ItemAt(i);
		BString path = BString(file->path->Path());
		if (path.StartsWith(fullPath)) 
		{
			watch_node(&file->nref, B_STOP_WATCHING, BMessenger(this));
			sTrackedEntries.RemoveItem(i);
			delete file;
		}
	}
	
}
void LocalFilesystem::RemoveTrackedEntry(node_ref * find) 
{	
	for (int i=0; i<sTrackedEntries.CountItems(); i++)
	{
		trackeddata *file = (trackeddata *)sTrackedEntries.ItemAt(i);
		
		if (find->node == file->nref.node && find->device == file->nref.device) 
		{
			sTrackedEntries.RemoveItem(i);
			delete file;
		}
	}
}

void LocalFilesystem::AddToIgnoreList(const char * fullPath)
{
	BString * ignoring = new BString(fullPath);
	sIgnoredEntriesLocker->Lock();
	sIgnoredEntries.AddItem(ignoring);
	sIgnoredEntriesLocker->Unlock();
}

void LocalFilesystem::RemoveFromIgnoreList(const char * fullPath)
{
	for (int i=0; i<sIgnoredEntries.CountItems(); i++)
	{
		BString *ignoring = (BString *)sIgnoredEntries.ItemAt(i);
		
		if (strcmp(ignoring->String(), fullPath) == 0) 
		{
			sIgnoredEntriesLocker->Lock();
			sIgnoredEntries.RemoveItem(i);
			sIgnoredEntriesLocker->Unlock();

			delete ignoring;
		}
	}
}

bool LocalFilesystem::IsInIgnoredList(const char *fullPath)
{
	for (int i=0; i<sIgnoredEntries.CountItems(); i++)
	{
		BString *ignoring = (BString *)sIgnoredEntries.ItemAt(i);
		
		if (strcmp(ignoring->String(), fullPath) == 0) 
		{
			return true;
		}
	}
	
	return false;
}

void LocalFilesystem::HandleCreated(BMessage * msg)
{
	entry_ref ref;
	BPath path;
	const char * name;
	msg->FindInt32("device", &ref.device);
	msg->FindInt64("directory", &ref.directory);
	msg->FindString("name", &name);
	ref.set_name(name);
	// doesn't support symlinks currently, so no "copying" folders from Desktop
	BEntry new_file = BEntry(&ref);
	new_file.GetPath(&path);
	BString dbpath = BString(path.Path());
	BString fullpath = BString(path.Path());
	ConvertFullPathToCloudRelativePath(dbpath);
	
	if (new_file.IsDirectory())
	{
		if (!IsInIgnoredList(path.Path())) {
	 		fManager->QueueCreate(dbpath);
			//need to recursively upload folder contents
			RecursiveAddToCloud(path.Path());
			WatchEntry(&new_file, WATCH_FLAGS);
		}
	}
	else
	{
		off_t size;
		time_t modified;
		new_file.GetModificationTime(&modified);
		new_file.GetSize(&size);
		if (!IsInIgnoredList(path.Path()) && new_file.Exists()) {
			fManager->QueueUpload(fullpath, dbpath, modified, size); 
		}
		WatchEntry(&new_file, WATCH_FLAGS);
	}
}

void LocalFilesystem::HandleMoved(BMessage * msg)
{
	entry_ref from_ref, to_ref;
	node_ref nref;
	BEntry from_entry, to_entry;
	BPath path;
	const char * name;
	BDirectory dbdirectory;
	BString dbpath = BString(fCloudRootPath);
	trackeddata * tracked_file;

	ApplyFullPathToRelativeBasePath(dbpath);
	dbdirectory = BDirectory(dbpath);
	
	//can't move from one device to another, so all the same
	msg->FindInt32("device", &from_ref.device);
	msg->FindInt32("device", &to_ref.device);
	msg->FindInt32("device", &nref.device);
	
	msg->FindInt64("from directory", &from_ref.directory);
	msg->FindInt64("to directory", &to_ref.directory);	
	msg->FindString("name", &name);
	msg->FindInt64("node", &nref.node);
	to_ref.set_name(name);
	
	to_entry = BEntry(&to_ref);
	to_entry.GetPath(&path);

	tracked_file = FindTrackedEntry(nref);
	if (dbdirectory.Contains(&to_entry))
	{
		if (tracked_file == NULL) {
			off_t size;
			time_t modified;

			// moving into DropBox
			to_entry.GetPath(&path);
			BString fullpath = BString(path.Path());
			BString topath = BString(path.Path());
			ConvertFullPathToCloudRelativePath(topath);
			to_entry.GetModificationTime(&modified);
			to_entry.GetSize(&size);
			if (!IsInIgnoredList(fullpath)) {
				if (to_entry.IsDirectory())
				{
					//upload contents of directory also as it's not being tracked yet
					RecursiveAddToCloud(fullpath);
				} else {
					fManager->QueueUpload(fullpath, topath, modified, size); 					
				}
				WatchEntry(&to_entry, WATCH_FLAGS);
			}
		}
		else {
			// moving within DropBox
			from_entry = BEntry(tracked_file->path->Path());
			BString frompath = BString(tracked_file->path->Path());
			to_entry.GetPath(&path);
			BString topath = BString(path.Path());
			ConvertFullPathToCloudRelativePath(frompath);
			ConvertFullPathToCloudRelativePath(topath);
			StopWatchingNodeRef(&tracked_file->nref);
			if (!IsInIgnoredList(path.Path())) {
				fManager->QueueMove(frompath, topath);
				WatchEntry(&to_entry, WATCH_FLAGS);
			}
		}
		
	} else {
			// deleted from DropBox
			if (tracked_file != NULL) {
				from_entry = BEntry(tracked_file->path->Path());
				BString frompath = BString(tracked_file->path->Path());
				ConvertFullPathToCloudRelativePath(frompath);
				if (!IsInIgnoredList(tracked_file->path->Path())) {
					if (from_entry.IsDirectory()) 
					{
						RemoveTrackedEntriesForPath(tracked_file->path->Path());	
					}
					else 
					{
						StopWatchingNodeRef(&tracked_file->nref);		
					}
					fManager->QueueDelete(frompath);
				}
			}
	}
}

void LocalFilesystem::HandleRemoved(BMessage * msg)
{
	node_ref nref;
	trackeddata * td;
	BEntry entry;
	BString path;
	
	msg->FindInt64("node", &nref.node);
	msg->FindInt32("device", &nref.device);
	td = FindTrackedEntry(nref);
	if (td != NULL && !IsInIgnoredList(td->path->Path())) 
	{
		entry = BEntry(td->path->Path());
		path = BString(td->path->Path());
		if (entry.IsDirectory()) 
		{
			RemoveTrackedEntriesForPath(td->path->Path());	
		}
		else 
		{
			StopWatchingNodeRef(&td->nref);		
		}
		ConvertFullPathToCloudRelativePath(path);			
		fManager->QueueDelete(path);
	}
}

void LocalFilesystem::HandleChanged(BMessage * msg)
{
	BPath path;
	node_ref nref;
	trackeddata * td;
	BEntry entry;
	time_t modified;
	off_t size;
	
	msg->FindInt64("node", &nref.node);
	msg->FindInt32("device", &nref.device);
	td = FindTrackedEntry(nref);
	if (td != NULL)
	{
		entry.SetTo(td->path->Path());
		BString dbpath = BString(td->path->Path());
		ConvertFullPathToCloudRelativePath(dbpath);

		entry.GetModificationTime(&modified);
		entry.GetSize(&size);
		if (!IsInIgnoredList(td->path->Path())) {
			fManager->QueueUpload(td->path->Path(), dbpath, modified, size); 
		}
	}
}

void LocalFilesystem::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case B_NODE_MONITOR:
		{
			HandleNodeEvent(msg);
			break;	
		}
	}
}

void LocalFilesystem::HandleNodeEvent(BMessage *msg)
{
	int32 opcode;
	if (msg->FindInt32("opcode",&opcode) == B_OK)
	{
		//many of these ops are triggered twice due to watching the folders
		//and the files themselves
		switch(opcode)
		{
			case B_ENTRY_CREATED:
				HandleCreated(msg);								
				break;
			case B_ENTRY_MOVED:
				HandleMoved(msg);
				break;			
			case B_ENTRY_REMOVED:
				HandleRemoved(msg);
				break;
			case B_STAT_CHANGED:
			{
				int32 fields;
				msg->FindInt32("fields", &fields);
				if ((fields & B_STAT_MODIFICATION_TIME) != 0)
				{
					HandleChanged(msg);
				}
				break;
			}	
		}	
	}
}
