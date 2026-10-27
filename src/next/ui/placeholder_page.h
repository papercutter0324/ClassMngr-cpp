#pragma once

#include "next/ui/page.h"

namespace ClassMngr::Next::Ui
{

class PlaceholderPage final : public Page
{
public:
    explicit PlaceholderPage(
        const PageContext& context,
        QString label,
        QWidget* parent = nullptr
        );

    [[nodiscard]] PageOperationResult<void> saveChanges() override;
    [[nodiscard]] PageOperationResult<void> discardChanges() override;
};

}  // namespace ClassMngr::Next::Ui
