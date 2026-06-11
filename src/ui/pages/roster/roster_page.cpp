#include "roster_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "services/dataservice.h"
#include "ui/pages/roster/roster_column_layout_controller.h"
#include "ui/pages/roster/roster_constants.h"
#include "ui/pages/roster/roster_header_view.h"
#include "ui/pages/roster/roster_item_delegate.h"
#include "ui/pages/roster/roster_model.h"
#include "ui/pages/roster/roster_table_view.h"

#include <QHeaderView>
#include <QHash>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

RosterPage::RosterPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
{
    buildUi();
}

void RosterPage::loadClass(
    const Classroom& classroom
    )
{
    m_classroom = classroom;
    updateHeaderText();

    m_loadingRoster = true;

    Roster roster;

    if (m_services && m_services->dataService() && m_classroom.id > 0)
    {
        roster =
            m_services
                ->dataService()
                ->loadRoster(m_classroom.id);
    }

    m_model->setRoster(roster);

    QVector<int> normalizedWidths;

    for (int column = 0; column < m_model->columnCount(); ++column)
    {
        const QString columnName =
            m_model->columnName(column);

        int sourceColumn = -1;

        for (int index = 0; index < roster.columns.size(); ++index)
        {
            const bool exactMatch =
                roster.columns[index].compare(columnName, Qt::CaseInsensitive) == 0;

            const bool legacyFallMatch =
                columnName.compare(QStringLiteral("Fall"), Qt::CaseInsensitive) == 0
                && roster.columns[index].compare(QStringLiteral("Autumn"), Qt::CaseInsensitive) == 0;

            if (exactMatch || legacyFallMatch)
            {
                sourceColumn = index;
                break;
            }
        }

        normalizedWidths.append(
            sourceColumn >= 0 && sourceColumn < roster.columnWidths.size()
                ? roster.columnWidths[sourceColumn]
                : 0
            );
    }

    m_layoutController->attach(
        m_table,
        m_model
        );

    m_layoutController->applyWidths(
        normalizedWidths
        );

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        m_table->setRowHeight(
            row,
            RosterUi::RowHeight
            );
    }

    m_model->clearDirty();
    m_widthsDirty = false;
    m_loadingRoster = false;
    updateActions();
}

void RosterPage::saveData()
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        )
    {
        return;
    }

    Roster roster =
        m_model->toRoster();

    roster.columnWidths =
        m_layoutController->currentWidths();

    m_services
        ->dataService()
        ->saveRoster(
            m_classroom.id,
            roster
            );

    m_model->clearDirty();
    m_widthsDirty = false;
    updateActions();
}

void RosterPage::addColumn()
{
    bool accepted = false;

    const QString name =
        QInputDialog::getText(
            this,
            tr("Add Column"),
            tr("Column name:"),
            QLineEdit::Normal,
            QString(),
            &accepted
            ).trimmed();

    if (!accepted)
    {
        return;
    }

    QString reason;

    if (!m_model->canAddColumn(name, &reason))
    {
        QMessageBox::warning(
            this,
            tr("Cannot Add Column"),
            reason
            );

        return;
    }

    if (!m_model->insertCustomColumn(name))
    {
        return;
    }

    const int column =
        m_model->columnCount() - 1;

    m_layoutController->applyResizeModes();

    m_table->setColumnWidth(
        column,
        RosterUi::defaultColumnWidth(
            m_model->columnName(column)
            )
        );

    const QModelIndex firstCell =
        m_model->index(
            0,
            column
            );

    m_table->setCurrentIndex(firstCell);
    m_table->edit(firstCell);

    updateActions();
}

void RosterPage::removeColumn()
{
    const QModelIndex current =
        m_table->currentIndex();

    QString reason;

    if (!m_model->canRemoveColumn(current.column(), &reason))
    {
        QMessageBox::warning(
            this,
            tr("Cannot Remove Column"),
            reason
            );

        return;
    }

    const QString columnName =
        m_model->columnName(
            current.column()
            );

    const QMessageBox::StandardButton response =
        QMessageBox::question(
            this,
            tr("Remove Column"),
            tr("Remove the \"%1\" column?").arg(columnName)
            );

    if (response != QMessageBox::Yes)
    {
        return;
    }

    m_model->removeRosterColumn(
        current.column()
        );

    m_layoutController->applyResizeModes();
    updateActions();
}

