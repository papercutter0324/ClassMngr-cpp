#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

void MainWindow::saveSubPrepPage()
{
    if (!m_openDatabase || !m_subPrepClassMaterialsTextBox)
    {
        return;
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    const auto saved = settings.saveBatch({
        {
            "subPrep/classMaterials",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepClassMaterialsTextBox.Text()
                ))}
        },
        {
            "subPrep/bookReportGrading",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepGradingTextBox.Text()
                ))}
        },
        {
            "subPrep/bookReportSpecialInstructions",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepSpecialInstructionsTextBox.Text()
                ))}
        },
        {
            "subPrep/subComments",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepNotesTextBox.Text()
                ))}
        }
        });
    if (!saved)
    {
        m_subPrepStatusText.Text(L"Sub Prep settings could not be saved.");
        m_subPrepValidationText.Text(winrt::hstring(
            L"Sub Prep settings error: " + asWide(saved.error().message)
            ));
        m_subPrepValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        updateSubPrepActions();
        return;
    }

    m_subPrepDirty = false;
    m_dirtyState.markClean();
    refreshSubPrepPage();
    m_subPrepStatusText.Text(L"Sub Prep settings saved.");
    m_subPrepValidationText.Text({});
    m_subPrepValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateSubPrepActions();
    updateFileCommandState();
}

void MainWindow::discardSubPrepPage()
{
    if (!m_openDatabase)
    {
        return;
    }

    m_subPrepDirty = false;
    m_dirtyState.markClean();
    refreshSubPrepPage();
    m_subPrepStatusText.Text(L"Sub Prep changes discarded.");
    updateSubPrepActions();
    updateFileCommandState();
}

void MainWindow::updateSubPrepActions()
{
    if (!m_subPrepClassMaterialsTextBox || !m_subPrepSaveButton)
    {
        return;
    }

    const bool enabled = static_cast<bool>(m_openDatabase);
    m_subPrepClassMaterialsTextBox.IsEnabled(enabled);
    m_subPrepGradingTextBox.IsEnabled(enabled);
    m_subPrepSpecialInstructionsTextBox.IsEnabled(enabled);
    m_subPrepNotesTextBox.IsEnabled(enabled);
    m_subPrepSaveButton.IsEnabled(enabled && m_subPrepDirty);
    m_subPrepDiscardButton.IsEnabled(enabled && m_subPrepDirty);
    if (m_subPrepPackageUserNameTextBox)
    {
        m_subPrepPackageUserNameTextBox.IsEnabled(enabled);
    }
    if (m_subPrepPackageDatesTextBox)
    {
        m_subPrepPackageDatesTextBox.IsEnabled(enabled);
    }
    if (m_subPrepPackageRosterTemplateCombo)
    {
        m_subPrepPackageRosterTemplateCombo.IsEnabled(enabled);
    }
    if (m_subPrepPackagePlanButton)
    {
        m_subPrepPackagePlanButton.IsEnabled(
            enabled && !m_subPrepPackageClassChecks.empty()
            );
    }
    if (m_subPrepPackageClassesList)
    {
        m_subPrepPackageClassesList.IsEnabled(enabled);
    }
    if (m_subPrepPackagePathsList)
    {
        m_subPrepPackagePathsList.IsEnabled(enabled);
    }
    if (m_subPrepScheduleList)
    {
        m_subPrepScheduleList.IsEnabled(enabled);
    }
    if (m_subPrepClassInformationList)
    {
        m_subPrepClassInformationList.IsEnabled(enabled);
    }
}

void MainWindow::markSubPrepDirty()
{
    if (m_subPrepLoading || !m_openDatabase)
    {
        return;
    }

    m_subPrepDirty = true;
    m_dirtyState.markDirty();
    if (m_subPrepStatusText)
    {
        m_subPrepStatusText.Text(L"Unsaved Sub Prep changes.");
    }
    updateSubPrepActions();
}

