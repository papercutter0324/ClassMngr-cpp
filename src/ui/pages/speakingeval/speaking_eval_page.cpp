#include "speaking_eval_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "models/roster.h"
#include "models/speaking_evaluation.h"
#include "services/dataservice.h"
#include "ui/pages/speakingeval/speaking_eval_delegate.h"
#include "ui/pages/speakingeval/speaking_eval_model.h"

#include <QAbstractButton>
#include <QDesktopServices>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QSizePolicy>
#include <QTableView>
#include <QTimer>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

inline constexpr int AutosaveDelayMs = 750;

int findColumn(
    const QStringList& columns,
    const QString& name
    )
{
    for (int column = 0; column < columns.size(); ++column)
    {
        if (columns[column].compare(name, Qt::CaseInsensitive) == 0)
        {
            return column;
        }
    }

    return -1;
}

void clearLayout(
    QLayout* layout
    )
{
    if (!layout)
    {
        return;
    }

    while (layout->count() > 0)
    {
        QLayoutItem* item =
            layout->takeAt(0);

        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }
}

class SpeakingEvalHeaderView : public QHeaderView
{
public:
    explicit SpeakingEvalHeaderView(
        Qt::Orientation orientation,
        QWidget* parent = nullptr
        )
        : QHeaderView(orientation, parent)
    {
        setDefaultAlignment(Qt::AlignCenter);
        setSectionResizeMode(QHeaderView::Fixed);
        setHighlightSections(false);
        setSectionsClickable(false);
        setFixedHeight(42);
    }

protected:
    void paintSection(
        QPainter* painter,
        const QRect& rect,
        int logicalIndex
        ) const override
    {
        if (!painter || !rect.isValid())
        {
            return;
        }

        painter->save();

        const auto column =
            SpeakingEval::columnFromInt(
                logicalIndex
                );

        const QColor baseColor =
            SpeakingEval::columnColor(column);

        const QColor headerColor =
            baseColor.darker(115);

        painter->fillRect(
            rect,
            headerColor
            );

        painter->setFont(
            FontManager::getUiFont(
                14,
                QFont::DemiBold
                )
            );

        painter->setPen(
            SpeakingEval::contrastTextColor(
                headerColor
                )
            );

        painter->drawText(
            rect.adjusted(4, 0, -4, 0),
            Qt::AlignCenter,
            model()
                ? model()
                      ->headerData(
                          logicalIndex,
                          Qt::Horizontal,
                          Qt::DisplayRole
                          )
                      .toString()
                : QString()
            );

        if (SpeakingEval::hasThickBorderAfter(column))
        {
            QPen pen(Qt::black);
            pen.setWidth(2);
            pen.setCosmetic(true);

            painter->setPen(pen);
            painter->drawLine(
                rect.right() - 1,
                rect.top(),
                rect.right() - 1,
                rect.bottom()
                );
        }

        painter->restore();
    }

    void paintEvent(
        QPaintEvent* event
        ) override
    {
        QHeaderView::paintEvent(event);

        const int rightEdge =
            contentRightEdge();

        if (rightEdge < 0)
        {
            return;
        }

        QPainter painter(viewport());

        if (rightEdge + 1 < viewport()->width())
        {
            painter.fillRect(
                QRect(
                    rightEdge + 1,
                    0,
                    viewport()->width() - rightEdge - 1,
                    viewport()->height()
                    ),
                trailingBackgroundBrush()
                );
        }

        QPen pen(Qt::black);
        pen.setWidth(2);
        pen.setCosmetic(true);

        painter.setPen(pen);
        painter.drawLine(
            0,
            height() - 1,
            rightEdge,
            height() - 1
            );
    }

private:
    int contentRightEdge() const
    {
        if (count() <= 0)
        {
            return -1;
        }

        const int lastSection =
            count() - 1;

        return sectionViewportPosition(lastSection)
            + sectionSize(lastSection)
            - 1;
    }

    QBrush trailingBackgroundBrush() const
    {
        if (
            const auto* table =
                qobject_cast<const QTableView*>(parentWidget())
            )
        {
            if (table->viewport())
            {
                return table
                    ->viewport()
                    ->palette()
                    .brush(QPalette::Base);
            }
        }

        return palette().brush(QPalette::Base);
    }
};

} // namespace

