#include "speaking_eval_report_dialog.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

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

bool usesAdvancedTemplate(
    const ClassInfo& info
    )
{
    const QString grade =
        info.classGrade.trimmed();
    const QString level =
        info.classLevel.trimmed();

    return (
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
}

} // namespace

SpeakingEvalReportDialog::SpeakingEvalReportDialog(
    const SpeakingEvalRows& rows,
    const ClassInfo& classInfo,
    QWidget* parent
    )
    : QDialog(parent)
    , m_rows(rows)
    , m_classInfo(classInfo)
{
    setWindowTitle(tr("Speaking Evaluation Reports"));
    resize(940, 900);

    auto* layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto* selectorLayout =
        new QHBoxLayout;

    auto* studentLabel =
        new QLabel(
            tr("Student:"),
            this
            );

    m_studentSelector =
        new QComboBox(this);

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
    layout->addLayout(selectorLayout);

    auto* scrollArea =
        new QScrollArea(this);

    scrollArea->setWidgetResizable(false);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    scrollArea->setFrameShape(QFrame::StyledPanel);

    m_report =
        new SpeakingEvalReportWidget(scrollArea);

    scrollArea->setWidget(m_report);
    layout->addWidget(scrollArea, 1);

    auto* buttonLayout =
        new QHBoxLayout;

    auto* printButton =
        new TextFitPushButton(
            tr("Print This Report"),
            this
            );
    auto* closeButton =
        new TextFitPushButton(
            tr("Close"),
            this
            );

    buttonLayout->addStretch();
    buttonLayout->addWidget(printButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    for (int row = 0; row < m_rows.size(); ++row)
    {
        const QString englishName =
            m_rows[row].value(
                SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
                ).trimmed();
        const QString koreanName =
            m_rows[row].value(
                SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
                ).trimmed();

        if (englishName.isEmpty() && koreanName.isEmpty())
        {
            continue;
        }

        m_studentSelector->addItem(
            studentDisplayName(row),
            row
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
        printButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalReportDialog::printCurrentReport
        );
    connect(
        closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept
        );

    updateReport();
}

void SpeakingEvalReportDialog::updateReport()
{
    if (!m_report || !m_studentSelector)
    {
        return;
    }

    const int row =
        m_studentSelector->currentData().toInt();

    if (row < 0 || row >= m_rows.size())
    {
        m_report->setReportData({});
        return;
    }

    m_report->setReportData(
        reportDataForRow(row)
        );
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
    if (!m_report || !m_studentSelector)
    {
        return;
    }

    const int row =
        m_studentSelector->currentData().toInt();

    if (row < 0 || row >= m_rows.size())
    {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);

    const QPageSize pageSize(
        QSizeF(7.5, 10.833333),
        QPageSize::Inch,
        QStringLiteral("SpeakingEvaluationA4")
        );

    printer.setPageLayout(
        QPageLayout(
            pageSize,
            QPageLayout::Portrait,
            QMarginsF()
            )
        );

    QPrintDialog printDialog(
        &printer,
        this
        );

    if (printDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QPainter painter;

    if (!painter.begin(&printer))
    {
        QMessageBox::warning(
            this,
            tr("Print Failed"),
            tr("The speaking evaluation report could not be printed.")
            );
        return;
    }

    m_report->paintReport(
        &painter,
        printer.pageRect(QPrinter::DevicePixel)
        );

    painter.end();
}

SpeakingEvalReportData SpeakingEvalReportDialog::reportDataForRow(
    int row
    ) const
{
    SpeakingEvalReportData data;

    if (row < 0 || row >= m_rows.size())
    {
        return data;
    }

    const QStringList& values =
        m_rows[row];

    data.englishName =
        values.value(
            SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
            );
    data.koreanName =
        values.value(
            SpeakingEval::toInt(SpeakingEvalColumn::KoreanName)
            );
    data.classLabel =
        classLabel(m_classInfo);
    data.nativeTeacher =
        m_classInfo.teacherEn;
    data.koreanTeacher =
        m_classInfo.teacherKr;
    data.comments =
        values.value(
            SpeakingEval::toInt(SpeakingEvalColumn::Comments)
            );
    data.useAdvancedTemplate =
        usesAdvancedTemplate(m_classInfo);
    data.date =
        QDate::currentDate().toString(
            data.useAdvancedTemplate
                ? QStringLiteral("MMM. yyyy")
                : QStringLiteral("MMMM yyyy")
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
        data.scores[index] =
            values.value(
                SpeakingEval::toInt(scoreColumns[index])
                );
    }

    return data;
}

QString SpeakingEvalReportDialog::studentDisplayName(
    int row
    ) const
{
    if (row < 0 || row >= m_rows.size())
    {
        return {};
    }

    const QString englishName =
        m_rows[row].value(
            SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)
            ).trimmed();
    const QString koreanName =
        m_rows[row].value(
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

    return tr("%1 (%2)")
        .arg(englishName, koreanName);
}
