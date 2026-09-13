#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"
#include "schedule_workbook_openxlsx_reader.h"

#include <cwctype>

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

namespace
{
std::wstring normalizedScheduleImportUserName(std::wstring_view value)
{
    std::wstring result;
    bool pendingSpace = false;
    for (const wchar_t character : value)
    {
        const bool letterOrNumber =
            (character >= L'0' && character <= L'9')
            || (character >= L'A' && character <= L'Z')
            || (character >= L'a' && character <= L'z')
            || character >= 0x80;
        if (letterOrNumber)
        {
            if (pendingSpace && !result.empty())
            {
                result.push_back(L' ');
            }
            result.push_back(std::towlower(character));
            pendingSpace = false;
        }
        else if (std::iswspace(character) != 0)
        {
            pendingSpace = !result.empty();
        }
    }
    return result;
}
}

void MainWindow::refreshScheduleWorkspace()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_scheduleTabs || !m_scheduleList || !m_scheduleWorkspaceStatusText)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    if (!hasDatabase)
    {
        m_scheduleDisplayMode =
            classmngr::engine::ScheduleReportDisplayMode::Regular;
    }
    else
    {
        classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
        const auto storedMode = settings.load("schedule_display_mode");
        const auto* mode = storedMode
            ? std::get_if<std::string>(&*storedMode)
            : nullptr;
        if (mode)
        {
            if (*mode == "intensive")
            {
                m_scheduleDisplayMode =
                    classmngr::engine::ScheduleReportDisplayMode::Intensive;
            }
            else if (*mode == "testing")
            {
                m_scheduleDisplayMode =
                    classmngr::engine::ScheduleReportDisplayMode::Testing;
            }
            else
            {
                m_scheduleDisplayMode =
                    classmngr::engine::ScheduleReportDisplayMode::Regular;
            }
        }
        else
        {
            const auto legacyMode = settings.load("schedule_show_intensive");
            m_scheduleDisplayMode =
                legacyMode
                    && MainWindowDetail::settingBool(*legacyMode, false)
                ? classmngr::engine::ScheduleReportDisplayMode::Intensive
                : classmngr::engine::ScheduleReportDisplayMode::Regular;
        }
    }
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

winrt::fire_and_forget MainWindow::openScheduleImportDialog()
{
    auto lifetime = get_strong();
    const auto xamlRoot = RootGrid().XamlRoot();
    if (!m_scheduleImportDialogRoot || m_ownedDialog || !xamlRoot)
    {
        co_return;
    }

    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    resetScheduleImportSource();
    m_scheduleImportDialogRoot.Width(420.0);

    auto dialog = ContentDialog();
    dialog.XamlRoot(xamlRoot);
    dialog.Title(box_value(hstring(L"Import Schedule")));
    dialog.Content(m_scheduleImportDialogRoot);
    dialog.PrimaryButtonText(L"Load");
    dialog.IsPrimaryButtonEnabled(false);
    dialog.SecondaryButtonText({});
    dialog.IsSecondaryButtonEnabled(false);
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(ContentDialogButton::Close);
    bool requestNameMismatchConfirmation = false;
    dialog.PrimaryButtonClick(
        [this, &dialog, &requestNameMismatchConfirmation](
            auto const&,
            auto const& arguments
            ) {
            if (m_scheduleImportLoading)
            {
                arguments.Cancel(true);
                return;
            }
            if (m_scheduleImportReviewVisible)
            {
                applyScheduleImport();
                if (m_scheduleImportPreviewReady)
                {
                    arguments.Cancel(true);
                }
            }
            else if (m_scheduleImportWorkbookLoaded
                && m_scheduleImportWorkbook)
            {
                arguments.Cancel(true);
                const auto checked = [](auto const& check) {
                    const auto value = check.IsChecked();
                    return value && value.Value();
                };
                if (hasScheduleImportNameMismatch()
                    && !checked(m_scheduleImportNameConfirmation)
                    && !m_scheduleImportNameMismatchConfirmed)
                {
                    requestNameMismatchConfirmation = true;
                    dialog.Hide();
                    return;
                }
                openScheduleImportReview();
            }
            else
            {
                arguments.Cancel(true);
                loadScheduleImportSource();
            }
        }
        );
    dialog.SecondaryButtonClick(
        [this](auto const&, auto const& arguments) {
            if (m_scheduleImportReviewVisible)
            {
                arguments.Cancel(true);
                restoreScheduleImportSource();
            }
        }
        );
    for (;;)
    {
        m_ownedDialog = dialog;
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await dialog.ShowAsync();
        }
        catch (...)
        {
            // Dialog cancellation during navigation or shell teardown is normal.
            break;
        }

        if (requestNameMismatchConfirmation)
        {
            requestNameMismatchConfirmation = false;
            auto confirmation = ContentDialog();
            confirmation.XamlRoot(xamlRoot);
            confirmation.Title(box_value(hstring(L"Name Mismatch")));
            auto message = TextBlock();
            message.Text(
                L"The selected name does not match the name entered on the "
                L"My Information page. Do you want to continue anyway?"
                );
            message.TextWrapping(TextWrapping::Wrap);
            confirmation.Content(message);
            confirmation.PrimaryButtonText(L"Continue");
            confirmation.CloseButtonText(L"Cancel");
            confirmation.DefaultButton(ContentDialogButton::Primary);
            m_ownedDialog = confirmation;
            ContentDialogResult confirmationResult = ContentDialogResult::None;
            try
            {
                confirmationResult = co_await confirmation.ShowAsync();
            }
            catch (...)
            {
                // Treat teardown as a cancelled confirmation.
            }
            m_ownedDialog = dialog;
            if (confirmationResult == ContentDialogResult::Primary)
            {
                m_scheduleImportNameMismatchConfirmed = true;
                openScheduleImportReview();
            }
            continue;
        }

        if (result == ContentDialogResult::Primary
            && m_scheduleImportReviewVisible
            && !m_scheduleImportPreviewReady)
        {
            // A successful Import hides the dialog after clearing the preview.
            break;
        }
        if (result != ContentDialogResult::Primary)
        {
            break;
        }
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
    resetScheduleImportSource();
}

void MainWindow::cancelScheduleImportLoad()
{
    if (m_scheduleImportLoadCancellation)
    {
        m_scheduleImportLoadCancellation->store(
            true,
            std::memory_order_relaxed
            );
    }
    ++m_scheduleImportLoadRequestId;
    m_scheduleImportLoadCancellation.reset();
}

bool MainWindow::hasScheduleImportNameMismatch() const
{
    const std::wstring profileName = asWide(m_personalDetails.name);
    if (profileName.find_first_not_of(L" \t\r\n") == std::wstring::npos)
    {
        return false;
    }

    return normalizedScheduleImportUserName(
        asWide(m_scheduleImportUser.name)
        ) != normalizedScheduleImportUserName(profileName);
}

