#pragma once

#include "classmngr/engine/schedule_report.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <functional>
#include <string>

namespace ClassMngrWinUIScheduleBoard
{

struct Callbacks
{
    std::function<void(int)> classClicked;
    std::function<void(
        std::wstring day,
        std::wstring timeLabel,
        std::wstring currentState,
        std::wstring defaultState,
        bool slotTogglingEnabled,
        bool testingBlockCreationEnabled
        )> slotClicked;
};

struct RenderOptions
{
    bool enabled = true;
    bool showEnglishNames = false;
    bool compactPreview = false;
};

winrt::Microsoft::UI::Xaml::Controls::Grid create(
    Callbacks callbacks = {}
    );

void render(
    winrt::Microsoft::UI::Xaml::Controls::Grid const& root,
    classmngr::engine::ScheduleReportModel const& model,
    RenderOptions const& options = {}
    );

} // namespace ClassMngrWinUIScheduleBoard
