#include "staff_directory_page.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "data/data_service.h"
#include "domain/models/gs_team_member.h"
#include "domain/models/native_english_teacher.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QAbstractItemView>
#include <QDate>
#include <QEvent>
#include <QFont>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QComboBox>
#include <QCoreApplication>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int IdRole = Qt::UserRole + 1;
constexpr int MinimumRowHeight = 42;
constexpr int MinimumHeaderHeight = 38;
constexpr int EditorVerticalMargin = 8;
constexpr int HeaderVerticalPadding = 16;

const QStringList NativeTeacherPositions{
    QStringLiteral("Co-ordinator"),
    QStringLiteral("Team Leader"),
    QStringLiteral("M3 Song's"),
    QStringLiteral("M2 Song's"),
    QStringLiteral("M1 Song's"),
    QStringLiteral("E6 Song's"),
    QStringLiteral("E5 Athena"),
    QStringLiteral("NET")
};

QString nativeTeacherPositionDisplayText(
    const QString& position
    )
{
    if (position == QStringLiteral("Co-ordinator"))
    {
        return QCoreApplication::translate(
            "StaffDirectoryPage",
            "Coordinator"
            );
    }
    if (position == QStringLiteral("Team Leader"))
    {
        return QCoreApplication::translate(
            "StaffDirectoryPage",
            "Team Leader"
            );
    }

    return position;
}

class NativeTeacherPositionDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem&,
        const QModelIndex&
        ) const override
    {
        auto* combo = new QComboBox(parent);
        for (const QString& position : NativeTeacherPositions)
        {
            combo->addItem(
                nativeTeacherPositionDisplayText(position),
                position
                );
        }
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->lineEdit()->setReadOnly(true);
        combo->lineEdit()->setAlignment(Qt::AlignCenter);
        auto* delegate = const_cast<NativeTeacherPositionDelegate*>(this);
        connect(
            combo,
            qOverload<int>(&QComboBox::activated),
            combo,
            [delegate, combo](int) {
                emit delegate->commitData(combo);
                emit delegate->closeEditor(combo);
            });
        return combo;
    }

    void setEditorData(
        QWidget* editor,
        const QModelIndex& index
        ) const override
    {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (!combo)
        {
            QStyledItemDelegate::setEditorData(editor, index);
            return;
        }

        const QString position = index.data(Qt::EditRole).toString();
        int optionIndex = combo->findData(position);
        if (optionIndex < 0 && !position.isEmpty())
        {
            combo->addItem(position, position);
            optionIndex = combo->count() - 1;
        }
        combo->setCurrentIndex(optionIndex);
    }

    void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index
        ) const override
    {
        if (const auto* combo = qobject_cast<QComboBox*>(editor))
        {
            model->setData(
                index,
                combo->currentData().toString(),
                Qt::EditRole
                );
            return;
        }

        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index
        ) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->text = nativeTeacherPositionDisplayText(
            index.data(Qt::DisplayRole).toString()
            );
        option->displayAlignment = Qt::AlignCenter;
    }
};

QTableWidgetItem* textItem(const QString& text, int id = -1)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    if (id > 0)
    {
        item->setData(IdRole, id);
    }
    return item;
}

QString cellText(const QTableWidget* table, int row, int column)
{
    const auto* item = table ? table->item(row, column) : nullptr;
    return item ? item->text().trimmed() : QString();
}

QString normalizedName(const QString& value)
{
    return value.simplified().toCaseFolded();
}
}

StaffDirectoryPage::StaffDirectoryPage(
    ApplicationServices* services,
    StaffDirectoryKind kind,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_kind(kind)
{
    buildUi();
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(750);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this]() {
        if (m_dirty && m_saveMode == SaveMode::Automatic)
        {
            saveDirectory(false);
        }
    });
}

