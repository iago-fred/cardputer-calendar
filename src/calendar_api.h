#ifndef CALENDAR_API_H
#define CALENDAR_API_H

#include <Arduino.h>
#include "storage.h"
#include <vector>

#define CAL_BASE "https://www.googleapis.com/calendar/v3/calendars/primary"

class CalendarAPI {
public:
    // Fetch events for date range (used on first sync / offline refresh)
    bool fetchEvents(const String& timeMin, const String& timeMax, std::vector<CalendarEvent>& events);

    // Sync using syncToken (incremental — only returns changes)
    bool syncEvents(const String& syncToken, std::vector<CalendarEvent>& changed, String& newSyncToken);

    // CRUD
    bool createEvent(const CalendarEvent& evt, CalendarEvent& created);
    bool updateEvent(const CalendarEvent& evt);
    bool deleteEvent(const String& eventId);

    // Get today's start/end ISO strings
    static String todayMin();
    static String todayMax();
    static String nowISO();

private:
    bool parseEventsList(const String& json, std::vector<CalendarEvent>& events, String& nextSyncToken);
    CalendarEvent parseEventItem(JsonObject& item);
    http_client _http;  // We'll use HTTPClient directly in .cpp

    String getAccessToken();
};

extern CalendarAPI calendar;

#endif
