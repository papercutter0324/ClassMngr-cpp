#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::NameTextBox_TextChanged(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_dirtyState.markDirty();
}

void MainWindow::PersonalDetailsField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        if (sender == m_personalTypedSignatureTextBox)
        {
            updatePersonalSignaturePreview();
        }
        return;
    }

    if (sender == m_personalTypedSignatureTextBox)
    {
        updatePersonalSignaturePreview();
    }

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    if (m_personalStatusText)
    {
        m_personalStatusText.Text(L"Unsaved personal detail changes.");
    }
    if (m_personalSaveButton)
    {
        m_personalSaveButton.IsEnabled(true);
    }
    if (m_personalDiscardButton)
    {
        m_personalDiscardButton.IsEnabled(true);
    }
}

void MainWindow::PersonalDetailsPassword_Changed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        return;
    }

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    if (m_personalStatusText)
    {
        m_personalStatusText.Text(L"Unsaved personal detail changes.");
    }
    if (m_personalSaveButton)
    {
        m_personalSaveButton.IsEnabled(true);
    }
    if (m_personalDiscardButton)
    {
        m_personalDiscardButton.IsEnabled(true);
    }
}

void MainWindow::PersonalDetailsZoomAvailability_Changed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_personalZoomNotAvailableCheck)
    {
        return;
    }

    const auto checkedValue = m_personalZoomNotAvailableCheck.IsChecked();
    const bool checked = checkedValue && checkedValue.Value();
    if (!m_personalDetailsLoading)
    {
        if (checked)
        {
            m_personalZoomLoginIdTextBox.Text(L"N/A");
            m_personalZoomPasswordBox.Password(L"N/A");
        }
        else
        {
            if (m_personalZoomLoginIdTextBox.Text() == L"N/A")
            {
                m_personalZoomLoginIdTextBox.Text({});
            }
            if (m_personalZoomPasswordBox.Password() == L"N/A")
            {
                m_personalZoomPasswordBox.Password({});
            }
        }
    }

    m_personalZoomLoginIdTextBox.IsEnabled(!checked && m_openDatabase);
    m_personalZoomPasswordBox.IsEnabled(!checked && m_openDatabase);
    if (!m_personalDetailsLoading && m_openDatabase)
    {
        m_personalDetailsDirty = true;
        m_dirtyState.markDirty();
        m_personalStatusText.Text(L"Unsaved personal detail changes.");
        m_personalSaveButton.IsEnabled(true);
        m_personalDiscardButton.IsEnabled(true);
    }
}

void MainWindow::PersonalDetailsCampus_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        return;
    }

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    m_personalStatusText.Text(L"Unsaved personal detail changes.");
    m_personalSaveButton.IsEnabled(true);
    m_personalDiscardButton.IsEnabled(true);
}

void MainWindow::PersonalDetailsSignatureMode_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    updatePersonalSignatureControls();
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        return;
    }

    if (sender == m_personalSignatureModeCombo)
    {
        const bool typed = m_personalSignatureModeCombo.SelectedIndex() == 1;
        m_personalTypedSignatureTextBox.IsEnabled(typed);
        m_personalSignatureFontCombo.IsEnabled(typed);
        m_personalImageStatusText.Text(
            typed
                ? L"Typed signature values are stored with the personal details."
                : L"Existing image data is retained; image selection is a Phase 7 placeholder."
            );
    }
    updatePersonalSignaturePreview();

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    m_personalStatusText.Text(L"Unsaved personal detail changes.");
    m_personalSaveButton.IsEnabled(true);
    m_personalDiscardButton.IsEnabled(true);
}

void MainWindow::PersonalDetailsSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_personalStatusText.Text(L"No database open; personal details were not saved.");
        return;
    }

    const auto name = asWString(m_personalNameTextBox.Text());
    if (name.find_first_not_of(L" \t\r\n") == std::wstring::npos)
    {
        m_personalValidationText.Text(L"Your name is required.");
        m_personalValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_personalNameTextBox.Focus(
            Microsoft::UI::Xaml::FocusState::Programmatic
            );
        return;
    }

    classmngr::engine::PersonalDetails draft = m_personalDetails;
    draft.name = asUtf8(name);
    draft.campus = asUtf8(selectedComboValue(m_personalCampusCombo));
    draft.zoomLoginId = asUtf8(asWString(m_personalZoomLoginIdTextBox.Text()));
    draft.zoomPassword = asUtf8(
        asWString(m_personalZoomPasswordBox.Password())
        );
    const auto checkedValue = m_personalZoomNotAvailableCheck.IsChecked();
    draft.zoomNotAvailable = checkedValue && checkedValue.Value();
    draft.signatureMode =
        m_personalSignatureModeCombo.SelectedIndex() == 1
            ? classmngr::engine::SignatureMode::Type
            : classmngr::engine::SignatureMode::Image;
    draft.typedSignatureText = asUtf8(
        asWString(m_personalTypedSignatureTextBox.Text())
        );
    draft.typedSignatureFont = m_personalSignatureFontCombo.SelectedIndex();

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetailsService service(settings);
    const auto saved = service.save(draft);
    if (!saved)
    {
        m_personalStatusText.Text(winrt::hstring(
            L"Personal details could not be saved: "
            + asWide(saved.error().message)
            ));
        m_personalValidationText.Text(L"The engine rejected the personal-details write.");
        m_personalValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_personalDetails = std::move(draft);
    m_personalDetailsLoaded = true;
    m_personalDetailsDirty = false;
    m_dirtyState.markClean();
    m_personalStatusText.Text(L"Personal details saved.");
    m_personalValidationText.Text({});
    m_personalValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_personalSaveButton.IsEnabled(false);
    m_personalDiscardButton.IsEnabled(false);
    updateFileCommandState();
}

void MainWindow::PersonalDetailsDiscardButton_Click(
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

    m_personalDetailsDirty = false;
    m_dirtyState.markClean();
    refreshPersonalDetailsPage();
}

} // namespace winrt::ClassMngrWinUI::implementation
