#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::ClassSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }
    if (m_classRosterDirty && !m_classDirty && !m_speakingEvaluationDirty)
    {
        const auto selected = sender.try_as<
            Microsoft::UI::Xaml::Controls::ComboBox>();
        const auto item = selected
            ? selected.SelectedItem().try_as<
                Microsoft::UI::Xaml::Controls::ComboBoxItem>()
            : nullptr;
        if (!selected || !item)
        {
            return;
        }
        const int requestedId = boxedInt(item.Tag());
        m_classLoading = true;
        m_classSelector.SelectedIndex(m_classSelectedIndex);
        m_classLoading = false;
        confirmClassRosterNavigation([this, requestedId]() {
            selectClassFromNavigation(requestedId);
        });
        return;
    }
    if (m_classDirty || m_speakingEvaluationDirty)
    {
        m_classLoading = true;
        m_classSelector.SelectedIndex(m_classSelectedIndex);
        m_classLoading = false;
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before selecting another."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before selecting another."
                    : L"Save or discard the current class before selecting another."
            );
        return;
    }

    const auto selected = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (!selected)
    {
        return;
    }
    const auto item = selected.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int selectedId = item ? boxedInt(item.Tag()) : -1;
    int resolvedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_classes.size()); ++index)
    {
        if (m_classes[static_cast<std::size_t>(index)].id == selectedId)
        {
            resolvedIndex = index;
            break;
        }
    }
    presentClass(resolvedIndex);
    clearClassDirty();
    m_classStatusText.Text(
        resolvedIndex >= 0
            ? L""
            : L"No class selected."
        );
    m_classNotesStatusText.Text(
        resolvedIndex >= 0
            ? L"Select a class tab to edit notes."
            : L"No class selected."
    );
    refreshClassRoster();
    refreshClassNavigation(false);
}

void MainWindow::ClassField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }
    if (sender == m_classColorTextBox && m_classColorPreview)
    {
        m_classColorPreview.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                uiColorFromHex(asUtf8(sender.Text()))
                )
            );
    }
    if (sender == m_classNotesTextBox
        || sender == m_classTimeFillerActivitiesTextBox)
    {
        m_classNotesDirty = true;
    }
    else
    {
        m_classDetailsDirty = true;
    }
    markClassDirty();
}

void MainWindow::ClassField_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }
    const auto combo = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (combo == m_classGradeCombo || combo == m_classLevelCombo)
    {
        refreshClassInformationOptions();
    }
    m_classDetailsDirty = true;
    markClassDirty();
}

void MainWindow::ClassNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_classDirty || m_classRosterDirty
        || m_speakingEvaluationDirty)
    {
        if (m_classStatusText && (m_classDirty || m_classRosterDirty
            || m_speakingEvaluationDirty))
        {
            m_classStatusText.Text(
                m_speakingEvaluationDirty
                    ? L"Save or discard the current speaking evaluation before creating another."
                    : m_classRosterDirty
                        ? L"Save or discard the current roster before creating another."
                        : L"Save or discard the current class before creating another."
                );
        }
        return;
    }

    m_classNew = true;
    m_classSelectedIndex = -1;
    m_classSelectedId = -1;
    m_classInfo = {};
    m_classLoading = true;
    m_classSelector.SelectedIndex(-1);
    m_classLoading = false;
    presentClass(-1);
    refreshClassNavigation(false);
    m_classDetailsDirty = true;
    m_classNotesDirty = false;
    m_classDirty = true;
    m_dirtyState.markDirty();
    m_classStatusText.Text(
        L"New class. Enter a name and class information, then save."
        );
    m_classNotesStatusText.Text(L"Save the new class before editing notes.");
    m_classValidationText.Text({});
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    refreshClassRoster();
    updateClassActions();
}

void MainWindow::ClassDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_classSelectedId <= 0
        || m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        return;
    }

    const int classId = m_classSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete class",
        L"Delete the selected class and its saved information?",
        L"Delete",
        {},
        L"Cancel",
        [weak, classId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::ClassRepository repository(
                    *self->m_openDatabase
                    );
                const auto removed = repository.remove(classId);
                if (!removed)
                {
                    self->m_classStatusText.Text(winrt::hstring(
                        L"Class could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_classSelectedId = -1;
                self->m_classSelectedIndex = -1;
                self->m_classNew = false;
                self->clearClassDirty();
                self->refreshClassesPage();
                self->m_classStatusText.Text(L"Class deleted.");
            }
        }
        );
}

