#include "schedule_import_dialog.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "features/schedule/import/schedule_workbook_parser.h"
#include "features/schedule/ui/schedule_import_dialog_shared.h"
#include "features/schedule/ui/schedule_import_review_dialog.h"
#include "features/teacher/import/teacher_import_name_utils.h"
#include "ui/shared/widgets/no_wheel_combobox.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace
{
constexpr int SourceDialogWidth = 436;
constexpr int SourceDialogHeight = 520;
constexpr int ContinuationSpacerHeight = 8;

struct ScheduleWorkbookLoadResult
{
    ScheduleImportWorkbook workbook;
    QString error;
    bool fileOpened = false;
    bool succeeded = false;
};
}

ScheduleImportDialog::ScheduleImportDialog(
    ApplicationServices* services,
    QWidget* parent
    )
    : QDialog(parent)
    , m_services(services)
{
    setWindowTitle(tr("Import Schedule"));
    setModal(true);
    buildUi();
    enforceStaticSize();
}

void ScheduleImportDialog::setFilePath(
    const QString& filePath
    )
{
    ++m_loadRequestId;
    setLoading(false);
    m_fileEdit->setText(filePath);
    m_workbookLoaded = false;
    m_sheetCombo->clear();
    m_worksheetSection->setVisible(false);
    resetUserSelection();
    m_scheduleTypeSection->setVisible(
        !filePath.trimmed().isEmpty()
        );
    setSourceStatus(
        filePath.trimmed().isEmpty()
            ? tr("Choose a file and schedule type.")
            : tr("Ready to read the spreadsheet.")
        );
    updateNavigation();
    enforceStaticSize();
}

