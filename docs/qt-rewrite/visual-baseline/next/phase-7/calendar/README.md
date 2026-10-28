# Phase 7 Calendar captures

Task ID: `phase7-c33-documentation-20261028`.

This package pairs four Next Calendar captures in `next/` with eight Legacy
captures in `legacy/`. Each image is 1280×920 and covers English/Korean in
Light/Dark. The manifests list the fixture and image states:

- [Next manifest](next/calendar-captures.json)
- [Legacy manifest](legacy/legacy-calendar-captures.json)

## Deterministic fixture

Both pages show October 2026, Monday-first, with Upcoming scope set to This
month and every event type enabled. The fixture contains Campus meeting on
October 6, 09:30–10:30; Term planning on October 15, 09:30–10:30; and the
all-day School holiday on October 26. No campus IDs are attached to the events.

## Capture selectors

Run these from the repository root in the configured macOS Debug build. The
Next selector writes `calendar-captures/` under its configured build directory;
the Legacy selector writes directly to the requested `legacy/` output folder.

```sh
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrNextCalendarPageTests \
  captureEnglishAndKoreanLightAndDark
```

```sh
CLASSMNGR_LEGACY_CALENDAR_CAPTURE_DIR=docs/qt-rewrite/visual-baseline/next/phase-7/calendar/legacy \
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 \
  build/macos-clang-debug/ClassMngrCalendarPageEventMutationParityTests \
  capturesLegacyFullCalendarPageWhenConfigured
```

## Observed differences and limits

- Next uses an Upcoming scope dropdown and compact colored controls; Legacy
  uses scope buttons and larger colored badges. Persisted event colors appear
  as swatches in Next Upcoming rows.
- Event-type filters affect Upcoming Events only. The month grid remains
  complete, matching Legacy behavior.
- Legacy's 840 px minimum month-grid height requires separate grid-top and
  Upcoming scroll captures. Next fits the grid and Upcoming section in one
  view. No clipping was observed.
- Legacy dark-theme grid cells remain light; Next uses the dark application
  palette for the grid.

These captures are qualitative visual references, not pixel-parity claims.
Next's manifest records 35 visible grid days, one retained range, three cached
events, Upcoming limit 10, and zero QML view/root objects after leaving the
page. Process-wide memory snapshots are observational only; they do not measure
page allocation or establish a Phase 9 memory budget.
