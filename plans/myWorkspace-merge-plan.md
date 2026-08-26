# My Workspace Consolidation Plan

## Phase 1 — Audit and Architecture

### Status

Completed — 2026-08-26. See `plans/myWorkspace-merge-audit.md`.

### Goal
Understand the existing implementations and prepare the consolidation without changing behavior yet.

### Tasks
1. Locate the current implementations for:
   - My Details
   - My Schedule
   - Calendar
   - Signature editor/preview
2. Identify:
   - Page classes and widgets
   - Page enums/IDs
   - `PageManager` / `QStackedWidget` registration
   - Sidebar/navigation actions
   - Direct links to Schedule, Details, or Calendar
   - Database/data-service calls
   - Save/refresh logic
   - Calendar initialization and loading behavior
   - Signature persistence and rendering
3. Determine which existing page classes can become child widgets without being rewritten.
4. Find any hard-coded page indexes that would be affected.
5. Confirm how the existing **N/A** checkbox behaves.
6. Confirm what format signatures are currently stored in.
7. Identify all places that consume the saved signature, such as reports, certificates, or print/export functions.

### Deliverable

```text
Current
├── My Details
├── My Schedule
└── Calendar

Target
My Workspace
├── My Details
├── My Schedule [default]
└── Calendar [lazy loaded]
```

### Completion Criteria
- All affected classes and navigation routes are identified.
- Existing functionality that should be reused is documented.
- No major UI functionality has been rewritten yet.

---

# Phase 2 — Create the My Workspace Shell

### Status

Completed — 2026-08-26. The shell, named tab API, and focused test are in place.

### Goal
Create the new parent page and tab infrastructure.

### Tasks
1. Create `MyWorkspacePage`.
2. Set the visible page title to **My Workspace**.
3. Add three tabs in this order:

```text
My Details
My Schedule
Calendar
```

4. Use the application's existing navigation/tab styling.
5. Use a `QStackedWidget` or equivalent internally for the content.
6. Define named tab identifiers rather than relying on raw integers.

For example:

```cpp
enum class WorkspaceTab {
    Details,
    Schedule,
    Calendar
};
```

7. Add a public navigation function such as:

```cpp
void openTab(WorkspaceTab tab);
```

8. Make **My Schedule** the default selected tab.
9. Do not initialize Calendar content yet.

### Completion Criteria
Opening My Workspace displays:

```text
My Workspace

[ My Details ] [ My Schedule ] [ Calendar ]
                  ^ selected
```

The Calendar tab can still contain only a placeholder at this stage.

---

# Phase 3 — Merge My Schedule and My Details

### Status

Completed — 2026-08-26. The existing pages are persistent My Workspace children with delegated page lifecycle behavior.

### Goal
Move the existing Details and Schedule functionality into My Workspace without changing their behavior.

## 3A — My Schedule

1. Reuse the existing Schedule implementation.
2. Embed its content into the My Schedule tab.
3. Remove or suppress any redundant page-level title.
4. Preserve:
   - Schedule rendering
   - Editing
   - Filters
   - Selection state
   - Database access
   - Refresh behavior
   - Save behavior
5. Make sure switching tabs does not recreate the Schedule widget.

## 3B — My Details

Move the existing personal-information UI into the My Details tab.

The primary fields should be:

```text
Name
Campus
Zoom ID
Zoom Password
N/A
```

Preserve the existing behavior for:

- N/A checkbox
- Zoom fields
- Password masking/reveal, if present
- Validation
- Save logic
- Data loading
- Campus selection

Do not change the data model simply because the UI moved.

### Completion Criteria
- My Schedule functions exactly as before.
- My Details loads and saves correctly.
- N/A behaves exactly as before.
- Switching between tabs does not lose unsaved UI state.
- Schedule remains the default workspace tab.

---

# Phase 4 — Rework the Signature Section

### Status

Completed — 2026-08-26. Signatures can be supplied as an embedded image or typed with one of four bundled script fonts.

