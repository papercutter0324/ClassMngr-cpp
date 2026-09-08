# Phase 5 table-parity handoff

Date: 2026-09-08 (Asia/Seoul)

Status: **Handoff prepared; Phase 6 implementation and parity acceptance have
not started.** This record captures the retained Qt geometry and visual rules
that Phase 6 must use. It does not treat the existing WinUI prototypes or
their captures as table-family acceptance evidence.

The parity target is the Qt content area: information hierarchy, geometry,
density, typography, colors, borders, shading, focus/selection/error states,
and interaction. Native WinUI window chrome and unavoidable platform-control
differences remain exceptions only when they are documented and do not change
the workflow or hierarchy.

## Phase 5 Campus Information review

The retained Qt Campus Information surface is **not a table**. It is a campus
selector followed by five tabs and form-style detail content:

- Page content margins are `12, 12, 12, 0`.
- The selector row uses `8 px` spacing. The campus field has a `190 px`
  minimum width and displays the campus name and code together.
- The tab order is Information, Directions, Address, Housing, Maps.
- The information detail uses a top-left, widget-resizable, no-frame scroll
  area. Its styled-panel container has `12 px` margins and its form rows use
  `10 px` spacing.
- Ordinary line fields use a `280 px` minimum width. Multiline fields wrap to
  the widget width and grow within the retained five-to-ten-line policy.
- Address subsections use `8 px` nested-form spacing; the complete-address
  field is at least `280 x 150 px`.
- Housing cards use `12 px` section/card margins and `10 px` internal
  spacing. The map surface uses `12 px` margins and `16 px` spacing.

The Qt source anchors are the [Campus page layout](../../../src/features/campus/ui/campus_dashboard_page_ui.cpp),
[Campus detail/form container](../../../src/features/campus/ui/campus_dashboard_page_detail.cpp),
[Campus information fields](../../../src/features/campus/ui/campus_dashboard_page_information_ui.cpp),
[Campus form behavior](../../../src/features/campus/ui/campus_dashboard_page_form.cpp),
and [shared form constants](../../../src/ui/shared/constants/gui_constants.h).

The current WinUI implementation uses a `32 px` root padding, `16 px`
spacing, `1100 px` maximum width, a `1:2` list/detail grid, a fixed `480 px`
list height, and concatenated `Label: value` text blocks. Those values are
prototype values in
[MainWindow.xaml.cpp](../../../src/platform/windows/winui/MainWindow.xaml.cpp)
and are not derived from Qt. The Phase 6 handoff therefore requires one of
these outcomes before Campus Information is accepted:

1. Rebase the WinUI surface to the Qt selector-plus-five-tabs hierarchy and
   form geometry; or
2. Obtain a separate, explicit owner-approved product exception for the
   list/detail hierarchy, capture that intended design, and use it as a new
   baseline.

The approved Phase 5 exception for missing Qt empty/populated/error fixtures
does **not** approve the current list/detail geometry and does not waive the
Phase 6 visual-parity work.

## Retained Qt table contract

### Shared table styling

The shared table rules are in the [light Qt stylesheet](../../../resources/assets/styles/light.qss)
and [dark Qt stylesheet](../../../resources/assets/styles/dark.qss):

| Token | Light | Dark |
| --- | --- | --- |
| Table background | `#f5f3ee` | `#1e1e1e` |
| Alternate row background | `#e5e4de` | `#2a2a2a` |
| Table text | `#27313a` | `#f0f0f0` |
| Gridline | `#d2d0c9` | `#3d3d3d` |
| Outer border | `1 px #c5c7c3` | `1 px #454545` |
| Selection background/text | `#cbdbe7` / `#1f2d38` | `#0a84ff` / `#ffffff` |
| Header section | `#deded8` / `#546169` | `#303030` / `#c8c8c8` |
| Header section border | right/bottom `1 px #c5c7c3` | right/bottom `1 px #454545` |
| Item/header padding | `4 px` | `4 px` |
| Vertical scrollbar | `10 px`; handle radius `5 px` | `10 px`; handle radius `5 px` |

The generic table item has no extra border. Schedule tables intentionally
remove the outer border and gridline. Do not apply the generic table border to
schedule content or to a form-style Campus detail surface.

The surrounding Qt surface also matters. The light/dark page backgrounds are
`#e9e8e3` / `#2b2b2b`; cards use `#f5f3ee` / `#303030` with a `2 px`
`#d2d0c9` / `#454545` border and an `8 px` radius; and inputs use a `1 px`
border, `6 px` radius, and `5 px 8 px` padding. A focused input uses a `2 px`
`#3daee9` border. The retained font contract is English UI `14 pt` by
default, Korean UI one point larger, with user size offsets applied centrally
by the Qt font manager. These tokens belong in the WinUI shared resources and
must not be replaced by the current generic card defaults without a reviewed
parity decision.

### Feature-family geometry and states