void MainWindow::ClassSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_classStatusText.Text(L"No database open.");
        return;
    }

    const std::wstring className = asWString(m_classNameTextBox.Text());
    if (className.find_first_not_of(L" \t\r\n") == std::wstring::npos)
    {
        m_classStatusText.Text(L"Class could not be saved.");
        m_classValidationText.Text(L"A class name is required.");
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classDetailsDirty = true;
        markClassDirty();
        return;
    }

    classmngr::engine::ClassInfo info = classInfoFromForm();
    info.classId = m_classNew ? 1 : m_classSelectedId;
    const auto normalized = classmngr::engine::ClassInfoValidator::normalized(info);
    const auto validation = classmngr::engine::ClassInfoValidator::validate(
        normalized
        );
    if (validation.hasErrors())
    {
        std::wstring summary = L"Engine validation failed:";
        for (const auto& issue : validation.errors())
        {
            summary += L"\n- ";
            summary += asWide(issue.code);
            if (!issue.field.empty())
            {
                summary += L" (" + asWide(issue.field) + L")";
            }
        }
        m_classStatusText.Text(L"Class could not be saved.");
        m_classValidationText.Text(winrt::hstring(summary));
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classDetailsDirty = true;
        markClassDirty();
        return;
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    int classId = m_classSelectedId;
    bool created = false;
    if (m_classNew)
    {
        const auto newId = repository.create(
            asUtf8(std::wstring_view(className))
            );
        if (!newId)
        {
            m_classStatusText.Text(winrt::hstring(
                L"Class could not be created: " + asWide(newId.error().message)
                ));
            return;
        }
        classId = *newId;
        created = true;
    }
    info.classId = classId;

    classmngr::engine::ClassInfoService service(*m_openDatabase);
    const auto saved = service.save(info);
    if (!saved)
    {
        if (created)
        {
            static_cast<void>(repository.remove(classId));
        }
        m_classStatusText.Text(winrt::hstring(
            L"Class could not be saved: " + asWide(saved.error().message)
            ));
        m_classValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classDetailsDirty = true;
        markClassDirty();
        return;
    }

    if (!created
        && classId > 0
        && m_classSelectedIndex >= 0
        && m_classSelectedIndex < static_cast<int>(m_classes.size())
        && m_classes[static_cast<std::size_t>(m_classSelectedIndex)].name
            != asUtf8(std::wstring_view(className)))
    {
        const auto renamed = repository.rename(
            classId,
            asUtf8(std::wstring_view(className))
            );
        if (!renamed)
        {
            m_classStatusText.Text(winrt::hstring(
                L"Class name could not be saved: "
                + asWide(renamed.error().message)
                ));
            m_classDetailsDirty = true;
            markClassDirty();
            return;
        }
    }

    m_classSelectedId = classId;
    m_classNew = false;
    clearClassDirty();
    refreshClassesPage();
    m_classStatusText.Text(L"Class information saved.");
    m_classNotesStatusText.Text(L"Class notes are ready to edit.");
    m_classValidationText.Text({});
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::ClassDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    m_classNew = false;
    clearClassDirty();
    refreshClassesPage();
}

void MainWindow::ClassNotesSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_classNotesStatusText.Text(L"No database open.");
        return;
    }
    if (m_classSelectedId <= 0)
    {
        m_classNotesStatusText.Text(L"Select a class before saving notes.");
        return;
    }
    if (m_classDetailsDirty || m_classNew)
    {
        m_classNotesStatusText.Text(
            L"Save class information before saving notes."
            );
        return;
    }

    classmngr::engine::ClassInfoService service(*m_openDatabase);
    const auto saved = service.saveNotes(
        m_classSelectedId,
        asUtf8(m_classNotesTextBox.Text()),
        asUtf8(m_classTimeFillerActivitiesTextBox.Text())
        );
    if (!saved)
    {
        m_classNotesStatusText.Text(winrt::hstring(
            L"Class notes could not be saved: "
            + asWide(saved.error().message)
            ));
        m_classNotesValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_classNotesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classNotesDirty = true;
        markClassDirty();
        return;
    }

    m_classInfo.notes = asUtf8(m_classNotesTextBox.Text());
    m_classInfo.timeFillerActivities = asUtf8(
        m_classTimeFillerActivitiesTextBox.Text()
        );
    m_classNotesDirty = false;
    m_classDirty = m_classDetailsDirty;
    if (!m_classDirty && !m_classRosterDirty)
    {
        m_dirtyState.markClean();
    }
    m_classNotesStatusText.Text(L"Class notes saved.");
    m_classNotesValidationText.Text({});
    m_classNotesValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateClassActions();
}

void MainWindow::ClassNotesDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_classSelectedId <= 0)
    {
        return;
    }
    m_classLoading = true;
    m_classNotesTextBox.Text(asWide(m_classInfo.notes));
    m_classTimeFillerActivitiesTextBox.Text(
        asWide(m_classInfo.timeFillerActivities)
        );
    m_classLoading = false;
    m_classNotesDirty = false;
    m_classDirty = m_classDetailsDirty;
    if (!m_classDirty && !m_classRosterDirty)
    {
        m_dirtyState.markClean();
    }
    m_classNotesStatusText.Text(L"Class note changes discarded.");
    m_classNotesValidationText.Text({});
    m_classNotesValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateClassActions();
}