### Goal
Allow users to either embed a signature image or type a signature, while keeping the existing image-based report output compatible.

## 4A — Compact the Existing Signature UI

1. Preserve the current signature image upload, replace, and remove workflow.
2. Reduce the preview size.
3. Target approximately:

```text
90–120 px preview height
```

4. Scale the signature proportionally.
5. Do not crop or distort existing signatures.

## 4B — Add Signature Modes

Add a selector such as:

```text
[ Image ] [ Type ]
```

### Image Mode

Use the current signature image workflow with minimal changes.

### Type Mode

Add:

```text
Type your name
```

followed by four signature-font choices arranged approximately:

```text
┌─────────────────┐ ┌─────────────────┐
│   Your Name     │ │   Your Name     │
│     Casual      │ │     Cursive     │
└─────────────────┘ └─────────────────┘

┌─────────────────┐ ┌─────────────────┐
│   Your Name     │ │   Your Name     │
│     Elegant     │ │   Handwritten   │
└─────────────────┘ └─────────────────┘
```

5. Update all four previews live while typing.
6. Use `Your Name` as the preview placeholder when no text has been entered.
7. Highlight the selected font using the application's existing accent/selection styling.
8. Use four clearly different handwriting/script fonts.
9. Bundle suitable open-license fonts with the application rather than depending on fonts installed on the OS.
10. Add the fonts to the Qt resource system.

## 4C — Keep Signature Compatibility

Prefer keeping the current signature storage/output format.

For example, if existing code expects a signature image:

```text
Embedded signature image
        ↓
    image output
        ↓
existing consumers

Typed signature
        ↓
render font to image
        ↓
same image output
        ↓
existing consumers
```

This avoids modifying every report, certificate, and export function.

### Completion Criteria
- Existing signature images still work.
- Typed signatures can be created.
- Four font choices are available.
- Font previews update immediately.
- Selection is visually clear.
- Typed signatures save and reload correctly.
- Existing report/certificate code continues working wherever possible without modification.

---

# Phase 5 — Add Lazy-Loaded Calendar

### Status

Completed — 2026-08-26. Calendar is constructed on first tab activation, initially loads only its visible month, and retains previously loaded months for the session.

### Goal
Prevent Calendar initialization from slowing down My Workspace startup.

This phase should be implemented carefully because simply hiding the Calendar widget is **not** sufficient.

## 5A — Delay Calendar Construction

When `MyWorkspacePage` is created:

```text
Create My Details
Create My Schedule
Do NOT create/populate Calendar
```

Use a lightweight placeholder for the Calendar tab.

When the user first selects Calendar:

```cpp
ensureCalendarInitialized();
```

Only then should the real Calendar widget be constructed.

## 5B — Load Only the Current Month

On first Calendar initialization:

```cpp
loadMonth(currentYear, currentMonth);
```

Do **not** initially load:

- Previous month
- Next month
- Several months
- Entire semester
- Entire year

The initial state should be:

```text
Current month
    ↓
Current-month data only
```

## 5C — Load Other Months on Demand

When the user navigates:

```text
August
   ↓ next
September
   ↓
load September
```

Only request that month's data.

## 5D — Add Session Caching

Where practical:

```cpp
QSet<YearMonth> loadedMonths;
```

or equivalent.

Before loading:

```cpp
if (!loadedMonths.contains(month))
    loadMonth(month);
```

This means:

```text
August → September → August
```

does not unnecessarily load August twice.

## 5E — Separate UI Creation from Data Loading

If the existing Calendar currently does everything in its constructor, refactor toward something like:

```cpp
initializeUi();
loadMonth(year, month);
ensureMonthLoaded(year, month);
```

This makes lazy loading explicit and testable.

