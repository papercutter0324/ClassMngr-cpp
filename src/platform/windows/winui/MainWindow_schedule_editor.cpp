#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::refreshScheduleWorkspace()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_scheduleTabs || !m_scheduleList || !m_scheduleWorkspaceStatusText)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const auto setEnabled = [hasDatabase](auto const& control) {
        if (control)
        {
            control.IsEnabled(hasDatabase);
        }
    };
    setEnabled(m_scheduleList);
    setEnabled(m_scheduleClassSelector);
    setEnabled(m_scheduleTypeCombo);
    setEnabled(m_scheduleDayCombo);
    setEnabled(m_scheduleStartTextBox);
    setEnabled(m_scheduleEndTextBox);
    setEnabled(m_scheduleSaveButton);
    setEnabled(m_scheduleClearButton);

    m_scheduleLoading = true;
    m_scheduleClasses.clear();
    m_scheduleInfos.clear();
    m_scheduleList.Items().Clear();
    m_scheduleClassSelector.Items().Clear();
    m_scheduleEditingKey.clear();

    if (!hasDatabase)
    {
        m_scheduleWorkspaceStatusText.Text(L"No database open.");
        refreshScheduleBoard();
        if (m_scheduleValidationText)
        {
            m_scheduleValidationText.Text({});
            m_scheduleValidationText.Visibility(Visibility::Collapsed);
        }
        m_scheduleLoading = false;
        return;
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classes = repository.list();
    classmngr::engine::ClassScheduleService scheduleService(*m_openDatabase);
    const auto infos = scheduleService.loadScheduleClassInfos();
    if (!classes || !infos)
    {
        const std::string message = !classes
            ? classes.error().message
            : infos.error().message;
        m_scheduleWorkspaceStatusText.Text(winrt::hstring(
            L"Schedules could not be loaded: " + asWide(message)
            ));
        if (m_scheduleValidationText)
        {
            m_scheduleValidationText.Text(winrt::hstring(
                L"Engine loading error: " + asWide(message)
                ));
            m_scheduleValidationText.Visibility(Visibility::Visible);
        }
        m_scheduleLoading = false;
        return;
    }

    m_scheduleClasses = *classes;
    m_scheduleInfos = *infos;
    int previousClassId = -1;
    if (m_scheduleClassSelector.SelectedItem())
    {
        const auto item = m_scheduleClassSelector.SelectedItem().try_as<
            ComboBoxItem>();
        previousClassId = item ? boxedInt(item.Tag()) : -1;
    }

    const auto className = [this](int classId) {
        for (const auto& classroom : m_scheduleClasses)
        {
            if (classroom.id == classId)
            {
                const std::wstring name = asWide(classroom.name);
                return name.empty()
                    ? L"Class " + std::to_wstring(classroom.id)
                    : name;
            }
        }
        return L"Class " + std::to_wstring(classId);
    };
    const auto addCell = [](Grid const& row,
                            std::wstring_view text,
                            uint32_t column) {
        auto cell = TextBlock();
        cell.Text(winrt::hstring(text));
        cell.Margin(Thickness{4.0, 4.0, 4.0, 4.0});
        cell.TextWrapping(TextWrapping::Wrap);
        Grid::SetColumn(cell, static_cast<int32_t>(column));
        row.Children().Append(cell);
    };
    const auto appendRow = [this, &className, &addCell](
                               classmngr::engine::ClassInfo const& info,
                               classmngr::engine::ScheduleType type,
                               classmngr::engine::ClassTime const* time
                               ) {
        const std::wstring name = className(info.classId);
        const std::wstring typeName = scheduleTypeText(type);
        const std::wstring day = time ? asWide(time->day) : L"-";
        const std::wstring start = time ? asWide(time->startTime) : L"-";
        const std::wstring end = time ? asWide(time->endTime) : L"-";
        const std::wstring keyDay = time ? asWide(time->day) : L"";
        const std::wstring keyStart = time ? asWide(time->startTime) : L"";
        const std::wstring keyEnd = time ? asWide(time->endTime) : L"";
        auto row = Microsoft::UI::Xaml::Controls::Grid();
        row.ColumnSpacing(8.0);
        row.MinWidth(680.0);
        const std::array<double, 5> widths{
            190.0, 105.0, 125.0, 120.0, 120.0
        };
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(
                GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
                );
            row.ColumnDefinitions().Append(definition);
        }
        addCell(row, name, 0);
        addCell(row, typeName, 1);
        addCell(row, day, 2);
        addCell(row, start, 3);
        addCell(row, end, 4);
        auto item = Microsoft::UI::Xaml::Controls::ListViewItem();
        const std::wstring key = scheduleSelectionKey(
            info.classId,
            type,
            keyDay,
            keyStart,
            keyEnd
            );
        item.Content(row);
        item.Tag(winrt::box_value(winrt::hstring(key)));
        item.IsTabStop(false);
        setAutomationName(item, L"Schedule row " + name + L" " + typeName);
        m_scheduleList.Items().Append(item);
    };

    int selectedClassIndex = -1;
    std::size_t slotCount = 0;
    for (const auto& info : m_scheduleInfos)
    {
        auto classItem = ComboBoxItem();
        const std::wstring name = className(info.classId);
        classItem.Content(box_value(hstring(name)));
        classItem.Tag(box_value(info.classId));
        setAutomationName(classItem, L"Schedule class " + name);
        m_scheduleClassSelector.Items().Append(classItem);
        if (info.classId == previousClassId)
        {
            selectedClassIndex = static_cast<int>(
                m_scheduleClassSelector.Items().Size() - 1
                );
        }

        for (const auto& time : info.classTimes)
        {
            appendRow(info, classmngr::engine::ScheduleType::Regular, &time);
            ++slotCount;
        }
        for (const auto& time : info.intensiveTimes)
        {
            appendRow(info, classmngr::engine::ScheduleType::Intensive, &time);
            ++slotCount;
        }
        if (info.classTimes.empty() && info.intensiveTimes.empty())
        {
            appendRow(info, classmngr::engine::ScheduleType::Regular, nullptr);
        }
    }
    if (selectedClassIndex < 0 && m_scheduleClassSelector.Items().Size() > 0)
    {
        selectedClassIndex = 0;
    }
    m_scheduleClassSelector.SelectedIndex(selectedClassIndex);
    if (m_scheduleTypeCombo.Items().Size() > 0)
    {
        m_scheduleTypeCombo.SelectedIndex(0);
    }
    if (m_scheduleDayCombo.Items().Size() > 0)
    {
        m_scheduleDayCombo.SelectedIndex(0);
    }
    if (m_scheduleValidationText)
    {
        m_scheduleValidationText.Text({});
        m_scheduleValidationText.Visibility(Visibility::Collapsed);
    }
    m_scheduleWorkspaceStatusText.Text(
        m_scheduleInfos.empty()
            ? L"No classes found for scheduling."
            : winrt::hstring(
                L"Loaded " + std::to_wstring(m_scheduleInfos.size())
                + L" classes and " + std::to_wstring(slotCount)
                + L" schedule slots."
                )
        );
    refreshScheduleBoard();
    m_scheduleLoading = false;
}