void ScheduleImportDialog::buildUi()
{
    auto* layout =
        new QVBoxLayout(this);
    layout->setSpacing(12);

    m_sourceStatus =
        new QLabel(
            tr("Choose a file and schedule type."),
            this
            );
    m_sourceStatus->setObjectName(
        QStringLiteral("scheduleImportSourceStatus")
        );
    m_sourceStatus->setWordWrap(true);
    m_sourceStatus->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_sourceStatus);

    m_fileSection =
        new QGroupBox(
            tr("Choose a spreadsheet"),
            this
            );
    m_fileSection->setObjectName(
        QStringLiteral("scheduleImportFileSection")
        );
    auto* fileLayout =
        new QHBoxLayout(m_fileSection);
    m_fileEdit =
        new QLineEdit(m_fileSection);
    m_fileEdit->setObjectName(
        QStringLiteral("scheduleImportFilePath")
        );
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText(
        tr("Select an XLSX schedule...")
        );
    m_browseButton =
        new QPushButton(
            tr("Browse"),
            m_fileSection
            );
    m_browseButton->setObjectName(
        QStringLiteral("scheduleImportBrowseButton")
        );
    fileLayout->addWidget(m_fileEdit, 1);
    fileLayout->addWidget(m_browseButton);
    layout->addWidget(m_fileSection);

    m_scheduleTypeSection =
        new QGroupBox(
            tr("Schedule type"),
            this
            );
    m_scheduleTypeSection->setObjectName(
        QStringLiteral("scheduleImportScheduleTypeSection")
        );
    auto* typeLayout =
        new QHBoxLayout(m_scheduleTypeSection);
    typeLayout->setSpacing(16);
    m_normalRadio =
        new QRadioButton(
            tr("Regular"),
            m_scheduleTypeSection
            );
    m_intensiveRadio =
        new QRadioButton(
            tr("Intensives"),
            m_scheduleTypeSection
            );
    m_normalRadio->setObjectName(
        QStringLiteral("scheduleImportNormalRadio")
        );
    m_intensiveRadio->setObjectName(
        QStringLiteral("scheduleImportIntensiveRadio")
        );
    auto* typeButtons =
        new QButtonGroup(m_scheduleTypeSection);
    typeButtons->addButton(m_normalRadio);
    typeButtons->addButton(m_intensiveRadio);
    typeLayout->addWidget(m_normalRadio);
    typeLayout->addWidget(m_intensiveRadio);
    typeLayout->addStretch();
    m_scheduleTypeSection->setVisible(false);
    layout->addWidget(m_scheduleTypeSection);

    m_worksheetSection =
        new QGroupBox(
            tr("Worksheet"),
            this
            );
    m_worksheetSection->setObjectName(
        QStringLiteral("scheduleImportWorksheetSection")
        );
    auto* worksheetLayout =
        new QVBoxLayout(m_worksheetSection);
    m_sheetCombo =
        new NoWheelComboBox(m_worksheetSection);
    m_sheetCombo->setObjectName(
        QStringLiteral("scheduleImportSheetCombo")
        );
    worksheetLayout->addWidget(m_sheetCombo);
    m_worksheetSection->setVisible(false);
    layout->addWidget(m_worksheetSection);

    m_userSection =
        new QGroupBox(
            tr("Select the schedule to import"),
            this
            );
    m_userSection->setObjectName(
        QStringLiteral("scheduleImportUserSection")
        );
    auto* userLayout =
        new QVBoxLayout(m_userSection);
    m_userStatus =
        new QLabel(m_userSection);
    m_userStatus->setObjectName(
        QStringLiteral("scheduleImportUserStatus")
        );
    m_userStatus->setWordWrap(true);
    userLayout->addWidget(m_userStatus);

    m_userCombo =
        new NoWheelComboBox(m_userSection);
    m_userCombo->setObjectName(
        QStringLiteral("scheduleImportUserCombo")
        );
    userLayout->addWidget(m_userCombo);

    m_nameConfirmation =
        new QCheckBox(
            tr(
                "Update my name on the My Information page "
                "to match the selected name."
                ),
            m_userSection
            );
    m_nameConfirmation->setObjectName(
        QStringLiteral("scheduleImportNameConfirmation")
        );
    userLayout->addWidget(m_nameConfirmation);
    m_userSection->setVisible(false);
    layout->addWidget(m_userSection);

    m_continuationSpacer =
        new QWidget(this);
    m_continuationSpacer->setObjectName(
        QStringLiteral("scheduleImportContinuationSpacer")
        );
    m_continuationSpacer->setFixedHeight(
        ContinuationSpacerHeight
        );
    m_continuationSpacer->setVisible(false);
    layout->addWidget(m_continuationSpacer);

    m_continuationHint =
        new QLabel(
            tr("Click Next to continue."),
            this
            );
    m_continuationHint->setObjectName(
        QStringLiteral("scheduleImportContinuationHint")
        );
    m_continuationHint->setAlignment(Qt::AlignHCenter);
    m_continuationHint->setVisible(false);
    layout->addWidget(m_continuationHint);

    m_progressBar =
        new QProgressBar(this);
    m_progressBar->setObjectName(
        QStringLiteral("scheduleImportProgressBar")
        );
    m_progressBar->setRange(0, 0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);
    layout->addStretch();

    m_buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Cancel,
            this
            );
    m_nextButton =
        m_buttons->addButton(
            tr("Load"),
            QDialogButtonBox::ActionRole
            );
    m_nextButton->setObjectName(
        QStringLiteral("scheduleImportNextButton")
        );
    layout->addWidget(m_buttons);

    connect(
        m_buttons,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );
    connect(
        m_nextButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportDialog::goNext
        );
    connect(
        m_browseButton,
        &QPushButton::clicked,
        this,
        &ScheduleImportDialog::browseForFile
        );

    const auto invalidate =
        [this]()
        {
            ++m_loadRequestId;
            setLoading(false);
            m_workbookLoaded = false;
            m_sheetCombo->clear();
            m_worksheetSection->setVisible(false);
            resetUserSelection();
            setSourceStatus(
                tr("Ready to read the spreadsheet.")
                );
            updateNavigation();
        };
    connect(
        m_normalRadio,
        &QRadioButton::toggled,
        this,
        invalidate
        );
    connect(
        m_intensiveRadio,
        &QRadioButton::toggled,
        this,
        invalidate
        );
    connect(
        m_sheetCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]()
        {
            updateSelectedSheet();
            updateNavigation();
        }
        );
    connect(
        m_userCombo,
        &QComboBox::currentIndexChanged,
        this,
        &ScheduleImportDialog::updateUserSelection
        );
    connect(
        m_nameConfirmation,
        &QCheckBox::toggled,
        this,
        &ScheduleImportDialog::updateNavigation
        );

    updateNavigation();
}

