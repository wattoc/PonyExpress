#ifndef LOCALFILESYSTEM_H
#define LOCALFILESYSTEM_H

#include <Directory.h>
#include <Entry.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <NodeMonitor.h>
#include <Path.h>

#include "config.h"

#define WATCH_FLAGS (B_WATCH_DIRECTORY | B_WATCH_STAT | B_WATCH_NAME)

class LocalFilesystem
{
	public:
		LocalFilesystem(SupportedClouds usingCloud, const char * rootPath) { cloud = usingCloud; cloudRootPath = rootPath; };

		bool TestLocation(BMessage * dbMessage);
		bool ResolveUnreferencedLocals(const char * leaf, BList & remote, BList & local, bool forceFull);
		bool SendMissing(BList & items);
		void ApplyFullPathToRelativeBasePath(BString &relative);
		void ConvertFullPathToCloudRelativePath(BString &full);
		static void RecursivelyWatchDirectory(const char * fullPath, uint32 flags);

		void WatchDirectories(void);
		void CheckOrCreateRootFolder(void);

		void HandleNodeEvent(BMessage *message);
		void HandleCreated(BMessage * message);
		void HandleMoved(BMessage * message);
		void HandleRemoved(BMessage * message);
		void HandleChanged(BMessage * message);

		static void AddToIgnoreList(const char * fullPath);
		static void RemoveFromIgnoreList(const char * fullPath);
		static void WatchEntry(BEntry *entry, uint32 flags);
		
	private:
		SupportedClouds cloud;
		const char * cloudRootPath;
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
		void RecursiveAddToCloud(const char *fullPath);
};


#endif