void MainWindow::resetScheduleImportSource()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_scheduleImportSourceRoot)
    {
        return;
    }

    cancelScheduleImportLoad();

    // RadioButton and ComboBox notifications are deliberately muted while
    // the source state is being rebuilt.  This keeps a cancelled or reopened
    // dialog at the same first step as the Qt dialog.
    m_scheduleImportLoading = true;
    m_scheduleImportFilePath.clear();
    m_scheduleImportSelectedWorksheet = -1;
    m_scheduleImportSelectedUser = -1;
    m_scheduleImportWorkbookLoaded = false;
    m_scheduleImportWorkbook.reset();
    m_scheduleImportReviewVisible = false;
    m_scheduleImportUser = {};
    m_scheduleImportPreview.reset();
    m_scheduleImportPreviewReady = false;
    m_scheduleImportNameMismatchConfirmed = false;

    m_scheduleImportFilePathTextBox.Text({});
    m_scheduleImportRegularRadioButton.IsChecked(false);
    m_scheduleImportIntensiveRadioButton.IsChecked(false);
    m_scheduleImportWorksheetCombo.Items().Clear();
    m_scheduleImportWorksheetCombo.SelectedIndex(-1);
    m_scheduleImportUserCombo.Items().Clear();
    m_scheduleImportUserCombo.SelectedIndex(-1);
    m_scheduleImportNameConfirmation.IsChecked(false);
    m_scheduleImportNameConfirmation.Visibility(Visibility::Collapsed);
    m_scheduleImportUserStatusText.Text({});
    m_scheduleImportUserStatusText.Visibility(Visibility::Collapsed);
    m_scheduleImportScheduleTypeSection.Visibility(Visibility::Collapsed);
    m_scheduleImportWorksheetSection.Visibility(Visibility::Collapsed);
    m_scheduleImportUserSection.Visibility(Visibility::Collapsed);
    m_scheduleImportProgressBar.Visibility(Visibility::Collapsed);
    m_scheduleImportContinuationHint.Visibility(Visibility::Collapsed);
    m_scheduleImportSourceRoot.Visibility(Visibility::Visible);
    m_scheduleImportReviewRoot.Visibility(Visibility::Collapsed);
    m_scheduleImportSourceStatusText.Text(
        L"Choose a file and schedule type."
        );
    m_scheduleImportStatusText.Text(L"Review is ready.");
    m_scheduleImportValidationText.Text({});
    m_scheduleImportValidationText.Visibility(Visibility::Collapsed);
    m_scheduleImportSourceActionButton.Content(
        box_value(hstring(L"Load"))
        );
    m_scheduleImportSourceActionButton.IsEnabled(false);
    m_scheduleImportApplyButton.IsEnabled(false);
    m_scheduleImportReviewTeacherActionCombos.clear();
    m_scheduleImportReviewTeacherRoomCombos.clear();
    m_scheduleImportReviewClassActionCombos.clear();
    m_scheduleImportReviewClassColors.clear();
    m_scheduleImportReviewFontColors.clear();
    if (m_scheduleImportReviewPreviewHost)
    {
        m_scheduleImportReviewPreviewHost.Children().Clear();
    }
    if (m_scheduleImportReviewClassesHost)
    {
        m_scheduleImportReviewClassesHost.Children().Clear();
    }
    if (m_scheduleImportReviewTeachersHost)
    {
        m_scheduleImportReviewTeachersHost.Children().Clear();
    }
    m_scheduleImportLoading = false;

    if (m_ownedDialog)
    {
        m_ownedDialog.Title(box_value(hstring(L"Import Schedule")));
        m_ownedDialog.PrimaryButtonText(L"Load");
        m_ownedDialog.IsPrimaryButtonEnabled(false);
        m_ownedDialog.SecondaryButtonText({});
        m_ownedDialog.IsSecondaryButtonEnabled(false);
        m_ownedDialog.CloseButtonText(L"Cancel");
        m_ownedDialog.DefaultButton(ContentDialogButton::Close);
    }
}

