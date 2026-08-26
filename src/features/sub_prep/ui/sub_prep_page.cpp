#include "sub_prep_page_p.h"
#include "ui/shared/dialogs/user_prompt_service.h"

SubPrepPage::SubPrepPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    Q_ASSERT(m_services);

    setProperty("role", UiRoles::SubPrep);

    buildUi();

    m_autosaveTimer =
        new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(
        AutosaveDelayMs
        );

    connect(
        m_autosaveTimer,
        &QTimer::timeout,
        this,
        &SubPrepPage::autosave
        );

}

void SubPrepPage::saveData()
{
    saveSubPrepInternal();
}

bool SubPrepPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    return saveSubPrepInternal();
}

bool SubPrepPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void SubPrepPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadPageData();
}

void SubPrepPage::refresh()
{
    BasePage::refresh();

    loadPersonalZoomInformation();
    loadCampuses();
    refreshGeneratedContent();

    if (!m_dirty)
    {
        loadStoredSettings();
    }
}

void SubPrepPage::clearDatabaseState()
{
    m_loading = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_campuses.clear();

    for (QLineEdit* edit : {
             m_officeNumberEdit,
             m_officeWifiEdit,
             m_officeWifiPasswordEdit,
             m_photocopierCodeEdit,
             m_zoomLoginIdEdit,
             m_zoomPasswordEdit
         })
    {
        if (edit)
        {
            edit->clear();
        }
    }

    for (QTextEdit* edit : {
             m_classMaterialsEdit,
             m_gradingInstructionsEdit,
             m_specialInstructionsEdit,
             m_subNotesEdit
         })
    {
        if (edit)
        {
            edit->clear();
        }
    }

    if (m_scheduleWidget)
    {
        m_scheduleWidget->clearDatabaseState();
    }

    clearClassInformation();
    updateReadOnlyFieldWidths();

    m_loading = false;
    clearDirty();
}

void SubPrepPage::retranslateUi()
{
    if (m_titleLabel)
    {
        m_titleLabel->setText(
            tr("Sub Prep")
            );
    }

    if (m_subtitleLabel)
    {
        m_subtitleLabel->setText(
            tr("Prepare substitute materials and class notes.")
            );
    }

    if (m_printButton)
    {
        m_printButton->setText(
            tr("Generate Sub Prep")
            );
        m_printButton->setToolTip(
            tr("Create a dated Sub Prep package with by-day rosters and optional paper copies.")
        );
    }

    if (m_koreanKeyboardButton)
    {
        m_koreanKeyboardButton->setToolTip(
            tr("Open Korean / English on-screen keyboard")
            );
        m_koreanKeyboardButton->setAccessibleName(
            tr("Korean Keyboard")
            );
    }

    if (m_importantInformationHeading)
    {
        m_importantInformationHeading->setText(
            tr("Important Information")
            );
    }

    if (m_campusCard)
    {
        m_campusCard->setTitle(
            tr("Campus Information")
            );
    }

    if (m_zoomCard)
    {
        m_zoomCard->setTitle(
            tr("Personal Zoom Information")
            );
    }

    if (m_materialsCard)
    {
        m_materialsCard->setTitle(
            tr("Class Materials & Lesson Notes")
            );
    }

    if (m_gradingCard)
    {
        m_gradingCard->setTitle(
            tr("Book Report Grading")
            );
    }

    if (m_scheduleHeading)
    {
        m_scheduleHeading->setText(
            tr("Schedule")
            );
    }

    if (m_classInformationHeading)
    {
        m_classInformationHeading->setText(
            tr("Class Information")
            );
    }

    if (m_materialsLocationLabel)
    {
        m_materialsLocationLabel->setText(
            tr("Materials Location")
            );
    }

    if (m_detailedClassNotesLabel)
    {
        m_detailedClassNotesLabel->setText(
            tr("Detailed Class & Lesson Notes")
            );
    }

    if (m_officeNumberLabel)
    {
        m_officeNumberLabel->setText(
            tr("Office Number")
            );
    }

    if (m_officeWifiLabel)
    {
        m_officeWifiLabel->setText(
            tr("Office WiFi")
            );
    }

    if (m_officeWifiPasswordLabel)
    {
        m_officeWifiPasswordLabel->setText(
            tr("WiFi Password")
            );
    }

    if (m_photocopierCodeLabel)
    {
        m_photocopierCodeLabel->setText(
            tr("Photocopier Code")
            );
    }

    if (m_zoomLoginIdLabel)
    {
        m_zoomLoginIdLabel->setText(
            tr("Zoom Login ID")
            );
    }

    if (m_zoomPasswordLabel)
    {
        m_zoomPasswordLabel->setText(
            tr("Zoom Password")
            );
    }

    if (m_gradingInstructionsLabel)
    {
        m_gradingInstructionsLabel->setText(
            tr("Grading Instructions")
            );
    }

    if (m_specialInstructionsLabel)
    {
        m_specialInstructionsLabel->setText(
            tr("Special Instructions")
            );
    }

    if (m_scheduleWidget)
    {
        m_scheduleWidget->retranslateUi();
    }

    rebuildClassInformation();
}

void SubPrepPage::scrollToSection(
    SubPrepSection section
    )
{
    m_currentSection = section;

    QWidget* target = nullptr;

    switch (section)
    {
    case SubPrepSection::ImportantInformation:
        target = m_importantInformationHeading;
        break;

    case SubPrepSection::SubNotes:
        target = m_materialsCard;
        break;
    }

    if (!m_scrollArea || !target)
    {
        return;
    }

    QTimer::singleShot(
        0,
        this,
        [this, target]()
        {
            if (!m_scrollArea || !target)
            {
                return;
            }

            m_scrollArea->ensureWidgetVisible(
                target,
                0,
                0
                );

            if (auto* scrollBar = m_scrollArea->verticalScrollBar())
            {
                scrollBar->setValue(
                    target->y()
                    );
            }
        }
        );
}