void MainWindow::planSubPrepPackage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_subPrepPackageDatesTextBox
        || !m_subPrepPackagePlanButton)
    {
        return;
    }

    m_subPrepPackagePlan = {};
    m_subPrepPackagePathsList.Items().Clear();

    const std::wstring rawDates = asWString(
        m_subPrepPackageDatesTextBox.Text()
        );
    std::vector<EngineCalendarDate> selectedDates;
    std::size_t start = 0;
    while (start <= rawDates.size())
    {
        const std::size_t separator = rawDates.find_first_of(
            L",;\r\n",
            start
            );
        const std::size_t end = separator == std::wstring::npos
            ? rawDates.size()
            : separator;
        std::size_t first = start;
        while (first < end && std::iswspace(rawDates[first]) != 0)
        {
            ++first;
        }
        std::size_t last = end;
        while (last > first && std::iswspace(rawDates[last - 1]) != 0)
        {
            --last;
        }
        if (first < last)
        {
            EngineCalendarDate date;
            if (!calendarDateFromText(
                    std::wstring_view(rawDates).substr(first, last - first),
                    date
                    ))
            {
                m_subPrepPackageStatusText.Text(
                    L"Package plan could not be created."
                    );
                m_subPrepValidationText.Text(
                    L"Selected dates must use yyyy-MM-dd, separated by commas."
                    );
                m_subPrepValidationText.Visibility(Visibility::Visible);
                return;
            }
            selectedDates.push_back(date);
        }
        if (separator == std::wstring::npos)
        {
            break;
        }
        start = separator + 1;
    }
    if (selectedDates.empty())
    {
        m_subPrepPackageStatusText.Text(L"Package plan could not be created.");
        m_subPrepValidationText.Text(
            L"Select at least one date before planning the package."
            );
        m_subPrepValidationText.Visibility(Visibility::Visible);
        return;
    }

    std::vector<int> classIds;
    for (const auto& check : m_subPrepPackageClassChecks)
    {
        const auto checked = check.IsChecked();
        if (!checked || !checked.Value())
        {
            continue;
        }
        const int classId = boxedInt(check.Tag());
        if (classId > 0)
        {
            classIds.push_back(classId);
        }
    }
    if (classIds.empty())
    {
        m_subPrepPackageStatusText.Text(L"Package plan could not be created.");
        m_subPrepValidationText.Text(
            L"Select at least one class before planning the package."
            );
        m_subPrepValidationText.Visibility(Visibility::Visible);
        return;
    }

    std::vector<classmngr::engine::SubPrepPackageSourceClass> sourceClasses;
    sourceClasses.reserve(m_subPrepSourceClasses.size());
    for (const auto& source : m_subPrepSourceClasses)
    {
        sourceClasses.push_back({
            source.classroom,
            source.info,
            source.teacher
        });
    }

    classmngr::engine::SubPrepPackageBuildOptions options;
    options.userName = asUtf8(asWString(
        m_subPrepPackageUserNameTextBox.Text()
        ));
    options.selectedDates = std::move(selectedDates);
    options.classIds = std::move(classIds);
    const int templateIndex = m_subPrepPackageRosterTemplateCombo
        ? m_subPrepPackageRosterTemplateCombo.SelectedIndex()
        : 0;
    if (templateIndex == 1)
    {
        options.rosterTemplate =
            classmngr::engine::SubPrepRosterTemplate::Daily;
    }
    else if (templateIndex == 2)
    {
        options.rosterTemplate =
            classmngr::engine::SubPrepRosterTemplate::PerClassWithExtraInfo;
    }

    const auto planned = classmngr::engine::SubPrepPackageService::build(
        sourceClasses,
        options
        );
    if (!planned)
    {
        m_subPrepPackageStatusText.Text(L"Package plan could not be created.");
        m_subPrepValidationText.Text(winrt::hstring(
            L"Package planning error: " + asWide(planned.error().message)
            ));
        m_subPrepValidationText.Visibility(Visibility::Visible);
        return;
    }

    m_subPrepPackagePlan = *planned;
    for (const std::string& path : m_subPrepPackagePlan.relativeDocumentPaths)
    {
        auto row = TextBlock();
        row.Text(asWide(path));
        row.TextWrapping(TextWrapping::Wrap);
        auto item = ListViewItem();
        item.Content(row);
        item.IsTabStop(false);
        setAutomationName(item, L"Sub Prep package path " + asWide(path));
        m_subPrepPackagePathsList.Items().Append(item);
    }

    std::wstring status = L"Package plan ready: ";
    status += std::to_wstring(m_subPrepPackagePlan.classes.size());
    status += L" classes, ";
    status += std::to_wstring(
        m_subPrepPackagePlan.relativeDocumentPaths.size()
        );
    status += L" document paths in folder \"";
    status += asWide(m_subPrepPackagePlan.folderName);
    status += L"\".";
    m_subPrepPackageStatusText.Text(winrt::hstring(status));
    m_subPrepValidationText.Text({});
    m_subPrepValidationText.Visibility(Visibility::Collapsed);
    updateSubPrepActions();
}

} // namespace winrt::ClassMngrWinUI::implementation