SpeakingEvalPage::SpeakingEvalPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
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
        &SpeakingEvalPage::autosave
        );
}

void SpeakingEvalPage::loadEvaluation(
    const Classroom& classroom,
    const QString& evaluationName
    )
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    m_loadingEvaluation = true;

    m_classroom =
        classroom;

    m_evaluationName =
        evaluationName;

    SpeakingEvalRows rows;

    if (
        m_services
        && m_services->dataService()
        && m_classroom.id > 0
        )
    {
        rows =
            m_services
                ->dataService()
                ->loadSpeakingEval(
                    m_classroom.id,
                    m_evaluationName
                    );
    }

    if (rows.isEmpty())
    {
        rows =
            SpeakingEval::emptyRows();
    }

    m_model->loadData(rows);
    setupTable();
    updateHeaderText();
    updateActions();

    m_loadingEvaluation = false;
}

void SpeakingEvalPage::saveData()
{
    saveEvaluationInternal(true, true);
}

bool SpeakingEvalPage::saveChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    if (!hasUnsavedChanges())
    {
        return true;
    }

    return saveEvaluationInternal(true, false);
}

bool SpeakingEvalPage::hasUnsavedChanges() const
{
    return m_model && m_model->isDirty();
}

void SpeakingEvalPage::discardChanges()
{
    if (m_autosaveTimer)
    {
        m_autosaveTimer->stop();
    }

    loadEvaluation(
        m_classroom,
        m_evaluationName
        );
}

QString SpeakingEvalPage::unsavedChangesTitle() const
{
    return tr("Unsaved Speaking Evaluation Changes");
}

QString SpeakingEvalPage::unsavedChangesMessage() const
{
    return tr("This speaking evaluation has unsaved changes.");
}

void SpeakingEvalPage::setSaveMode(
    SaveMode mode
    )
{
    if (m_saveMode == mode)
    {
        return;
    }

    m_saveMode = mode;

    if (!m_autosaveTimer)
    {
        return;
    }

    if (m_saveMode == SaveMode::Automatic && hasUnsavedChanges())
    {
        m_autosaveTimer->start();
    }
    else
    {
        m_autosaveTimer->stop();
    }
}

bool SpeakingEvalPage::saveEvaluationInternal(
    bool showValidationMessages,
    bool showSuccessMessage
    )
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        || m_evaluationName.trimmed().isEmpty()
        )
    {
        return false;
    }

    m_model->revalidateAll();
    m_table->viewport()->update();

    if (m_model->hasErrors())
    {
        if (showValidationMessages)
        {
            QMessageBox message(this);
            message.setIcon(QMessageBox::Warning);
            message.setWindowTitle(
                tr("Validation Errors")
                );
            message.setText(
                tr("Fix validation errors before saving.")
                );
            message.setDetailedText(
                m_model->errorList().join(QLatin1Char('\n'))
                );
            message.exec();
        }

        return false;
    }

    const bool saved =
        m_services
            ->dataService()
            ->saveSpeakingEval(
                m_classroom.id,
                m_evaluationName,
                m_model->rows(),
                m_model->changedCells()
                );

    if (!saved)
    {
        if (showValidationMessages)
        {
            QMessageBox::warning(
                this,
                tr("Save Failed"),
                tr("The speaking evaluation could not be saved.")
                );
        }

        return false;
    }

    m_model->markSaved();
    updateActions();

    if (showSuccessMessage)
    {
        QMessageBox::information(
            this,
            tr("Saved"),
            tr("Speaking evaluation saved.")
            );
    }

    return true;
}

void SpeakingEvalPage::refresh()
{
    BasePage::refresh();

    if (m_table)
    {
        m_table->viewport()->update();
    }

    if (m_table && m_table->horizontalHeader())
    {
        m_table->horizontalHeader()->viewport()->update();
    }
}