| Qt family | Geometry and headers | Borders, shading, and behavior |
| --- | --- | --- |
| Roster | Rows are `50 px`; grouped header is `40 px` plus `30 px` column header (`70 px` total). Default widths are English `170`, Korean `120`, evaluation columns `130`, other custom columns `100`; Student Information has a `220 px` minimum. | No grid and no alternating rows. Group colors are Student Names `#64A0FF`, Evaluations `#78C878`, and Student Information `#C8C8C8`; column fills are softened toward white. Group and column boundaries use the custom header's separator lines. Headers use centered 11 px/10 px DemiBold text. Selection is extended cell selection; editing is supported. See [roster constants](../../../src/features/roster/ui/roster_constants.h), [header](../../../src/features/roster/ui/roster_header_view.cpp), and [table view](../../../src/features/roster/ui/roster_table_view.cpp). |
| Speaking evaluation | Header is `42 px`; rows are `50 px`. Widths are Index `40`, English/Korean `180` each, score columns `150` each, Comments `500`, Notes `300`. | Per-column fills are Index `#d9d9d9`, names `#ffffff`, Grammar `#d9d2e9`, Pronunciation `#cfe2f3`, Fluency `#f4cccc`, Manner `#fce5cd`, Content `#d9ead3`, Overall Effort `#cfe2f3`, Comments `#eeeeee`, Notes `#e6e0c9`. A `2 px` black separator follows Index, Korean Name, Overall Effort, and Comments; row separators are `1 px` dotted black. Selection, Korean-name caution, error, and dirty overlays are distinct. See [speaking model](../../../src/domain/models/speaking_evaluation.h), [header](../../../src/features/speaking_eval/ui/speaking_eval_header_view.h), and [delegate](../../../src/features/speaking_eval/ui/speaking_eval_delegate.cpp). |
| Schedule | Normal mode: time column `90 px`, header `42 px`, row `48 px`. Compact preview: `84/36/40 px`. The time column is fixed and day columns stretch. | No frame, no grid, no selection, no editing; word wrapping is enabled for slot content. The header uses centered 12 px DemiBold text. Slot backgrounds and teacher/room lines are feature states, not generic table shading. See [schedule renderer](../../../src/features/schedule/ui/schedule_table_renderer.cpp). |
| Staff directory | Native English has six columns; GS Team has five. All columns stretch. Minimum row height is `42 px`; minimum header height is `38 px`, then grows from font metrics plus `16 px` padding. | Alternating rows are enabled; rows use extended row selection. Sorting, moving, and word wrapping are disabled; text elides right. Double-click, edit-key, and selected-click editing are enabled. Headers are centered, fixed in place, and not clickable. See [staff directory](../../../src/features/teacher/ui/staff_directory_page.cpp). |
| Analytics ranking | Fixed columns are Index `40`, English `160`, Korean `140`, Average `90`; six criterion columns stretch. Header is `42 px`; grade badges are `38 x 26 px`. | Read-only single-row selection, no grid, no wrapping. Needs-attention rows, grade badges, `2 px` group separators, and dotted `1 px` row borders are rendered by the custom delegate/header. See [analytics page](../../../src/features/classes/ui/class_analytics_page.cpp), [ranking header](../../../src/features/classes/ui/class_analytics_ranking_header.h), and [ranking delegate](../../../src/features/classes/ui/class_analytics_ranking_delegate.h). |

These are feature-specific contracts on top of the shared stylesheet. A WinUI
implementation must not replace them with one generic row height, one generic
header, or one generic selection color.

## Phase 6 implementation handoff

The first Phase 6 shared work item is to turn the contract above into WinUI
resources and feature-scoped templates:

1. Create shared tokens for table backgrounds, header fills, text, borders,
   selection, focus, hover, dirty, validation, warning, and error states.
2. Create a column-metrics policy that keeps header/body columns aligned during
   resizing, localization, DPI changes, and horizontal scrolling.
3. Use `ListView` for ordinary row collections and `ItemsRepeater` for dense
   repeated rows/cells. Use a `Grid` only inside a realized row or for a
   genuinely small fixed surface; do not materialize a large logical table in
   a page-level grid.
4. Preserve stable row/cell keys, draft values, keyboard traversal, Korean IME
   composition, clipboard behavior, validation, dirty state, focus restoration,
   and database round trips while rows are recycled.
5. Capture paired Qt/WinUI evidence at 100%, 125%, 150%, 200%, and 300% DPI,
   light/dark themes, English/Korean text, populated/empty/error states, and
   representative selection, focus, dirty, and validation states.

The Phase 5 review of the existing Campus list/detail surface is therefore a
required handoff input, not a completed table-parity row. The [Phase 6 plan](../../../plans/windows-winui3-port-plan/phase-6-data-entry-feature-migration.md)
must begin with this reconciliation before roster, schedule, evaluation, or
other table-heavy work is accepted.
