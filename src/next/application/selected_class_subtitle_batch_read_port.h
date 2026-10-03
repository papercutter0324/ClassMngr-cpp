#pragma once

#include "next/application/selected_class_subtitle_read_snapshot.h"

#include <vector>

namespace ClassMngr::Next::Application
{

using SelectedClassSubtitleBatchReadResult =
    Domain::Result<std::vector<SelectedClassSubtitleReadSnapshot>>;

class SelectedClassSubtitleBatchReadPort
{
public:
    virtual ~SelectedClassSubtitleBatchReadPort() = default;

    [[nodiscard]] virtual SelectedClassSubtitleBatchReadResult
    readSelectedClassSubtitles(
        const std::vector<Domain::ClassId>& classIds
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
