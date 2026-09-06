#pragma once

namespace ClassMngrWinUIDialogs
{

// These values deliberately match the three actions exposed by a standard
// ContentDialog.  They are independent of WinUI so shell policy can be
// checked without constructing a XamlRoot.
enum class DialogOutcome
{
    Primary,
    Secondary,
    Cancel,
};

enum class UnsavedChangesDecision
{
    Proceed,
    Save,
    Discard,
    Stay,
};

class DirtyState
{
public:
    void markDirty() noexcept;
    void markClean() noexcept;
    [[nodiscard]] bool isDirty() const noexcept;

private:
    bool m_isDirty{};
};

// A clean document does not need confirmation.  For a dirty document,
// primary requests save, secondary discards, and close/cancel keeps editing.
[[nodiscard]] UnsavedChangesDecision resolveUnsavedChanges(
    DirtyState const& state,
    DialogOutcome outcome
    ) noexcept;

[[nodiscard]] bool runDialogContractChecks() noexcept;

} // namespace ClassMngrWinUIDialogs