void ScheduleImportDialog::browseForFile()
{
    const QString selected =
        QFileDialog::getOpenFileName(
            this,
            tr("Select Schedule Import File"),
            QFileInfo(m_fileEdit->text()).absolutePath(),
            tr("Excel Workbooks (*.xlsx)")
            );
    if (!selected.isEmpty())
    {
        setFilePath(selected);
    }
}

bool ScheduleImportDialog::loadWorkbook()
{
    if (m_fileEdit->text().trimmed().isEmpty())
    {
        setSourceStatus(
            tr("Choose an XLSX file.")
            );
        return false;
    }
    if (
        QFileInfo(m_fileEdit->text()).suffix()
            .compare(
                QStringLiteral("xlsx"),
                Qt::CaseInsensitive
                ) != 0
        )
    {
        setSourceStatus(
            tr("Choose an Excel workbook with the .xlsx extension.")
            );
        return false;
    }
    if (
        !m_normalRadio->isChecked()
        && !m_intensiveRadio->isChecked()
        )
    {
        setSourceStatus(
            tr("Choose Regular or Intensives.")
            );
        return false;
    }

    const ScheduleImportKind kind =
        selectedKind();

    if (
        m_workbookLoaded
        && m_loadedFilePath == m_fileEdit->text()
        && m_loadedKind == kind
        )
    {
        return true;
    }

    if (m_loading)
    {
        return true;
    }

    const QString filePath =
        m_fileEdit->text();
    const quint64 requestId =
        ++m_loadRequestId;
    setLoading(true);

    auto* watcher =
        new QFutureWatcher<ScheduleWorkbookLoadResult>(
            this
            );
    connect(
        watcher,
        &QFutureWatcher<ScheduleWorkbookLoadResult>::finished,
        this,
        [this, watcher, requestId, filePath, kind]()
        {
            const ScheduleWorkbookLoadResult result =
                watcher->result();
            watcher->deleteLater();

            if (requestId != m_loadRequestId)
            {
                return;
            }

            setLoading(false);
            if (!result.fileOpened)
            {
                setSourceStatus(
                    tr("The selected workbook could not be opened.")
                    );
                updateNavigation();
                return;
            }
            if (!result.succeeded)
            {
                setSourceStatus(
                    tr("Invalid workbook: %1")
                        .arg(result.error)
                    );
                updateNavigation();
                return;
            }

            applyLoadedWorkbook(
                result.workbook,
                filePath,
                kind
                );
        }
        );
    watcher->setFuture(
        QtConcurrent::run(
            [filePath, kind]()
            {
                ScheduleWorkbookLoadResult result;
                QFile file(filePath);
                if (!file.open(QIODevice::ReadOnly))
                {
                    return result;
                }

                result.fileOpened = true;
                const auto parsed =
                    parseScheduleImportWorkbook(
                        file.readAll(),
                        kind
                        );
                if (!parsed)
                {
                    result.error = parsed.error();
                    return result;
                }

                result.workbook = *parsed;
                result.succeeded = true;
                return result;
            }
            )
        );
    return true;
}

