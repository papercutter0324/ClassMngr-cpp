# Phase 4 — Shared UX and High-Risk Controls

> Progress is tracked in [00-START-HERE.md](00-START-HERE.md).

## Goal

Prove that WinUI 3 can support ClassMngr's densest interaction patterns before
porting feature pages at scale.

## Implementation Sequence

1. Build product-scoped components for form fields, validation presentation,
   empty/error states, cards, filter bars, status surfaces, and autosave state.
2. Evaluate first-party WinUI virtualization primitives for class, roster,
   schedule, and speaking-evaluation data. Approve an external grid dependency
   only through a separate license, maintenance, accessibility, and performance
   review.
3. Prototype the three riskiest editors:
   - schedule/time-slot editing;
   - roster selection, transfer, and keyboard editing;
   - speaking-evaluation scores, pasted ranges, and analytics navigation.
4. Prove keyboard traversal, accelerators, selection, clipboard, undo/redo,
   drag/drop where required, and context menus. Touch and custom pointer
   interaction are outside this program's target feature set.
5. Prove Korean and English IME composition, Unicode grapheme navigation,
   selection, replacement, and validation in every editable control pattern.
6. Test large datasets and require recycling/virtualization so visual and
   view-model counts remain bounded by the visible region.
7. Implement charts with WinUI primitives first. Use a narrowly scoped
   Direct2D/DirectWrite surface only when profiling or fidelity demonstrates a
   real need, and keep its input semantics in the WinUI tree.
8. Add semantic tests for measurement, commands, selection, edit commit/cancel,
   scrolling, focus, validation, and dirty-state behavior.

## Validation

- Control gallery passes at 100–300% DPI, light/dark, English/Korean, keyboard,
  and mouse settings. Touch and high-contrast smoke settings are out of scope.
- Large lists and grids meet explicit frame, allocation, and memory budgets.
- Korean IME and pasted-range behavior pass on a real interactive session.
- Accessibility automation is not a target feature for this program. Standard
  controls retain their platform defaults; no custom accessibility claim is made.
- No prototype duplicates engine rules or persists data directly.

## Exit Gate

The high-risk control patterns are proven with representative data and input,
have an accepted dependency strategy, and meet semantic, IME, DPI, and
performance gates before broad feature migration begins. Touch, high-contrast,
and accessibility-automation validation are out of scope.
