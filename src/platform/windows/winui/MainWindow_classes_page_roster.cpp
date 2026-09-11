#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation
{
using namespace MainWindowDetail;

Microsoft::UI::Xaml::Controls::StackPanel
MainWindow::buildClassRosterSection()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto makeRoot = [](StackPanel const& content) {
        content.Padding(Thickness{16.0, 12.0, 16.0, 24.0});
        content.Spacing(16.0);
        content.MaxWidth(2000.0);
        content.HorizontalAlignment(HorizontalAlignment::Stretch);
        return content;
    };

    auto rosterRoot = makeRoot(StackPanel());

    auto rosterCard = ClassMngrWinUISharedUX::buildCard({
        L"",
        L"",
        L"Roster editor"
        });
    rosterCard.root.Padding(Thickness{24.0, 24.0, 24.0, 24.0});
    rosterCard.content.Spacing(16.0);

    m_classRosterStatusText = TextBlock();
    m_classRosterStatusText.Text(L"Select a class to edit its roster.");
    m_classRosterStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_classRosterStatusText, L"Class roster status");
    rosterCard.content.Children().Append(m_classRosterStatusText);

    m_classRosterValidationText = TextBlock();
    m_classRosterValidationText.TextWrapping(TextWrapping::Wrap);
    m_classRosterValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_classRosterValidationText,
        L"Class roster validation summary"
        );
    rosterCard.content.Children().Append(m_classRosterValidationText);

    auto rosterActions = Grid();
    rosterActions.ColumnSpacing(8.0);
    auto rosterActionsLeft = ColumnDefinition();
    rosterActionsLeft.Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    rosterActions.ColumnDefinitions().Append(rosterActionsLeft);
    auto rosterActionsSpacer = ColumnDefinition();
    rosterActionsSpacer.Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );
    rosterActions.ColumnDefinitions().Append(rosterActionsSpacer);
    auto rosterActionsRight = ColumnDefinition();
    rosterActionsRight.Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    rosterActions.ColumnDefinitions().Append(rosterActionsRight);

    m_classRosterImportScoresButton = Button();
    m_classRosterImportScoresButton.Content(box_value(hstring(L"Import Scores")));
    m_classRosterImportScoresButton.IsTabStop(true);
    m_classRosterImportScoresButton.TabIndex(12);
    m_classRosterImportScoresButton.Click(
        [this](auto const&, auto const&) { importClassRosterScores(); }
        );
    setAutomationName(m_classRosterImportScoresButton, L"Import roster scores");
    Grid::SetColumn(m_classRosterImportScoresButton, 0);
    rosterActions.Children().Append(m_classRosterImportScoresButton);

    auto rosterColumnActions = StackPanel();
    rosterColumnActions.Orientation(Orientation::Horizontal);
    rosterColumnActions.Spacing(8.0);

    m_classRosterAddColumnButton = Button();
    m_classRosterAddColumnButton.Content(box_value(hstring(L"Add Column")));
    m_classRosterAddColumnButton.IsTabStop(true);
    m_classRosterAddColumnButton.TabIndex(13);
    m_classRosterAddColumnButton.Click(
        [this](auto const&, auto const&) { addClassRosterColumn(); }
        );
    setAutomationName(m_classRosterAddColumnButton, L"Add roster column");
    rosterColumnActions.Children().Append(m_classRosterAddColumnButton);

    m_classRosterRemoveColumnButton = Button();
    m_classRosterRemoveColumnButton.Content(box_value(hstring(L"Remove Column")));
    m_classRosterRemoveColumnButton.IsTabStop(true);
    m_classRosterRemoveColumnButton.TabIndex(14);
    m_classRosterRemoveColumnButton.Click(
        [this](auto const&, auto const&) { removeClassRosterColumn(); }
        );
    setAutomationName(
        m_classRosterRemoveColumnButton,
        L"Remove selected roster column"
        );
    rosterColumnActions.Children().Append(m_classRosterRemoveColumnButton);
    Grid::SetColumn(rosterColumnActions, 2);
    rosterActions.Children().Append(rosterColumnActions);

    m_classRosterHeaderGrid = Grid();
    m_classRosterHeaderGrid.ColumnSpacing(2.0);
    setAutomationName(m_classRosterHeaderGrid, L"Class roster column headers");
    rosterCard.content.Children().Append(m_classRosterHeaderGrid);

    m_classRosterList = ListView();
    m_classRosterList.SelectionMode(ListViewSelectionMode::Single);
    m_classRosterList.IsTabStop(true);
    m_classRosterList.TabIndex(15);
    m_classRosterList.Height(440.0);
    m_classRosterList.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_classRosterList.SelectionChanged(
        [this](auto const&, auto const&) { updateClassRosterActions(); }
        );
    setAutomationName(m_classRosterList, L"Class roster student grid");
    rosterCard.content.Children().Append(m_classRosterList);

    m_classRosterActions = rosterActions;
    rosterRoot.Children().Append(rosterCard.root);


    return rosterRoot;
}

