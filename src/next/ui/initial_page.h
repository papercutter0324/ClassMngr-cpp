#pragma once

#include "next/ui/page.h"

namespace ClassMngr::Next::Ui
{

class InitialPage final : public Page
{
public:
    explicit InitialPage(const PageContext& context, QWidget* parent = nullptr);
};

}  // namespace ClassMngr::Next::Ui