void RosterPage::importScores()
{
    if (
        !m_services
        || !m_services->dataService()
        || m_classroom.id <= 0
        )
    {
        return;
    }

    const auto findModelColumn =
        [this](const QString& name)
        {
            for (int column = 0; column < m_model->columnCount(); ++column)
            {
                if (m_model->columnName(column).compare(name, Qt::CaseInsensitive) == 0)
                {
                    return column;
                }
            }

            return -1;
        };

    const int englishColumn =
        findModelColumn(QStringLiteral("English"));

    const int koreanColumn =
        findModelColumn(QStringLiteral("Korean"));

    if (englishColumn < 0 || koreanColumn < 0)
    {
        QMessageBox::warning(
            this,
            tr("Import Scores"),
            tr("Roster must contain 'English' and 'Korean' columns.")
            );
        return;
    }

    const auto keyFor =
        [](const QString& englishName, const QString& koreanName)
        {
            return englishName.trimmed()
                + QChar(0x001F)
                + koreanName.trimmed();
        };

    const QStringList evaluationColumns{
        QStringLiteral("Winter"),
        QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };

    int changeCount = 0;

    for (const QString& evaluationName : evaluationColumns)
    {
        const int scoreColumn =
            findModelColumn(evaluationName);

        if (scoreColumn < 0)
        {
            continue;
        }

        const QList<SpeakingEvalScore> scores =
            m_services
                ->dataService()
                ->buildRosterScoreImport(
                    m_classroom.id,
                    evaluationName
                    );

        if (scores.isEmpty())
        {
            continue;
        }

        QHash<QString, QString> lookup;

        for (const SpeakingEvalScore& score : scores)
        {
            lookup.insert(
                keyFor(
                    score.englishName,
                    score.koreanName
                    ),
                score.finalGrade
                );
        }

        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const QString englishName =
                m_model
                    ->index(
                        row,
                        englishColumn
                        )
                    .data(Qt::EditRole)
                    .toString()
                    .trimmed();

            const QString koreanName =
                m_model
                    ->index(
                        row,
                        koreanColumn
                        )
                    .data(Qt::EditRole)
                    .toString()
                    .trimmed();

            if (englishName.isEmpty() || koreanName.isEmpty())
            {
                continue;
            }

            const QString finalGrade =
                lookup.value(
                    keyFor(
                        englishName,
                        koreanName
                        )
                    );

            if (finalGrade.isEmpty())
            {
                continue;
            }

            const QModelIndex index =
                m_model->index(
                    row,
                    scoreColumn
                    );

            if (
                !index.isValid()
                || !(m_model->flags(index) & Qt::ItemIsEditable)
                || index.data(Qt::EditRole).toString() == finalGrade
                )
            {
                continue;
            }

            if (
                m_model->setData(
                    index,
                    finalGrade,
                    Qt::EditRole
                    )
                )
            {
                ++changeCount;
            }
        }
    }

    if (changeCount == 0)
    {
        QMessageBox::information(
            this,
            tr("Import Scores"),
            tr("Scores are already up to date.")
            );
        return;
    }

    updateActions();

    QMessageBox::information(
        this,
        tr("Import Scores"),
        tr("Scores imported successfully.")
        );
}

void RosterPage::updateActions()
{
    if (!m_saveButton || !m_removeColumnButton)
    {
        return;
    }

    m_saveButton->setEnabled(
        hasUnsavedChanges()
        && m_classroom.id > 0
        );

    QString reason;

    m_removeColumnButton->setEnabled(
        m_model
        && m_model->canRemoveColumn(
            m_table->currentIndex().column(),
            &reason
            )
        );
}

void RosterPage::buildUi()
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
            tr("Class Roster"),
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

    m_model =
        new RosterModel(this);

    m_layoutController =
        new RosterColumnLayoutController(this);

    m_table =
        new RosterTableView(this);

    m_header =
        new RosterHeaderView(
            Qt::Horizontal,
            m_table
            );

    m_header->setLayoutController(
        m_layoutController
        );

    m_table->setHorizontalHeader(m_header);
    m_table->setModel(m_model);
    m_table->setLayoutController(m_layoutController);

    m_layoutController->attach(
        m_table,
        m_model
        );

    m_delegate =
        new RosterItemDelegate(
            m_layoutController,
            this
            );

    m_table->setItemDelegate(m_delegate);
    m_table->verticalHeader()->setDefaultSectionSize(RosterUi::RowHeight);

    m_layoutController->applyWidths({});

    contentLayout()->addWidget(m_table);

    m_importButton =
        new QPushButton(
            tr("Import Scores"),
            this
            );

    m_importButton->setEnabled(true);
    m_importButton->setToolTip(
        tr("Import final grades from speaking evaluations.")
        );

    m_addColumnButton =
        new QPushButton(
            tr("Add Column"),
            this
            );

    m_removeColumnButton =
        new QPushButton(
            tr("Remove Column"),
            this
            );

    m_saveButton =
        new QPushButton(
            tr("Save Changes"),
            this
            );

    m_saveButton->setObjectName("primaryButton");

    bottomLayout()->addWidget(m_importButton);
    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_addColumnButton);
    bottomLayout()->addWidget(m_removeColumnButton);
    bottomLayout()->addWidget(m_saveButton);

    connect(
        m_addColumnButton,
        &QPushButton::clicked,
        this,
        &RosterPage::addColumn
        );

    connect(
        m_removeColumnButton,
        &QPushButton::clicked,
        this,
        &RosterPage::removeColumn
        );

    connect(
        m_saveButton,
        &QPushButton::clicked,
        this,
        &RosterPage::saveData
        );

    connect(
        m_importButton,
        &QPushButton::clicked,
        this,
        &RosterPage::importScores
        );

    connect(
        m_model,
        &RosterModel::dirtyChanged,
        this,
        &RosterPage::updateActions
        );

    connect(
        m_table->selectionModel(),
        &QItemSelectionModel::currentChanged,
        this,
        &RosterPage::updateActions
        );

    connect(
        m_table->horizontalHeader(),
        &QHeaderView::sectionResized,
        this,
        [this](int, int, int)
        {
            if (m_loadingRoster)
            {
                return;
            }

            m_widthsDirty = true;
            updateActions();
        }
        );

    updateActions();
}

void RosterPage::updateHeaderText()
{
    m_titleLabel->setText(
        tr("Class Roster")
        );

    const QString className =
        m_classroom.name.trimmed().isEmpty()
            ? tr("Class %1").arg(m_classroom.id)
            : m_classroom.name.trimmed();

    m_subtitleLabel->setText(className);
}

bool RosterPage::hasUnsavedChanges() const
{
    return m_widthsDirty
        || (m_model && m_model->isDirty());
}
