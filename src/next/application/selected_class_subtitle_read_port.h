#pragma once

#include "next/application/selected_class_subtitle_read_snapshot.h"

namespace ClassMngr::Next::Application
{

using SelectedClassSubtitleReadResult =
    Domain::Result<SelectedClassSubtitleReadSnapshot>;

class SelectedClassSubtitleReadPort
{
public:
    virtual ~SelectedClassSubtitleReadPort() = default;

    [[nodiscard]] virtual SelectedClassSubtitleReadResult
    readSelectedClassSubtitle(
        const Domain::ClassId& classId
        ) const = 0;
};

} // namespace ClassMngr::Next::Application
