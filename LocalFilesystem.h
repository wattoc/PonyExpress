#ifndef LOCALFILESYSTEM_H
#define LOCALFILESYSTEM_H

#include <Directory.h>
#include <Entry.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <NodeMonitor.h>
#include <Path.h>

#include "DropboxSupport.h"

#define WATCH_FLAGS (B_WATCH_DIRECTORY | B_WATCH_STAT | B_WATCH_NAME)

class LocalFilesystem
{
	public:
		LocalFilesystem(void) {};

		static bool TestLocation(const char * rootPath, BMessage * dbMessage);
		static bool ResolveUnreferencedLocals(const char * rootPath, const char * leaf, BList & remote, BList & local, bool forceFull);
		static void ApplyFullPathToRelativeBasePath(BString &relative);
		static void ConvertFullPathToDropboxRelativePath(BString &full);
		static void RecursivelyWatchDirectory(const char * fullPath, uint32 flags);

		static void WatchDirectories(void);
		static void CheckOrCreateRootFolder(const char * rootPath);

		static void HandleNodeEvent(BMessage *message);
		static void HandleCreated(BMessage * message);
		static void HandleMoved(BMessage * message);
		static void HandleRemoved(BMessage * message);
		static void HandleChanged(BMessage * message);

		static void AddToIgnoreList(const char * fullPath);
		static void RemoveFromIgnoreList(const char * fullPath);
		static void WatchEntry(BEntry *entry, uint32 flags);
		
	private:
		class trackeddata {
			public:
				trackeddata(void) { path = new BPath(); };
				~trackeddata(void) { delete path; }
			node_ref nref;
			BPath * path;
		};


		static BList tracked_entries;
		static BList ignored_entries;
		static BLocker * ignored_entries_locker;
		static void StopWatchingNodeRef(node_ref *nref);
		static bool IsInRemoteList(const char * path, time_t localModified, BList & remote);
		static bool IsInIgnoredList(const char *fullPath);
		static trackeddata * FindTrackedEntry(node_ref find);
		static void RemoveTrackedEntry(node_ref * find);
		static void RemoveTrackedEntriesForPath(const char *fullPath);
		static void RecursiveDelete(const char *path);
		static void RecursiveAddToCloud(DropboxSupport *db, const char *fullPath);
};


#endif