void ScheduleImportDialog::applyLoadedWorkbook(
    ScheduleImportWorkbook workbook,
    const QString& filePath,
    ScheduleImportKind kind
    )
{
    m_workbook = std::move(workbook);
    m_loadedFilePath = filePath;
    m_loadedKind = kind;
    m_workbookLoaded = true;
    const QSignalBlocker sheetComboBlocker(m_sheetCombo);
    m_sheetCombo->clear();

    QList<int> visibleIndexes;
    for (int index = 0; index < m_workbook.sheets.size(); ++index)
    {
        if (m_workbook.sheets[index].visible)
        {
            visibleIndexes.append(index);
        }
    }

    if (visibleIndexes.size() > 1)
    {
        m_sheetCombo->addItem(
            tr("Select a worksheet...")
            );
        m_sheetCombo->setItemData(0, -1);
    }

    for (int sheetIndex : visibleIndexes)
    {
        m_sheetCombo->addItem(
            m_workbook.sheets[sheetIndex].name,
            sheetIndex
            );
    }

    if (visibleIndexes.size() == 1)
    {
        m_sheetCombo->setCurrentIndex(0);
    }

    const bool multiple =
        visibleIndexes.size() > 1;
    m_worksheetSection->setVisible(multiple);
    setSourceStatus(
        multiple
            ? tr("Choose the worksheet to import.")
            : tr("Workbook and worksheet are valid.")
        );
    updateSelectedSheet();
    updateNavigation();
    enforceStaticSize();
}

void ScheduleImportDialog::setLoading(
    bool loading
    )
{
    m_loading = loading;
    m_progressBar->setVisible(loading);
    m_fileEdit->setEnabled(!loading);
    m_browseButton->setEnabled(!loading);
    m_normalRadio->setEnabled(!loading);
    m_intensiveRadio->setEnabled(!loading);
    m_sheetCombo->setEnabled(!loading);
    m_userCombo->setEnabled(!loading);
    m_nameConfirmation->setEnabled(!loading);

    if (loading)
    {
        setSourceStatus(
            tr("Loading workbook...")
            );
    }
    updateNavigation();
}

void ScheduleImportDialog::setSourceStatus(
    const QString& text,
    bool showContinuationHint
    )
{
    m_sourceStatus->setText(text);
    m_continuationSpacer->setVisible(showContinuationHint);
    m_continuationHint->setVisible(showContinuationHint);
}

void ScheduleImportDialog::resetUserSelection()
{
    if (m_userCombo)
    {
        const QSignalBlocker blocker(m_userCombo);
        m_userCombo->clear();
    }
    if (m_nameConfirmation)
    {
        m_nameConfirmation->setChecked(false);
        m_nameConfirmation->setVisible(false);
    }
    if (m_userStatus)
    {
        m_userStatus->clear();
        m_userStatus->setVisible(false);
    }
    if (m_userSection)
    {
        m_userSection->setVisible(false);
    }
    if (m_continuationSpacer)
    {
        m_continuationSpacer->setVisible(false);
    }
    if (m_continuationHint)
    {
        m_continuationHint->setVisible(false);
    }
}

void ScheduleImportDialog::updateSelectedSheet()
{
    resetUserSelection();
    const ScheduleImportSheet* sheet =
        selectedSheet();
    if (!sheet)
    {
        return;
    }

    if (sheet->users.isEmpty())
    {
        if (!sheet->diagnostics.isEmpty())
        {
            QStringList diagnostics;
            for (const ScheduleImportDiagnostic& diagnostic :
                 sheet->diagnostics)
            {
                diagnostics.append(
                    tr("%1: %2")
                        .arg(
                            diagnostic.cellReference,
                            diagnostic.message
                            )
                    );
            }
            setSourceStatus(
                diagnostics.join(QLatin1Char('\n'))
                );
        }
        else
        {
            setSourceStatus(
                tr("The selected worksheet contains no supported user schedules.")
                );
        }
        return;
    }

    setSourceStatus(
        tr("Workbook and worksheet are valid.")
        );
    prepareUserSelection();
}

