#include "speaking_eval_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "models/roster.h"
#include "models/speaking_evaluation.h"
#include "services/dataservice.h"
#include "ui/pages/speakingeval/speaking_eval_delegate.h"
#include "ui/pages/speakingeval/speaking_eval_model.h"

#include <QDesktopServices>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QTableView>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

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
}

void SpeakingEvalPage::loadEvaluation(
    const Classroom& classroom,
    const QString& evaluationName
    )
{
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
}

void SpeakingEvalPage::saveData()
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        || m_evaluationName.trimmed().isEmpty()
        )
    {
        return;
    }

    m_model->revalidateAll();
    m_table->viewport()->update();

    if (m_model->hasErrors())
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
        return;
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
        QMessageBox::warning(
            this,
            tr("Save Failed"),
            tr("The speaking evaluation could not be saved.")
            );
        return;
    }

    m_model->markSaved();
    updateActions();

    QMessageBox::information(
        this,
        tr("Saved"),
        tr("Speaking evaluation saved.")
        );
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

    m_table->applyChanges(
        changes,
        tr("Import Names")
        );

    updateActions();

    QMessageBox::information(
        this,
        tr("Import Names"),
        tr("Roster names imported successfully.")
        );
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
        &SpeakingEvalPage::updateActions
        );

    updateActions();
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