winrt::fire_and_forget MainWindow::selectScheduleImportFile()
{
    auto lifetime = get_strong();
    if (m_filePickerActive || m_scheduleImportLoading)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            m_scheduleImportSourceStatusText.Text(
                L"The spreadsheet file picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.FileTypeFilter().Append(L".xlsx");
            const auto file = co_await picker.PickSingleFileAsync();
            if (file)
            {
                m_scheduleImportLoading = true;
                m_scheduleImportFilePath = asWString(file.Path());
                m_scheduleImportFilePathTextBox.Text(
                    winrt::hstring(m_scheduleImportFilePath)
                    );
                m_scheduleImportWorkbookLoaded = false;
                m_scheduleImportWorkbook.reset();
                m_scheduleImportSelectedWorksheet = -1;
                m_scheduleImportSelectedUser = -1;
                m_scheduleImportUser = {};
                m_scheduleImportNameMismatchConfirmed = false;
                m_scheduleImportPreview.reset();
                m_scheduleImportPreviewReady = false;
                m_scheduleImportWorksheetCombo.Items().Clear();
                m_scheduleImportUserCombo.Items().Clear();
                m_scheduleImportUserStatusText.Text({});
                m_scheduleImportUserStatusText.Visibility(
                    Microsoft::UI::Xaml::Visibility::Collapsed
                    );
                m_scheduleImportContinuationHint.Visibility(
                    Microsoft::UI::Xaml::Visibility::Collapsed
                    );
                m_scheduleImportRegularRadioButton.IsChecked(false);
                m_scheduleImportIntensiveRadioButton.IsChecked(false);
                m_scheduleImportWorksheetSection.Visibility(
                    Microsoft::UI::Xaml::Visibility::Collapsed
                    );
                m_scheduleImportUserSection.Visibility(
                    Microsoft::UI::Xaml::Visibility::Collapsed
                    );
                m_scheduleImportNameConfirmation.IsChecked(false);
                m_scheduleImportNameConfirmation.Visibility(
                    Microsoft::UI::Xaml::Visibility::Collapsed
                    );
                m_scheduleImportSourceStatusText.Text(
                    L"Ready to read the spreadsheet."
                    );
                m_scheduleImportLoading = false;
                updateScheduleImportSourceState();
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        m_scheduleImportSourceStatusText.Text(winrt::hstring(
            L"The spreadsheet picker could not be opened: "
            + asWide(winrt::to_string(error.message()))
            ));
        m_scheduleImportLoading = false;
        updateScheduleImportSourceState();
    }
    catch (...)
    {
        m_scheduleImportSourceStatusText.Text(
            L"The spreadsheet picker could not be opened."
            );
        m_scheduleImportLoading = false;
        updateScheduleImportSourceState();
    }

    m_filePickerActive = false;
}

void MainWindow::updateScheduleImportSourceState()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_scheduleImportSourceRoot)
    {
        return;
    }
    const auto checked = [](auto const& radio) {
        const auto value = radio.IsChecked();
        return value && value.Value();
    };
    const bool hasPath = !m_scheduleImportFilePath.empty();
    const bool hasKind = checked(m_scheduleImportRegularRadioButton)
        || checked(m_scheduleImportIntensiveRadioButton);

    if (m_scheduleImportLoading)
    {
        m_scheduleImportProgressBar.Visibility(Visibility::Visible);
        m_scheduleImportBrowseButton.IsEnabled(false);
        m_scheduleImportRegularRadioButton.IsEnabled(false);
        m_scheduleImportIntensiveRadioButton.IsEnabled(false);
        m_scheduleImportWorksheetCombo.IsEnabled(false);
        m_scheduleImportUserCombo.IsEnabled(false);
        m_scheduleImportSourceActionButton.IsEnabled(false);
        m_scheduleImportContinuationHint.Visibility(Visibility::Collapsed);
        if (m_ownedDialog)
        {
            m_ownedDialog.IsPrimaryButtonEnabled(false);
        }
        return;
    }

    m_scheduleImportProgressBar.Visibility(Visibility::Collapsed);
    m_scheduleImportBrowseButton.IsEnabled(true);
    m_scheduleImportRegularRadioButton.IsEnabled(hasPath);
    m_scheduleImportIntensiveRadioButton.IsEnabled(hasPath);
    m_scheduleImportWorksheetCombo.IsEnabled(true);
    m_scheduleImportUserCombo.IsEnabled(true);

    if (!hasPath)
    {
        m_scheduleImportScheduleTypeSection.Visibility(Visibility::Collapsed);
        m_scheduleImportWorksheetSection.Visibility(Visibility::Collapsed);
        m_scheduleImportUserSection.Visibility(Visibility::Collapsed);
        m_scheduleImportContinuationHint.Visibility(Visibility::Collapsed);
        m_scheduleImportSourceStatusText.Text(
            L"Choose a file and schedule type."
            );
        m_scheduleImportSourceActionButton.Content(
            box_value(hstring(L"Load"))
            );
        m_scheduleImportSourceActionButton.IsEnabled(false);
        if (m_ownedDialog)
        {
            m_ownedDialog.PrimaryButtonText(L"Load");
            m_ownedDialog.IsPrimaryButtonEnabled(false);
        }
        return;
    }

    m_scheduleImportScheduleTypeSection.Visibility(Visibility::Visible);
    if (!m_scheduleImportWorkbookLoaded || !m_scheduleImportWorkbook)
    {
        m_scheduleImportWorksheetSection.Visibility(Visibility::Collapsed);
        m_scheduleImportUserSection.Visibility(Visibility::Collapsed);
        m_scheduleImportContinuationHint.Visibility(Visibility::Collapsed);
        m_scheduleImportSourceStatusText.Text(
            L"Ready to read the spreadsheet."
            );
        m_scheduleImportSourceActionButton.Content(
            box_value(hstring(L"Load"))
            );
        m_scheduleImportSourceActionButton.IsEnabled(hasKind);
        if (m_ownedDialog)
        {
            m_ownedDialog.PrimaryButtonText(L"Load");
            m_ownedDialog.IsPrimaryButtonEnabled(hasKind);
        }
        return;
    }

    std::size_t visibleSheetCount = 0;
    for (const auto& sheet : m_scheduleImportWorkbook->sheets)
    {
        visibleSheetCount += sheet.visible ? 1u : 0u;
    }

    const auto selectedSheet =
        m_scheduleImportSelectedWorksheet >= 0
        && static_cast<std::size_t>(m_scheduleImportSelectedWorksheet)
            < m_scheduleImportWorkbook->sheets.size()
        ? &m_scheduleImportWorkbook->sheets.at(
            static_cast<std::size_t>(m_scheduleImportSelectedWorksheet)
            )
        : nullptr;
    const bool worksheetReady = selectedSheet && selectedSheet->visible;
    if (!worksheetReady)
    {
        m_scheduleImportUserSection.Visibility(Visibility::Collapsed);
        m_scheduleImportContinuationHint.Visibility(Visibility::Collapsed);
        m_scheduleImportSourceStatusText.Text(
            visibleSheetCount > 1
                ? L"Choose the worksheet to import."
                : L"The workbook contains no visible schedule worksheet."
            );
        m_scheduleImportSourceActionButton.Content(
            box_value(hstring(L"Next"))
            );
        m_scheduleImportSourceActionButton.IsEnabled(false);
        if (m_ownedDialog)
        {
            m_ownedDialog.PrimaryButtonText(L"Next");
            m_ownedDialog.IsPrimaryButtonEnabled(false);
        }
        return;
    }

    if (selectedSheet->users.empty())
    {
        std::wstring diagnostics;
        for (const auto& diagnostic : selectedSheet->diagnostics)
        {
            if (!diagnostics.empty())
            {
                diagnostics.append(L"\n");
            }
            diagnostics.append(asWide(diagnostic.cellReference));
            diagnostics.append(L": ");
            diagnostics.append(asWide(diagnostic.message));
        }
        m_scheduleImportUserSection.Visibility(Visibility::Collapsed);
        m_scheduleImportContinuationHint.Visibility(Visibility::Collapsed);
        m_scheduleImportSourceStatusText.Text(
            diagnostics.empty()
                ? L"The selected worksheet contains no supported user schedules."
                : hstring(diagnostics)
            );
        m_scheduleImportSourceActionButton.Content(
            box_value(hstring(L"Next"))
            );
        m_scheduleImportSourceActionButton.IsEnabled(false);
        if (m_ownedDialog)
        {
            m_ownedDialog.PrimaryButtonText(L"Next");
            m_ownedDialog.IsPrimaryButtonEnabled(false);
        }
        return;
    }

    const bool userReady = m_scheduleImportSelectedUser >= 0
        && static_cast<std::size_t>(m_scheduleImportSelectedUser)
            < selectedSheet->users.size();
    m_scheduleImportSourceStatusText.Text(
        L"Workbook and worksheet are valid."
        );
    m_scheduleImportWorksheetSection.Visibility(
        m_scheduleImportWorksheetCombo.Items().Size() > 1
            ? Visibility::Visible
            : Visibility::Collapsed
        );
    m_scheduleImportUserSection.Visibility(Visibility::Visible);
    m_scheduleImportContinuationHint.Visibility(
        userReady ? Visibility::Visible : Visibility::Collapsed
        );
    m_scheduleImportSourceActionButton.Content(
        box_value(hstring(L"Next"))
        );
    const bool ready = hasKind && worksheetReady && userReady;
    m_scheduleImportSourceActionButton.IsEnabled(ready);
    if (m_ownedDialog)
    {
        m_ownedDialog.PrimaryButtonText(L"Next");
        m_ownedDialog.IsPrimaryButtonEnabled(ready);
    }
}