winrt::fire_and_forget MainWindow::openScheduleClassEditor(int classId)
{
    auto lifetime = get_strong();
    if (m_ownedDialog || !m_openDatabase || !RootGrid().XamlRoot())
    {
        co_return;
    }

    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Media;

    classmngr::engine::ClassInfoService service(*m_openDatabase);
    const auto loaded = service.load(classId);
    if (!loaded)
    {
        if (m_scheduleWorkspaceStatusText)
        {
            m_scheduleWorkspaceStatusText.Text(winrt::hstring(
                L"Class information could not be loaded: "
                    + asWide(loaded.error().message)
                ));
        }
        co_return;
    }

    classmngr::engine::ClassInfo draft = *loaded;
    const std::string originalGrade = draft.classGrade;
    const std::string originalLevel = draft.classLevel;
    std::string classColor = draft.classColor.empty()
        ? "#FFFFFF"
        : draft.classColor;
    std::string fontColor = draft.fontColor.empty()
        ? "#000000"
        : draft.fontColor;

    auto form = StackPanel();
    form.Spacing(10.0);
    form.MaxWidth(520.0);

    auto title = TextBlock();
    title.Text(L"Edit Class Information");
    title.FontSize(22.0);
    title.FontWeight(Windows::UI::Text::FontWeights::Bold());
    setAutomationName(title, L"Edit Class Information");
    form.Children().Append(title);

    const auto makeReadOnlyField = [&form](std::wstring_view header,
                                            std::string_view value,
                                            std::wstring_view automationName) {
        auto field = TextBox();
        field.Header(box_value(hstring(header)));
        field.Text(asWide(value));
        field.IsReadOnly(true);
        field.IsTabStop(false);
        setAutomationName(field, automationName);
        form.Children().Append(field);
        return field;
    };
    const auto teacherField = makeReadOnlyField(
        L"Korean Teacher",
        draft.teacherKr,
        L"Schedule class Korean teacher"
        );
    const auto roomField = makeReadOnlyField(
        L"Room Number",
        draft.roomNumber,
        L"Schedule class room number"
        );
    static_cast<void>(teacherField);
    static_cast<void>(roomField);

    const auto addChoice = [](ComboBox const& combo,
                              std::string_view value) {
        auto item = ComboBoxItem();
        const hstring text{asWide(value)};
        item.Content(box_value(text));
        item.Tag(box_value(text));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox const& combo,
                                 std::string_view value) {
        for (int index = 0;
             index < static_cast<int>(combo.Items().Size());
             ++index)
        {
            const auto item = combo.Items().GetAt(index)
                .try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == asWide(value))
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };

    auto grade = ComboBox();
    grade.Header(box_value(hstring(L"Class Grade")));
    grade.MinWidth(260.0);
    grade.IsTabStop(true);
    setAutomationName(grade, L"Schedule class grade");
    for (const std::string& value : classmngr::engine::ClassInfoConfig::grades())
    {
        addChoice(grade, value);
    }
    selectChoice(grade, draft.classGrade);
    form.Children().Append(grade);

    auto level = ComboBox();
    level.Header(box_value(hstring(L"Class Level")));
    level.MinWidth(260.0);
    level.IsTabStop(true);
    setAutomationName(level, L"Schedule class level");
    form.Children().Append(level);

    bool loadingOptions = true;
    const auto rebuildLevels = [&]() {
        const std::wstring selected = selectedComboValue(level);
        level.Items().Clear();
        for (const std::string& value :
             classmngr::engine::ClassInfoConfig::levelsForGrade(
                 asUtf8(selectedComboValue(grade))
                 ))
        {
            addChoice(level, value);
        }
        selectChoice(
            level,
            selected.empty() ? draft.classLevel : asUtf8(selected)
            );
    };
    grade.SelectionChanged(
        [&loadingOptions, &rebuildLevels](auto const&, auto const&) {
            if (!loadingOptions)
            {
                rebuildLevels();
            }
        }
        );
    rebuildLevels();
    selectChoice(level, draft.classLevel);
    loadingOptions = false;

    auto classColorRow = StackPanel();
    classColorRow.Orientation(Orientation::Horizontal);
    classColorRow.Spacing(8.0);
    auto classPreview = Button();
    classPreview.Width(38.0);
    classPreview.Height(38.0);
    classPreview.Padding(Thickness{0.0, 0.0, 0.0, 0.0});
    classPreview.IsTabStop(false);
    classPreview.Content(box_value(hstring(L"")));
    setAutomationName(classPreview, L"Schedule class color preview");
    auto classColorButton = Button();
    classColorButton.Content(box_value(hstring(L"Choose Color")));
    setAutomationName(classColorButton, L"Choose schedule class color");
    classColorRow.Children().Append(classPreview);
    classColorRow.Children().Append(classColorButton);
    auto classColorLabel = TextBlock();
    classColorLabel.Text(L"Class Color");
    classColorLabel.VerticalAlignment(VerticalAlignment::Center);
    form.Children().Append(classColorLabel);
    form.Children().Append(classColorRow);

    auto fontColorRow = StackPanel();
    fontColorRow.Orientation(Orientation::Horizontal);
    fontColorRow.Spacing(8.0);
    auto fontPreview = Button();
    fontPreview.Width(38.0);
    fontPreview.Height(38.0);
    fontPreview.Padding(Thickness{0.0, 0.0, 0.0, 0.0});
    fontPreview.IsTabStop(false);
    fontPreview.Content(box_value(hstring(L"")));
    setAutomationName(fontPreview, L"Schedule font color preview");
    auto fontColorButton = Button();
    fontColorButton.Content(box_value(hstring(L"Choose Color")));
    setAutomationName(fontColorButton, L"Choose schedule font color");
    fontColorRow.Children().Append(fontPreview);
    fontColorRow.Children().Append(fontColorButton);
    auto fontColorLabel = TextBlock();
    fontColorLabel.Text(L"Font Color");
    fontColorLabel.VerticalAlignment(VerticalAlignment::Center);
    form.Children().Append(fontColorLabel);
    form.Children().Append(fontColorRow);
    const auto updatePreview = [](Button const& preview,
                                                std::string_view value) {
        preview.Background(SolidColorBrush(uiColorFromHex(value)));
        preview.BorderBrush(
            SolidColorBrush(Windows::UI::Color{255, 128, 128, 128})
            );
        preview.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        preview.CornerRadius(CornerRadius{5.0, 5.0, 5.0, 5.0});
    };
    updatePreview(classPreview, classColor);
    updatePreview(fontPreview, fontColor);

    auto validation = TextBlock();
    validation.TextWrapping(TextWrapping::Wrap);
    validation.Visibility(Visibility::Collapsed);
    validation.Foreground(
        SolidColorBrush(Windows::UI::Color{255, 196, 43, 28})
        );
    setAutomationName(validation, L"Schedule class validation");
    form.Children().Append(validation);

    auto dialog = ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(L"Edit Schedule Cell")));
    dialog.Content(form);
    dialog.PrimaryButtonText(L"Save");
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(ContentDialogButton::Primary);
    int requestedColor = -1;
    const auto requestColor = [&dialog, &requestedColor](int colorIndex) {
        requestedColor = colorIndex;
        dialog.Hide();
    };
    classPreview.Click([requestColor](auto const&, auto const&) {
        requestColor(0);
    });
    classColorButton.Click([requestColor](auto const&, auto const&) {
        requestColor(0);
    });
    fontPreview.Click([requestColor](auto const&, auto const&) {
        requestColor(1);
    });
    fontColorButton.Click([requestColor](auto const&, auto const&) {
        requestColor(1);
    });
    m_ownedDialog = dialog;

    for (;;)
    {
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await dialog.ShowAsync();
        }
        catch (...)
        {
            break;
        }
        if (requestedColor >= 0)
        {
            const int colorIndex = requestedColor;
            requestedColor = -1;
            const std::string& currentColor = colorIndex == 0
                ? classColor : fontColor;
            auto picker = ClassMngrWinUISharedUX::buildColorPickerDialog(
                RootGrid().XamlRoot(),
                colorIndex == 0 ? L"Select Class Color" : L"Select Font Color",
                uiColorFromHex(currentColor),
                colorIndex == 0
                    ? L"Schedule class color picker"
                    : L"Schedule font color picker"
                );
            m_ownedDialog = picker.dialog;
            ContentDialogResult pickerResult = ContentDialogResult::None;
            try
            {
                pickerResult = co_await picker.dialog.ShowAsync();
            }
            catch (...)
            {
                m_ownedDialog = dialog;
                break;
            }
            m_ownedDialog = dialog;
            if (pickerResult == ContentDialogResult::Primary)
            {
                const std::string selected = uiHexFromColor(picker.picker.Color());
                if (colorIndex == 0)
                {
                    classColor = selected;
                    updatePreview(classPreview, classColor);
                }
                else
                {
                    fontColor = selected;
                    updatePreview(fontPreview, fontColor);
                }
            }
            continue;
        }
        if (result != ContentDialogResult::Primary)
        {
            break;
        }

        const std::string newGrade = asUtf8(selectedComboValue(grade));
        const std::string newLevel = asUtf8(selectedComboValue(level));
        draft.classGrade = newGrade;
        draft.classLevel = newLevel;
        draft.classColor = classColor;
        draft.fontColor = fontColor;
        if (newGrade != originalGrade || newLevel != originalLevel)
        {
            draft.readingBook.clear();
            draft.essayBook.clear();
        }

        const auto saved = service.save(draft);
        if (!saved)
        {
            validation.Text(winrt::hstring(
                L"The class information could not be saved: "
                    + asWide(saved.error().message)
                ));
            validation.Visibility(Visibility::Visible);
            continue;
        }

        m_dirtyState.markDirty();
        updateFileCommandState();
        refreshScheduleWorkspace();
        if (m_scheduleWorkspaceStatusText)
        {
            m_scheduleWorkspaceStatusText.Text(
                L"Class information saved."
                );
        }
        break;
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
}