Microsoft::UI::Xaml::Controls::StackPanel
MainWindow::buildClassSpeakingSection()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto makeRoot = [](StackPanel const& content) {
        content.Padding(Thickness{16.0, 12.0, 16.0, 24.0});
        content.Spacing(16.0);
        content.MaxWidth(2000.0);
        content.HorizontalAlignment(HorizontalAlignment::Stretch);
        return content;
    };

    auto speakingRoot = makeRoot(StackPanel());
    auto speakingTopBar = Grid();
    speakingTopBar.ColumnSpacing(16.0);
    auto speakingTitleColumn = ColumnDefinition();
    speakingTitleColumn.Width(GridLengthHelper::FromValueAndType(
        1.0,
        GridUnitType::Star
        ));
    speakingTopBar.ColumnDefinitions().Append(speakingTitleColumn);
    speakingTopBar.ColumnDefinitions().Append(ColumnDefinition());

    auto speakingCard = ClassMngrWinUISharedUX::buildCard({
        {},
        {},
        L"Evaluations editor"
        });
    speakingCard.root.Padding(Thickness{12.0, 12.0, 12.0, 12.0});
    speakingCard.content.Spacing(10.0);

    m_speakingEvaluationStatusText = TextBlock();
    m_speakingEvaluationStatusText.Text(
        L"Select a class to edit evaluations."
        );
    m_speakingEvaluationStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(
        m_speakingEvaluationStatusText,
        L"Speaking evaluation status"
        );

    m_speakingEvaluationValidationText = TextBlock();
    m_speakingEvaluationValidationText.TextWrapping(TextWrapping::Wrap);
    m_speakingEvaluationValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_speakingEvaluationValidationText,
        L"Speaking evaluation validation summary"
        );

    m_speakingEvaluationSelector = ComboBox();
    m_speakingEvaluationSelector.Header(
        box_value(hstring(L"Evaluation"))
        );
    m_speakingEvaluationSelector.MinWidth(200.0);
    m_speakingEvaluationSelector.Width(220.0);
    m_speakingEvaluationSelector.HorizontalAlignment(HorizontalAlignment::Right);
    m_speakingEvaluationSelector.IsTabStop(true);
    m_speakingEvaluationSelector.TabIndex(0);
    setAutomationName(
        m_speakingEvaluationSelector,
        L"Speaking evaluation selector"
        );
    m_speakingEvaluationName = "Winter";
    m_speakingEvaluationLoading = true;
    for (const std::string_view evaluationName :
         classmngr::engine::SpeakingEvaluationNames)
    {
        auto item = ComboBoxItem();
        const std::wstring display = asWide(evaluationName);
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(display)));
        setAutomationName(item, L"Speaking evaluation " + display);
        m_speakingEvaluationSelector.Items().Append(item);
    }
    m_speakingEvaluationSelector.SelectedIndex(0);
    m_speakingEvaluationSelector.SelectionChanged(
        [this](Windows::Foundation::IInspectable const& rawSender,
               SelectionChangedEventArgs const&) {
            if (m_speakingEvaluationLoading)
            {
                return;
            }
            const auto sender = rawSender.try_as<ComboBox>();
            if (!sender)
            {
                return;
            }
            if (m_speakingEvaluationDirty)
            {
                m_speakingEvaluationLoading = true;
                int restoreIndex = 0;
                for (int index = 0;
                     index < static_cast<int>(sender.Items().Size());
                     ++index)
                {
                    const auto item = sender.Items().GetAt(index).try_as<
                        ComboBoxItem>();
                    if (item && boxedString(item.Tag())
                        == asWide(m_speakingEvaluationName))
                    {
                        restoreIndex = index;
                        break;
                    }
                }
                sender.SelectedIndex(restoreIndex);
                m_speakingEvaluationLoading = false;
                m_speakingEvaluationStatusText.Text(
                    L"Save or discard the current speaking evaluation before selecting another."
                    );
                return;
            }
            const auto item = sender.SelectedItem().try_as<ComboBoxItem>();
            if (!item)
            {
                return;
            }
            m_speakingEvaluationName = asUtf8(boxedString(item.Tag()));
            refreshSpeakingEvaluation();
        }
        );
    m_speakingEvaluationLoading = false;
    Grid::SetColumn(m_speakingEvaluationSelector, 1);
    speakingTopBar.Children().Append(m_speakingEvaluationSelector);
    speakingRoot.Children().Append(speakingTopBar);

    auto speakingActions = Grid();
    speakingActions.ColumnSpacing(8.0);
    speakingActions.ColumnDefinitions().Append(ColumnDefinition());
    speakingActions.ColumnDefinitions().Append(ColumnDefinition());
    speakingActions.ColumnDefinitions().Append(ColumnDefinition());
    speakingActions.ColumnDefinitions().GetAt(0).Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    speakingActions.ColumnDefinitions().GetAt(1).Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );
    speakingActions.ColumnDefinitions().GetAt(2).Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );

    m_speakingEvaluationImportNamesButton = Button();
    m_speakingEvaluationImportNamesButton.Content(
        box_value(hstring(L"Import Names"))
        );
    m_speakingEvaluationImportNamesButton.IsTabStop(true);
    m_speakingEvaluationImportNamesButton.TabIndex(1);
    m_speakingEvaluationImportNamesButton.Click(
        [this](auto const&, auto const&) {
            importSpeakingEvaluationNames();
        }
        );
    setAutomationName(
        m_speakingEvaluationImportNamesButton,
        L"Import speaking evaluation names"
        );
    Grid::SetColumn(m_speakingEvaluationImportNamesButton, 0);
    speakingActions.Children().Append(m_speakingEvaluationImportNamesButton);

    m_speakingEvaluationSaveButton = Button();
    m_speakingEvaluationSaveButton.Content(
        box_value(hstring(L"Save Evaluation"))
        );
    m_speakingEvaluationSaveButton.IsTabStop(true);
    m_speakingEvaluationSaveButton.TabIndex(2);
    m_speakingEvaluationSaveButton.Click(
        [this](auto const&, auto const&) { saveSpeakingEvaluation(); }
        );
    setAutomationName(
        m_speakingEvaluationSaveButton,
        L"Save speaking evaluation"
        );

    m_speakingEvaluationDiscardButton = Button();
    m_speakingEvaluationDiscardButton.Content(
        box_value(hstring(L"Discard Changes"))
        );
    m_speakingEvaluationDiscardButton.IsTabStop(true);
    m_speakingEvaluationDiscardButton.TabIndex(3);
    m_speakingEvaluationDiscardButton.Click(
        [this](auto const&, auto const&) { discardSpeakingEvaluation(); }
        );
    setAutomationName(
        m_speakingEvaluationDiscardButton,
        L"Discard speaking evaluation changes"
        );

    auto speakingReportActions = StackPanel();
    speakingReportActions.Orientation(Orientation::Horizontal);
    speakingReportActions.Spacing(8.0);
    speakingReportActions.HorizontalAlignment(HorizontalAlignment::Right);

    m_speakingEvaluationReportEditorButton = Button();
    m_speakingEvaluationReportEditorButton.Content(
        box_value(hstring(L"Report Editor"))
        );
    m_speakingEvaluationReportEditorButton.IsTabStop(true);
    m_speakingEvaluationReportEditorButton.TabIndex(2);
    m_speakingEvaluationReportEditorButton.Click(
        [this](auto const&, auto const&) {
            openSpeakingReportEditor();
        }
        );
    setAutomationName(
        m_speakingEvaluationReportEditorButton,
        L"Open speaking evaluation report editor"
        );
    speakingReportActions.Children().Append(
        m_speakingEvaluationReportEditorButton
        );

    m_speakingEvaluationGenerateCommentsButton = Button();
    m_speakingEvaluationGenerateCommentsButton.Content(
        box_value(hstring(L"Generate Comments"))
        );
    m_speakingEvaluationGenerateCommentsButton.IsTabStop(true);
    m_speakingEvaluationGenerateCommentsButton.TabIndex(3);
    m_speakingEvaluationGenerateCommentsButton.Click(
        [this](auto const&, auto const&) {
            openSpeakingAiDialog();
        }
        );
    setAutomationName(
        m_speakingEvaluationGenerateCommentsButton,
        L"Open speaking evaluation comments dialog"
        );
    speakingReportActions.Children().Append(
        m_speakingEvaluationGenerateCommentsButton
        );
    Grid::SetColumn(speakingReportActions, 2);
    speakingActions.Children().Append(speakingReportActions);

    m_speakingEvaluationHeaderGrid = Grid();
    m_speakingEvaluationHeaderGrid.ColumnSpacing(4.0);
    m_speakingEvaluationHeaderGrid.MinHeight(42.0);
    setAutomationName(
        m_speakingEvaluationHeaderGrid,
        L"Speaking evaluation column headers"
        );
    speakingCard.content.Children().Append(m_speakingEvaluationHeaderGrid);

    m_speakingEvaluationList = ListView();
    m_speakingEvaluationList.SelectionMode(ListViewSelectionMode::Single);
    m_speakingEvaluationList.IsTabStop(true);
    m_speakingEvaluationList.TabIndex(4);
    m_speakingEvaluationList.Height(620.0);
    m_speakingEvaluationList.HorizontalAlignment(
        HorizontalAlignment::Stretch
        );
    setAutomationName(
        m_speakingEvaluationList,
        L"Speaking evaluation grid"
        );
    m_speakingEvaluationList.SelectionChanged(
        [this](auto const&, auto const&) {
            refreshSpeakingAiSelection();
        }
        );
    speakingCard.content.Children().Append(m_speakingEvaluationList);

    m_speakingEvaluationPasteTextBox = TextBox();
    m_speakingEvaluationPasteTextBox.Header(
        box_value(hstring(L"Paste score range (tab/newline)"))
        );
    m_speakingEvaluationPasteTextBox.PlaceholderText(L"A+	A	B+\nA	B+	B");
    m_speakingEvaluationPasteTextBox.AcceptsReturn(true);
    m_speakingEvaluationPasteTextBox.Height(72.0);
    m_speakingEvaluationPasteTextBox.IsTabStop(true);
    m_speakingEvaluationPasteTextBox.TabIndex(5);
    setAutomationName(
        m_speakingEvaluationPasteTextBox,
        L"Speaking evaluation score range"
        );

    m_speakingEvaluationPasteButton = Button();
    m_speakingEvaluationPasteButton.Content(
        box_value(hstring(L"Apply Range to Scores"))
        );
    m_speakingEvaluationPasteButton.IsTabStop(true);
    m_speakingEvaluationPasteButton.TabIndex(6);
    m_speakingEvaluationPasteButton.Click(
        [this](auto const&, auto const&) { applySpeakingEvaluationPaste(); }
        );
    setAutomationName(
        m_speakingEvaluationPasteButton,
        L"Apply speaking evaluation score range"
        );

    m_speakingEvaluationActions = speakingActions;

    // The Qt product presents batch comments as a small two-step dialog. Keep
    // this root separate from the page cards so its contents are not reparented
    // when the ContentDialog opens.
    m_speakingAiDialogRoot = Border();
    m_speakingAiDialogRoot.Padding(Thickness{12.0});
    m_speakingAiDialogRoot.MinWidth(850.0);
    m_speakingAiDialogRoot.MaxWidth(1050.0);
    setAutomationName(m_speakingAiDialogRoot, L"Generate class comments workflow");
    auto aiTabs = Pivot();
    aiTabs.IsTabStop(true);
    aiTabs.TabIndex(7);
    setAutomationName(aiTabs, L"Generate class comments tabs");
    m_speakingAiDialogRoot.Child(aiTabs);

    m_speakingAiVoiceSelector = ComboBox();
    m_speakingAiVoiceSelector.Header(box_value(hstring(L"Comment voice")));
    m_speakingAiVoiceSelector.IsTabStop(true);
    m_speakingAiVoiceSelector.TabIndex(8);
    setAutomationName(m_speakingAiVoiceSelector, L"Speaking AI comment voice");
    const auto appendAiVoice = [this](std::wstring_view label, int tag) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(label)));
        item.Tag(box_value(tag));
        setAutomationName(item, L"AI voice " + std::wstring(label));
        m_speakingAiVoiceSelector.Items().Append(item);
    };
    appendAiVoice(L"Direct to Student", 0);
    appendAiVoice(L"Third Person", 1);
    m_speakingAiVoiceSelector.SelectedIndex(0);

    const auto configureAiEditor = [](
        TextBox& editor,
        std::wstring_view header,
        std::wstring_view placeholder,
        double height,
        int tabIndex
        ) {
        editor.Header(box_value(hstring(header)));
        editor.PlaceholderText(hstring(placeholder));
        editor.AcceptsReturn(true);
        editor.TextWrapping(TextWrapping::Wrap);
        editor.Height(height);
        editor.IsTabStop(true);
        editor.TabIndex(tabIndex);
    };
    // Retain the focused student-prompt controls for the existing command and
    // Phase 6 smoke path, but do not place them in this batch-only dialog.
    m_speakingAiDidWellTextBox = TextBox();
    configureAiEditor(
        m_speakingAiDidWellTextBox,
        L"Did Well observations",
        L"Clear pronunciation\nUses complete sentences",
        84.0,
        8
        );
    m_speakingAiNeedsImprovementTextBox = TextBox();
    configureAiEditor(
        m_speakingAiNeedsImprovementTextBox,
        L"Needs Improvement observations",
        L"Add supporting details\nPractice fluency",
        84.0,
        9
        );
    m_speakingAiGenerateButton = Button();
    m_speakingAiGenerateButton.Content(
        box_value(hstring(L"Generate Student Prompt"))
        );
    m_speakingAiGenerateButton.Click(
        [this](auto const&, auto const&) { generateSpeakingAiPrompt(); }
        );
    m_speakingAiGenerateBatchButton = Button();
    m_speakingAiGenerateBatchButton.Content(
        box_value(hstring(L"Create Class Prompt"))
        );
    m_speakingAiGenerateBatchButton.IsTabStop(true);
    m_speakingAiGenerateBatchButton.TabIndex(11);
    m_speakingAiGenerateBatchButton.HorizontalAlignment(HorizontalAlignment::Right);
    m_speakingAiGenerateBatchButton.Click(
        [this](auto const&, auto const&) {
            generateSpeakingAiBatchPrompt();
        }
        );
    m_speakingAiPromptTextBox = TextBox();
    configureAiEditor(
        m_speakingAiPromptTextBox,
        L"",
        L"Select students and create the class prompt.",
        250.0,
        12
        );
    m_speakingAiPromptTextBox.IsReadOnly(true);
    setAutomationName(m_speakingAiPromptTextBox, L"Speaking AI prompt");

    auto aiPromptActions = StackPanel();
    aiPromptActions.Orientation(Orientation::Horizontal);
    aiPromptActions.Spacing(8.0);

    m_speakingAiCopyButton = Button();
    m_speakingAiCopyButton.Content(box_value(hstring(L"Copy Prompt")));
    m_speakingAiCopyButton.IsTabStop(true);
    m_speakingAiCopyButton.TabIndex(13);
    m_speakingAiCopyButton.HorizontalAlignment(HorizontalAlignment::Right);
    m_speakingAiCopyButton.Click(
        [this](auto const&, auto const&) { copySpeakingAiPrompt(false); }
        );
    setAutomationName(m_speakingAiCopyButton, L"Copy speaking AI prompt");
    aiPromptActions.Children().Append(m_speakingAiCopyButton);

    m_speakingAiCopyOpenButton = Button();
    m_speakingAiCopyOpenButton.Content(box_value(hstring(L"Copy Prompt and Open ChatGPT")));
    m_speakingAiCopyOpenButton.IsTabStop(true);
    m_speakingAiCopyOpenButton.TabIndex(14);
    m_speakingAiCopyOpenButton.Click(
        [this](auto const&, auto const&) { copySpeakingAiPrompt(true); }
        );
    setAutomationName(
        m_speakingAiCopyOpenButton,
        L"Copy speaking AI prompt and open ChatGPT"
        );
    aiPromptActions.Children().Append(m_speakingAiCopyOpenButton);

    m_speakingAiResponseTextBox = TextBox();
    configureAiEditor(
        m_speakingAiResponseTextBox,
        L"",
        L"Paste the response containing the STUDENT blocks here...",
        190.0,
        15
        );
    m_speakingAiResponseTextBox.TextChanging(
        [this](auto const&, auto const&) { updateSpeakingAiActions(); }
    );
    setAutomationName(m_speakingAiResponseTextBox, L"Speaking AI response");

    m_speakingAiApplyStudentButton = Button();
    m_speakingAiApplyStudentButton.Content(
        box_value(hstring(L"Apply Student Comment"))
        );
    m_speakingAiApplyStudentButton.Click(
        [this](auto const&, auto const&) {
            applySpeakingAiStudentComment();
        }
        );
    m_speakingAiParseBatchButton = Button();
    m_speakingAiParseBatchButton.Content(
        box_value(hstring(L"Parse Response"))
        );
    m_speakingAiParseBatchButton.IsTabStop(true);
    m_speakingAiParseBatchButton.TabIndex(17);
    m_speakingAiParseBatchButton.HorizontalAlignment(HorizontalAlignment::Right);
    m_speakingAiParseBatchButton.Click(
        [this](auto const&, auto const&) {
            parseSpeakingAiBatchResponse();
        }
        );
    m_speakingAiApplyBatchButton = Button();
    m_speakingAiApplyBatchButton.Content(
        box_value(hstring(L"Apply Selected Comments"))
        );
    m_speakingAiApplyBatchButton.IsTabStop(true);
    m_speakingAiApplyBatchButton.TabIndex(18);
    m_speakingAiApplyBatchButton.HorizontalAlignment(HorizontalAlignment::Right);
    m_speakingAiApplyBatchButton.Click(
        [this](auto const&, auto const&) {
            applySpeakingAiBatchComments();
        }
        );
    auto promptPage = StackPanel();
    promptPage.Spacing(8.0);
    auto selectionLabel = TextBlock();
    selectionLabel.Text(L"Select students. Eligible students without comments are selected automatically.");
    selectionLabel.TextWrapping(TextWrapping::Wrap);
    selectionLabel.FontSize(18.0);
    promptPage.Children().Append(selectionLabel);
    promptPage.Children().Append(m_speakingAiVoiceSelector);
    m_speakingAiBatchSelectionList = ListView();
    m_speakingAiBatchSelectionList.SelectionMode(ListViewSelectionMode::None);
    m_speakingAiBatchSelectionList.Height(230.0);
    setAutomationName(m_speakingAiBatchSelectionList, L"Speaking AI student selection table");
    promptPage.Children().Append(m_speakingAiBatchSelectionList);
    promptPage.Children().Append(m_speakingAiGenerateBatchButton);
    auto privacyLabel = TextBlock();
    privacyLabel.Text(L"Real student names are removed from the prompt and are restored locally after the response is pasted.");
    privacyLabel.TextWrapping(TextWrapping::Wrap);
    privacyLabel.FontSize(16.0);
    promptPage.Children().Append(privacyLabel);
    promptPage.Children().Append(m_speakingAiPromptTextBox);
    aiPromptActions.HorizontalAlignment(HorizontalAlignment::Right);
    promptPage.Children().Append(aiPromptActions);
    auto promptItem = PivotItem();
    promptItem.Header(box_value(hstring(L"1. Create Prompt")));
    promptItem.Content(promptPage);
    aiTabs.Items().Append(promptItem);

    auto reviewPage = StackPanel();
    reviewPage.Spacing(8.0);
    auto responseLabel = TextBlock();
    responseLabel.Text(L"Paste the complete AI response below.");
    responseLabel.FontSize(18.0);
    reviewPage.Children().Append(responseLabel);
    reviewPage.Children().Append(m_speakingAiResponseTextBox);
    reviewPage.Children().Append(m_speakingAiParseBatchButton);
    m_speakingAiParseSummary = TextBlock();
    m_speakingAiParseSummary.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_speakingAiParseSummary, L"Speaking AI response parse summary");
    reviewPage.Children().Append(m_speakingAiParseSummary);
    m_speakingAiBatchReviewList = ListView();
    m_speakingAiBatchReviewList.SelectionMode(ListViewSelectionMode::None);
    m_speakingAiBatchReviewList.Height(310.0);
    setAutomationName(m_speakingAiBatchReviewList, L"Speaking AI comment review table");
    reviewPage.Children().Append(m_speakingAiBatchReviewList);
    m_speakingAiStatusText = TextBlock();
    m_speakingAiStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_speakingAiStatusText, L"Speaking AI comment status");
    reviewPage.Children().Append(m_speakingAiStatusText);
    reviewPage.Children().Append(m_speakingAiApplyBatchButton);
    auto reviewItem = PivotItem();
    reviewItem.Header(box_value(hstring(L"2. Paste and Review")));
    reviewItem.Content(reviewPage);
    aiTabs.Items().Append(reviewItem);

    speakingRoot.Children().Append(speakingCard.root);

    auto batchReportCard = ClassMngrWinUISharedUX::buildCard({
        L"Batch report operations",
        L"Plan a renderer-neutral batch of speaking reports. PDF rendering, printing, and PowerPoint automation are completed by the output services in the next phase.",
        L"Speaking evaluation batch report operations"
        });
    m_speakingBatchStatusText = TextBlock();
    m_speakingBatchStatusText.Text(
        L"Choose an output mode and plan the named students in this evaluation."
        );
    m_speakingBatchStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(
        m_speakingBatchStatusText,
        L"Speaking batch report status"
        );
    batchReportCard.content.Children().Append(m_speakingBatchStatusText);

    const auto appendBatchOption = [](
        ComboBox& selector,
        std::wstring_view label,
        int tag
        ) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(label)));
        item.Tag(box_value(tag));
        selector.Items().Append(item);
    };
    m_speakingBatchRendererSelector = ComboBox();
    m_speakingBatchRendererSelector.Header(
        box_value(hstring(L"Renderer"))
        );
    m_speakingBatchRendererSelector.MinWidth(320.0);
    m_speakingBatchRendererSelector.IsTabStop(true);
    m_speakingBatchRendererSelector.TabIndex(19);
    appendBatchOption(
        m_speakingBatchRendererSelector,
        L"Internal renderer",
        0
        );
    appendBatchOption(
        m_speakingBatchRendererSelector,
        L"PowerPoint renderer",
        1
        );
    m_speakingBatchRendererSelector.SelectedIndex(0);
    m_speakingBatchRendererSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchRendererSelector,
        L"Speaking batch report renderer"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchRendererSelector
        );

    m_speakingBatchTemplateSelector = ComboBox();
    m_speakingBatchTemplateSelector.Header(
        box_value(hstring(L"Report template"))
        );
    m_speakingBatchTemplateSelector.MinWidth(320.0);
    m_speakingBatchTemplateSelector.IsTabStop(true);
    m_speakingBatchTemplateSelector.TabIndex(20);
    appendBatchOption(
        m_speakingBatchTemplateSelector,
        L"Standard",
        0
        );
    appendBatchOption(
        m_speakingBatchTemplateSelector,
        L"Advanced",
        1
        );
    m_speakingBatchTemplateSelector.SelectedIndex(0);
    m_speakingBatchTemplateSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchTemplateSelector,
        L"Speaking batch report template"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchTemplateSelector
        );

    m_speakingBatchSavePdfCheck = CheckBox();
    m_speakingBatchSavePdfCheck.Content(
        box_value(hstring(L"Save PDF output"))
        );
    m_speakingBatchSavePdfCheck.IsChecked(true);
    m_speakingBatchSavePdfCheck.IsTabStop(true);
    m_speakingBatchSavePdfCheck.TabIndex(21);
    m_speakingBatchSavePdfCheck.Checked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    m_speakingBatchSavePdfCheck.Unchecked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchSavePdfCheck,
        L"Save speaking report PDF output"
        );
    batchReportCard.content.Children().Append(m_speakingBatchSavePdfCheck);

    m_speakingBatchPrintCheck = CheckBox();
    m_speakingBatchPrintCheck.Content(
        box_value(hstring(L"Print reports"))
        );
    m_speakingBatchPrintCheck.IsChecked(false);
    m_speakingBatchPrintCheck.IsTabStop(true);
    m_speakingBatchPrintCheck.TabIndex(22);
    m_speakingBatchPrintCheck.Checked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    m_speakingBatchPrintCheck.Unchecked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchPrintCheck,
        L"Print speaking reports"
        );
    batchReportCard.content.Children().Append(m_speakingBatchPrintCheck);

    m_speakingBatchKeepIndividualPdfsCheck = CheckBox();
    m_speakingBatchKeepIndividualPdfsCheck.Content(
        box_value(hstring(L"Keep individual PDFs when creating a ZIP archive"))
        );
    m_speakingBatchKeepIndividualPdfsCheck.IsChecked(false);
    m_speakingBatchKeepIndividualPdfsCheck.IsTabStop(true);
    m_speakingBatchKeepIndividualPdfsCheck.TabIndex(23);
    setAutomationName(
        m_speakingBatchKeepIndividualPdfsCheck,
        L"Keep individual speaking report PDFs"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchKeepIndividualPdfsCheck
        );

    m_speakingBatchOutputDirectoryTextBox = TextBox();
    m_speakingBatchOutputDirectoryTextBox.Header(
        box_value(hstring(L"Output folder (required for PDF output)"))
        );
    m_speakingBatchOutputDirectoryTextBox.PlaceholderText(
        L"Type or choose the output folder in the output phase"
        );
    m_speakingBatchOutputDirectoryTextBox.MinWidth(420.0);
    m_speakingBatchOutputDirectoryTextBox.IsTabStop(true);
    m_speakingBatchOutputDirectoryTextBox.TabIndex(24);
    m_speakingBatchOutputDirectoryTextBox.TextChanging(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchOutputDirectoryTextBox,
        L"Speaking batch report output folder"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchOutputDirectoryTextBox
        );

    m_speakingBatchChooseOutputButton = Button();
    m_speakingBatchChooseOutputButton.Content(
        box_value(hstring(L"Choose..."))
        );
    m_speakingBatchChooseOutputButton.IsTabStop(true);
    m_speakingBatchChooseOutputButton.TabIndex(25);
    m_speakingBatchChooseOutputButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingBatchChooseOutputButton.Click(
        [this](auto const&, auto const&) {
            chooseSpeakingBatchOutputDirectory();
        }
        );
    setAutomationName(
        m_speakingBatchChooseOutputButton,
        L"Choose speaking batch report output folder"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchChooseOutputButton
        );

    m_speakingBatchPlanButton = Button();
    m_speakingBatchPlanButton.Content(
        box_value(hstring(L"Plan Batch Reports"))
        );
    m_speakingBatchPlanButton.IsTabStop(true);
    m_speakingBatchPlanButton.TabIndex(26);
    m_speakingBatchPlanButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingBatchPlanButton.Click(
        [this](auto const&, auto const&) {
            planSpeakingBatchReports();
        }
        );
    setAutomationName(
        m_speakingBatchPlanButton,
        L"Plan speaking batch reports"
        );
    batchReportCard.content.Children().Append(m_speakingBatchPlanButton);
    m_speakingBatchDialogRoot = batchReportCard.root;


    return speakingRoot;
}

} // namespace winrt::ClassMngrWinUI::implementation