winrt::fire_and_forget MainWindow::loadScheduleImportSource()
{
    auto lifetime = get_strong();
    if (m_scheduleImportLoading || m_scheduleImportWorkbookLoaded)
    {
        co_return;
    }

    const auto checked = [](auto const& radio) {
        const auto value = radio.IsChecked();
        return value && value.Value();
    };
    const bool intensive = checked(m_scheduleImportIntensiveRadioButton);
    if (m_scheduleImportFilePath.empty()
        || (!intensive && !checked(m_scheduleImportRegularRadioButton)))
    {
        updateScheduleImportSourceState();
        co_return;
    }

    const auto kind = intensive
        ? classmngr::engine::ScheduleImportKind::Intensive
        : classmngr::engine::ScheduleImportKind::Normal;
    const std::wstring path = m_scheduleImportFilePath;
    const std::uint64_t requestId = ++m_scheduleImportLoadRequestId;
    const auto cancellation = std::make_shared<std::atomic_bool>(false);
    m_scheduleImportLoadCancellation = cancellation;

    m_scheduleImportLoading = true;
    m_scheduleImportSourceStatusText.Text(L"Loading workbook...");
    updateScheduleImportSourceState();

    std::optional<classmngr::engine::ScheduleImportWorkbook> loadedWorkbook;
    std::wstring readError;
    try
    {
        co_await winrt::resume_background();
        auto reader = classmngr::winui::makeScheduleWorkbookReader();
        const auto result = reader->read(
            std::filesystem::path(path),
            kind,
            [cancellation]() {
                return cancellation->load(std::memory_order_relaxed);
            }
            );
        if (result)
        {
            loadedWorkbook = std::move(*result);
        }
        else
        {
            readError = asWide(result.error().message);
        }
    }
    catch (...)
    {
        readError = L"The selected workbook could not be read.";
    }

    co_await ResumeOnDispatcherQueue{
        DispatcherQueue(),
        Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal
    };

    if (requestId != m_scheduleImportLoadRequestId
        || cancellation->load(std::memory_order_relaxed))
    {
        co_return;
    }
    m_scheduleImportLoading = false;
    if (m_scheduleImportLoadCancellation == cancellation)
    {
        m_scheduleImportLoadCancellation.reset();
    }
    if (!loadedWorkbook)
    {
        updateScheduleImportSourceState();
        m_scheduleImportSourceStatusText.Text(
            winrt::hstring(readError.empty()
                ? L"The selected workbook could not be read."
                : readError)
            );
        co_return;
    }

    applyScheduleImportWorkbook(std::move(*loadedWorkbook), path);
}

void MainWindow::applyScheduleImportWorkbook(
    classmngr::engine::ScheduleImportWorkbook workbook,
    std::wstring filePath
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    m_scheduleImportWorkbook = std::move(workbook);
    m_scheduleImportFilePath = std::move(filePath);
    m_scheduleImportFilePathTextBox.Text(
        winrt::hstring(m_scheduleImportFilePath)
        );
    m_scheduleImportWorkbookLoaded = true;
    m_scheduleImportSelectedWorksheet = -1;
    m_scheduleImportSelectedUser = -1;
    m_scheduleImportUser = {};
    m_scheduleImportPreview.reset();
    m_scheduleImportPreviewReady = false;

    m_scheduleImportWorksheetCombo.Items().Clear();
    m_scheduleImportUserCombo.Items().Clear();
    std::vector<int> visibleSheetIndexes;
    visibleSheetIndexes.reserve(m_scheduleImportWorkbook->sheets.size());
    for (int index = 0;
         index < static_cast<int>(m_scheduleImportWorkbook->sheets.size());
         ++index)
    {
        if (m_scheduleImportWorkbook->sheets.at(
                static_cast<std::size_t>(index)).visible)
        {
            visibleSheetIndexes.push_back(index);
        }
    }

    if (visibleSheetIndexes.size() > 1)
    {
        auto placeholder = ComboBoxItem();
        placeholder.Content(box_value(hstring(L"Select a worksheet...")));
        placeholder.Tag(box_value(-1));
        setAutomationName(placeholder, L"Select a worksheet");
        m_scheduleImportWorksheetCombo.Items().Append(placeholder);
    }
    for (const int index : visibleSheetIndexes)
    {
        auto item = ComboBoxItem();
        const auto& sheet = m_scheduleImportWorkbook->sheets.at(
            static_cast<std::size_t>(index)
            );
        item.Content(box_value(hstring(asWide(sheet.name))));
        item.Tag(box_value(index));
        setAutomationName(item, asWide(sheet.name));
        m_scheduleImportWorksheetCombo.Items().Append(item);
    }

    if (visibleSheetIndexes.size() == 1)
    {
        m_scheduleImportWorksheetCombo.SelectedIndex(0);
        m_scheduleImportSelectedWorksheet = visibleSheetIndexes.front();
    }
    else
    {
        m_scheduleImportWorksheetCombo.SelectedIndex(-1);
    }

    m_scheduleImportNameMismatchConfirmed = false;
    updateScheduleImportSelectedWorksheet();
    updateScheduleImportSourceState();
}

void MainWindow::updateScheduleImportSelectedWorksheet()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    m_scheduleImportSelectedUser = -1;
    m_scheduleImportUser = {};
    m_scheduleImportUserCombo.Items().Clear();
    m_scheduleImportUserCombo.SelectedIndex(-1);
    m_scheduleImportUserStatusText.Text({});
    m_scheduleImportUserStatusText.Visibility(Visibility::Collapsed);
    m_scheduleImportNameConfirmation.IsChecked(false);
    m_scheduleImportNameConfirmation.Visibility(Visibility::Collapsed);
    m_scheduleImportNameMismatchConfirmed = false;

    if (!m_scheduleImportWorkbookLoaded || !m_scheduleImportWorkbook
        || m_scheduleImportSelectedWorksheet < 0
        || static_cast<std::size_t>(m_scheduleImportSelectedWorksheet)
            >= m_scheduleImportWorkbook->sheets.size())
    {
        return;
    }

    const auto& sheet = m_scheduleImportWorkbook->sheets.at(
        static_cast<std::size_t>(m_scheduleImportSelectedWorksheet)
        );
    if (!sheet.visible || sheet.users.empty())
    {
        return;
    }

    const std::wstring profileName = asWide(m_personalDetails.name);
    const std::wstring normalizedProfile =
        normalizedScheduleImportUserName(profileName);
    int exactIndex = -1;
    int exactCount = 0;
    for (int index = 0; index < static_cast<int>(sheet.users.size()); ++index)
    {
        if (!normalizedProfile.empty()
            && normalizedScheduleImportUserName(
                asWide(sheet.users.at(static_cast<std::size_t>(index)).name)
                ) == normalizedProfile)
        {
            exactIndex = index;
            ++exactCount;
        }
    }

    const bool requireExplicit = normalizedProfile.empty() || exactCount != 1;
    if ((requireExplicit && sheet.users.size() != 1)
        || (requireExplicit && normalizedProfile.empty()))
    {
        auto placeholder = ComboBoxItem();
        placeholder.Content(box_value(hstring(L"Select a detected name...")));
        placeholder.Tag(box_value(-1));
        setAutomationName(placeholder, L"Select a detected name");
        m_scheduleImportUserCombo.Items().Append(placeholder);
    }
    for (int index = 0; index < static_cast<int>(sheet.users.size()); ++index)
    {
        auto item = ComboBoxItem();
        const auto& user = sheet.users.at(static_cast<std::size_t>(index));
        item.Content(box_value(hstring(asWide(user.name))));
        item.Tag(box_value(index));
        setAutomationName(item, asWide(user.name));
        m_scheduleImportUserCombo.Items().Append(item);
    }

    const auto selectUserIndex = [this](int userIndex) {
        for (int index = 0;
             index < static_cast<int>(m_scheduleImportUserCombo.Items().Size());
             ++index)
        {
            const auto item = m_scheduleImportUserCombo.Items().GetAt(index)
                .try_as<ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == userIndex)
            {
                m_scheduleImportUserCombo.SelectedIndex(index);
                return;
            }
        }
    };
    if (exactCount == 1)
    {
        selectUserIndex(exactIndex);
    }
    else if (sheet.users.size() == 1 && !normalizedProfile.empty())
    {
        selectUserIndex(0);
    }
    updateScheduleImportSelectedUser();
}