void MainWindow::saveScheduleEntry()
{
    using namespace Microsoft::UI::Xaml;

    if (!m_openDatabase || !m_scheduleClassSelector
        || !m_scheduleStartTextBox || !m_scheduleEndTextBox)
    {
        return;
    }

    const auto selectedClass = m_scheduleClassSelector.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int classId = selectedClass ? boxedInt(selectedClass.Tag()) : -1;
    const std::wstring day = selectedComboValue(m_scheduleDayCombo);
    const std::wstring start = m_scheduleStartTextBox.Text().c_str();
    const std::wstring end = m_scheduleEndTextBox.Text().c_str();
    const auto type = m_scheduleTypeCombo.SelectedIndex() == 1
        ? classmngr::engine::ScheduleType::Intensive
        : classmngr::engine::ScheduleType::Regular;
    const auto showValidation = [this](std::wstring message) {
        if (m_scheduleValidationText)
        {
            m_scheduleValidationText.Text(winrt::hstring(message));
            m_scheduleValidationText.Visibility(
                Microsoft::UI::Xaml::Visibility::Visible
                );
        }
        if (m_scheduleWorkspaceStatusText)
        {
            m_scheduleWorkspaceStatusText.Text(L"Schedule could not be saved.");
        }
    };
    if (classId <= 0 || day.empty() || start.empty() || end.empty())
    {
        showValidation(L"Choose a class, weekday, start time, and end time.");
        return;
    }

    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    const auto loaded = infoService.load(classId);
    if (!loaded)
    {
        showValidation(L"Class information could not be loaded: "
            + asWide(loaded.error().message));
        return;
    }
    classmngr::engine::ClassInfo info = *loaded;
    auto& times = type == classmngr::engine::ScheduleType::Intensive
        ? info.intensiveTimes
        : info.classTimes;
    if (const auto previous = scheduleSelectionFromKey(m_scheduleEditingKey))
    {
        auto& previousTimes = previous->type ==
                classmngr::engine::ScheduleType::Intensive
            ? info.intensiveTimes
            : info.classTimes;
        previousTimes.erase(
            std::remove_if(
                previousTimes.begin(),
                previousTimes.end(),
                [&previous](const classmngr::engine::ClassTime& value) {
                    return value.day == asUtf8(previous->day)
                        && value.startTime == asUtf8(previous->startTime)
                        && value.endTime == asUtf8(previous->endTime);
                }
                ),
            previousTimes.end()
            );
    }
    times.push_back({asUtf8(day), asUtf8(start), asUtf8(end)});

    classmngr::engine::ClassScheduleService scheduleService(*m_openDatabase);
    const auto conflicts = scheduleService.getClassTimeConflicts(
        classId,
        times,
        type
        );
    if (!conflicts)
    {
        showValidation(L"Schedule conflict checking failed: "
            + asWide(conflicts.error().message));
        return;
    }
    if (!conflicts->empty())
    {
        showValidation(
            L"The engine rejected an overlapping schedule with "
            + asWide(conflicts->front().conflictingClassName) + L"."
            );
        return;
    }

    const auto saved = infoService.save(info);
    if (!saved)
    {
        showValidation(L"The engine rejected the schedule: "
            + asWide(saved.error().message));
        return;
    }
    m_scheduleEditingKey.clear();
    refreshScheduleWorkspace();
    if (m_scheduleWorkspaceStatusText)
    {
        m_scheduleWorkspaceStatusText.Text(L"Schedule slot saved.");
    }
}