void StaffDirectoryPage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin, UiConstants::Pages::Margin,
        UiConstants::Pages::Margin, UiConstants::Pages::Margin);
    contentLayout()->setSpacing(UiConstants::Pages::Spacing);

    auto* headerLayout = new QVBoxLayout;
    headerLayout->setContentsMargins(
        UiConstants::Pages::HeaderMargin, UiConstants::Pages::HeaderMargin,
        UiConstants::Pages::HeaderMargin, UiConstants::Pages::HeaderMargin);
    headerLayout->setSpacing(UiConstants::Pages::HeaderSpacing);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("pageTitle"));
    m_titleLabel->setFont(FontManager::getUiFont(
        UiConstants::Pages::TitleFontSize, QFont::Bold));
    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName(QStringLiteral("pageSubtitle"));
    m_subtitleLabel->setFont(FontManager::getUiFont(
        UiConstants::Pages::SubtitleFontSize));
    m_subtitleLabel->setWordWrap(true);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(UiConstants::Pages::HeaderContentSpacing);

    m_table = new QTableWidget(this);
    m_table->installEventFilter(this);
    m_table->setObjectName(
        m_kind == StaffDirectoryKind::NativeEnglishTeachers
            ? QStringLiteral("nativeEnglishTeachersTable")
            : QStringLiteral("gsTeamTable"));
    m_table->setColumnCount(
        m_kind == StaffDirectoryKind::NativeEnglishTeachers ? 6 : 5);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(
        QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed
        | QAbstractItemView::SelectedClicked);
    m_table->setSortingEnabled(false);
    m_table->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    auto* horizontalHeader = m_table->horizontalHeader();
    horizontalHeader->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader->setDefaultAlignment(Qt::AlignCenter);
    horizontalHeader->setSectionsMovable(false);
    horizontalHeader->setSectionsClickable(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        m_table->setItemDelegateForColumn(
            1, new NativeTeacherPositionDelegate(m_table));
    }
    updateTableMetrics();
    contentLayout()->addWidget(m_table, 1);

    m_addButton = new TextFitPushButton(tr("Add"), this);
    m_deleteButton = new TextFitPushButton(tr("Delete"), this);
    m_discardButton = new TextFitPushButton(tr("Discard Changes"), this);
    m_saveButton = new TextFitPushButton(tr("Save Changes"), this);
    bottomLayout()->addWidget(m_addButton);
    bottomLayout()->addWidget(m_deleteButton);
    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_discardButton);
    bottomLayout()->addWidget(m_saveButton);

    connect(m_addButton, &QPushButton::clicked, this, &StaffDirectoryPage::addRow);
    connect(m_deleteButton, &QPushButton::clicked, this, &StaffDirectoryPage::deleteSelectedRows);
    connect(m_discardButton, &QPushButton::clicked, this, &StaffDirectoryPage::discardChanges);
    connect(m_saveButton, &QPushButton::clicked, this, [this]() { saveDirectory(true); });
    connect(m_table, &QTableWidget::cellChanged, this, [this](int, int) {
        markDirty();
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &StaffDirectoryPage::updateActions);

    retranslateUi();
    updateActions();
}

bool StaffDirectoryPage::loadDirectory()
{
    auto* dataService = m_services ? m_services->dataService() : nullptr;
    if (!dataService || !dataService->isOpen())
    {
        clearDatabaseState();
        return false;
    }

    m_loading = true;
    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(0);

    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        const QList<NativeEnglishTeacher> teachers = dataService->getNativeEnglishTeachers();
        m_table->setRowCount(teachers.size());
        for (int row = 0; row < teachers.size(); ++row)
        {
            const NativeEnglishTeacher& teacher = teachers.at(row);
            m_table->setItem(row, 0, textItem(teacher.name, teacher.id));
            m_table->setItem(row, 1, textItem(teacher.position));
            m_table->setItem(row, 2, textItem(teacher.phoneNumber));
            m_table->setItem(row, 3, textItem(teacher.email));
            m_table->setItem(row, 4, textItem(teacher.birthday));
            m_table->setItem(row, 5, textItem(teacher.nationality));
        }
    }
    else
    {
        const QList<GsTeamMember> members = dataService->getGsTeamMembers();
        m_table->setRowCount(members.size());
        for (int row = 0; row < members.size(); ++row)
        {
            const GsTeamMember& member = members.at(row);
            m_table->setItem(row, 0, textItem(member.name, member.id));
            m_table->setItem(row, 1, textItem(member.koreanName));
            m_table->setItem(row, 2, textItem(member.position));
            m_table->setItem(row, 3, textItem(member.phoneNumber));
            m_table->setItem(row, 4, textItem(member.birthday));
        }
    }

    m_deletedIds.clear();
    m_dirty = false;
    m_loading = false;
    updateActions();
    return true;
}

