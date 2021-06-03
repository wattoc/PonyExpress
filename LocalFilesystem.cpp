#include "LocalFilesystem.h"

#include <Application.h>
#include <Message.h>
#include <FindDirectory.h>
#include <NodeMonitor.h>
#include <String.h>

#include "DropboxSupport.h"
#include "Globals.h"

BList LocalFilesystem::tracked_entries = BList();
BList LocalFilesystem::ignored_entries = BList();
BLocker* LocalFilesystem::ignored_entries_locker  = new BLocker(true);

void LocalFilesystem::CheckOrCreateRootFolder(const char * rootPath)
{
	BPath userpath;
	BDirectory directory;
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(rootPath);
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
		if (fsentry.IsDirectory()) {
			fsentry.GetPath(&userpath);
			RecursiveDelete(userpath.Path());	
		}
		fsentry.GetNodeRef(&ref);
		StopWatchingNodeRef(&ref);
		fsentry.Remove();		
	}
}

bool LocalFilesystem::TestLocation(const char * rootPath, BMessage * dbMessage) 
{
	BPath userpath;
	BDirectory directory;
	BEntry fsentry;
	bool needsUpdate = false;
	BString entryPath = NULL;
	BString entryType = NULL;
	
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(rootPath);
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
				sSize=dbMessage->GetDouble("size",0); //this shouldn't be a double?
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
				needsUpdate = true;
				WatchEntry(&fsentry, WATCH_FLAGS);
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
			LogInfoLine("Unhandled entryType encountered");
		}	
	}
	return needsUpdate;	
}

//
bool LocalFilesystem::ResolveUnreferencedLocals(const char * rootPath, const char * leaf, BList & remote, BList & local, bool forceFull) 
{
	BPath userpath;
	BDirectory directory;
	BEntry fsentry;
	
	if (find_directory(B_USER_DIRECTORY, &userpath) == B_OK)
	{
		userpath.Append(rootPath);
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
				ResolveUnreferencedLocals(rootPath, strippedPath.String(), remote, local, forceFull);
			}	
		}
		return true;
	}
	
	return false;
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
	LocalFilesystem::RemoveTrackedEntry(nref);
	watch_node(nref, B_STOP_WATCHING, be_app_messenger);
}

void LocalFilesystem::WatchEntry(BEntry *entry, uint32 flags)
{
	node_ref nref;
	entry->GetNodeRef(&nref);
	
	if ((flags & B_STOP_WATCHING) != 0) 
	{
		LocalFilesystem::RemoveTrackedEntry(&nref);
	}
	else 
	{	
		trackeddata * td = new trackeddata();
		td->nref = nref;
		entry->GetPath(td->path);
		tracked_entries.AddItem(td);
	}	
	watch_node(&nref, flags, be_app_messenger);
}

