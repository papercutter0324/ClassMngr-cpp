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
