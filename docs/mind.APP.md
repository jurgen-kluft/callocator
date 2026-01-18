# Mind App

Can you write a local MacOS app in Golang.

What are the functionalities that we are looking for:

- Written in Golang with as little dependencies as possible
- Use gmail OAUTH to send and receive email
- Data
    - Local JSON file
      - List of Events
      - List of Reminders
      - List of Timers
      - List of Pushover recipients 
      - List of Email recipients
- Event:
  - Title of event
  - UUID of event is automatically generated
  - Description of Event
  - Reference to Reminder (when to notify before event)
  - List references to recipients (email or pushover)
  - Recurrence options (e.g., daily, weekly, monthly)
  - Trigger conditions (e.g., specific date/time, relative to another event)
- Timer:
  - A timer is an event that triggers at a specific date/time
  - When the timer triggers, it sends a message to the list of recipients
  - Two types; single-shot and recurring timers
  - Auto disable itself when done (default for timers)
- Reminder:
  - A reminder is an event that notifies the user before another event occurs
  - When the reminder triggers, it sends a message to the list of recipients
  - Single-shot or recurring reminders
  - Auto disable itself when done (default for reminders)
- Notifications:
  - Use gmail OAUTH to send and receive email notifications to recipients
    - Certain (particularly formatted) emails can create, update, or delete events
  - Use Pushover service to send important notifications to recipients, normal
      notifications are sent by email
  - Support for multiple recipients per event
  - Customizable notification messages based on event details
- REST API:
  - Local REST API to manage events, timers, reminders, and recipients
  - Endpoints for creating, updating, disabling, and retrieving events
  - Endpoints for managing recipients and notification settings
