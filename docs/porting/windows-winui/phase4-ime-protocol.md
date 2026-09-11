# Phase 4 Korean IME protocol

Run this protocol in an interactive Windows desktop session with the Korean
Microsoft IME enabled. It applies to the shared form field, editable schedule
slot, roster text boxes, and speaking score/paste entry controls.

1. Start composition with `ㅎ`, continue through a composed Hangul syllable,
   and commit it without moving focus. Verify no intermediate composition text
   is validated or persisted by the prototype.
2. Use left/right movement, Shift+arrow selection, replacement, Backspace, and
   Delete around mixed Korean, English, emoji, and combining-character text.
   Navigation and selection must follow platform grapheme behavior.
3. Replace a selected Korean name and verify that the form validation/status
   surface updates only after the normal `TextBox` text event.
4. Paste a tab/newline range containing Korean and English values into the
   speaking prototype. The bounded visible cells must receive exactly the
   pasted UTF-16 text; excess rows/columns must not create cells.
5. Repeat in both `en-US` and `ko-KR` resource settings and at 100%, 200%,
   and 300% scaling. Inspect automation names with Accessibility Insights.

The phase deliberately relies on WinUI `TextBox` composition, selection,
clipboard, context-menu, and undo/redo behavior. No component transforms text
during composition, and no gallery control persists the result.