void SubPrepPage::scrollToTop()
{
    m_currentSection =
        SubPrepSection::ImportantInformation;

    QTimer::singleShot(
        0,
        this,
        [this]()
        {
            if (auto* scrollBar =
                    m_scrollArea
                        ? m_scrollArea->verticalScrollBar()
                        : nullptr)
            {
                scrollBar->setValue(
                    scrollBar->minimum()
                    );
            }
        }
        );
}

QString SubPrepPage::currentSectionName() const
{
    switch (m_currentSection)
    {
    case SubPrepSection::ImportantInformation:
        return tr("Important Information");

    case SubPrepSection::SubNotes:
        return tr("Class Materials & Lesson Notes");
    }

    return {};
}

QString SubPrepPage::currentSectionKey() const
{
    switch (m_currentSection)
    {
    case SubPrepSection::ImportantInformation:
        return QStringLiteral("sub_prep_important");

    case SubPrepSection::SubNotes:
        return QStringLiteral("sub_prep_notes");
    }

    return {};
}

bool SubPrepPage::eventFilter(
    QObject* watched,
    QEvent* event
    )
{
    if (
        watched == m_gradingInstructionsEdit
        && event
        && event->type() == QEvent::FocusOut
        && restoreGradingDefaultIfNeeded()
        )
    {
        handleEditableChanged();
    }

    return BasePage::eventFilter(
        watched,
        event
        );
}

void SubPrepPage::handleEditableChanged()
{
    if (m_loading)
    {
        return;
    }

    m_dirty = true;

    if (m_autosaveTimer)
    {
        m_autosaveTimer->start();
    }
}

void SubPrepPage::autosave()
{
    if (m_dirty)
    {
        saveSubPrepInternal();
    }
}

void SubPrepPage::generateSubPrep()
{
    if (hasUnsavedChanges() && !saveChanges())
    {
        return;
    }

    refreshGeneratedContent();

    QList<CalendarEvent> calendarEvents;
    const QDate currentDate = QDate::currentDate();

    if (auto* calendarService = openCalendarService(m_services))
    {
        const Result<QList<CalendarEvent>> loadedEvents =
            calendarService->eventsInRange(
                QDate(1, 1, 1),
                QDate(9999, 12, 31)
                );
        if (loadedEvents)
        {
            calendarEvents = *loadedEvents;
        }
    }

    const ScheduleViewModel fullSchedule =
        m_scheduleWidget
            ? m_scheduleWidget->scheduleModel()
            : ScheduleViewModel();

    SubPrepPrintDialog dialog(
        m_services,
        fullSchedule,
        calendarEvents,
        currentDate,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    SubPrepPrintService::Request subPrepRequest;
    subPrepRequest.parent = this;
    subPrepRequest.campus = {
        m_officeNumberEdit ? m_officeNumberEdit->text() : QString(),
        m_officeWifiEdit ? m_officeWifiEdit->text() : QString(),
        m_officeWifiPasswordEdit ? m_officeWifiPasswordEdit->text() : QString(),
        m_photocopierCodeEdit ? m_photocopierCodeEdit->text() : QString()
    };
    subPrepRequest.zoom = {
        m_zoomLoginIdEdit ? m_zoomLoginIdEdit->text() : QString(),
        m_zoomPasswordEdit ? m_zoomPasswordEdit->text() : QString()
    };
    subPrepRequest.classMaterials =
        m_classMaterialsEdit
            ? m_classMaterialsEdit->toPlainText()
            : QString();
    subPrepRequest.gradingInstructions =
        m_gradingInstructionsEdit
            ? m_gradingInstructionsEdit->toPlainText()
            : QString();
    subPrepRequest.specialInstructions =
        m_specialInstructionsEdit
            ? m_specialInstructionsEdit->toPlainText()
            : QString();
    subPrepRequest.schedule =
        scheduleForDays(
            fullSchedule,
            dialog.selectedDays()
            );
    subPrepRequest.classInformation =
        buildClassInformation(subPrepRequest.schedule);
    subPrepRequest.subNotes =
        m_subNotesEdit
            ? m_subNotesEdit->toPlainText()
            : QString();

    SubPrepPackageService::Request packageRequest;
    packageRequest.parent = this;
    packageRequest.services = m_services;
    packageRequest.subPrep = subPrepRequest;
    packageRequest.selectedDates = dialog.selectedDates();
    packageRequest.classIds = dialog.selectedClassIds();
    packageRequest.useIntensiveSchedule =
        m_scheduleWidget
        && m_scheduleWidget->displayState().displayMode
            == ScheduleDisplayMode::Intensive;
    packageRequest.createFolder = dialog.createFolder();
    packageRequest.targetRoot = dialog.targetRoot();
    packageRequest.userName = dialog.userName();
    packageRequest.replaceExisting = dialog.replaceExisting();
    packageRequest.printPaperCopies = dialog.printPaperCopies();
    packageRequest.openFolderAfterGeneration =
        dialog.openFolderAfterGeneration();
    const SubPrepPackageService::Result result =
        SubPrepPackageService::generate(packageRequest);

    if (result.status == SubPrepPackageService::Status::Failed)
    {
        QString message = result.message;

        if (result.folderCreated && !result.outputDirectory.isEmpty())
        {
            message += tr("\n\nThe package was created at:\n%1")
                .arg(result.outputDirectory);
        }

        DialogServices::showWarning(
            this,
            tr("Generate Sub Prep"),
            message
            );
    }
}