void SpeakingEvalPage::importNames()
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        )
    {
        return;
    }

    const Roster roster =
        m_services
            ->dataService()
            ->loadRoster(
                m_classroom.id
                );

    if (roster.rows.isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Import Names"),
            tr("No roster data found.")
            );
        return;
    }

    if (
        findColumn(
            roster.columns,
            QStringLiteral("English")
            ) < 0
        || findColumn(
            roster.columns,
            QStringLiteral("Korean")
            ) < 0
        )
    {
        QMessageBox::warning(
            this,
            tr("Import Names"),
            tr("Roster must contain 'English' and 'Korean' columns.")
            );
        return;
    }

    const QList<SpeakingEvalCellEdit> changes =
        nameImportChanges(
            roster.columns,
            roster.rows
            );

    if (changes.isEmpty())
    {
        QMessageBox::information(
            this,
            tr("Import Names"),
            tr("Names are already up to date.")
            );
        return;
    }

    m_importingNames = true;

    m_table->applyChanges(
        changes,
        tr("Import Names")
        );

    m_importingNames = false;
    m_table->viewport()->update();

    updateActions();

    QMessageBox::information(
        this,
        tr("Import Names"),
        tr("Roster names imported successfully.")
        );
}

void SpeakingEvalPage::autosave()
{
    if (!hasUnsavedChanges())
    {
        return;
    }

    saveEvaluationInternal(false, false);
}

void SpeakingEvalPage::openKoreanKeyboard()
{
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("https://www.branah.com/korean"))
        );
}

void SpeakingEvalPage::updateActions()
{
    if (!m_saveButton || !m_model)
    {
        return;
    }

    m_saveButton->setEnabled(
        m_classroom.id > 0
        && !m_model->changedCells().isEmpty()
        );
}

void SpeakingEvalPage::buildUi()
{
    contentLayout()->setContentsMargins(
        24,
        18,
        24,
        0
        );

    contentLayout()->setSpacing(12);

    auto* headerLayout =
        new QVBoxLayout;

    headerLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    headerLayout->setSpacing(2);

    m_titleLabel =
        new QLabel(
            tr("Speaking Evaluation"),
            this
            );

    m_titleLabel->setObjectName("pageTitle");
    m_titleLabel->setFont(
        FontManager::getUiFont(
            20,
            QFont::DemiBold
            )
        );

    m_subtitleLabel =
        new QLabel(
            tr("No class selected"),
            this
            );

    m_subtitleLabel->setObjectName("pageSubtitle");
    m_subtitleLabel->setFont(
        FontManager::getUiFont(11)
        );

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);

    m_undoStack =
        new QUndoStack(this);

    m_undoStack->setUndoLimit(100);

    m_model =
        new SpeakingEvalModel(this);

    m_table =
        new SpeakingEvalTableView(this);

    m_table->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    m_table->setModel(m_model);
    m_table->setUndoStack(m_undoStack);

    auto* header =
        new SpeakingEvalHeaderView(
            Qt::Horizontal,
            m_table
            );

    m_table->setHorizontalHeader(header);

    m_delegate =
        new SpeakingEvalDelegate(m_table);

    m_table->setItemDelegate(m_delegate);

    contentLayout()->addWidget(m_table);

    setupTable();

    clearLayout(
        bottomLayout()
        );

    bottomLayout()->addStretch();

    m_importNamesButton =
        new QPushButton(
            tr("Import Names"),
            this
            );

    m_importNamesButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    bottomLayout()->addWidget(m_importNamesButton);
    bottomLayout()->addSpacing(20);

    const QList<QString> disabledLabels{
        tr("Create Reports"),
        tr("Print Reports"),
        tr("Create Certificates"),
        tr("Print Certificates"),
        tr("Auto-Select Winners")
    };

    for (int index = 0; index < disabledLabels.size(); ++index)
    {
        auto* button =
            new QPushButton(
                disabledLabels[index],
                this
                );

        button->setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Preferred
            );

        button->setEnabled(false);
        button->setToolTip(
            tr("This action is not available yet.")
            );

        bottomLayout()->addWidget(button);

        if (index == 1 || index == 3)
        {
            bottomLayout()->addSpacing(20);
        }
    }

    bottomLayout()->addSpacing(20);

    m_koreanKeyboardButton =
        new QPushButton(
            tr("Korean Keyboard"),
            this
            );

    m_koreanKeyboardButton->setToolTip(
        tr("Open Korean typing website")
        );

    m_koreanKeyboardButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    bottomLayout()->addWidget(m_koreanKeyboardButton);

    m_saveButton =
        new QPushButton(
            tr("Save Changes"),
            this
            );

    m_saveButton->setObjectName("primaryButton");
    m_saveButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );

    bottomLayout()->addWidget(m_saveButton);
    bottomLayout()->addStretch();

    connect(
        m_importNamesButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalPage::importNames
        );

    connect(
        m_koreanKeyboardButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalPage::openKoreanKeyboard
        );

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &SpeakingEvalPage::saveData
        );

    connect(
        m_model,
        &SpeakingEvalModel::dirtyChanged,
        this,
        &SpeakingEvalPage::updateActions
        );

    connect(
        m_model,
        &SpeakingEvalModel::dataModified,
        this,
        [this]()
        {
            updateActions();
            scheduleAutosave();
        }
        );

    connect(
        m_model,
        &QAbstractItemModel::dataChanged,
        this,
        [this](
            const QModelIndex& topLeft,
            const QModelIndex& bottomRight,
            const QList<int>&
            )
        {
            handleNameCellChanged(
                topLeft,
                bottomRight
                );
        }
        );

    updateActions();
}

