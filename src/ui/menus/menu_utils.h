#pragma once

#include <QMenu>
#include <initializer_list>

#include "ui/state/option_state.h"

template<typename T>
void addOptionMenu(
    QMenu* menu,
    OptionState<T>* state,
    std::initializer_list<T> order
    )
{
    if (!menu || !state)
        return;

    for (const auto& value : order)
    {
        if (auto* action = state->action(value))
            menu->addAction(action);
    }
}