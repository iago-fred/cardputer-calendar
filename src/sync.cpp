#include "sync.h"
#include "calendar_api.h"
#include "storage.h"
#include <algorithm>

SyncManager syncMgr;

int SyncManager::pendingCount() {
    std::vector<PendingChange> pending;
    storage.loadPending(pending);
    return pending.size();
}

// ─── Push local changes to Google ───
bool SyncManager::pushPending() {
    std::vector<PendingChange> pending;
    if (!storage.loadPending(pending) || pending.empty()) {
        _synced = true;
        return true;
    }

    std::vector<PendingChange> succeeded;

    for (auto& p : pending) {
        if (p.action == "create") {
            CalendarEvent created;
            if (calendar.createEvent(p.event, created)) {
                // Replace pending event ID with real Google ID in cache
                storage.updateEventInCache(created);
                succeeded.push_back(p);
            } else {
                log_e("Failed to push create: %s", p.event.summary.c_str());
            }
        }
        else if (p.action == "update") {
            if (calendar.updateEvent(p.event)) {
                p.event.dirty = false;
                storage.updateEventInCache(p.event);
                succeeded.push_back(p);
            } else {
                log_e("Failed to push update: %s", p.event.summary.c_str());
            }
        }
        else if (p.action == "delete") {
            bool ok = false;
            if (p.event.id.length() > 5) {
                // Has Google ID — delete remotely
                ok = calendar.deleteEvent(p.event.id);
            } else {
                // Never synced — just remove locally
                ok = true;
            }
            if (ok) {
                storage.deleteEventFromCache(p.event.id);
                succeeded.push_back(p);
            } else {
                log_e("Failed to push delete: %s", p.event.id.c_str());
            }
        }
    }

    // Remove succeeded changes from pending
    // Keep only those that failed
    std::vector<PendingChange> remaining;
    for (auto& p : pending) {
        bool found = false;
        for (auto& s : succeeded) {
            if (p.action == s.action && p.event.id == s.event.id &&
                p.event.summary == s.event.summary) {
                found = true;
                break;
            }
        }
        if (!found) remaining.push_back(p);
    }
    storage.savePending(remaining);

    _synced = remaining.empty();
    log_i("Push: %d/%d succeeded, %d remaining",
          succeeded.size(), pending.size(), remaining.size());
    return remaining.empty();
}

// ─── Pull incremental (using syncToken) ───
bool SyncManager::pullIncremental() {
    String syncToken;
    int64_t lastSyncMs;
    storage.loadLastSync(syncToken, lastSyncMs);

    if (syncToken.length() == 0) {
        log_i("No sync token — doing full pull instead");
        return false;
    }

    std::vector<CalendarEvent> changes;
    String newSyncToken;

    if (!calendar.syncEvents(syncToken, changes, newSyncToken)) {
        log_w("Incremental sync failed or token expired");
        return false;
    }

    if (!applyRemoteChanges(changes, newSyncToken)) return false;
    _synced = true;
    return true;
}

// ─── Full pull (initial or after expired token) ───
bool SyncManager::pullFull(const String& timeMin, const String& timeMax) {
    std::vector<CalendarEvent> events;
    if (!calendar.fetchEvents(timeMin, timeMax, events)) {
        return false;
    }

    // Merge with local events (preserve dirty local-only events)
    std::vector<CalendarEvent> local;
    storage.loadEvents(local);

    for (auto& remote : events) {
        bool found = false;
        for (auto& l : local) {
            if (l.id == remote.id) {
                l = remote;  // Replace with server version
                found = true;
                break;
            }
        }
        if (!found && !remote.deleted) {
            local.push_back(remote);
        }
    }

    // Remove server-deleted
    local.erase(std::remove_if(local.begin(), local.end(),
        [](const CalendarEvent& e) { return e.deleted; }), local.end());

    storage.saveEvents(local);
    // Don't save sync token here — full pull doesn't provide one.
    // Save a placeholder timestamp so we know when we last synced.
    storage.saveLastSync("FULL_SYNC_" + String(millis()), millis());
    _synced = true;
    log_i("Full pull: %d events merged", local.size());
    return true;
}

// ─── Full sync (push + pull) ───
bool SyncManager::fullSync() {
    if (!pushPending()) {
        log_w("Push failed, aborting sync");
        return false;
    }

    // Try incremental first
    if (!pullIncremental()) {
        // Fall back to full pull for next 30 days
        time_t now = time(nullptr);
        struct tm* t = gmtime(&now);
        char buf[32];

        strftime(buf, sizeof(buf), "%Y-%m-%dT00:00:00Z", t);
        String timeMin(buf);

        t->tm_mday += 30;
        mktime(t);
        strftime(buf, sizeof(buf), "%Y-%m-%dT00:00:00Z", t);
        String timeMax(buf);

        if (!pullFull(timeMin, timeMax)) return false;
    }

    _synced = true;
    return true;
}

// ─── Apply remote changes to local cache ───
bool SyncManager::applyRemoteChanges(const std::vector<CalendarEvent>& remoteEvents, const String& newSyncToken) {
    std::vector<CalendarEvent> local;
    storage.loadEvents(local);

    for (auto& remote : remoteEvents) {
        bool found = false;
        for (auto& l : local) {
            if (l.id == remote.id) {
                found = true;
                if (remote.deleted) {
                    l.deleted = true;
                } else if (!l.dirty) {
                    l = remote;
                }
                break;
            }
        }
        if (!found && !remote.deleted) {
            local.push_back(remote);
        }
    }

    // Remove soft-deleted
    local.erase(std::remove_if(local.begin(), local.end(),
        [](const CalendarEvent& e) { return e.deleted; }), local.end());

    storage.saveEvents(local);
    storage.saveLastSync(newSyncToken, millis());
    log_i("Applied %d remote changes", remoteEvents.size());
    return true;
}