### Completion Criteria
- Opening the application does not load Calendar data.
- Opening My Workspace does not load Calendar data.
- Opening My Schedule does not load Calendar data.
- Calendar is initialized only when its tab is selected.
- First Calendar load requests only the current month.
- Other months load only when visited.
- Returning to Calendar does not recreate it.
- Previously loaded months are not unnecessarily queried again.

---

# Phase 6 — Consolidate Navigation

### Status

In progress — 2026-08-26.

### Goal
Replace the old separate navigation destinations with My Workspace.

### Tasks
1. Add one top-level navigation destination: **My Workspace**.
2. Remove the standalone navigation entries for:
   - My Details
   - My Schedule
   - Calendar
3. Normal navigation to My Workspace should open:

```text
My Workspace → My Schedule
```

4. Preserve actions that intentionally open a specific section.

For example:

```cpp
openWorkspace(WorkspaceTab::Details);
openWorkspace(WorkspaceTab::Schedule);
openWorkspace(WorkspaceTab::Calendar);
```

5. Search the entire repository for:
   - Old page enums
   - Page indexes
   - `setCurrentIndex(...)`
   - Navigation actions
   - Sidebar entries
   - Keyboard shortcuts
   - Dashboard links
   - Menu actions
   - Signals that directly reference old pages
6. Redirect those callers before removing obsolete page registrations.

### Completion Criteria
There is only one primary navigation entry:

```text
My Workspace
```

but code can still directly open:

```text
My Workspace → My Details
My Workspace → My Schedule
My Workspace → Calendar
```

when appropriate.

---

# Phase 7 — Cleanup and Refactoring

### Goal
Remove obsolete architecture after the merged version is stable.

### Tasks
1. Remove unused standalone page registrations.
2. Remove obsolete page enum entries if they are no longer required.
3. Remove dead signals and slots.
4. Remove duplicate page headers.
5. Remove unused constructors or wrappers.
6. Eliminate hard-coded page indexes.
7. Consolidate duplicated data-loading logic discovered during the migration.
8. Make sure child widgets are not independently registered as top-level application pages.
9. Update CMake/QRC files for newly bundled fonts.
10. Ensure font license files are included where required.

### Completion Criteria
- No orphaned page code remains.
- No duplicate navigation entries remain.
- No raw page indexes remain where a named enum/API should be used.
- Build produces no new warnings.

---

# Phase 8 — Final Testing and Polish

## Functional Testing

Test:

- My Workspace opens correctly.
- My Schedule is selected by default.
- Schedule editing and saving works.
- Details load and save.
- Campus works.
- Zoom ID works.
- Zoom Password works.
- N/A works.
- Embedded signature images work.
- Existing signatures load.
- Typed signatures work.
- All four fonts work.
- Typed signatures persist after restart.
- Reports/certificates render signatures properly.
- Calendar loads only when opened.
- Current month is the only initial calendar query.
- Month navigation works.
- Calendar cache works.
- Tab switching preserves state.

## UI Testing

Check at:

- Normal desktop width
- Narrow window width
- High-DPI scaling
- Windows
- Linux
- macOS, if available

Pay particular attention to the four signature cards at smaller widths.

## Performance Testing

Compare startup behavior before and after.

Specifically verify that these operations generate **zero Calendar queries**:

```text
Application startup
Opening My Workspace
Using My Schedule
Using My Details
```

Calendar queries should begin only after:

```text
User clicks Calendar
```

---

# Recommended Codex Execution Order

Give Codex the phases individually instead of asking it to implement the entire feature in one pass:

```text
Phase 1  Audit
   ↓
Phase 2  Workspace shell
   ↓
Phase 3  Details + Schedule
   ↓
Phase 4  Signature system
   ↓
Phase 5  Lazy Calendar
   ↓
Phase 6  Navigation consolidation
   ↓
Phase 7  Cleanup
   ↓
Phase 8  Testing
```

A useful checkpoint is **after Phase 3**. At that point, the page-consolidation architecture is established while the more invasive signature and Calendar changes have not yet been introduced, making regressions easier to isolate.