void MainWindow::updateScheduleImportSelectedUser()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    m_scheduleImportUser = {};
    m_scheduleImportUserStatusText.Text({});
    m_scheduleImportUserStatusText.Visibility(Visibility::Collapsed);
    m_scheduleImportNameConfirmation.Visibility(Visibility::Collapsed);
    m_scheduleImportNameConfirmation.IsChecked(false);

    if (!m_scheduleImportWorkbookLoaded || !m_scheduleImportWorkbook
        || m_scheduleImportSelectedWorksheet < 0
        || static_cast<std::size_t>(m_scheduleImportSelectedWorksheet)
            >= m_scheduleImportWorkbook->sheets.size())
    {
        m_scheduleImportSelectedUser = -1;
        return;
    }
    const auto& sheet = m_scheduleImportWorkbook->sheets.at(
        static_cast<std::size_t>(m_scheduleImportSelectedWorksheet)
        );
    if (m_scheduleImportSelectedUser < 0
        || static_cast<std::size_t>(m_scheduleImportSelectedUser)
            >= sheet.users.size())
    {
        m_scheduleImportSelectedUser = -1;
        return;
    }

    m_scheduleImportUser = sheet.users.at(
        static_cast<std::size_t>(m_scheduleImportSelectedUser)
        );
    const std::wstring profileName = asWide(m_personalDetails.name);
    const bool profileBlank = profileName.find_first_not_of(
        L" \t\r\n"
        ) == std::wstring::npos;
    const bool mismatch = hasScheduleImportNameMismatch();
    if (profileBlank)
    {
        m_scheduleImportUserStatusText.Text(
            L"My Information has no name. The selected spreadsheet name will "
            L"be saved after a successful import."
            );
        m_scheduleImportUserStatusText.Visibility(Visibility::Visible);
    }
    else if (mismatch)
    {
        m_scheduleImportUserStatusText.Text(
            winrt::hstring(L"Entered name on the My Information page: "
                + profileName)
            );
        m_scheduleImportUserStatusText.Visibility(Visibility::Visible);
        m_scheduleImportNameConfirmation.Visibility(Visibility::Visible);
    }
}

void MainWindow::openScheduleImportReview()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_scheduleImportWorkbookLoaded
        || !m_scheduleImportWorkbook
        || m_scheduleImportSelectedWorksheet < 0
        || m_scheduleImportSelectedUser < 0)
    {
        updateScheduleImportSourceState();
        return;
    }

    m_scheduleImportReviewVisible = false;
    previewScheduleImport();
    if (!m_scheduleImportPreviewReady)
    {
        return;
    }

    m_scheduleImportReviewVisible = true;
    m_scheduleImportSourceRoot.Visibility(Visibility::Collapsed);
    m_scheduleImportReviewRoot.Visibility(Visibility::Visible);
    m_scheduleImportDialogRoot.Width(1120.0);
    if (m_ownedDialog)
    {
        m_ownedDialog.Title(box_value(hstring(L"Review & Reconcile")));
        m_ownedDialog.PrimaryButtonText(L"Import");
        m_ownedDialog.SecondaryButtonText(L"Back");
        m_ownedDialog.IsSecondaryButtonEnabled(true);
        m_ownedDialog.CloseButtonText(L"Cancel");
        m_ownedDialog.DefaultButton(ContentDialogButton::Primary);
    }
    rebuildScheduleImportReview();
    updateScheduleImportReviewState();
}

void MainWindow::restoreScheduleImportSource()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    m_scheduleImportReviewVisible = false;
    m_scheduleImportReviewRoot.Visibility(Visibility::Collapsed);
    m_scheduleImportSourceRoot.Visibility(Visibility::Visible);
    m_scheduleImportDialogRoot.Width(420.0);
    if (m_ownedDialog)
    {
        m_ownedDialog.Title(box_value(hstring(L"Import Schedule")));
        m_ownedDialog.SecondaryButtonText({});
        m_ownedDialog.IsSecondaryButtonEnabled(false);
        m_ownedDialog.CloseButtonText(L"Cancel");
        m_ownedDialog.DefaultButton(ContentDialogButton::Close);
    }
    updateScheduleImportSourceState();
}

std::optional<classmngr::engine::ScheduleImportPlan>
MainWindow::currentScheduleImportPlan() const
{
    using namespace Microsoft::UI::Xaml::Controls;
    using classmngr::engine::ScheduleImportClassAction;
    using classmngr::engine::ScheduleImportTeacherAction;

    if (!m_scheduleImportPreviewReady || !m_scheduleImportPreview)
    {
        return std::nullopt;
    }

    const auto checked = [](auto const& check) {
        const auto value = check.IsChecked();
        return value && value.Value();
    };
    const auto selectedTag = [](ComboBox const& combo, int fallback) {
        const auto selected = combo.SelectedItem().try_as<ComboBoxItem>();
        return selected ? boxedInt(selected.Tag()) : fallback;
    };

    classmngr::engine::ScheduleImportPlan plan;
    plan.kind = m_scheduleImportPreview->kind;
    plan.intensiveMode =
        classmngr::engine::ScheduleImportIntensiveMode::UpdateExisting;
    plan.selectedUserName = m_scheduleImportPreview->user.name;
    plan.saveProfileNameIfBlank = true;
    plan.updateProfileName = checked(m_scheduleImportNameConfirmation);
    plan.unknownCellsAcknowledged = true;
    plan.candidates = m_scheduleImportPreview->user.classes;
    plan.intensiveSlotStates = m_scheduleImportPreview->user.intensiveSlotStates;
    plan.diagnostics = m_scheduleImportPreview->user.diagnostics;

    for (std::size_t index = 0;
         index < m_scheduleImportPreview->teachers.size();
         ++index)
    {
        const auto& teacher = m_scheduleImportPreview->teachers[index];
        int actionValue =
            static_cast<int>(ScheduleImportTeacherAction::Create);
        if (index < m_scheduleImportReviewTeacherActionCombos.size())
        {
            actionValue = selectedTag(
                m_scheduleImportReviewTeacherActionCombos[index],
                actionValue
                );
        }
        else if (index == 0)
        {
            actionValue = selectedTag(
                m_scheduleImportTeacherActionCombo,
                actionValue
                );
        }
        if (actionValue < static_cast<int>(ScheduleImportTeacherAction::Reuse)
            || actionValue > static_cast<int>(ScheduleImportTeacherAction::Skip))
        {
            actionValue = static_cast<int>(ScheduleImportTeacherAction::Create);
        }
        const auto action = static_cast<ScheduleImportTeacherAction>(actionValue);
        int targetTeacherId = -1;
        if ((action == ScheduleImportTeacherAction::Reuse
             || action == ScheduleImportTeacherAction::UpdateRoom)
            && !teacher.matchingTeacherIds.empty())
        {
            targetTeacherId = teacher.matchingTeacherIds.front();
        }
        std::string selectedRoom;
        if (index < m_scheduleImportReviewTeacherRoomCombos.size())
        {
            selectedRoom = asUtf8(selectedComboValue(
                m_scheduleImportReviewTeacherRoomCombos[index]
                ));
        }
        if (selectedRoom.empty() && !teacher.importedRooms.empty())
        {
            selectedRoom = teacher.importedRooms.front();
        }
        plan.teachers.push_back({
            teacher.teacherKey,
            action,
            targetTeacherId,
            std::move(selectedRoom)
        });
    }

    for (std::size_t index = 0;
         index < m_scheduleImportPreview->classes.size();
         ++index)
    {
        const auto& classPreview = m_scheduleImportPreview->classes[index];
        int actionValue = static_cast<int>(ScheduleImportClassAction::CreateNew);
        if (index < m_scheduleImportReviewClassActionCombos.size())
        {
            actionValue = selectedTag(
                m_scheduleImportReviewClassActionCombos[index],
                actionValue
                );
        }
        else if (index == 0)
        {
            actionValue = selectedTag(m_scheduleImportClassActionCombo, actionValue);
        }
        if (actionValue < static_cast<int>(ScheduleImportClassAction::UpdateExisting)
            || actionValue > static_cast<int>(ScheduleImportClassAction::Skip))
        {
            actionValue = static_cast<int>(ScheduleImportClassAction::CreateNew);
        }
        const auto action = static_cast<ScheduleImportClassAction>(actionValue);
        std::string classColor = "#FFFFFF";
        std::string fontColor = "#000000";
        if (index < m_scheduleImportReviewClassColors.size())
        {
            classColor = m_scheduleImportReviewClassColors[index];
        }
        else if (classPreview.candidateIndex >= 0
            && static_cast<std::size_t>(classPreview.candidateIndex)
                < m_scheduleImportPreview->user.classes.size())
        {
            const auto& candidate = m_scheduleImportPreview->user.classes.at(
                static_cast<std::size_t>(classPreview.candidateIndex)
                );
            if (!candidate.importedColors.empty())
            {
                classColor = candidate.importedColors.front();
            }
        }
        if (index < m_scheduleImportReviewFontColors.size())
        {
            fontColor = m_scheduleImportReviewFontColors[index];
        }
        plan.classes.push_back({
            classPreview.candidateIndex,
            action,
            action == ScheduleImportClassAction::UpdateExisting
                ? classPreview.suggestedClassId
                : -1,
            std::move(classColor),
            std::move(fontColor)
        });
    }
    return plan;
}