void SpeakingEvalPage::scheduleAutosave()
{
    if (
        m_loadingEvaluation
        || m_saveMode != SaveMode::Automatic
        || !m_autosaveTimer
        || m_classroom.id <= 0
        || m_evaluationName.trimmed().isEmpty()
        )
    {
        return;
    }

    m_autosaveTimer->start();
}

void SpeakingEvalPage::setupTable()
{
    if (!m_table)
    {
        return;
    }

    m_table->setShowGrid(false);

    for (int column = 0; column < SpeakingEval::ColumnCount; ++column)
    {
        m_table->setColumnWidth(
            column,
            SpeakingEval::columnWidth(
                SpeakingEval::columnFromInt(column)
                )
            );
    }

    for (int row = 0; row < SpeakingEval::RowCount; ++row)
    {
        m_table->setRowHeight(
            row,
            SpeakingEval::RowHeight
            );
    }
}

void SpeakingEvalPage::updateHeaderText()
{
    if (!m_titleLabel || !m_subtitleLabel)
    {
        return;
    }

    m_titleLabel->setText(
        m_evaluationName.trimmed().isEmpty()
            ? tr("Speaking Evaluation")
            : tr("%1 Speaking Evaluation").arg(m_evaluationName)
        );

    m_subtitleLabel->setText(
        m_classroom.id > 0
            ? m_classroom.name.trimmed().isEmpty()
                  ? tr("Class %1").arg(m_classroom.id)
                  : m_classroom.name
            : tr("No class selected")
        );
}

QList<SpeakingEvalCellEdit> SpeakingEvalPage::nameImportChanges(
    const QStringList& rosterColumns,
    const QList<QStringList>& rosterRows
    ) const
{
    QList<SpeakingEvalCellEdit> changes;

    const int rosterEnglishColumn =
        findColumn(
            rosterColumns,
            QStringLiteral("English")
            );

    const int rosterKoreanColumn =
        findColumn(
            rosterColumns,
            QStringLiteral("Korean")
            );

    if (rosterEnglishColumn < 0 || rosterKoreanColumn < 0)
    {
        return changes;
    }

    int targetRow = 0;

    const auto appendChange =
        [this, &changes](int row, SpeakingEvalColumn column, const QString& value)
        {
            const QModelIndex index =
                m_model->index(
                    row,
                    SpeakingEval::toInt(column)
                    );

            if (
                !index.isValid()
                || !(m_model->flags(index) & Qt::ItemIsEditable)
                )
            {
                return;
            }

            const QString oldValue =
                index.data(Qt::EditRole).toString();

            if (oldValue == value)
            {
                return;
            }

            changes.append(
                {
                    row,
                    SpeakingEval::toInt(column),
                    oldValue,
                    value
                }
                );
        };

    for (const QStringList& rosterRow : rosterRows)
    {
        const QString englishName =
            rosterEnglishColumn < rosterRow.size()
                ? rosterRow[rosterEnglishColumn].trimmed()
                : QString();

        const QString koreanName =
            rosterKoreanColumn < rosterRow.size()
                ? rosterRow[rosterKoreanColumn].trimmed()
                : QString();

        if (englishName.isEmpty() && koreanName.isEmpty())
        {
            continue;
        }

        if (targetRow >= SpeakingEval::RowCount)
        {
            break;
        }

        appendChange(
            targetRow,
            SpeakingEvalColumn::EnglishName,
            englishName
            );

        appendChange(
            targetRow,
            SpeakingEvalColumn::KoreanName,
            koreanName
            );

        ++targetRow;
    }

    return changes;
}

