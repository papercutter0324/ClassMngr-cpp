# ClassMngr 0.18.0 Release Notes

ClassMngr 0.18.0 reduces memory use by loading pages and resources on demand,
makes roster editing more flexible, and improves validation for Korean student
names.

## Performance and Resource Loading

- Reduced memory use by creating larger pages and views only when they are
  needed.
- Updated resource loading so application resources are loaded dynamically as
  required.

## Rosters and Speaking Evaluations

- Roster tables now support selecting multiple cells.
- Korean student names with one or five or more syllables are clearly marked
  for verification.
- Saving a roster or speaking evaluation with a questionable Korean name now
  gives you the opportunity to verify it or save it anyway.

## App Updates

- Improved startup handling so an automatic update prompt stays visible above
  the main window while the app finishes opening.

# ClassMngr 0.17.1 Release Notes

ClassMngr 0.17.1 adds birthday reminders, expands access to the Korean /
English on-screen keyboard, and streamlines Preferences and class navigation.

## Birthday Reminders

- Added an Upcoming Birthdays view for birthdays today and over the next two
  weeks.
- Birthdays are shown for Korean teachers, Native English Teachers, and GS Team
  members.
- ClassMngr can show the Upcoming Birthdays reminder when it starts.

## Classes and Navigation

- Combined class details, roster, analytics, evaluations, and notes into one
  class page with consistent tabs.
- Refined class navigation with options for showing all classes or only
  active-schedule classes, plus improved class filtering and selection behavior.

## Preferences

- Consolidated schedule, calendar, and class-navigation settings into the
  Preferences dialog.
- Added clearer sections and controls for configuring the app's navigation and
  schedule behavior.

## On-Screen Keyboard

- Added quick access to the Korean / English on-screen keyboard from more pages
  and dialogs.
- Added on-screen keyboard support to the Initial Setup wizard.

# ClassMngr 0.17.0 Release Notes

ClassMngr 0.17.0 introduces Class Analytics for a clear, at-a-glance view of
speaking-evaluation results. It also refreshes app navigation and includes
several setup, calendar, and theme refinements.

## Class Analytics

- Added an Analytics tab to each class, with a selector for viewing a specific
  speaking evaluation.
- See the class average, the number of students assessed, and the strongest
  and focus areas for the selected evaluation.
- Use Class Shape and By Criterion charts to understand grade distributions
  and performance across the evaluation criteria.
- Review a student ranking table with English and Korean names, overall grades,
  and criterion-level results.

## Navigation

- Redesigned the navigation tabs used across the app for a more consistent,
  compact experience.
- Added filtering and display settings to help you focus the Classes list on
  the classes that matter now.

## Initial Setup and Preferences

- Refined the Initial Setup wizard and Getting Started banner layout.
- Cancelling the Initial Setup wizard no longer saves incomplete setup data.
- Restored the option to follow the system theme in Preferences.

## Calendar and Schedule

- Improved how term schedule dates are updated in the Calendar.
- Reduced unused vertical space in schedule cells.

# ClassMngr 0.16.0 Release Notes

ClassMngr 0.16.0 adds an on-screen Korean / English keyboard, makes update
status easier to follow, and refines the look of Preferences and Dark-theme
menus.

## On-Screen Keyboard

- Added a Korean / English on-screen keyboard for people who do not have an
  easy way to type in Korean.
- Use the new keyboard button in roster and speaking-evaluation tables to type
  Korean or English directly into the selected editable cell.

## App Updates

- Refined update-check status messages and layout so the download and installed
  version details are easier to understand.

## Preferences

- Improved spacing between options and sections for a clearer Preferences
  dialog.

## Dark Theme

- Made menu dividers more visible in the Dark theme.
- Ensured the on-screen keyboard button uses an icon that matches the selected
  theme.

# ClassMngr 0.15.0 Release Notes

ClassMngr 0.15.0 makes it quicker to get started with a new file, improves
calendar responsiveness, and reorganizes commonly used menu actions.

## Initial Setup

- Added an Initial Setup wizard on the start page to guide you through
  creating a new ClassMngr file.
- The wizard can import a Korean teacher list and schedule, collect your
  personal information, and help create your first teachers, class, and class
  times.

## Calendar

- Calendar events now load progressively in the background, keeping the
  calendar and upcoming-events panels more responsive.
- Upcoming-event sections now clearly show when more events are loading.

## Menus

- Replaced the nested Manage menu with separate top-level Classes and Teachers
  menus.
- Added a Print / Export menu that makes the current page's available print
  and export actions easier to find.

## Preferences

- Added a Workbook timeout setting for teacher and schedule imports. Choose
  30 seconds, 1 minute, 2 minutes, or 5 minutes; the default is now 2 minutes.
- Refined preference labels, AI website details, and default sidebar text
  animation for a clearer setup experience.

## App Updates

- Improved the update-check dialog layout and status details.

# ClassMngr 0.14.1 Release Notes

ClassMngr 0.14.1 improves access to app settings and adds a Windows Start
Menu shortcut option for easier setup.

## Preferences

- Replaced the old Options menu with a new Preferences dialog.
- Made app settings easier to find and use from a more consistent interface.

## Windows Installation

- Added an option to place a ClassMngr shortcut in the Windows Start Menu
  during installation.
  
# ClassMngr 0.14.0 Release Notes

ClassMngr 0.14.0 makes it easier to keep the app up to date, improves schedule
imports and substitute notes, and resolves a couple of platform and layout
issues.

## App Updates

- ClassMngr can now automatically check for published updates when it starts.
- Updates are delivered through GitHub Releases and verified after download.
- You can skip notifications for a particular version, or pause, resume, and
  discard an in-progress download.

## Schedule Imports

- Simplified the instructions for selecting your name from an imported
  schedule.
- When the selected name differs from the name in My Information, you can
  continue with the import or update My Information to match it.

## Substitute Notes

- Reworked the Substitute Notes page into a more compact, easier-to-scan
  layout.
- Materials Location and Detailed Class & Lesson Notes now appear together in
  the same section, both in the app and in generated documents.

## Fixes

- Fixed roster-print previews so they resize correctly when the template type
  changes.
- Fixed a macOS crash that could occur when opening a file.
