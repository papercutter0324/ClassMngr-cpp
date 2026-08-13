# Dialog Policy

This policy is the Phase 1 contract for the unified dialog system. Feature
migrations happen in later phases.

## Ownership and modality

- Controllers pass the `MainWindow` as the parent.
- Pages and dialogs pass the page or dialog that owns the interaction.
- A null parent is allowed only during startup before the main window exists.
- Service-created dialogs are window-modal and retain the requested parent.

## Prompt behavior

- Information, warning, and error severities map to the corresponding Qt
  information, warning, and critical icons.
- Button placement follows the host platform's ordering through semantic Qt
  button roles.
- Escape always chooses the safe reject action for confirmations.
- A normal confirmation defaults to its accept action. A destructive
  confirmation defaults to its reject action.
- Destructive requests use a specific action verb supplied by the caller. The
  fallback is `Delete`; the safe fallback is `Cancel`.
- Message and details text are treated as plain text. Detailed technical
  information belongs in the expandable details area, not in the primary
  message.
- Closing a confirmation without choosing a button returns `Canceled` rather
  than accepting or rejecting on the caller's behalf.

## File dialog behavior

- macOS uses the native file dialog by default.
- Windows and Linux use Qt's non-native file dialog so
  `FileDialogIconStyle` and the common sidebar locations are available.
- Tests may force the Qt backend on any host.
- A save request with an embedded option such as `Open after saving` uses the
  Qt backend because native dialogs do not expose a portable accessory area.
- Every request has a typed purpose. A successful selection remembers the
  containing directory for that purpose only.
- An explicit initial directory takes precedence over a remembered directory.
- Opened files/directories are returned as canonical paths when possible.
  Save targets use a canonical parent directory and a cleaned filename.
- Save requests opt into overwrite confirmation by default and carry an
  explicit default suffix.

Phase 2 routes teacher profiles, class transfers, workbook imports, signature
images, generated PDFs, report exports, and sub-prep packages through typed
purpose keys. Feature code obtains the interface through
`DialogServices::fileDialogs()`; tests can replace it with a recording fake.

## Testing seam

Features depend on `IUserPromptService` and `IFileDialogService`. Tests use the
recording fakes in `tests/fakes`, script results, and assert the complete typed
request without opening modal UI.