void SpeakingEvalPage::handleNameCellChanged(
    const QModelIndex& topLeft,
    const QModelIndex& bottomRight
    )
{
    if (
        m_loadingEvaluation
        || m_importingNames
        || m_resolvingDuplicateName
        || !m_model
        || !m_table
        || !topLeft.isValid()
        || !bottomRight.isValid()
        )
    {
        return;
    }

    m_table->viewport()->update();

    const int englishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);

    const int koreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        for (int column = topLeft.column(); column <= bottomRight.column(); ++column)
        {
            if (column == englishColumn || column == koreanColumn)
            {
                resolveDuplicateName(
                    row,
                    column
                    );
                return;
            }
        }
    }
}

void SpeakingEvalPage::resolveDuplicateName(
    int row,
    int editedColumn
    )
{
    if (!m_model || !m_table)
    {
        return;
    }

    const int englishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);

    const int koreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    if (editedColumn != englishColumn && editedColumn != koreanColumn)
    {
        return;
    }

    const QList<int> duplicateRows =
        m_model->duplicateNameRows(row);

    if (duplicateRows.isEmpty())
    {
        return;
    }

    QStringList duplicateRowLabels;

    for (int duplicateRow : duplicateRows)
    {
        duplicateRowLabels.append(
            QString::number(duplicateRow + 1)
            );
    }

    const QString suggestedName =
        m_model->suggestedKoreanNameWithSuffix(row);

    const QList<QStringList> rosterCandidates =
        unmatchedRosterNamePairs();

    QMessageBox dialog(this);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(
        tr("Duplicate Student Name")
        );
    dialog.setText(
        tr("This English/Korean name combination already exists.")
        );
    dialog.setInformativeText(
        tr("Duplicate row(s): %1").arg(
            duplicateRowLabels.join(QStringLiteral(", "))
            )
        );

    QPushButton* suffixButton =
        dialog.addButton(
            suggestedName.isEmpty()
                ? tr("No Suffix Available")
                : tr("Use %1").arg(suggestedName),
            QMessageBox::AcceptRole
            );

    suffixButton->setEnabled(
        !suggestedName.isEmpty()
        );

    QPushButton* clearButton =
        dialog.addButton(
            tr("Clear Edited Cell"),
            QMessageBox::DestructiveRole
            );

    QPushButton* locateButton =
        dialog.addButton(
            tr("Locate Duplicate"),
            QMessageBox::ActionRole
            );

    QPushButton* matchButton = nullptr;

    if (!rosterCandidates.isEmpty())
    {
        matchButton =
            dialog.addButton(
                tr("Use Roster Match..."),
                QMessageBox::ActionRole
                );
    }

    QPushButton* keepButton =
        dialog.addButton(
            tr("Keep As-Is"),
            QMessageBox::RejectRole
            );

    if (!suggestedName.isEmpty())
    {
        dialog.setDefaultButton(suffixButton);
    }
    else
    {
        dialog.setDefaultButton(clearButton);
    }

    dialog.exec();

    QAbstractButton* clickedButton =
        dialog.clickedButton();

    const auto applySingleChange =
        [this, row](int column, const QString& newValue, const QString& description)
        {
            const QModelIndex index =
                m_model->index(
                    row,
                    column
                    );

            if (!index.isValid())
            {
                return;
            }

            const QString oldValue =
                index.data(Qt::EditRole).toString();

            if (oldValue == newValue)
            {
                return;
            }

            m_resolvingDuplicateName = true;
            m_table->applyChanges(
                {
                    {
                        row,
                        column,
                        oldValue,
                        newValue
                    }
                },
                description
                );
            m_resolvingDuplicateName = false;
        };

    if (clickedButton == suffixButton && !suggestedName.isEmpty())
    {
        applySingleChange(
            koreanColumn,
            suggestedName,
            tr("Resolve Duplicate Name")
            );

        selectEvaluationCell(
            row,
            SpeakingEvalColumn::KoreanName
            );
    }
    else if (clickedButton == clearButton)
    {
        applySingleChange(
            editedColumn,
            QString(),
            tr("Clear Duplicate Name")
            );

        selectEvaluationCell(
            row,
            SpeakingEval::columnFromInt(editedColumn)
            );
    }
    else if (clickedButton == locateButton)
    {
        selectEvaluationCell(
            duplicateRows.first(),
            SpeakingEvalColumn::EnglishName
            );
    }
    else if (matchButton && clickedButton == matchButton)
    {
        QStringList options;

        for (const QStringList& candidate : rosterCandidates)
        {
            options.append(
                tr("%1 / %2")
                    .arg(candidate.value(0))
                    .arg(candidate.value(1))
                );
        }

        bool accepted = false;

        const QString selected =
            QInputDialog::getItem(
                this,
                tr("Match Roster Student"),
                tr("Student:"),
                options,
                0,
                false,
                &accepted
                );

        const int selectedIndex =
            options.indexOf(selected);

        if (accepted && selectedIndex >= 0)
        {
            const QStringList candidate =
                rosterCandidates[selectedIndex];

            QList<SpeakingEvalCellEdit> changes;

            const QModelIndex englishIndex =
                m_model->index(
                    row,
                    englishColumn
                    );

            const QModelIndex koreanIndex =
                m_model->index(
                    row,
                    koreanColumn
                    );

            const QString oldEnglish =
                englishIndex.data(Qt::EditRole).toString();

            const QString oldKorean =
                koreanIndex.data(Qt::EditRole).toString();

            if (oldEnglish != candidate.value(0))
            {
                changes.append(
                    {
                        row,
                        englishColumn,
                        oldEnglish,
                        candidate.value(0)
                    }
                    );
            }

            if (oldKorean != candidate.value(1))
            {
                changes.append(
                    {
                        row,
                        koreanColumn,
                        oldKorean,
                        candidate.value(1)
                    }
                    );
            }

            m_resolvingDuplicateName = true;
            m_table->applyChanges(
                changes,
                tr("Match Roster Name")
                );
            m_resolvingDuplicateName = false;

            selectEvaluationCell(
                row,
                SpeakingEvalColumn::EnglishName
                );
        }
    }
    else
    {
        Q_UNUSED(keepButton);
    }

    m_table->viewport()->update();
    updateActions();
}

