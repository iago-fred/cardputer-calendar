#ifndef SYNC_H
#define SYNC_H

#include <Arduino.h>
#include "storage.h"
#include <vector>

class SyncManager {
public:
    // Full sync: push pending changes, then pull fresh events
    bool fullSync();

    // Push pending changes to Google Calendar
    bool pushPending();

    // Pull events using syncToken (incremental)
    bool pullIncremental();

    // Full pull (no syncToken — fetch all)
    bool pullFull(const String& timeMin, const String& timeMax);

    // Status
    bool isSynced() { return _synced; }
    int  pendingCount();

private:
    bool _synced = false;
    bool applyRemoteChanges(const std::vector<CalendarEvent>& remoteEvents, const String& newSyncToken);
};

extern SyncManager syncMgr;

#endif
