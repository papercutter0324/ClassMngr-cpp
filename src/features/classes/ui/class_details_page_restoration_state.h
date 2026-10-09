#pragma once

#include "domain/models/classroom.h"
#include "next/application/class_details_page_snapshot.h"

#include <utility>

class ClassDetailsPageRestorationState final
{
public:
    ClassDetailsPageRestorationState(
        Classroom classroom,
        ClassMngr::Next::Application::ClassDetailsPageReadSnapshot
            displaySnapshot,
        int scrollPosition
        )
        : m_classroom(std::move(classroom))
        , m_displaySnapshot(std::move(displaySnapshot))
        , m_scrollPosition(scrollPosition)
    {
    }

    [[nodiscard]] const Classroom& classroom() const noexcept
    {
        return m_classroom;
    }

    [[nodiscard]] const ClassMngr::Next::Application::
        ClassDetailsPageReadSnapshot& displaySnapshot() const noexcept
    {
        return m_displaySnapshot;
    }

    [[nodiscard]] int scrollPosition() const noexcept
    {
        return m_scrollPosition;
    }

private:
    const Classroom m_classroom;
    const ClassMngr::Next::Application::ClassDetailsPageReadSnapshot
        m_displaySnapshot;
    const int m_scrollPosition;
};
