#ifndef LOCALFILESYSTEM_H
#define LOCALFILESYSTEM_H

#include <Directory.h>
#include <Entry.h>
#include <List.h>
#include <Locker.h>
#include <Looper.h>
#include <Message.h>
#include <NodeMonitor.h>
#include <Path.h>

#include "config.h"
#include "Manager.h"

#define WATCH_FLAGS (B_WATCH_DIRECTORY | B_WATCH_STAT | B_WATCH_NAME)

class LocalFilesystem : public BLooper
{
	public:
		LocalFilesystem(Manager * manager, const char * rootPath) : BLooper() { fManager = manager; fCloudRootPath = rootPath; };

		bool TestLocation(BMessage * dbMessage);
		bool ResolveUnreferencedLocals(const char * leaf, BList & remote, BList & local, bool forceFull);
		bool SendMissing(BList & items);
		void ApplyFullPathToRelativeBasePath(BString &relative);
		void ConvertFullPathToCloudRelativePath(BString &full);
		void RecursivelyWatchDirectory(const char * fullPath, uint32 flags);

		void WatchDirectories(void);
		void CheckOrCreateRootFolder(void);

		void MessageReceived(BMessage *msg);
		
		void HandleNodeEvent(BMessage *message);
		void HandleCreated(BMessage * message);
		void HandleMoved(BMessage * message);
		void HandleRemoved(BMessage * message);
		void HandleChanged(BMessage * message);

		void AddToIgnoreList(const char * fullPath);
		void RemoveFromIgnoreList(const char * fullPath);
		void WatchEntry(BEntry *entry, uint32 flags);
		
	private:
		Manager * fManager;
		const char * fCloudRootPath;
		class trackeddata {
			public:
				trackeddata(void) { path = new BPath(); };
				~trackeddata(void) { delete path; }
			node_ref nref;
			BPath * path;
		};


		static BList sTrackedEntries;
		static BList sIgnoredEntries;
		static BLocker * sIgnoredEntriesLocker;
		void StopWatchingNodeRef(node_ref *nref);
		static bool IsInRemoteList(const char * path, time_t localModified, BList & remote);
		static bool IsInIgnoredList(const char *fullPath);
		static trackeddata * FindTrackedEntry(node_ref find);
		void RemoveTrackedEntry(node_ref * find);
		void RemoveTrackedEntriesForPath(const char *fullPath);
		void RecursiveDelete(const char *path);
		void RecursiveAddToCloud(const char *fullPath);
};


#endif