void LocalFilesystem::RecursivelyWatchDirectory(const char * fullPath, uint32 flags)
{
	BDirectory directory;
	BEntry entry;
	
	directory.SetTo(fullPath);
	//add node watcher to this dir
	directory.GetEntry(&entry);
	WatchEntry(&entry, flags);
	LogInfo("Watching directory: ");
	LogInfoLine(fullPath);
	
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

void LocalFilesystem::ConvertFullPathToDropboxRelativePath(BString &full)
{
	BString dbpath = BString("Dropbox/");
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
		LogInfo("Starting watcher\n");
		BString path = BString("Dropbox/");
		LocalFilesystem::ApplyFullPathToRelativeBasePath(path);
		LocalFilesystem::RecursivelyWatchDirectory(path, WATCH_FLAGS);	
}

LocalFilesystem::trackeddata * LocalFilesystem::FindTrackedEntry(node_ref find) 
{	
	for (int i=0; i<tracked_entries.CountItems(); i++)
	{
		trackeddata * file = (trackeddata *)tracked_entries.ItemAt(i);
		if (find.node == file->nref.node && find.device == file->nref.device)
			return file;
	}
	return NULL;
}

void LocalFilesystem::RemoveTrackedEntry(node_ref * find) 
{	
	node_ref ref;
	for (int i=0; i<tracked_entries.CountItems(); i++)
	{
		trackeddata *file = (trackeddata *)tracked_entries.ItemAt(i);
		
		if (find->node == file->nref.node && find->device == file->nref.device) 
		{
			tracked_entries.RemoveItem(i);
			delete file;
		}
	}
}

void LocalFilesystem::AddToIgnoreList(const char * fullPath)
{
	BString * ignoring = new BString(fullPath);
	ignored_entries_locker->Lock();
	ignored_entries.AddItem(ignoring);
	ignored_entries_locker->Unlock();
}

void LocalFilesystem::RemoveFromIgnoreList(const char * fullPath)
{
	for (int i=0; i<ignored_entries.CountItems(); i++)
	{
		BString *ignoring = (BString *)ignored_entries.ItemAt(i);
		
		if (strcmp(ignoring->String(), fullPath) == 0) 
		{
			ignored_entries_locker->Lock();
			ignored_entries.RemoveItem(i);
			ignored_entries_locker->Unlock();

			delete ignoring;
		}
	}
}

bool LocalFilesystem::IsInIgnoredList(const char *fullPath)
{
	for (int i=0; i<ignored_entries.CountItems(); i++)
	{
		BString *ignoring = (BString *)ignored_entries.ItemAt(i);
		
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
	off_t size;
	time_t modified;
	const char * name;
	DropboxSupport * db = new DropboxSupport();
	
	msg->FindInt32("device", &ref.device);
	msg->FindInt64("directory", &ref.directory);
	msg->FindString("name", &name);
	ref.set_name(name);
	BEntry new_file = BEntry(&ref);			
	new_file.GetPath(&path);
	BString dbpath = BString(path.Path());
	ConvertFullPathToDropboxRelativePath(dbpath);
	LogInfo(dbpath.String());
	
	if (new_file.IsDirectory())
	{
		if (!IsInIgnoredList(path.Path()))
	 		db->CreatePath(dbpath);
		LocalFilesystem::RecursivelyWatchDirectory(path.Path(), WATCH_FLAGS);		
		LogInfoLine(" Entry Folder Created");

	}
	else
	{
		new_file.GetModificationTime(&modified);
		new_file.GetSize(&size);
		if (!IsInIgnoredList(path.Path())) {
			db->Upload(path.Path(), dbpath, DropboxSupport::ConvertSystemToTimestamp(modified), size); 
			LogInfoLine(" Entry File Created");
		}
		WatchEntry(&new_file, WATCH_FLAGS);
	}
	delete db;				
}

void LocalFilesystem::HandleMoved(BMessage * msg)
{
	entry_ref from_ref, to_ref;
	node_ref nref;
	BEntry from_entry, to_entry;
	BPath path;
	const char * name;
	BDirectory dbdirectory;
	BString dbpath = BString("Dropbox/");
	trackeddata * tracked_file;

	DropboxSupport * db = new DropboxSupport();
	
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
			BString topath = BString(path.Path());
			ConvertFullPathToDropboxRelativePath(topath);
			to_entry.GetModificationTime(&modified);
			to_entry.GetSize(&size);
			if (!IsInIgnoredList(path.Path())) {
				db->Upload(path.Path(), topath, DropboxSupport::ConvertSystemToTimestamp(modified), size); 
				LogInfoLine("Move into DropBox");
				WatchEntry(&to_entry, WATCH_FLAGS);
			}
		}
		else {
			// moving within DropBox
			from_entry = BEntry(tracked_file->path->Path());
			BString frompath = BString(tracked_file->path->Path());
			to_entry.GetPath(&path);
			BString topath = BString(path.Path());
			ConvertFullPathToDropboxRelativePath(frompath);
			ConvertFullPathToDropboxRelativePath(topath);
			StopWatchingNodeRef(&tracked_file->nref);
			if (!IsInIgnoredList(path.Path())) {
				LogInfoLine("Move within DropBox");
				db->Move(frompath, topath);
				WatchEntry(&to_entry, WATCH_FLAGS);
			}
		}
		
	} else {
			// deleted from DropBox
			if (tracked_file == NULL) {
				LogInfoLine("Phantom move received?");
			} else {
				from_entry = BEntry(tracked_file->path->Path());
				BString frompath = BString(tracked_file->path->Path());
				ConvertFullPathToDropboxRelativePath(frompath);
				if (!IsInIgnoredList(tracked_file->path->Path())) {
					db->DeletePath(frompath);
					StopWatchingNodeRef(&tracked_file->nref);
					LogInfoLine("Remove from DropBox");
				}
			}

	}
	
	delete db;				
}

void LocalFilesystem::HandleRemoved(BMessage * msg)
{
	BPath path;
	node_ref nref;
	trackeddata * td;
	BEntry entry;
	
	DropboxSupport * db = new DropboxSupport();
	
	msg->FindInt64("node", &nref.node);
	msg->FindInt32("device", &nref.device);
	td = FindTrackedEntry(nref);
	if (td == NULL) {
		msg->PrintToStream();
		LogInfoLine("Phantom delete?");
	}
	else {	
		if (!IsInIgnoredList(td->path->Path())) {
			db->DeletePath(td->path->Path());
			StopWatchingNodeRef(&td->nref);
			LogInfoLine("Entry Removed");
		}
	}
	
	delete db;
	

}
void LocalFilesystem::HandleChanged(BMessage * msg)
{
	BPath path;
	node_ref nref;
	trackeddata * td;
	BEntry entry;
	time_t modified;
	off_t size;
	
	DropboxSupport * db = new DropboxSupport();
	
	msg->FindInt64("node", &nref.node);
	msg->FindInt32("device", &nref.device);
	td = FindTrackedEntry(nref);
	entry.SetTo(td->path->Path());
	BString dbpath = BString(td->path->Path());
	ConvertFullPathToDropboxRelativePath(dbpath);

	entry.GetModificationTime(&modified);
	entry.GetSize(&size);
	if (!IsInIgnoredList(td->path->Path())) {
		db->Upload(td->path->Path(), dbpath, DropboxSupport::ConvertSystemToTimestamp(modified), size); 
		LogInfoLine("Entry Changed");
	}
	delete db;
}

void LocalFilesystem::HandleNodeEvent(BMessage *msg)
{
	int32 opcode;
	if (msg->FindInt32("opcode",&opcode) == B_OK)
	{
		switch(opcode)
		{
			case B_ENTRY_CREATED:
				HandleCreated(msg);								
				break;
			case B_ENTRY_MOVED:
				HandleMoved(msg);
				break;
				
			case B_ENTRY_REMOVED:
			{
				HandleRemoved(msg);
				break;
			}	
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
