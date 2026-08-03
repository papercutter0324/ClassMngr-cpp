#include "speaking_eval_report_dialog.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include <QComboBox>
#include <QDate>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <array>

QString speakingEvalReportDate(
    const QDate& date,
    SpeakingEvalReportTemplate reportTemplate
    )
{
    if (reportTemplate == SpeakingEvalReportTemplate::Advanced)
    {
        return date.toString(QStringLiteral("MMM. yyyy"));
    }

    return date.toString(
        date.month() >= 5 && date.month() <= 7
            ? QStringLiteral("MMMM yyyy")
            : QStringLiteral("MMM. yyyy")
        );
}

namespace
{

QString classLabel(
    const ClassInfo& info
    )
{
    QStringList parts;

    if (!info.classGrade.trimmed().isEmpty())
    {
        parts.append(info.classGrade.trimmed());
    }

    if (!info.classLevel.trimmed().isEmpty())
    {
        parts.append(info.classLevel.trimmed());
    }

    return parts.join(QLatin1Char(' '));
}

SpeakingEvalReportTemplate reportTemplateForClass(
    const ClassInfo& info
    )
{
    const QString grade =
        info.classGrade.trimmed();
    const QString level =
        info.classLevel.trimmed();

    const bool usesAdvancedTemplate = (
               grade.compare(
                   QStringLiteral("E5"),
                   Qt::CaseInsensitive
                   ) == 0
               && level.compare(
                   QStringLiteral("Athena"),
                   Qt::CaseInsensitive
                   ) == 0
               )
        || (
               grade.compare(
                   QStringLiteral("E6"),
                   Qt::CaseInsensitive
                   ) == 0
               && level.compare(
                   QStringLiteral("Song's"),
                   Qt::CaseInsensitive
                   ) == 0
               );

    return usesAdvancedTemplate
        ? SpeakingEvalReportTemplate::Advanced
        : SpeakingEvalReportTemplate::Standard;
}

QString studentDisplayName(
    const SpeakingEvalRows& rows,
    int row
    )
{
    if (row < 0 || row >= rows.size())
    {
        return {};
    }

    const QString englishName =
        rows[row].value(
            SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
            ).trimmed();
    const QString koreanName =
        rows[row].value(
            SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
            ).trimmed();

    if (englishName.isEmpty())
    {
        return koreanName;
    }
    if (koreanName.isEmpty())
    {
        return englishName;
    }

    return QObject::tr("%1 (%2)").arg(englishName, koreanName);
}

SpeakingEvalReportData reportDataForRow(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    int row
    )
{
    SpeakingEvalReportData data;
    if (row < 0 || row >= rows.size())
    {
        return data;
    }

    const QStringList& values = rows[row];
    data.englishName = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
        );
    data.koreanName = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
        );
    data.classLabel = classLabel(classInfo);
    data.nativeTeacher = classInfo.teacherEn;
    data.koreanTeacher = classInfo.teacherKr;
    data.comments = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::Comments)
        );
    data.notes = values.value(
        SpeakingEval::toInt(SpeakingEvalColumn::Notes)
        );
    data.reportTemplate = reportTemplateForClass(classInfo);
    data.date = speakingEvalReportDate(
        QDate::currentDate(),
        data.reportTemplate
        );

    const std::array<SpeakingEvalColumn, 6> scoreColumns{
        SpeakingEvalColumn::Grammar,
        SpeakingEvalColumn::Pronunciation,
        SpeakingEvalColumn::Fluency,
        SpeakingEvalColumn::Manner,
        SpeakingEvalColumn::Content,
        SpeakingEvalColumn::OverallEffort
    };
    for (int index = 0; index < scoreColumns.size(); ++index)
    {
        data.scores[index] = values.value(
            SpeakingEval::toInt(scoreColumns[index])
            );
    }

    return data;
}

} // namespace