void ScheduleImportDialog::prepareUserSelection()
{
    const ScheduleImportSheet* sheet =
        selectedSheet();
    const QSignalBlocker userComboBlocker(m_userCombo);
    m_userCombo->clear();
    m_nameConfirmation->setChecked(false);

    DataService* dataService =
        openScheduleImportDataService(m_services);
    m_profileName =
        dataService
            ? dataService
                ->loadSetting(
                    QStringLiteral("myInfo/name"),
                    QString()
                    )
                .toString()
                .trimmed()
            : QString();

    if (!sheet)
    {
        return;
    }

    const QString normalizedProfile =
        normalizedScheduleImportUserName(
            m_profileName
            );
    int exactIndex = -1;
    int exactCount = 0;

    for (int index = 0; index < sheet->users.size(); ++index)
    {
        if (
            !normalizedProfile.isEmpty()
            && normalizedScheduleImportUserName(
                sheet->users[index].name
                ) == normalizedProfile
            )
        {
            exactIndex = index;
            ++exactCount;
        }
    }

    const bool requireExplicit =
        m_profileName.isEmpty()
        || exactCount != 1;

    if (
        requireExplicit
        && sheet->users.size() != 1
        )
    {
        m_userCombo->addItem(
            tr("Select a detected name..."),
            -1
            );
    }
    else if (
        requireExplicit
        && m_profileName.isEmpty()
        )
    {
        m_userCombo->addItem(
            tr("Select the detected name..."),
            -1
            );
    }

    for (int index = 0; index < sheet->users.size(); ++index)
    {
        m_userCombo->addItem(
            sheet->users[index].name,
            index
            );
    }

    if (exactCount == 1)
    {
        const int comboIndex =
            m_userCombo->findData(exactIndex);
        m_userCombo->setCurrentIndex(comboIndex);
    }
    else if (
        sheet->users.size() == 1
        && !m_profileName.isEmpty()
        )
    {
        m_userCombo->setCurrentIndex(
            m_userCombo->findData(0)
            );
    }

    m_userSection->setVisible(true);
    updateUserSelection();
}

void ScheduleImportDialog::updateUserSelection()
{
    const ScheduleImportUserBlock* user =
        selectedUser();
    const bool profileBlank =
        m_profileName.trimmed().isEmpty();
    const bool mismatch =
        user
        && !profileBlank
        && normalizedScheduleImportUserName(user->name)
            != normalizedScheduleImportUserName(
                m_profileName
                );

    if (user && profileBlank)
    {
        m_userStatus->setText(
            tr("My Information has no name. The selected spreadsheet name will be saved after a successful import.")
            );
        m_userStatus->setVisible(true);
    }
    else if (mismatch)
    {
        m_userStatus->setText(
            tr("Entered name on the My Information page: %1")
                .arg(m_profileName)
            );
        m_userStatus->setVisible(true);
    }
    else
    {
        m_userStatus->clear();
        m_userStatus->setVisible(false);
    }

    m_nameConfirmation->setVisible(mismatch);
    if (!mismatch)
    {
        m_nameConfirmation->setChecked(false);
    }
    setSourceStatus(
        m_sourceStatus->text(),
        user != nullptr
        );
    updateNavigation();
    enforceStaticSize();
}

void ScheduleImportDialog::updateNavigation()
{
    m_nextButton->setText(
        m_workbookLoaded
            ? tr("Next")
            : tr("Load")
        );

    bool nextEnabled = false;
    if (!m_loading && !m_reviewDialog)
    {
        const bool sourceReady =
            !m_fileEdit->text().trimmed().isEmpty()
            && (
                m_normalRadio->isChecked()
                || m_intensiveRadio->isChecked()
                );
        if (!m_workbookLoaded)
        {
            nextEnabled = sourceReady;
        }
        else
        {
            const ScheduleImportUserBlock* user =
                selectedUser();
            nextEnabled =
                sourceReady
                && selectedSheet()
                && user;
        }
    }

    m_nextButton->setEnabled(nextEnabled);
}