void MainWindow::clearScheduleEntry()
{
    if (m_scheduleLoading)
    {
        return;
    }
    m_scheduleEditingKey.clear();
    if (m_scheduleList)
    {
        m_scheduleList.SelectedIndex(-1);
    }
    if (m_scheduleStartTextBox)
    {
        m_scheduleStartTextBox.Text({});
    }
    if (m_scheduleEndTextBox)
    {
        m_scheduleEndTextBox.Text({});
    }
    if (m_scheduleValidationText)
    {
        m_scheduleValidationText.Text({});
        m_scheduleValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
    }
    if (m_scheduleWorkspaceStatusText)
    {
        m_scheduleWorkspaceStatusText.Text(
            L"Choose a day and time to add a schedule slot."
            );
    }
}

void MainWindow::previewScheduleImport()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_scheduleImportStatusText)
    {
        return;
    }

    const auto showValidation = [this](std::wstring message) {
        if (m_scheduleImportValidationText)
        {
            m_scheduleImportValidationText.Text(winrt::hstring(message));
            m_scheduleImportValidationText.Visibility(Visibility::Visible);
        }
        if (m_scheduleImportApplyButton)
        {
            m_scheduleImportApplyButton.IsEnabled(false);
        }
        m_scheduleImportPreview.reset();
        m_scheduleImportPreviewReady = false;
        if (m_scheduleImportStatusText)
        {
            m_scheduleImportStatusText.Text(L"Import preview failed.");
        }
    };

    const std::wstring userName = m_scheduleImportUserTextBox.Text().c_str();
    const std::wstring teacher = m_scheduleImportTeacherTextBox.Text().c_str();
    const std::wstring grade = m_scheduleImportGradeTextBox.Text().c_str();
    const std::wstring level = m_scheduleImportLevelTextBox.Text().c_str();
    const std::wstring room = m_scheduleImportRoomTextBox.Text().c_str();
    const std::wstring start = m_scheduleImportStartTextBox.Text().c_str();
    const std::wstring end = m_scheduleImportEndTextBox.Text().c_str();
    const std::vector<std::wstring> days = scheduleImportDays(
        m_scheduleImportDaysTextBox.Text().c_str()
        );
    if (userName.empty() || teacher.empty() || grade.empty() || level.empty()
        || room.empty() || start.empty() || end.empty() || days.empty())
    {
        showValidation(
            L"Enter a profile, Korean teacher, grade, level, room, meeting "
            L"days, start time, and end time before previewing."
            );
        return;
    }

    classmngr::engine::ScheduleImportClassCandidate candidate;
    candidate.teacherKey = asUtf8(teacher);
    candidate.teacherKr = candidate.teacherKey;
    candidate.rooms.push_back(asUtf8(room));
    candidate.classGrade = asUtf8(grade);
    candidate.classLevel = asUtf8(level);
    candidate.sourceCells.push_back("WinUI schedule import");
    for (const std::wstring& day : days)
    {
        candidate.times.push_back({
            asUtf8(day),
            asUtf8(start),
            asUtf8(end)
        });
    }

    m_scheduleImportUser = {};
    m_scheduleImportUser.name = asUtf8(userName);
    m_scheduleImportUser.headerCell = "WinUI";
    m_scheduleImportUser.classes.push_back(std::move(candidate));
    const auto kind = m_scheduleImportKindCombo.SelectedIndex() == 1
        ? classmngr::engine::ScheduleImportKind::Intensive
        : classmngr::engine::ScheduleImportKind::Normal;
    classmngr::engine::ScheduleImportService service(*m_openDatabase);
    const auto preview = service.previewImport(m_scheduleImportUser, kind);
    if (!preview)
    {
        showValidation(L"The engine rejected the preview: "
            + asWide(preview.error().message));
        return;
    }
    if (preview->classes.empty() || preview->teachers.empty())
    {
        showValidation(L"The preview did not produce a class and teacher candidate.");
        return;
    }

    m_scheduleImportPreview = *preview;
    m_scheduleImportPreviewReady = true;
    const bool hasMatchingTeacher = !preview->teachers.front().matchingTeacherIds.empty();
    m_scheduleImportTeacherActionCombo.SelectedIndex(hasMatchingTeacher ? 0 : 1);
    const bool hasSuggestedClass = preview->classes.front().suggestedClassId > 0;
    m_scheduleImportClassActionCombo.SelectedIndex(hasSuggestedClass ? 0 : 1);
    if (m_scheduleImportValidationText)
    {
        m_scheduleImportValidationText.Text({});
        m_scheduleImportValidationText.Visibility(Visibility::Collapsed);
    }
    m_scheduleImportApplyButton.IsEnabled(true);
    m_scheduleImportStatusText.Text(winrt::hstring(
        L"Preview ready: "
        + std::to_wstring(preview->user.classes.size())
        + L" imported class, "
        + std::to_wstring(preview->inventory.classCount)
        + L" existing classes, "
        + (hasMatchingTeacher ? L"matching teacher found" : L"new teacher required")
        + L", "
        + (hasSuggestedClass ? L"suggested existing class" : L"new class suggested")
        + L"."
        ));
}

