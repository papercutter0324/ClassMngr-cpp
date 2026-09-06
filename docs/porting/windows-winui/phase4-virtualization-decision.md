# Phase 4 virtualization decision

## Decision

Phase 4 uses only first-party WinUI 3 controls. No external grid dependency is
approved.

| Product data | WinUI primitive | Recycling contract |
| --- | --- | --- |
| Class list | `ListView` with `ItemsStackPanel` | Container recycling is required; items are lightweight view-model projections. |
| Roster | `ListView` with row templates | The active editor owns only the selected row and visible row containers. |
| Schedule/time slots | `ItemsRepeater` with `StackLayout` | Repeated slots are bound as data; cells do not own schedule rules. |
| Speaking evaluation | `ItemsRepeater` with virtualized rows | The focused cell is the only editable cell; rows and scores remain engine-owned data. |

`ScrollViewer` is retained around each scrolling region.  These patterns use
WinUI focus, automation, clipboard, and input behavior instead of replacing
them with a custom drawing surface.

## External dependency gate

An external grid can be proposed only after a separate review records all of:

1. compatible license and source/provenance;
2. maintenance owner, supported Windows App SDK versions, and removal plan;
3. keyboard, Korean IME, UI Automation, high-contrast, and touch evidence;
4. a representative profiling comparison with the first-party prototype;
5. a dependency approval commit.

Until that review is accepted, a missing first-party capability is a Phase 4
gap to resolve with a small product component, not an authorization to add a
grid package.

## Measurement gate

The gallery's large-data scenarios must show that realized containers and
view-model projections are bounded by the visible region plus a small cache,
not the total source row count.  The Phase 4 semantic test asserts this policy
before any feature page consumes the patterns.