void ScheduleImportDialog::enforceStaticSize()
{
    int targetWidth =
        SourceDialogWidth;

    if (QScreen* targetScreen = screen())
    {
        const QSize available =
            targetScreen->availableGeometry().size();
        targetWidth =
            std::min(targetWidth, available.width());
    }

    QLayout* dialogLayout =
        layout();
    if (dialogLayout)
    {
        dialogLayout->invalidate();
    }
    const int layoutHeight =
        dialogLayout
        ? dialogLayout->totalHeightForWidth(targetWidth)
        : 0;
    int targetHeight =
        std::max(SourceDialogHeight, layoutHeight);

    if (QScreen* targetScreen = screen())
    {
        targetHeight =
            std::min(
                targetHeight,
                targetScreen->availableGeometry().height()
                );
    }

    setFixedHeight(
        std::max(1, targetHeight)
        );
    resize(
        std::max(1, targetWidth),
        height()
        );
}

void ScheduleImportDialog::goNext()
{
    const bool workbookWasLoaded =
        m_workbookLoaded
        && m_loadedFilePath == m_fileEdit->text()
        && m_loadedKind == selectedKind();
    if (!loadWorkbook())
    {
        return;
    }
    if (!workbookWasLoaded)
    {
        return;
    }
    if (!selectedSheet())
    {
        setSourceStatus(
            tr("Choose a worksheet.")
            );
        updateNavigation();
        return;
    }
    if (!m_userSection->isVisible())
    {
        updateSelectedSheet();
        return;
    }
    if (!selectedUser())
    {
        return;
    }

    const bool mismatch =
        !m_profileName.isEmpty()
        && normalizedScheduleImportUserName(
            selectedUser()->name
            )
            != normalizedScheduleImportUserName(
                m_profileName
                );
    if (
        mismatch
        && !m_nameConfirmation->isChecked()
        && QMessageBox::question(
            this,
            tr("Name Mismatch"),
            tr(
                "The selected name does not match the name entered "
                "on the My Information page. "
                "Do you want to continue anyway?"
                ),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
            ) != QMessageBox::Yes
        )
    {
        return;
    }

    openReviewDialog();
}

void ScheduleImportDialog::openReviewDialog()
{
    if (m_reviewDialog)
    {
        m_reviewDialog->raise();
        m_reviewDialog->activateWindow();
        return;
    }

    ScheduleImportReviewRequest request;
    request.user = *selectedUser();
    request.kind = selectedKind();
    request.profileName = m_profileName;
    request.updateProfileName =
        m_nameConfirmation->isChecked();

    auto* review =
        new ScheduleImportReviewDialog(
            m_services,
            std::move(request),
            this
            );
    if (!review->prepare())
    {
        delete review;
        return;
    }

    m_reviewDialog = review;
    review->setAttribute(
        Qt::WA_DeleteOnClose
        );
    connect(
        review,
        &QDialog::accepted,
        this,
        &QDialog::accept
        );
    connect(
        review,
        &QDialog::rejected,
        this,
        [this]()
        {
            m_reviewDialog = nullptr;
            updateNavigation();
        }
        );
    updateNavigation();
    review->open();
}

ScheduleImportKind ScheduleImportDialog::selectedKind() const
{
    return m_intensiveRadio->isChecked()
        ? ScheduleImportKind::Intensive
        : ScheduleImportKind::Normal;
}

const ScheduleImportSheet* ScheduleImportDialog::selectedSheet() const
{
    if (!m_workbookLoaded)
    {
        return nullptr;
    }
    const int sheetIndex =
        m_sheetCombo->currentData().toInt();
    return sheetIndex >= 0
        && sheetIndex < m_workbook.sheets.size()
        ? &m_workbook.sheets[sheetIndex]
        : nullptr;
}

const ScheduleImportUserBlock* ScheduleImportDialog::selectedUser() const
{
    const ScheduleImportSheet* sheet =
        selectedSheet();
    const int userIndex =
        m_userCombo->currentData().toInt();
    return sheet
        && userIndex >= 0
        && userIndex < sheet->users.size()
        ? &sheet->users[userIndex]
        : nullptr;
}
