#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshClassInformationOptions()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classGradeCombo || !m_classLevelCombo
        || !m_classReadingBookCombo || !m_classEssayBookCombo)
    {
        return;
    }

    const std::wstring currentLevel = selectedComboValue(m_classLevelCombo);
    const std::wstring currentReadingBook =
        selectedComboValue(m_classReadingBookCombo);
    const std::wstring currentEssayBook =
        selectedComboValue(m_classEssayBookCombo);
    const std::wstring grade = selectedComboValue(m_classGradeCombo);

    const auto appendChoice = [](ComboBox combo,
                                 std::wstring_view display,
                                 std::wstring_view value) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(value)));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox combo, std::wstring_view value) {
        for (int index = 0;
             index < static_cast<int>(combo.Items().Size());
             ++index)
        {
            const auto item = combo.Items().GetAt(index).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == value)
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };

    m_classLoading = true;
    m_classLevelCombo.Items().Clear();
    appendChoice(m_classLevelCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::levelsForGrade(
             asUtf8(std::wstring_view(grade))
             ))
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(m_classLevelCombo, wideValue, wideValue);
    }
    selectChoice(m_classLevelCombo, currentLevel);

    m_classReadingBookCombo.Items().Clear();
    appendChoice(m_classReadingBookCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::readingBooks(
             asUtf8(std::wstring_view(grade)),
             asUtf8(std::wstring_view(selectedComboValue(m_classLevelCombo)))
             ))
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(
            m_classReadingBookCombo,
            wideValue.empty() ? L"Not set" : wideValue,
            wideValue
            );
    }
    selectChoice(m_classReadingBookCombo, currentReadingBook);

    m_classEssayBookCombo.Items().Clear();
    appendChoice(m_classEssayBookCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::essayBooks(
             asUtf8(std::wstring_view(grade)),
             asUtf8(std::wstring_view(selectedComboValue(m_classLevelCombo)))
             ))
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(
            m_classEssayBookCombo,
            wideValue.empty() ? L"Not set" : wideValue,
            wideValue
            );
    }
    selectChoice(m_classEssayBookCombo, currentEssayBook);
    m_classLoading = false;
}

void MainWindow::refreshClassCoTeacher()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classCoTeacherKrCombo || !m_classCoTeacherEnCombo)
    {
        return;
    }

    m_classCoTeacherLoading = true;
    const auto clearTeacher = [this]() {
        m_classCoTeacherKrCombo.Items().Clear();
        m_classCoTeacherEnCombo.Items().Clear();
        m_classCoTeacherKrCombo.SelectedIndex(-1);
        m_classCoTeacherEnCombo.SelectedIndex(-1);
        m_classCoTeacherRoomTextBox.Text({});
        m_classCoTeacherInternetTypeTextBox.Text({});
        m_classCoTeacherWifiNameTextBox.Text({});
        m_classCoTeacherWifiPasswordTextBox.Text({});
        m_classCoTeacherProjectionTypeTextBox.Text({});
        m_classCoTeacherZoomIdTextBox.Text({});
        m_classCoTeacherZoomPasswordTextBox.Text({});
        m_classCoTeacherSelectedId = -1;
    };

    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        clearTeacher();
        m_classCoTeacherLoading = false;
        return;
    }

    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        clearTeacher();
        m_classCoTeacherLoading = false;
        if (m_classStatusText)
        {
            m_classStatusText.Text(winrt::hstring(
                L"Co-teacher directory could not be loaded: "
                + asWide(loaded.error().message)
                ));
        }
        return;
    }

    auto teachers = *loaded;
    std::sort(
        teachers.begin(),
        teachers.end(),
        [](const auto& left, const auto& right) {
            return classmngr::engine::teacherDisplayLessThan(left, right);
        }
        );

    const auto appendTeacher = [](ComboBox combo,
                                  std::wstring_view display,
                                  int id) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(id));
        combo.Items().Append(item);
    };
    appendTeacher(m_classCoTeacherKrCombo, L"Unassigned", -1);
    appendTeacher(m_classCoTeacherEnCombo, L"Unassigned", -1);
    for (const auto& teacher : teachers)
    {
        std::wstring korean = asWide(teacher.teacherKr);
        std::wstring english = asWide(teacher.teacherEn);
        if (korean.empty())
        {
            korean = asWide(teacher.preferredDisplayName());
        }
        if (english.empty())
        {
            english = asWide(teacher.preferredDisplayName());
        }
        appendTeacher(m_classCoTeacherKrCombo, korean, teacher.id);
        appendTeacher(m_classCoTeacherEnCombo, english, teacher.id);
    }

    const auto selectTeacher = [](ComboBox combo, int teacherId) {
        for (int index = 0;
             index < static_cast<int>(combo.Items().Size());
             ++index)
        {
            const auto item = combo.Items().GetAt(index).try_as<ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == teacherId)
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(0);
    };
    m_classCoTeacherSelectedId = m_classInfo.teacherId;
    selectTeacher(m_classCoTeacherKrCombo, m_classCoTeacherSelectedId);
    selectTeacher(m_classCoTeacherEnCombo, m_classCoTeacherSelectedId);

    m_classCoTeacherRoomTextBox.Text(asWide(m_classInfo.roomNumber));
    m_classCoTeacherInternetTypeTextBox.Text(
        asWide(m_classInfo.internetType)
        );
    m_classCoTeacherWifiNameTextBox.Text(asWide(m_classInfo.wifiName));
    m_classCoTeacherWifiPasswordTextBox.Text(
        asWide(m_classInfo.wifiPassword)
        );
    m_classCoTeacherProjectionTypeTextBox.Text(
        asWide(m_classInfo.projectionType)
        );
    m_classCoTeacherZoomIdTextBox.Text(asWide(m_classInfo.zoomId));
    m_classCoTeacherZoomPasswordTextBox.Text(
        asWide(m_classInfo.zoomPassword)
        );
    m_classCoTeacherLoading = false;
}

std::vector<classmngr::engine::ClassTime>
MainWindow::classScheduleFromForm(bool intensive) const
{
    const auto& days = intensive
        ? m_classIntensiveDayCombos
        : m_classRegularDayCombos;
    const auto& startHours = intensive
        ? m_classIntensiveStartHourCombos
        : m_classRegularStartHourCombos;
    const auto& startMinutes = intensive
        ? m_classIntensiveStartMinuteCombos
        : m_classRegularStartMinuteCombos;
    const auto& startPeriods = intensive
        ? m_classIntensiveStartPeriodCombos
        : m_classRegularStartPeriodCombos;
    const auto& ends = intensive
        ? m_classIntensiveEndCombos
        : m_classRegularEndCombos;
    const std::size_t rowCount = std::min(
        days.size(),
        std::min(
            std::min(startHours.size(), startMinutes.size()),
            std::min(startPeriods.size(), ends.size())
            )
        );
    std::vector<classmngr::engine::ClassTime> result;
    result.reserve(rowCount);
    for (std::size_t index = 0; index < rowCount; ++index)
    {
        result.push_back({
            asUtf8(selectedComboValue(days[index])),
            asUtf8(selectedComboValue(startHours[index]))
                + asUtf8(selectedComboValue(startMinutes[index])) + " "
                + asUtf8(selectedComboValue(startPeriods[index])),
            asUtf8(selectedComboValue(ends[index]))
        });
    }
    return result;
}

} // namespace winrt::ClassMngrWinUI::implementation