QList<SpeakingEvalBatchReportService::StudentReport>
buildSpeakingEvalStudentReports(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    const QByteArray& signatureImage
    )
{
    QList<SpeakingEvalBatchReportService::StudentReport> reports;
    for (int row = 0; row < rows.size(); ++row)
    {
        const QString displayName = studentDisplayName(rows, row);
        if (displayName.isEmpty())
        {
            continue;
        }

        SpeakingEvalReportData report =
            reportDataForRow(rows, classInfo, row);
        report.signatureImage = signatureImage;
        reports.append({ displayName, report, row });
    }
    return reports;
}

SpeakingEvalReportDialog::SpeakingEvalReportDialog(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    QWidget* parent
    )
    : SpeakingEvalReportDialog(
        buildSpeakingEvalStudentReports(rows, classInfo),
        0,
        parent,
        true
        )
{
}

SpeakingEvalReportDialog::SpeakingEvalReportDialog(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    const QByteArray& signatureImage,
    QWidget* parent
    )
    : SpeakingEvalReportDialog(
        buildSpeakingEvalStudentReports(
            rows,
            classInfo,
            signatureImage
            ),
        0,
        parent,
        true
        )
{
}

SpeakingEvalReportDialog::SpeakingEvalReportDialog(
    const QList<SpeakingEvalBatchReportService::StudentReport>& reports,
    int currentStudentIndex,
    QWidget* parent,
    bool interactive
    )
    : QDialog(parent)
    , m_reports(reports)
    , m_interactive(interactive)
{
    setWindowTitle(tr("Speaking Evaluation Reports"));
    resize(940, 900);

    auto* layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* previewColumn =
        new QWidget(this);
    auto* previewLayout =
        new QVBoxLayout(previewColumn);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(12);

    auto* selectorLayout =
        new QHBoxLayout;
    selectorLayout->setContentsMargins(0, 0, 0, 0);

    auto* studentLabel =
        new QLabel(
            tr("Student:"),
            this
            );

    m_studentSelector =
        new QComboBox(this);

    m_studentSelector->setObjectName(
        QStringLiteral("speakingEvalReportStudentSelector")
        );
    m_studentSelector->setSizeAdjustPolicy(
        QComboBox::AdjustToContents
        );

    auto* previousButton =
        new TextFitPushButton(
            tr("Previous"),
            this
            );
    auto* nextButton =
        new TextFitPushButton(
            tr("Next"),
            this
            );

    selectorLayout->addWidget(studentLabel);
    selectorLayout->addWidget(m_studentSelector, 1);
    selectorLayout->addWidget(previousButton);
    selectorLayout->addWidget(nextButton);
    previewLayout->addLayout(selectorLayout);

    m_notesLabel =
        new QLabel(
            tr("Private Notes (not included in the report)"),
            previewColumn
            );
    m_notesEdit =
        new QPlainTextEdit(previewColumn);
    m_notesEdit->setPlaceholderText(
        tr("Add internal notes about this student…")
        );
    m_notesEdit->setFixedHeight(72);
    m_notesEdit->setTabChangesFocus(true);
    m_notesLabel->setVisible(m_interactive);
    m_notesEdit->setVisible(m_interactive);
    previewLayout->addWidget(m_notesLabel);
    previewLayout->addWidget(m_notesEdit);

    auto* scrollArea =
        new QScrollArea(this);

    scrollArea->setWidgetResizable(false);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    scrollArea->setFrameShape(QFrame::StyledPanel);

    m_report =
        new SpeakingEvalReportWidget(previewColumn);
    m_report->setInteractive(m_interactive);

    previewLayout->addWidget(m_report);
    previewColumn->setFixedSize(
        m_report->width(),
        previewLayout->sizeHint().height()
        );
    scrollArea->setWidget(previewColumn);
    layout->addWidget(scrollArea, 1);

    auto* buttonLayout =
        new QHBoxLayout;

    auto* printButton =
        new TextFitPushButton(
            tr("Print"),
            this
            );
    printButton->setObjectName(
        QStringLiteral("speakingEvalReportPrintButton")
        );

    auto* saveAsPdfButton =
        new TextFitPushButton(
            tr("Save As PDF"),
            this
            );
    saveAsPdfButton->setObjectName(
        QStringLiteral("speakingEvalReportSaveAsPdfButton")
        );

    auto* closeButton =
        new TextFitPushButton(
            tr("Close"),
            this
            );

    buttonLayout->addWidget(printButton);
    buttonLayout->addWidget(saveAsPdfButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    for (int index = 0; index < m_reports.size(); ++index)
    {
        m_studentSelector->addItem(
            m_reports.at(index).displayName,
            index
            );
    }

    const bool hasStudents =
        m_studentSelector->count() > 0;

    if (!hasStudents)
    {
        m_studentSelector->addItem(
            tr("No students available")
            );
    }

    m_studentSelector->setEnabled(hasStudents);
    previousButton->setEnabled(hasStudents);
    nextButton->setEnabled(hasStudents);
    printButton->setEnabled(hasStudents);
    saveAsPdfButton->setEnabled(hasStudents);
    m_notesEdit->setEnabled(hasStudents);
    m_report->setInteractive(m_interactive && hasStudents);
    if (hasStudents)
    {
        m_studentSelector->setCurrentIndex(
            qBound(0, currentStudentIndex, m_reports.size() - 1)
            );
    }

    connect(
        m_studentSelector,
        &QComboBox::currentIndexChanged,
        this,
        [this](int)
        {
            updateReport();
        }
        );
    connect(
        previousButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::moveToPreviousStudent
        );
    connect(
        nextButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::moveToNextStudent
        );
    connect(
        m_report,
        &SpeakingEvalReportWidget::scoreEdited,
        this,
        [this](int metricIndex, const QString& score)
        {
            static constexpr std::array<SpeakingEvalColumn, 6> columns{
                SpeakingEvalColumn::Grammar,
                SpeakingEvalColumn::Pronunciation,
                SpeakingEvalColumn::Fluency,
                SpeakingEvalColumn::Manner,
                SpeakingEvalColumn::Content,
                SpeakingEvalColumn::OverallEffort
            };
            const int reportIndex =
                m_studentSelector
                    ? m_studentSelector->currentData().toInt()
                    : -1;
            if (
                !m_interactive
                || reportIndex < 0
                || reportIndex >= m_reports.size()
                || metricIndex < 0
                || metricIndex >= columns.size()
                )
            {
                return;
            }

            m_reports[reportIndex].report.scores[metricIndex] = score;
            emit reportValueEdited(
                m_reports.at(reportIndex).sourceRow,
                columns[metricIndex],
                score
                );
        }
        );
    connect(
        m_report,
        &SpeakingEvalReportWidget::commentsEdited,
        this,
        [this](const QString& comments)
        {
            const int reportIndex =
                m_studentSelector
                    ? m_studentSelector->currentData().toInt()
                    : -1;
            if (
                !m_interactive
                || reportIndex < 0
                || reportIndex >= m_reports.size()
                )
            {
                return;
            }

            m_reports[reportIndex].report.comments = comments;
            emit reportValueEdited(
                m_reports.at(reportIndex).sourceRow,
                SpeakingEvalColumn::Comments,
                comments
                );
        }
        );
    connect(
        m_notesEdit,
        &QPlainTextEdit::textChanged,
        this,
        [this]()
        {
            const int reportIndex =
                m_studentSelector
                    ? m_studentSelector->currentData().toInt()
                    : -1;
            if (
                !m_interactive
                || reportIndex < 0
                || reportIndex >= m_reports.size()
                )
            {
                return;
            }

            const QString notes = m_notesEdit->toPlainText();
            m_reports[reportIndex].report.notes = notes;
            emit reportValueEdited(
                m_reports.at(reportIndex).sourceRow,
                SpeakingEvalColumn::Notes,
                notes
                );
        }
        );
    connect(
        printButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::printCurrentReport
        );
    connect(
        saveAsPdfButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::saveCurrentReportAsPdf
        );
    connect(
        closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept
        );

    updateReport();
}

const SpeakingEvalBatchReportService::StudentReport*
SpeakingEvalReportDialog::currentReport() const
{
    if (!m_studentSelector)
    {
        return nullptr;
    }

    const int reportIndex =
        m_studentSelector->currentData().toInt();
    if (reportIndex < 0 || reportIndex >= m_reports.size())
    {
        return nullptr;
    }

    return &m_reports.at(reportIndex);
}

void SpeakingEvalReportDialog::updateReport()
{
    if (!m_report || !m_studentSelector)
    {
        return;
    }

    const int row =
        m_studentSelector->currentData().toInt();

    if (row < 0 || row >= m_reports.size())
    {
        m_report->setReportData({});
        if (m_notesEdit)
        {
            const QSignalBlocker blocker(m_notesEdit);
            m_notesEdit->clear();
        }
        return;
    }

    m_report->setReportData(
        m_reports.at(row).report
        );

    if (m_notesEdit)
    {
        const QSignalBlocker blocker(m_notesEdit);
        m_notesEdit->setPlainText(
            m_reports.at(row).report.notes
            );
    }
}

void SpeakingEvalReportDialog::moveToPreviousStudent()
{
    if (!m_studentSelector || m_studentSelector->count() < 2)
    {
        return;
    }

    const int previousIndex =
        (m_studentSelector->currentIndex() - 1 + m_studentSelector->count())
        % m_studentSelector->count();

    m_studentSelector->setCurrentIndex(previousIndex);
}

void SpeakingEvalReportDialog::moveToNextStudent()
{
    if (!m_studentSelector || m_studentSelector->count() < 2)
    {
        return;
    }

    m_studentSelector->setCurrentIndex(
        (m_studentSelector->currentIndex() + 1)
        % m_studentSelector->count()
        );
}

void SpeakingEvalReportDialog::printCurrentReport()
{
    const SpeakingEvalBatchReportService::StudentReport* selectedReport =
        currentReport();
    if (!selectedReport)
    {
        return;
    }

    SpeakingEvalBatchReportService::Request request;
    request.parent = this;
    request.reports = { *selectedReport };
    request.renderer =
        SpeakingEvalBatchReportService::Renderer::Internal;
    request.printReports = true;

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    if (result.status
        == SpeakingEvalBatchReportService::Status::Failed
        || result.status
            == SpeakingEvalBatchReportService::Status::InternalRendererFailed)
    {
        QMessageBox::warning(
            this,
            tr("Report Printing Failed"),
            result.message
            );
    }
}

void SpeakingEvalReportDialog::saveCurrentReportAsPdf()
{
    const SpeakingEvalBatchReportService::StudentReport* selectedReport =
        currentReport();
    if (!selectedReport)
    {
        return;
    }

    const QString documentsDirectory =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation
            );
    const QString suggestedFileName =
        SpeakingEvalBatchReportService::safeFileName(
            selectedReport->report.englishName,
            selectedReport->report.koreanName
            );

    QFileDialog dialog(
        this,
        tr("Save Speaking Evaluation Report As"),
        documentsDirectory,
        tr("PDF Documents (*.pdf)")
        );
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setDefaultSuffix(QStringLiteral("pdf"));
    dialog.selectFile(suggestedFileName);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QStringList selectedFiles =
        dialog.selectedFiles();
    if (selectedFiles.isEmpty())
    {
        return;
    }

    QString savePath =
        selectedFiles.constFirst();
    if (QFileInfo(savePath).suffix().isEmpty())
    {
        savePath += QStringLiteral(".pdf");
    }

    SpeakingEvalBatchReportService::Request request;
    request.parent = this;
    request.reports = { *selectedReport };
    request.renderer =
        SpeakingEvalBatchReportService::Renderer::Internal;
    request.savePdf = true;
    request.overwriteExisting = true;
    request.outputFilePath = savePath;

    const SpeakingEvalBatchReportService::Result result =
        SpeakingEvalBatchReportService::exportReports(request);
    if (result.status
        == SpeakingEvalBatchReportService::Status::Failed
        || result.status
            == SpeakingEvalBatchReportService::Status::InternalRendererFailed)
    {
        QMessageBox::warning(
            this,
            tr("Save PDF Failed"),
            result.message
            );
    }
}
