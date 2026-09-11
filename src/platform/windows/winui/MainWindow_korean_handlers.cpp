#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::KoreanTeacherSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading)
    {
        return;
    }

    const auto selector = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (!selector)
    {
        return;
    }

    if (m_koreanTeacherDirty)
    {
        m_koreanTeacherLoading = true;
        selector.SelectedIndex(m_koreanTeacherSelectedIndex);
        m_koreanTeacherLoading = false;
        m_koreanTeacherStatusText.Text(
            L"Save or discard the current Korean teacher before selecting another."
            );
        return;
    }

    const auto item = selector.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int selectedId = item ? boxedInt(item.Tag()) : -1;
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_koreanTeachers.size()); ++index)
    {
        if (m_koreanTeachers[static_cast<std::size_t>(index)].id == selectedId)
        {
            selectedIndex = index;
            break;
        }
    }
    m_koreanTeacherSelectedIndex = selectedIndex;
    m_koreanTeacherSelectedId = selectedId;
    m_koreanTeacherNew = false;
    presentKoreanTeacher(selectedIndex);
    m_koreanTeacherStatusText.Text(
        selectedIndex >= 0
            ? L"Korean teacher selected."
            : L"Select a Korean teacher or choose New Teacher."
        );
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::KoreanTeacherField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading || !m_openDatabase)
    {
        return;
    }

    if (sender == m_koreanTeacherEnTextBox
        || sender == m_koreanTeacherRomanizationTextBox)
    {
        refreshKoreanTeacherPreferredNames();
    }
    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(L"Unsaved Korean teacher changes.");
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSelector.IsEnabled(false);
}

void MainWindow::KoreanTeacherPassword_Changed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(L"Unsaved Korean teacher changes.");
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSelector.IsEnabled(false);
}

void MainWindow::KoreanTeacherCombo_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(L"Unsaved Korean teacher changes.");
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSelector.IsEnabled(false);
}

void MainWindow::KoreanTeacherNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_koreanTeacherDirty)
    {
        if (m_koreanTeacherStatusText && m_koreanTeacherDirty)
        {
            m_koreanTeacherStatusText.Text(
                L"Save or discard the current Korean teacher before creating another."
                );
        }
        return;
    }

    m_koreanTeacherNew = true;
    m_koreanTeacherSelectedIndex = -1;
    m_koreanTeacherSelectedId = -1;
    m_koreanTeacherLoading = true;
    m_koreanTeacherSelector.SelectedIndex(-1);
    m_koreanTeacherLoading = false;
    presentKoreanTeacher(-1);
    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(
        L"New Korean teacher. Enter the required name fields and save."
        );
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_koreanTeacherSelector.IsEnabled(false);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::KoreanTeacherDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_koreanTeacherSelectedId <= 0
        || m_koreanTeacherDirty)
    {
        return;
    }

    const int teacherId = m_koreanTeacherSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete Korean teacher",
        L"Delete the selected Korean teacher from the active database?",
        L"Delete",
        {},
        L"Cancel",
        [weak, teacherId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
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
                classmngr::engine::TeacherService service(
                    *self->m_openDatabase
                    );
                const auto removed = service.remove(teacherId);
                if (!removed)
                {
                    self->m_koreanTeacherStatusText.Text(winrt::hstring(
                        L"Korean teacher could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_koreanTeacherSelectedId = -1;
                self->m_koreanTeacherSelectedIndex = -1;
                self->m_koreanTeacherDirty = false;
                self->m_koreanTeacherNew = false;
                self->m_dirtyState.markClean();
                self->refreshKoreanTeachersPage();
                self->m_koreanTeacherStatusText.Text(
                    L"Korean teacher deleted."
                    );
            }
        }
        );
}

void MainWindow::KoreanTeacherSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_koreanTeacherStatusText.Text(L"No database open.");
        return;
    }

    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto saved = service.save(koreanTeacherFromForm());
    if (!saved)
    {
        m_koreanTeacherStatusText.Text(winrt::hstring(
            L"Korean teacher could not be saved: "
            + asWide(saved.error().message)
            ));
        m_koreanTeacherValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_koreanTeacherValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_koreanTeacherSelectedId = *saved;
    m_koreanTeacherSelectedIndex = -1;
    m_koreanTeacherNew = false;
    m_koreanTeacherDirty = false;
    m_dirtyState.markClean();
    refreshKoreanTeachersPage();
    m_koreanTeacherStatusText.Text(L"Korean teacher saved.");
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::KoreanTeacherDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_koreanTeacherDirty = false;
    m_koreanTeacherNew = false;
    m_dirtyState.markClean();
    refreshKoreanTeachersPage();
}

} // namespace winrt::ClassMngrWinUI::implementation
