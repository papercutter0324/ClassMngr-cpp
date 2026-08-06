# ClassMngr

ClassMngr is a cross-platform desktop application for managing classes, schedules, rosters, speaking evaluations, substitute preparation, teacher details, campus information, and academic-calendar events.

### [Download the latest release](https://github.com/papercutter0324/ClassMngr-cpp/releases/latest)

ClassMngr is designed to keep day-to-day classroom administration in one place. Release builds include the Qt runtime they need, so teachers can run the installed application without installing Qt separately.

## Table of Contents

* [Features](#features)
  * [Classes, Schedules, and Rosters](#classes-schedules-and-rosters)
  * [Speaking Evaluations and Reports](#speaking-evaluations-and-reports)
  * [AI-Assisted Comment Workflow](#ai-assisted-comment-workflow)
  * [Substitute Preparation and Teaching Documents](#substitute-preparation-and-teaching-documents)
  * [Staff, Campus, and Calendar Information](#staff-campus-and-calendar-information)
  * [Language, Appearance, and Updates](#language-appearance-and-updates)
* [Getting Started](#getting-started)
* [Project Documentation](#project-documentation)
* [Building](#building)

## Features

#### Classes, Schedules, and Rosters

Manage classes, teachers, schedules, rosters, notes, and testing assignments. Import schedule and teacher data, transfer students between classes, and print schedules and roster templates.

#### Speaking Evaluations and Reports

Record speaking-evaluation scores and observations, then create individual or whole-class reports.

#### AI-Assisted Comment Workflow

Prepare name-redacted prompts for AI-assisted evaluation comments, review the pasted results, and choose which comments to apply. ClassMngr opens the configured AI website; it does not send observations to an AI API itself.

#### Substitute Preparation and Teaching Documents

Create printable substitute-preparation notes and packages, and browse, export, or print the bundled teaching-document catalog.

#### Staff, Campus, and Calendar Information

View staff and campus directories, housing details, directions, maps, and academic-calendar events.

#### Language, Appearance, and Updates

Use light or dark themes and switch the interface between English and Korean. When release builds are configured with their endpoints, ClassMngr can receive signed application updates and independently updated resource packs.

## Getting Started

Launch ClassMngr and create or open a database. The in-app Getting Started panel guides new users through adding or importing Korean teachers, creating classes, and then adding schedules and rosters.

New databases, saved copies, and exports use the `.tps` ClassMngr Database format. Existing legacy `.db` databases can still be opened.

## Project Documentation

* [Current release notes](release-notes.md)
* [Auto-update documentation](docs/auto_updates.md)
* [Resource-pack documentation](docs/resource_packs.md)

## Building

Building is not required to use ClassMngr. Instructions for setting up the required tools, creating platform-specific release packages, and running tests are in [BUILDING.md](BUILDING.md).