void MainWindow::applyScheduleImport()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_scheduleImportPreviewReady
        || !m_scheduleImportPreview || !m_scheduleImportStatusText)
    {
        return;
    }
    if (m_scheduleImportPreview->user.classes.empty()
        || m_scheduleImportPreview->teachers.empty()
        || m_scheduleImportPreview->classes.empty())
    {
        return;
    }

    const auto showValidation = [this](std::wstring message) {
        if (m_scheduleImportValidationText)
        {
            m_scheduleImportValidationText.Text(winrt::hstring(message));
            m_scheduleImportValidationText.Visibility(Visibility::Visible);
        }
        if (m_scheduleImportStatusText)
        {
            m_scheduleImportStatusText.Text(L"Import could not be applied.");
        }
    };
    const auto teacherItem = m_scheduleImportTeacherActionCombo.SelectedItem()
        .try_as<ComboBoxItem>();
    const auto classItem = m_scheduleImportClassActionCombo.SelectedItem()
        .try_as<ComboBoxItem>();
    const auto teacherAction = teacherItem
        ? static_cast<classmngr::engine::ScheduleImportTeacherAction>(
            boxedInt(teacherItem.Tag())
            )
        : classmngr::engine::ScheduleImportTeacherAction::Create;
    const auto classAction = classItem
        ? static_cast<classmngr::engine::ScheduleImportClassAction>(
            boxedInt(classItem.Tag())
            )
        : classmngr::engine::ScheduleImportClassAction::CreateNew;
    const auto& teacherPreview = m_scheduleImportPreview->teachers.front();
    const auto& classPreview = m_scheduleImportPreview->classes.front();
    if (teacherAction == classmngr::engine::ScheduleImportTeacherAction::Reuse
        && teacherPreview.matchingTeacherIds.empty())
    {
        showValidation(L"Reuse is unavailable because no matching teacher was found.");
        return;
    }
    if (classAction == classmngr::engine::ScheduleImportClassAction::UpdateExisting
        && classPreview.suggestedClassId <= 0)
    {
        showValidation(L"Update existing is unavailable because the preview found no target.");
        return;
    }

    classmngr::engine::ScheduleImportPlan plan;
    plan.kind = m_scheduleImportPreview->kind;
    plan.selectedUserName = m_scheduleImportPreview->user.name;
    plan.saveProfileNameIfBlank = true;
    plan.unknownCellsAcknowledged = true;
    plan.candidates = m_scheduleImportPreview->user.classes;
    plan.teachers.push_back({
        plan.candidates.front().teacherKey,
        teacherAction,
        teacherAction == classmngr::engine::ScheduleImportTeacherAction::Reuse
            ? teacherPreview.matchingTeacherIds.front()
            : -1,
        plan.candidates.front().rooms.empty()
            ? std::string{}
            : plan.candidates.front().rooms.front()
    });
    plan.classes.push_back({
        0,
        classAction,
        classAction == classmngr::engine::ScheduleImportClassAction::UpdateExisting
            ? classPreview.suggestedClassId
            : -1,
        "#FFFFFF",
        "#000000"
    });

    classmngr::engine::ScheduleImportService service(*m_openDatabase);
    const auto valid = service.validateImport(plan);
    if (!valid)
    {
        showValidation(L"Import validation failed: "
            + asWide(valid.error().message));
        return;
    }
    const auto imported = service.importSchedule(plan);
    if (!imported)
    {
        showValidation(L"Import failed and was rolled back: "
            + asWide(imported.error().message));
        return;
    }

    m_scheduleImportPreviewReady = false;
    m_scheduleImportPreview.reset();
    m_scheduleImportApplyButton.IsEnabled(false);
    if (m_scheduleImportValidationText)
    {
        m_scheduleImportValidationText.Text({});
        m_scheduleImportValidationText.Visibility(Visibility::Collapsed);
    }
    m_scheduleImportStatusText.Text(winrt::hstring(
        L"Import applied atomically: "
        + std::to_wstring(imported->classesCreated)
        + L" classes created, "
        + std::to_wstring(imported->classesUpdated)
        + L" updated, "
        + std::to_wstring(imported->teachersCreated)
        + L" teachers created."
        ));
    refreshScheduleWorkspace();
}

} // namespace winrt::ClassMngrWinUI::implementation