void MainWindow::importClassRosterScores()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        if (m_classRosterStatusText)
        {
            m_classRosterStatusText.Text(
                L"Save the selected class before importing scores."
                );
        }
        return;
    }

    classmngr::engine::Roster roster = classRosterFromForm();
    const auto columnIndex = [&roster](std::string_view name) {
        for (std::size_t index = 0; index < roster.columns.size(); ++index)
        {
            if (roster.columns[index] == name)
            {
                return static_cast<int>(index);
            }
        }
        return -1;
    };
    const int englishColumn = columnIndex("English");
    const int koreanColumn = columnIndex("Korean");
    if (englishColumn < 0 || koreanColumn < 0)
    {
        m_classRosterStatusText.Text(
            L"Roster must contain English and Korean columns to import scores."
            );
        return;
    }

    classmngr::engine::SpeakingEvaluationPersistenceService service(
        *m_openDatabase
        );
    int imported = 0;
    for (const std::string_view evaluation :
         classmngr::engine::SpeakingEvaluationNames)
    {
        const int evaluationColumn = columnIndex(evaluation);
        if (evaluationColumn < 0)
        {
            continue;
        }

        const auto scores = service.buildRosterScoreImport(
            m_classSelectedId,
            evaluation
            );
        if (!scores)
        {
            m_classRosterStatusText.Text(winrt::hstring(
                L"Scores could not be imported: " + asWide(scores.error().message)
                ));
            return;
        }

        std::map<std::string, std::string> scoresByStudent;
        for (const auto& score : *scores)
        {
            scoresByStudent.emplace(
                classmngr::engine::StudentNameService::namePairKey(
                    score.englishName,
                    score.koreanName
                    ),
                score.finalGrade
                );
        }
        for (auto& row : roster.rows)
        {
            if (static_cast<std::size_t>(englishColumn) >= row.size()
                || static_cast<std::size_t>(koreanColumn) >= row.size()
                || static_cast<std::size_t>(evaluationColumn) >= row.size())
            {
                continue;
            }
            const auto score = scoresByStudent.find(
                classmngr::engine::StudentNameService::namePairKey(
                    row[static_cast<std::size_t>(englishColumn)],
                    row[static_cast<std::size_t>(koreanColumn)]
                    )
                );
            if (score != scoresByStudent.end()
                && row[static_cast<std::size_t>(evaluationColumn)]
                    != score->second)
            {
                row[static_cast<std::size_t>(evaluationColumn)] = score->second;
                ++imported;
            }
        }
    }

    if (imported == 0)
    {
        m_classRosterStatusText.Text(
            L"Scores are already current or no saved evaluations match this roster."
            );
        return;
    }

    m_classRoster = std::move(roster);
    padRosterRows(m_classRoster);
    rebuildClassRosterGrid();
    markClassRosterDirty();
    saveClassRoster();
    if (!m_classRosterDirty)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Imported " + std::to_wstring(imported)
                + L" evaluation score" + (imported == 1 ? L"." : L"s.")
            ));
    }
}

void MainWindow::ClassCoTeacherSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || m_classCoTeacherLoading || !m_openDatabase)
    {
        return;
    }

    using namespace Microsoft::UI::Xaml::Controls;
    const auto combo = sender.try_as<ComboBox>();
    if (!combo
        || (combo != m_classCoTeacherKrCombo
            && combo != m_classCoTeacherEnCombo))
    {
        return;
    }

    const auto item = combo.SelectedItem().try_as<ComboBoxItem>();
    const int teacherId = item ? boxedInt(item.Tag()) : -1;
    m_classCoTeacherSelectedId = teacherId;
    m_classCoTeacherLoading = true;

    const auto selectTeacher = [](ComboBox target, int id) {
        for (int index = 0;
             index < static_cast<int>(target.Items().Size());
             ++index)
        {
            const auto candidate = target.Items().GetAt(index)
                .try_as<ComboBoxItem>();
            if (candidate && boxedInt(candidate.Tag()) == id)
            {
                target.SelectedIndex(index);
                return;
            }
        }
        target.SelectedIndex(0);
    };
    selectTeacher(m_classCoTeacherKrCombo, teacherId);
    selectTeacher(m_classCoTeacherEnCombo, teacherId);

    classmngr::engine::Teacher teacher;
    if (teacherId > 0)
    {
        classmngr::engine::TeacherService service(*m_openDatabase);
        const auto loaded = service.get(teacherId);
        if (loaded)
        {
            teacher = *loaded;
        }
    }
    m_classCoTeacherRoomTextBox.Text(asWide(teacher.roomNumber));
    m_classCoTeacherInternetTypeTextBox.Text(
        asWide(teacher.internetType)
        );
    m_classCoTeacherWifiNameTextBox.Text(asWide(teacher.wifiName));
    m_classCoTeacherWifiPasswordTextBox.Text(
        asWide(teacher.wifiPassword)
        );
    m_classCoTeacherProjectionTypeTextBox.Text(
        asWide(teacher.projectionType)
        );
    m_classCoTeacherZoomIdTextBox.Text(asWide(teacher.zoomId));
    m_classCoTeacherZoomPasswordTextBox.Text(
        asWide(teacher.zoomPassword)
        );
    m_classCoTeacherLoading = false;
    m_classDetailsDirty = true;
    markClassDirty();
}

void MainWindow::ClassCoTeacherSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    ClassSaveButton_Click(sender, arguments);
}

void MainWindow::ClassCoTeacherDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    ClassDiscardButton_Click(sender, arguments);
}

} // namespace winrt::ClassMngrWinUI::implementation
