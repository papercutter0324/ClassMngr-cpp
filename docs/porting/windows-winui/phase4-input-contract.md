# Phase 4 input contract

The control gallery is the acceptance surface for the following WinUI-owned
input behavior. The product must preserve the standard control semantics; no
prototype is permitted to intercept text input and reimplement an editor.

| Capability | Gallery exercise | Pass condition |
| --- | --- | --- |
| Keyboard traversal | Tab/Shift+Tab through shell, form field, roster, and score cells | Tab order is visible, deterministic, and the focused control has an automation name. |
| Accelerators | Standard button invocation with Space/Enter | The focused command is invoked once without bypassing validation/status feedback. |
| Selection and clipboard | Roster selection and the pasted score range | Selected row is announced; tab/newline text fills only the bounded prototype range. WinUI `TextBox` may normalize pasted tabs to spaces, so the score-range parser accepts both separators. |
| Undo/redo | Native `TextBox` editing | Ctrl+Z/Ctrl+Y retain standard text editing behavior. |
| Drag/drop | Roster transfer uses an explicit button until a feature requires physical drag/drop | No custom drag/drop is claimed before that feature's interaction evidence exists. |
| Touch and pointer capture | Standard `ListView`, `TextBox`, and `Button` controls | Selection and text entry remain owned by WinUI; no custom pointer capture is installed. |
| Context menus | Standard text-editing context menus | Text controls keep the platform cut/copy/paste menu and automation pattern. |

The schedule, roster, and speaking prototypes are intentionally composed from
standard WinUI controls so their keyboard, touch, selection, accessibility,
clipboard, undo/redo, and context-menu semantics remain in the WinUI tree.
Feature migration must add a scenario capture for any row-level drag/drop
requirement; it cannot inherit approval from the button-based transfer proof.