QList<QStringList> SpeakingEvalPage::unmatchedRosterNamePairs() const
{
    QList<QStringList> candidates;

    if (
        !m_services
        || !m_services->dataService()
        || !m_model
        || m_classroom.id <= 0
        )
    {
        return candidates;
    }

    const Roster roster =
        m_services
            ->dataService()
            ->loadRoster(
                m_classroom.id
                );

    const int englishColumn =
        findColumn(
            roster.columns,
            QStringLiteral("English")
            );

    const int koreanColumn =
        findColumn(
            roster.columns,
            QStringLiteral("Korean")
            );

    if (englishColumn < 0 || koreanColumn < 0)
    {
        return candidates;
    }

    QSet<QString> seen;

    for (const QStringList& row : roster.rows)
    {
        const QString englishName =
            englishColumn < row.size()
                ? row[englishColumn].trimmed()
                : QString();

        const QString koreanName =
            koreanColumn < row.size()
                ? row[koreanColumn].trimmed()
                : QString();

        if (
            englishName.isEmpty()
            || koreanName.isEmpty()
            || m_model->containsNamePair(
                englishName,
                koreanName
                )
            )
        {
            continue;
        }

        const QString key =
            englishName
            + QChar(0x001F)
            + koreanName;

        if (seen.contains(key))
        {
            continue;
        }

        seen.insert(key);
        candidates.append(
            {
                englishName,
                koreanName
            }
            );
    }

    return candidates;
}

void SpeakingEvalPage::selectEvaluationCell(
    int row,
    SpeakingEvalColumn column
    )
{
    if (!m_model || !m_table)
    {
        return;
    }

    const QModelIndex index =
        m_model->index(
            row,
            SpeakingEval::toInt(column)
            );

    if (!index.isValid())
    {
        return;
    }

    m_table->setCurrentIndex(index);
    m_table->scrollTo(index);
}