void StaffDirectoryPage::addRow()
{
    m_table->setSortingEnabled(false);
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    for (int column = 0; column < m_table->columnCount(); ++column)
    {
        m_table->setItem(row, column, textItem(QString()));
    }
    m_table->setCurrentCell(row, 0);
    m_table->editItem(m_table->item(row, 0));
    markDirty(false);
}

void StaffDirectoryPage::deleteSelectedRows()
{
    const QModelIndexList selected = m_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        return;
    }
    if (QMessageBox::question(
            this,
            tr("Delete Directory Entries"),
            selected.size() == 1
                ? tr("Delete the selected entry?")
                : tr("Delete the selected %1 entries?")
                    .arg(selected.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    QList<int> rows;
    for (const QModelIndex& index : selected)
    {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows)
    {
        const auto* item = m_table->item(row, 0);
        const int id = item ? item->data(IdRole).toInt() : -1;
        if (id > 0) m_deletedIds.append(id);
        m_table->removeRow(row);
    }
    markDirty();
}

bool StaffDirectoryPage::validateBirthday(const QString& value) const
{
    if (value.trimmed().isEmpty()) return true;
    const QDate date = QDate::fromString(
        QStringLiteral("2000-%1").arg(value.trimmed()),
        QStringLiteral("yyyy-MM-dd"));
    return date.isValid();
}

bool StaffDirectoryPage::saveDirectory(bool showErrors)
{
    auto* dataService = m_services ? m_services->dataService() : nullptr;
    if (!dataService || !dataService->isOpen()) return false;

    Status status;
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        QList<NativeEnglishTeacher> teachers;
        QSet<QString> names;
        for (int row = 0; row < m_table->rowCount(); ++row)
        {
            NativeEnglishTeacher teacher;
            teacher.id = m_table->item(row, 0)
                ? m_table->item(row, 0)->data(IdRole).toInt() : -1;
            teacher.name = cellText(m_table, row, 0).simplified();
            teacher.position = cellText(m_table, row, 1);
            teacher.phoneNumber = cellText(m_table, row, 2);
            teacher.email = cellText(m_table, row, 3);
            teacher.birthday = cellText(m_table, row, 4);
            teacher.nationality = cellText(m_table, row, 5);
            const QString key = normalizedName(teacher.name);
            if (key.isEmpty() || names.contains(key) || !validateBirthday(teacher.birthday))
            {
                status = std::unexpected(tr("Each Native English Teacher needs a unique name and a valid MM-dd birthday."));
                break;
            }
            names.insert(key);
            teachers.append(teacher);
        }
        if (status) status = dataService->saveNativeEnglishTeacherDirectory(teachers, m_deletedIds);
    }
    else
    {
        QList<GsTeamMember> members;
        QSet<QString> englishNames;
        QSet<QString> koreanNames;
        for (int row = 0; row < m_table->rowCount(); ++row)
        {
            GsTeamMember member;
            member.id = m_table->item(row, 0)
                ? m_table->item(row, 0)->data(IdRole).toInt() : -1;
            member.name = cellText(m_table, row, 0).simplified();
            member.koreanName = cellText(m_table, row, 1).simplified();
            member.position = cellText(m_table, row, 2);
            member.phoneNumber = cellText(m_table, row, 3);
            member.birthday = cellText(m_table, row, 4);
            const QString english = normalizedName(member.name);
            const QString korean = normalizedName(member.koreanName);
            if ((english.isEmpty() && korean.isEmpty())
                || (!english.isEmpty() && englishNames.contains(english))
                || (!korean.isEmpty() && koreanNames.contains(korean))
                || !validateBirthday(member.birthday))
            {
                status = std::unexpected(tr("Each GS Team member needs a unique name or Korean name and a valid MM-dd birthday."));
                break;
            }
            if (!english.isEmpty()) englishNames.insert(english);
            if (!korean.isEmpty()) koreanNames.insert(korean);
            members.append(member);
        }
        if (status) status = dataService->saveGsTeamDirectory(members, m_deletedIds);
    }

    if (!status)
    {
        if (showErrors)
        {
            QMessageBox::warning(this, tr("Save Directory"), status.error());
        }
        return false;
    }

    loadDirectory();
    emit directorySaved();
    return true;
}

void StaffDirectoryPage::markDirty(bool scheduleAutosave)
{
    if (m_loading) return;
    m_dirty = true;
    updateActions();
    if (scheduleAutosave && m_saveMode == SaveMode::Automatic)
    {
        m_autosaveTimer->start();
    }
}

void StaffDirectoryPage::saveData()
{
    saveDirectory(true);
}

bool StaffDirectoryPage::saveChanges()
{
    return !m_dirty || saveDirectory(true);
}

bool StaffDirectoryPage::hasUnsavedChanges() const
{
    return m_dirty;
}

void StaffDirectoryPage::discardChanges()
{
    m_autosaveTimer->stop();
    loadDirectory();
}

QString StaffDirectoryPage::unsavedChangesTitle() const
{
    return tr("Unsaved Directory Changes");
}

QString StaffDirectoryPage::unsavedChangesMessage() const
{
    return tr("This staff directory has unsaved changes.");
}

void StaffDirectoryPage::setSaveMode(SaveMode mode)
{
    m_saveMode = mode;
    updateActions();
    if (mode == SaveMode::Automatic && m_dirty) m_autosaveTimer->start();
    else m_autosaveTimer->stop();
}

void StaffDirectoryPage::refresh()
{
    BasePage::refresh();
    if (isVisible() && !m_dirty) loadDirectory();
}

void StaffDirectoryPage::clearDatabaseState()
{
    m_loading = true;
    m_table->clearContents();
    m_table->setRowCount(0);
    m_deletedIds.clear();
    m_dirty = false;
    m_loading = false;
    updateActions();
}

void StaffDirectoryPage::retranslateUi()
{
    const bool native = m_kind == StaffDirectoryKind::NativeEnglishTeachers;
    m_titleLabel->setText(native ? tr("Native English Teachers") : tr("GS Team"));
    m_subtitleLabel->setText(native
        ? tr("View and maintain all Native English Teacher contact information.")
        : tr("View and maintain all GS and CS team contact information."));
    m_table->setHorizontalHeaderLabels(native
        ? QStringList{tr("Name"), tr("Position"), tr("Phone Number"), tr("Email"), tr("Birthday"), tr("Nationality")}
        : QStringList{tr("Name"), tr("Korean Name"), tr("Position"), tr("Phone Number"), tr("Birthday")});
    m_addButton->setText(tr("Add"));
    m_deleteButton->setText(tr("Delete"));
    m_discardButton->setText(tr("Discard Changes"));
    updateActions();
}

void StaffDirectoryPage::changeEvent(QEvent* event)
{
    BasePage::changeEvent(event);

    if (event
        && (event->type() == QEvent::FontChange
            || event->type() == QEvent::ApplicationFontChange
            || event->type() == QEvent::StyleChange))
    {
        updateTableMetrics();
    }
}

bool StaffDirectoryPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_table
        && event
        && (event->type() == QEvent::FontChange
            || event->type() == QEvent::StyleChange))
    {
        updateTableMetrics();
    }

    return BasePage::eventFilter(watched, event);
}

