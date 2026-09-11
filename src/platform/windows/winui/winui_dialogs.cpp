#include "pch.h"

#include "winui_dialogs.h"

namespace ClassMngrWinUIDialogs
{

void DirtyState::markDirty() noexcept
{
    m_isDirty = true;
}

void DirtyState::markClean() noexcept
{
    m_isDirty = false;
}

bool DirtyState::isDirty() const noexcept
{
    return m_isDirty;
}

UnsavedChangesDecision resolveUnsavedChanges(
    DirtyState const& state,
    DialogOutcome outcome
    ) noexcept
{
    if (!state.isDirty())
    {
        return UnsavedChangesDecision::Proceed;
    }

    switch (outcome)
    {
    case DialogOutcome::Primary:
        return UnsavedChangesDecision::Save;
    case DialogOutcome::Secondary:
        return UnsavedChangesDecision::Discard;
    case DialogOutcome::Cancel:
        return UnsavedChangesDecision::Stay;
    }

    return UnsavedChangesDecision::Stay;
}

bool runDialogContractChecks() noexcept
{
    DirtyState state;
    const bool initiallyClean = !state.isDirty()
        && resolveUnsavedChanges(state, DialogOutcome::Primary)
            == UnsavedChangesDecision::Proceed
        && resolveUnsavedChanges(state, DialogOutcome::Secondary)
            == UnsavedChangesDecision::Proceed
        && resolveUnsavedChanges(state, DialogOutcome::Cancel)
            == UnsavedChangesDecision::Proceed;

    state.markDirty();
    const bool dirtyOutcomes = state.isDirty()
        && resolveUnsavedChanges(state, DialogOutcome::Primary)
            == UnsavedChangesDecision::Save
        && resolveUnsavedChanges(state, DialogOutcome::Secondary)
            == UnsavedChangesDecision::Discard
        && resolveUnsavedChanges(state, DialogOutcome::Cancel)
            == UnsavedChangesDecision::Stay;

    state.markClean();
    return initiallyClean && dirtyOutcomes && !state.isDirty();
}

} // namespace ClassMngrWinUIDialogs