void MainWindow::rebuildScheduleImportReview()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Media;

    if (!m_scheduleImportPreview
        || !m_scheduleImportReviewPreviewHost
        || !m_scheduleImportReviewClassesHost
        || !m_scheduleImportReviewTeachersHost)
    {
        return;
    }

    const auto makeText = [](std::wstring_view text, double fontSize = 0.0) {
        auto value = TextBlock();
        value.Text(winrt::hstring(text));
        value.TextWrapping(TextWrapping::Wrap);
        if (fontSize > 0.0)
        {
            value.FontSize(fontSize);
        }
        return value;
    };
    const auto appendChoice = [](ComboBox const& combo,
                                 std::wstring_view text,
                                 int tag) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(text)));
        item.Tag(box_value(tag));
        setAutomationName(item, text);
        combo.Items().Append(item);
    };
    const auto timeText = [](classmngr::engine::ClassTime const& time) {
        return asWide(time.day) + L" "
            + asWide(time.startTime) + L" - " + asWide(time.endTime);
    };
    const auto candidateFor = [this](int index)
        -> classmngr::engine::ScheduleImportClassCandidate const* {
        if (!m_scheduleImportPreview || index < 0
            || static_cast<std::size_t>(index)
                >= m_scheduleImportPreview->user.classes.size())
        {
            return nullptr;
        }
        return &m_scheduleImportPreview->user.classes.at(
            static_cast<std::size_t>(index)
            );
    };
    const auto candidateTitle = [&candidateFor, &timeText](
                                    int candidateIndex) {
        const auto* candidate = candidateFor(candidateIndex);
        if (!candidate)
        {
            return std::wstring(L"Imported class");
        }
        std::wstring title = asWide(candidate->classGrade) + L" "
            + asWide(candidate->classLevel) + L" — "
            + asWide(candidate->teacherKr);
        if (!candidate->times.empty())
        {
            title += L" (" + timeText(candidate->times.front()) + L")";
        }
        return title;
    };

    m_scheduleImportReviewTeacherActionCombos.clear();
    m_scheduleImportReviewTeacherRoomCombos.clear();
    m_scheduleImportReviewClassActionCombos.clear();
    m_scheduleImportReviewClassColors.clear();
    m_scheduleImportReviewFontColors.clear();
    m_scheduleImportReviewPreviewHost.Children().Clear();
    m_scheduleImportReviewClassesHost.Children().Clear();
    m_scheduleImportReviewTeachersHost.Children().Clear();

    const std::array<std::string, 6> palette{
        "#FFFF99", "#C6E0B4", "#9FE2BF", "#F4B183", "#D9EAD3", "#D9D2E9"
    };
    const auto colorForClassPreview = [this, &palette](std::size_t index) {
        if (!m_scheduleImportPreview
            || index >= m_scheduleImportPreview->classes.size())
        {
            return std::string("#FFFFFF");
        }
        const auto& classPreview = m_scheduleImportPreview->classes[index];
        if (classPreview.candidateIndex >= 0
            && static_cast<std::size_t>(classPreview.candidateIndex)
                < m_scheduleImportPreview->user.classes.size())
        {
            const auto& candidate = m_scheduleImportPreview->user.classes.at(
                static_cast<std::size_t>(classPreview.candidateIndex)
                );
            if (!candidate.importedColors.empty())
            {
                return candidate.importedColors.front();
            }
        }
        return palette[index % palette.size()];
    };
    for (std::size_t index = 0;
         index < m_scheduleImportPreview->classes.size();
         ++index)
    {
        m_scheduleImportReviewClassColors.push_back(
            colorForClassPreview(index)
            );
        m_scheduleImportReviewFontColors.push_back("#000000");
    }

    // Build a compact read-only schedule board before the resolution cards.
    // It deliberately follows the Qt preview's five-weekday shape while
    // remaining renderer-neutral and bounded by the imported time slots.
    std::vector<std::wstring> timeRows;
    for (const auto& candidate : m_scheduleImportPreview->user.classes)
    {
        for (const auto& time : candidate.times)
        {
            const std::wstring label = asWide(time.startTime) + L"\n"
                + asWide(time.endTime);
            if (std::find(timeRows.begin(), timeRows.end(), label)
                == timeRows.end())
            {
                timeRows.push_back(label);
            }
        }
    }
    if (timeRows.empty())
    {
        timeRows.push_back(L"No time\nassigned");
    }
    auto board = Grid();
    board.ColumnSpacing(2.0);
    board.RowSpacing(2.0);
    board.HorizontalAlignment(HorizontalAlignment::Stretch);
    board.MinHeight(280.0);
    auto timeColumn = ColumnDefinition();
    timeColumn.Width(GridLengthHelper::FromValueAndType(
        72.0,
        GridUnitType::Pixel
        ));
    board.ColumnDefinitions().Append(timeColumn);
    for (int column = 0; column < 5; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Star
            ));
        board.ColumnDefinitions().Append(definition);
    }
    for (std::size_t row = 0; row <= timeRows.size(); ++row)
    {
        auto definition = RowDefinition();
        definition.Height(GridLengthHelper::FromValueAndType(
            row == 0 ? 32.0 : 52.0,
            GridUnitType::Pixel
            ));
        board.RowDefinitions().Append(definition);
    }
    const std::array<std::wstring_view, 6> headers{
        L"Time", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday"
    };
    const auto addBoardCell = [&board](std::wstring text,
                                       int row,
                                       int column,
                                       std::string color) {
        auto cell = Border();
        cell.Padding(Thickness{4.0, 3.0, 4.0, 3.0});
        cell.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
        cell.Background(SolidColorBrush(uiColorFromHex(color)));
        auto label = TextBlock();
        label.Text(winrt::hstring(text));
        label.TextWrapping(TextWrapping::Wrap);
        label.TextAlignment(TextAlignment::Center);
        cell.Child(label);
        Grid::SetRow(cell, row);
        Grid::SetColumn(cell, column);
        board.Children().Append(cell);
    };
    for (int column = 0; column < 6; ++column)
    {
        addBoardCell(std::wstring(headers[static_cast<std::size_t>(column)]),
                     0,
                     column,
                     "#30343B");
    }
    const auto dayColumn = [](std::string_view day) {
        if (day == "Monday" || day == "Mon") return 1;
        if (day == "Tuesday" || day == "Tue") return 2;
        if (day == "Wednesday" || day == "Wed") return 3;
        if (day == "Thursday" || day == "Thurs" || day == "Thu") return 4;
        if (day == "Friday" || day == "Fri") return 5;
        return -1;
    };
    std::vector<std::wstring> boardTexts(
        (timeRows.size() + 1) * 6
        );
    for (std::size_t row = 0; row < timeRows.size(); ++row)
    {
        addBoardCell(timeRows[row], static_cast<int>(row + 1), 0, "#20242A");
    }
    for (std::size_t candidateIndex = 0;
         candidateIndex < m_scheduleImportPreview->user.classes.size();
         ++candidateIndex)
    {
        const auto& candidate = m_scheduleImportPreview->user.classes[candidateIndex];
        const std::wstring text = asWide(candidate.classGrade) + L" "
            + asWide(candidate.classLevel) + L"\n"
            + asWide(candidate.teacherKr) + L" - "
            + (candidate.rooms.empty() ? L"" : asWide(candidate.rooms.front()));
        std::string color = palette[candidateIndex % palette.size()];
        for (std::size_t previewIndex = 0;
             previewIndex < m_scheduleImportPreview->classes.size();
             ++previewIndex)
        {
            if (m_scheduleImportPreview->classes[previewIndex].candidateIndex
                == static_cast<int>(candidateIndex))
            {
                color = m_scheduleImportReviewClassColors[previewIndex];
                break;
            }
        }
        for (const auto& time : candidate.times)
        {
            const int column = dayColumn(time.day);
            const std::wstring rowLabel = asWide(time.startTime) + L"\n"
                + asWide(time.endTime);
            const auto rowIt = std::find(timeRows.begin(), timeRows.end(), rowLabel);
            if (column < 1 || rowIt == timeRows.end())
            {
                continue;
            }
            const std::size_t boardIndex =
                (static_cast<std::size_t>(std::distance(timeRows.begin(), rowIt)) + 1)
                * 6 + static_cast<std::size_t>(column);
            if (!boardTexts[boardIndex].empty())
            {
                boardTexts[boardIndex] += L"\n";
            }
            boardTexts[boardIndex] += text;
        }
    }
    for (std::size_t row = 0; row < timeRows.size(); ++row)
    {
        for (int column = 1; column < 6; ++column)
        {
            const std::size_t boardIndex = (row + 1) * 6
                + static_cast<std::size_t>(column);
            addBoardCell(
                boardTexts[boardIndex].empty() ? L"Essay" : boardTexts[boardIndex],
                static_cast<int>(row + 1),
                column,
                boardTexts[boardIndex].empty() ? "#F5F5F5" : "#FFFF99"
                );
        }
    }
    m_scheduleImportReviewPreviewHost.Children().Append(board);

    for (std::size_t index = 0;
         index < m_scheduleImportPreview->classes.size();
         ++index)
    {
        const auto& classPreview = m_scheduleImportPreview->classes[index];
        const auto* candidate = candidateFor(classPreview.candidateIndex);
        if (!candidate)
        {
            continue;
        }
        const std::wstring title = candidateTitle(classPreview.candidateIndex);
        auto card = ClassMngrWinUISharedUX::buildCard({
            hstring(title),
            hstring{},
            hstring(L"Schedule import class resolution")
        });
        auto explanation = makeText(
            classPreview.matchExplanation.empty()
                ? (classPreview.suggestedClassId > 0
                    ? L"One existing class matches the imported schedule."
                    : L"No existing class matches the imported schedule.")
                : asWide(classPreview.matchExplanation)
            );
        card.content.Children().Append(explanation);
        auto action = ComboBox();
        action.Header(box_value(hstring(L"Import Action")));
        action.MinWidth(300.0);
        action.IsTabStop(true);
        if (classPreview.suggestedClassId > 0)
        {
            appendChoice(
                action,
                L"Update suggested: " + title,
                static_cast<int>(classmngr::engine::ScheduleImportClassAction::UpdateExisting)
                );
        }
        appendChoice(
            action,
            L"Create new class",
            static_cast<int>(classmngr::engine::ScheduleImportClassAction::CreateNew)
            );
        appendChoice(
            action,
            L"Skip class",
            static_cast<int>(classmngr::engine::ScheduleImportClassAction::Skip)
            );
        action.SelectedIndex(0);
        action.SelectionChanged(
            [this](auto const&, auto const&) {
                updateScheduleImportReviewState();
            }
            );
        setAutomationName(action, L"Schedule import class action");
        card.content.Children().Append(action);
        m_scheduleImportReviewClassActionCombos.push_back(action);

        auto colorRow = StackPanel();
        colorRow.Orientation(Orientation::Horizontal);
        colorRow.Spacing(8.0);
        auto color = Border();
        color.Width(26.0);
        color.Height(26.0);
        color.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
        color.Background(SolidColorBrush(uiColorFromHex(
            m_scheduleImportReviewClassColors[index]
            )));
        colorRow.Children().Append(makeText(L"Color"));
        colorRow.Children().Append(color);
        card.content.Children().Append(colorRow);
        m_scheduleImportReviewClassesHost.Children().Append(card.root);
    }

    for (std::size_t index = 0;
         index < m_scheduleImportPreview->teachers.size();
         ++index)
    {
        const auto& teacher = m_scheduleImportPreview->teachers[index];
        std::wstring title = asWide(teacher.teacherKr);
        if (!teacher.importedRooms.empty())
        {
            title += L" (" + asWide(teacher.importedRooms.front()) + L")";
        }
        auto card = ClassMngrWinUISharedUX::buildCard({
            hstring(title),
            hstring{},
            hstring(L"Schedule import Korean teacher resolution")
        });
        card.content.Children().Append(makeText(
            teacher.matchingTeacherIds.empty()
                ? L"No existing Korean teacher matches the imported name."
                : L"An existing Korean teacher matches the imported name."
            ));
        auto action = ComboBox();
        action.Header(box_value(hstring(L"Import Action")));
        action.MinWidth(300.0);
        appendChoice(action, L"Reuse existing teacher", 0);
        appendChoice(action, L"Update room", 1);
        appendChoice(action, L"Create new teacher", 2);
        appendChoice(action, L"Skip teacher", 3);
        action.SelectedIndex(
            teacher.matchingTeacherIds.empty() ? 2 : 0
            );
        action.SelectionChanged(
            [this](auto const&, auto const&) {
                updateScheduleImportReviewState();
            }
            );
        setAutomationName(action, L"Schedule import teacher action");
        card.content.Children().Append(action);
        m_scheduleImportReviewTeacherActionCombos.push_back(action);

        auto room = ComboBox();
        room.Header(box_value(hstring(L"Imported Room")));
        room.MinWidth(220.0);
        if (teacher.importedRooms.empty())
        {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(L"No room imported")));
            item.Tag(box_value(hstring(L"")));
            room.Items().Append(item);
        }
        else
        {
            for (const std::string& importedRoom : teacher.importedRooms)
            {
                auto item = ComboBoxItem();
                item.Content(box_value(hstring(asWide(importedRoom))));
                item.Tag(box_value(hstring(asWide(importedRoom))));
                room.Items().Append(item);
            }
        }
        room.SelectedIndex(0);
        room.SelectionChanged(
            [this](auto const&, auto const&) {
                updateScheduleImportReviewState();
            }
            );
        setAutomationName(room, L"Schedule import imported room");
        card.content.Children().Append(room);
        m_scheduleImportReviewTeacherRoomCombos.push_back(room);
        m_scheduleImportReviewTeachersHost.Children().Append(card.root);
    }

    if (!m_scheduleImportReviewClassActionCombos.empty())
    {
        m_scheduleImportClassActionCombo.SelectedIndex(
            m_scheduleImportPreview->classes.front().suggestedClassId > 0
                ? 0
                : 1
            );
    }
    if (!m_scheduleImportReviewTeacherActionCombos.empty())
    {
        m_scheduleImportTeacherActionCombo.SelectedIndex(
            m_scheduleImportPreview->teachers.front().matchingTeacherIds.empty()
                ? 1
                : 0
            );
    }
}

