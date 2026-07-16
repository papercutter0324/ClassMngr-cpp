#include "sub_prep_page_p.h"

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

    loadPageData();
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

    if (!isVisible())
    {
        return;
    }

    loadPersonalZoomInformation();
    loadCampuses();
    refreshGeneratedContent();

    if (!m_dirty)
    {
        loadStoredSettings();
    }
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
            tr("Print Sub Prep")
            );
        m_printButton->setToolTip(
            tr("Print all sub prep information as an A4 PDF.")
            );
    }

    if (m_importantInformationHeading)
    {
        m_importantInformationHeading->setText(
            tr("Important Information")
            );
    }

    if (m_subNotesHeading)
    {
        m_subNotesHeading->setText(
            tr("Additional Notes")
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
            tr("Class Materials")
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

    if (m_notesCard)
    {
        m_notesCard->setTitle(
            tr("Notes")
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
        target = m_subNotesHeading;
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
        return tr("Additional Notes");
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

void SubPrepPage::showEvent(
    QShowEvent* event
    )
{
    BasePage::showEvent(event);

    loadPersonalZoomInformation();
    loadCampuses();
    refreshGeneratedContent();

    if (!m_dirty)
    {
        loadStoredSettings();
    }
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

void SubPrepPage::printSubPrep()
{
    if (hasUnsavedChanges() && !saveChanges())
    {
        return;
    }

    refreshGeneratedContent();

    QList<CalendarEvent> calendarEvents;
    const QDate currentDate = QDate::currentDate();

    if (auto* dataService = openDataService(m_services))
    {
        const QDate currentWeekStart =
            currentDate.addDays(
                Qt::Monday - currentDate.dayOfWeek()
                );
        calendarEvents =
            dataService->loadCalendarEventsInRange(
                currentWeekStart,
                currentWeekStart.addDays(11)
                );
    }

    SubPrepPrintDialog dialog(
        calendarEvents,
        currentDate,
        this
        );

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    SubPrepPrintService::Request request;
    request.parent = this;
    request.campus = {
        m_officeNumberEdit ? m_officeNumberEdit->text() : QString(),
        m_officeWifiEdit ? m_officeWifiEdit->text() : QString(),
        m_officeWifiPasswordEdit ? m_officeWifiPasswordEdit->text() : QString(),
        m_photocopierCodeEdit ? m_photocopierCodeEdit->text() : QString()
    };
    request.zoom = {
        m_zoomLoginIdEdit ? m_zoomLoginIdEdit->text() : QString(),
        m_zoomPasswordEdit ? m_zoomPasswordEdit->text() : QString()
    };
    request.classMaterials =
        m_classMaterialsEdit
            ? m_classMaterialsEdit->toPlainText()
            : QString();
    request.gradingInstructions =
        m_gradingInstructionsEdit
            ? m_gradingInstructionsEdit->toPlainText()
            : QString();
    request.specialInstructions =
        m_specialInstructionsEdit
            ? m_specialInstructionsEdit->toPlainText()
            : QString();
    request.schedule =
        scheduleForDays(
            m_scheduleWidget
                ? m_scheduleWidget->scheduleModel()
                : ScheduleViewModel(),
            dialog.selectedDays()
            );
    request.classInformation =
        buildClassInformation(request.schedule);
    request.subNotes =
        m_subNotesEdit
            ? m_subNotesEdit->toPlainText()
            : QString();

    const SubPrepPrintService::Result result =
        dialog.selectedAction() == SubPrepPrintDialog::Action::SaveAs
            ? SubPrepPrintService::saveSubPrepPdf(
                request,
                dialog.selectedSavePath()
                )
            : SubPrepPrintService::printSubPrep(request);

    if (result.status == SubPrepPrintService::Status::Failed)
    {
        QMessageBox::warning(
            this,
            tr("Print Sub Prep"),
            result.message
            );
    }
}