void StaffDirectoryPage::updateTableMetrics()
{
    if (!m_table) return;

    QLineEdit editorProbe;
    editorProbe.setFont(m_table->font());
    editorProbe.ensurePolished();

    const int rowHeight = std::max(
        MinimumRowHeight,
        editorProbe.sizeHint().height() + EditorVerticalMargin);
    auto* verticalHeader = m_table->verticalHeader();
    verticalHeader->setMinimumSectionSize(rowHeight);
    verticalHeader->setDefaultSectionSize(rowHeight);

    auto* horizontalHeader = m_table->horizontalHeader();
    const int headerHeight = std::max(
        MinimumHeaderHeight,
        horizontalHeader->fontMetrics().height() + HeaderVerticalPadding);
    horizontalHeader->setFixedHeight(headerHeight);
}

void StaffDirectoryPage::updateActions()
{
    if (!m_table) return;
    m_deleteButton->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty());
    const bool manual = m_saveMode != SaveMode::Automatic;
    m_discardButton->setVisible(manual);
    m_discardButton->setEnabled(manual && m_dirty);
    m_saveButton->setVisible(manual);
    m_saveButton->setEnabled(manual && m_dirty);
    m_saveButton->setText(m_dirty ? tr("Save Changes *") : tr("Save Changes"));
}