void MainWindow::updateScheduleImportReviewState()
{
    using namespace Microsoft::UI::Xaml;

    const auto plan = currentScheduleImportPlan();
    if (!plan || !m_openDatabase || !m_scheduleImportStatusText)
    {
        if (m_scheduleImportApplyButton)
        {
            m_scheduleImportApplyButton.IsEnabled(false);
        }
        if (m_ownedDialog && m_scheduleImportReviewVisible)
        {
            m_ownedDialog.IsPrimaryButtonEnabled(false);
        }
        return;
    }

    classmngr::engine::ScheduleImportService service(*m_openDatabase);
    const auto valid = service.validateImport(*plan);
    if (!valid)
    {
        if (m_scheduleImportValidationText)
        {
            m_scheduleImportValidationText.Text(winrt::hstring(
                L"Import validation failed: " + asWide(valid.error().message)
                ));
            m_scheduleImportValidationText.Visibility(Visibility::Visible);
        }
        m_scheduleImportStatusText.Text(L"Resolve the highlighted import actions.");
        m_scheduleImportApplyButton.IsEnabled(false);
        if (m_ownedDialog && m_scheduleImportReviewVisible)
        {
            m_ownedDialog.IsPrimaryButtonEnabled(false);
        }
        return;
    }

    int teachersCreated = 0;
    int teachersUpdated = 0;
    int teachersSkipped = 0;
    for (const auto& teacher : plan->teachers)
    {
        using Action = classmngr::engine::ScheduleImportTeacherAction;
        if (teacher.action == Action::Create) ++teachersCreated;
        if (teacher.action == Action::UpdateRoom) ++teachersUpdated;
        if (teacher.action == Action::Skip) ++teachersSkipped;
    }
    int classesCreated = 0;
    int classesUpdated = 0;
    int classesSkipped = 0;
    for (const auto& classResolution : plan->classes)
    {
        using Action = classmngr::engine::ScheduleImportClassAction;
        if (classResolution.action == Action::CreateNew) ++classesCreated;
        if (classResolution.action == Action::UpdateExisting) ++classesUpdated;
        if (classResolution.action == Action::Skip) ++classesSkipped;
    }
    if (m_scheduleImportReviewVisible)
    {
        m_scheduleImportStatusText.Text(
            L"All required resolutions are complete."
            );
    }
    if (m_scheduleImportValidationText)
    {
        m_scheduleImportValidationText.Text(winrt::hstring(
            L"Proposed import: "
            + std::to_wstring(teachersCreated)
            + L" teacher(s) created, "
            + std::to_wstring(teachersUpdated)
            + L" room update(s), "
            + std::to_wstring(teachersSkipped)
            + L" teacher(s) skipped; "
            + std::to_wstring(classesCreated)
            + L" class(es) created, "
            + std::to_wstring(classesUpdated)
            + L" updated, "
            + std::to_wstring(classesSkipped)
            + L" skipped; 0 existing schedule(s) cleared; 0 occupied cell(s) "
              L"acknowledged and ignored."
            ));
        m_scheduleImportValidationText.Visibility(Visibility::Visible);
    }
    m_scheduleImportApplyButton.IsEnabled(true);
    if (m_ownedDialog && m_scheduleImportReviewVisible)
    {
        m_ownedDialog.IsPrimaryButtonEnabled(true);
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

    const bool sourceLoaded = m_scheduleImportWorkbookLoaded
        && m_scheduleImportWorkbook
        && m_scheduleImportSelectedWorksheet >= 0
        && m_scheduleImportSelectedUser >= 0
        && !m_scheduleImportUser.classes.empty();
    if (!sourceLoaded)
    {
        showValidation(L"Load and select a workbook schedule before previewing.");
        return;
    }

    const auto kind = (m_scheduleImportIntensiveRadioButton.IsChecked()
        && m_scheduleImportIntensiveRadioButton.IsChecked().Value())
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
    if (m_scheduleImportReviewVisible)
    {
        rebuildScheduleImportReview();
    }
    updateScheduleImportReviewState();
}

void MainWindow::applyScheduleImport()
{
    using namespace Microsoft::UI::Xaml;

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
    const auto plan = currentScheduleImportPlan();
    if (!plan)
    {
        showValidation(L"The import review is no longer available.");
        return;
    }

    classmngr::engine::ScheduleImportService service(*m_openDatabase);
    const auto valid = service.validateImport(*plan);
    if (!valid)
    {
        showValidation(L"Import validation failed: "
            + asWide(valid.error().message));
        return;
    }
    const auto imported = service.importSchedule(*plan);
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
    m_dirtyState.markDirty();
    updateFileCommandState();
    refreshScheduleWorkspace();
    if (m_ownedDialog)
    {
        try
        {
            m_ownedDialog.Hide();
        }
        catch (...)
        {
            // Dialog teardown is cancellation, not an import failure.
        }
    }
}

} // namespace winrt::ClassMngrWinUI::implementation
