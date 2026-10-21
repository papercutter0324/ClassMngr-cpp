#pragma once

namespace ClassMngr::Next::Application
{

class InitialSetupClassDetailsRequirements final
{
public:
    [[nodiscard]] constexpr bool complete() const noexcept
    {
        return m_koreanTeacherPresent
            && m_classColorChosen
            && m_gradePresent
            && m_levelPresent;
    }

    [[nodiscard]] constexpr bool koreanTeacherMissing() const noexcept
    {
        return !m_koreanTeacherPresent;
    }

    [[nodiscard]] constexpr bool classColorMissing() const noexcept
    {
        return !m_classColorChosen;
    }

    [[nodiscard]] constexpr bool gradeMissing() const noexcept
    {
        return !m_gradePresent;
    }

    [[nodiscard]] constexpr bool levelMissing() const noexcept
    {
        return !m_levelPresent;
    }

private:
    constexpr InitialSetupClassDetailsRequirements(
        bool koreanTeacherPresent,
        bool classColorChosen,
        bool gradePresent,
        bool levelPresent
        ) noexcept
        : m_koreanTeacherPresent(koreanTeacherPresent),
          m_classColorChosen(classColorChosen),
          m_gradePresent(gradePresent),
          m_levelPresent(levelPresent)
    {
    }

    bool m_koreanTeacherPresent;
    bool m_classColorChosen;
    bool m_gradePresent;
    bool m_levelPresent;

    friend constexpr InitialSetupClassDetailsRequirements
    initialSetupClassDetailsRequirements(
        bool koreanTeacherPresent,
        bool classColorChosen,
        bool gradePresent,
        bool levelPresent
        ) noexcept;
};

[[nodiscard]] constexpr InitialSetupClassDetailsRequirements
initialSetupClassDetailsRequirements(
    bool koreanTeacherPresent,
    bool classColorChosen,
    bool gradePresent,
    bool levelPresent
    ) noexcept
{
    return InitialSetupClassDetailsRequirements(
        koreanTeacherPresent,
        classColorChosen,
        gradePresent,
        levelPresent
        );
}

} // namespace ClassMngr::Next::Application
