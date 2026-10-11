#include "staff_directory_page.h"
#include "ui/shared/dialogs/user_prompt_service.h"

#include "core/application_services.h"
#include "core/fontmanager.h"
#include "next/application/gs_team_directory_read_query.h"
#include "next/application/gs_team_directory_save_use_case.h"
#include "next/application/native_english_teacher_directory_read_query.h"
#include "next/application/native_english_teacher_directory_save_use_case.h"
#include "next/platform/application_services_gs_team_directory_read_port.h"
#include "next/platform/application_services_gs_team_directory_save_port.h"
#include "next/platform/application_services_native_english_teacher_directory_read_port.h"
#include "next/platform/application_services_native_english_teacher_directory_save_port.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/on_screen_keyboard.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QAbstractItemView>
#include <QAbstractTableModel>
#include <QEvent>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

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

struct StaffDirectoryRow final
{
    int persistedId = -1;
    QStringList values;
};

class StaffDirectoryTableModel final : public QAbstractTableModel
{
public:
    explicit StaffDirectoryTableModel(
        const int columnCount,
        QObject* parent = nullptr
        )
        : QAbstractTableModel(parent)
        , m_columnCount(columnCount)
    {
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_columnCount;
    }

    QVariant data(const QModelIndex& index, const int role) const override
    {
        if (!index.isValid()
            || index.row() < 0
            || index.row() >= rowCount()
            || index.column() < 0
            || index.column() >= m_columnCount)
        {
            return {};
        }

        const StaffDirectoryRow& row = m_rows.at(
            static_cast<std::size_t>(index.row()));
        if (role == Qt::DisplayRole || role == Qt::EditRole)
        {
            return row.values.at(index.column());
        }
        if (role == Qt::TextAlignmentRole)
        {
            return static_cast<int>(Qt::AlignCenter);
        }
        if (role == IdRole && index.column() == 0 && row.persistedId > 0)
        {
            return row.persistedId;
        }
        return {};
    }

    QVariant headerData(
        const int section,
        const Qt::Orientation orientation,
        const int role
        ) const override
    {
        if (orientation == Qt::Horizontal
            && role == Qt::DisplayRole
            && section >= 0
            && section < m_headers.size())
        {
            return m_headers.at(section);
        }
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid())
        {
            return Qt::NoItemFlags;
        }
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    bool setData(
        const QModelIndex& index,
        const QVariant& value,
        const int role
        ) override
    {
        if (role != Qt::EditRole
            || !index.isValid()
            || index.row() < 0
            || index.row() >= rowCount()
            || index.column() < 0
            || index.column() >= m_columnCount)
        {
            return false;
        }

        StaffDirectoryRow& row = m_rows[index.row()];
        const QString text = value.toString();
        if (row.values.at(index.column()) == text)
        {
            return true;
        }

        row.values[index.column()] = text;
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }

    bool insertRows(
        const int row,
        const int count,
        const QModelIndex& parent = QModelIndex()
        ) override
    {
        if (parent.isValid() || row < 0 || row > rowCount() || count <= 0)
        {
            return false;
        }

        beginInsertRows(QModelIndex(), row, row + count - 1);
        for (int offset = 0; offset < count; ++offset)
        {
            StaffDirectoryRow newRow;
            newRow.values.fill(QString(), m_columnCount);
            m_rows.insert(
                m_rows.begin() + static_cast<std::ptrdiff_t>(row + offset),
                std::move(newRow));
        }
        endInsertRows();
        return true;
    }

    bool removeRows(
        const int row,
        const int count,
        const QModelIndex& parent = QModelIndex()
        ) override
    {
        if (parent.isValid()
            || row < 0
            || count <= 0
            || row + count > rowCount())
        {
            return false;
        }

        beginRemoveRows(QModelIndex(), row, row + count - 1);
        m_rows.erase(
            m_rows.begin() + static_cast<std::ptrdiff_t>(row),
            m_rows.begin() + static_cast<std::ptrdiff_t>(row + count));
        endRemoveRows();
        return true;
    }

    void setRows(QVector<StaffDirectoryRow> rows)
    {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
    }

    void setHeaders(QStringList headers)
    {
        m_headers = std::move(headers);
        if (!m_headers.isEmpty())
        {
            emit headerDataChanged(
                Qt::Horizontal,
                0,
                static_cast<int>(m_headers.size()) - 1);
        }
    }

    const StaffDirectoryRow& rowAt(const int row) const
    {
        return m_rows.at(static_cast<std::size_t>(row));
    }

private:
    int m_columnCount = 0;
    QVector<StaffDirectoryRow> m_rows;
    QStringList m_headers;
};

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

QString cellText(const QAbstractItemModel* model, int row, int column)
{
    if (!model
        || row < 0
        || row >= model->rowCount()
        || column < 0
        || column >= model->columnCount())
    {
        return {};
    }
    return model->index(row, column).data(Qt::EditRole).toString().trimmed();
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

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(UiConstants::Pages::HeaderSpacing);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch();

    m_koreanKeyboardButton = new QPushButton(this);
    m_koreanKeyboardButton->setObjectName(
        QStringLiteral("staffDirectoryKoreanKeyboardButton")
        );
    m_koreanKeyboardButton->setMinimumSize(44, 40);
    m_koreanKeyboardButton->setMaximumWidth(52);
    m_koreanKeyboardButton->setToolTip(
        tr("Open Korean / English on-screen keyboard")
        );
    m_koreanKeyboardButton->setAccessibleName(
        tr("Korean Keyboard")
        );
    m_onScreenKeyboard = new OnScreenKeyboard(this);
    m_onScreenKeyboard->setTriggerButton(m_koreanKeyboardButton);
    titleRow->addWidget(m_koreanKeyboardButton);

    headerLayout->addLayout(titleRow);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout()->addLayout(headerLayout);
    contentLayout()->addSpacing(UiConstants::Pages::HeaderContentSpacing);

    m_table = new QTableView(this);
    m_table->installEventFilter(this);
    m_table->setObjectName(
        m_kind == StaffDirectoryKind::NativeEnglishTeachers
            ? QStringLiteral("nativeEnglishTeachersTable")
            : QStringLiteral("gsTeamTable"));
    const bool native = m_kind == StaffDirectoryKind::NativeEnglishTeachers;
    auto* model = new StaffDirectoryTableModel(native ? 6 : 5, this);
    m_model = model;
    m_table->setModel(model);
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
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() {
        markDirty();
    });
    connect(
        m_table->selectionModel(),
        &QItemSelectionModel::selectionChanged,
        this,
        [this](const QItemSelection&, const QItemSelection&) {
            updateActions();
        });
    connect(
        m_koreanKeyboardButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (m_onScreenKeyboard)
            {
                m_onScreenKeyboard->showForFocusScope(this);
            }
        }
        );

    retranslateUi();
    updateActions();
}

bool StaffDirectoryPage::loadDirectory()
{
    auto* model = static_cast<StaffDirectoryTableModel*>(m_model);
    m_loading = true;
    model->setRows({});

    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        const ClassMngr::Next::Platform::
            ApplicationServicesNativeEnglishTeacherDirectoryReadPort port(
                m_services);
        const ClassMngr::Next::Application::
            NativeEnglishTeacherDirectoryReadQuery query(port);
        const ClassMngr::Next::Application::
            NativeEnglishTeacherDirectoryReadResult loadedTeachers =
                query.execute();
        if (!loadedTeachers)
        {
            m_loading = false;
            updateActions();
            if (loadedTeachers.error().code
                == ClassMngr::Next::Domain::ErrorCode::NotFound)
            {
                clearDatabaseState();
                return false;
            }

            const std::string& message = loadedTeachers.error().message;
            DialogServices::showWarning(
                this,
                tr("Load Directory"),
                QString::fromUtf8(
                    message.data(),
                    static_cast<qsizetype>(message.size()))
                );
            return false;
        }

        const auto& teachers = loadedTeachers.value();
        QVector<StaffDirectoryRow> rows;
        rows.reserve(static_cast<qsizetype>(teachers.size()));
        for (const auto& teacher : teachers)
        {
            StaffDirectoryRow row;
            row.persistedId = teacher.id.value();
            row.values = {
                QString::fromStdU16String(teacher.name),
                QString::fromStdU16String(teacher.position),
                QString::fromStdU16String(teacher.phoneNumber),
                QString::fromStdU16String(teacher.email),
                QString::fromStdU16String(teacher.birthday),
                QString::fromStdU16String(teacher.nationality)
            };
            rows.push_back(std::move(row));
        }
        model->setRows(std::move(rows));
    }
    else
    {
        const ClassMngr::Next::Platform::
            ApplicationServicesGsTeamDirectoryReadPort port(m_services);
        const ClassMngr::Next::Application::GsTeamDirectoryReadQuery query(port);
        const ClassMngr::Next::Application::GsTeamDirectoryReadResult
            loadedMembers = query.execute();
        if (!loadedMembers)
        {
            m_loading = false;
            updateActions();
            if (loadedMembers.error().code
                == ClassMngr::Next::Domain::ErrorCode::NotFound)
            {
                clearDatabaseState();
                return false;
            }

            const std::string& message = loadedMembers.error().message;
            DialogServices::showWarning(
                this,
                tr("Load Directory"),
                QString::fromUtf8(
                    message.data(),
                    static_cast<qsizetype>(message.size()))
                );
            return false;
        }

        const auto& members = loadedMembers.value();
        QVector<StaffDirectoryRow> rows;
        rows.reserve(static_cast<qsizetype>(members.size()));
        for (const auto& member : members)
        {
            StaffDirectoryRow row;
            row.persistedId = member.id.value();
            row.values = {
                QString::fromStdU16String(member.name),
                QString::fromStdU16String(member.koreanName),
                QString::fromStdU16String(member.position),
                QString::fromStdU16String(member.phoneNumber),
                QString::fromStdU16String(member.birthday)
            };
            rows.push_back(std::move(row));
        }
        model->setRows(std::move(rows));
    }

    m_deletedGsTeamMemberIds.clear();
    m_deletedNativeEnglishTeacherIds.clear();
    m_dirty = false;
    m_loading = false;
    updateActions();
    markRefreshed();
    return true;
}

void StaffDirectoryPage::addRow()
{
    auto* model = static_cast<StaffDirectoryTableModel*>(m_model);
    const int row = model->rowCount();
    model->insertRows(row, 1);
    const QModelIndex firstCell = model->index(row, 0);
    m_table->setCurrentIndex(firstCell);
    m_table->edit(firstCell);
    markDirty(false);
}

void StaffDirectoryPage::deleteSelectedRows()
{
    auto* model = static_cast<StaffDirectoryTableModel*>(m_model);
    const QModelIndexList selected = m_table->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        return;
    }
    if (DialogServices::confirm(
            this,
            tr("Delete Directory Entries"),
            selected.size() == 1
                ? tr("Delete the selected entry?")
                : tr("Delete the selected %1 entries?")
                    .arg(selected.size()),
            tr("Delete"),
            tr("Cancel"),
            true
            ) != PromptChoice::Destructive)
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
        const int id = model->rowAt(row).persistedId;
        if (id > 0)
        {
            if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
            {
                m_deletedNativeEnglishTeacherIds.append(
                    ClassMngr::Next::Domain::NativeEnglishTeacherId(id));
            }
            else
            {
                m_deletedGsTeamMemberIds.append(
                    ClassMngr::Next::Domain::GsTeamMemberId(id));
            }
        }
        model->removeRows(row, 1);
    }
    markDirty();
}

bool StaffDirectoryPage::saveDirectory(bool showErrors)
{
    const auto* model = static_cast<const StaffDirectoryTableModel*>(m_model);
    if (m_kind == StaffDirectoryKind::NativeEnglishTeachers)
    {
        const ClassMngr::Next::Platform::
            ApplicationServicesNativeEnglishTeacherDirectorySavePort port(
                m_services);
        if (!port.hasActiveSession())
        {
            return false;
        }

        ClassMngr::Next::Application::
            NativeEnglishTeacherDirectorySaveRequest request;
        request.rows.reserve(static_cast<std::size_t>(model->rowCount()));
        for (int row = 0; row < model->rowCount(); ++row)
        {
            const int id = model->rowAt(row).persistedId;
            const QString name = cellText(model, row, 0).simplified();
            const QString birthday = cellText(model, row, 4);

            ClassMngr::Next::Application::NativeEnglishTeacherDirectorySaveRow
                teacher;
            if (id > 0)
            {
                teacher.id = ClassMngr::Next::Domain::NativeEnglishTeacherId(id);
            }
            teacher.name = name.toStdU16String();
            teacher.position = cellText(model, row, 1).toStdU16String();
            teacher.phoneNumber = cellText(model, row, 2).toStdU16String();
            teacher.email = cellText(model, row, 3).toStdU16String();
            teacher.birthday = birthday.toStdU16String();
            teacher.nationality = cellText(model, row, 5).toStdU16String();
            teacher.normalizedNameKey =
                normalizedName(name).toStdU16String();
            request.rows.push_back(std::move(teacher));
        }

        request.deletedIds.reserve(
            static_cast<std::size_t>(m_deletedNativeEnglishTeacherIds.size()));
        for (const auto id : m_deletedNativeEnglishTeacherIds)
        {
            request.deletedIds.push_back(id);
        }

        const auto outcome =
            ClassMngr::Next::Application::
                NativeEnglishTeacherDirectorySaveUseCase::execute(
                    request,
                    port
                    );
        if (!outcome.validation.isValid())
        {
            if (showErrors)
            {
                DialogServices::showWarning(
                    this,
                    tr("Save Directory"),
                    tr("Each Native English Teacher needs a unique name and a valid MM-dd birthday.")
                    );
            }
            return false;
        }
        if (!outcome.saved)
        {
            if (outcome.error
                && outcome.error->code
                    == ClassMngr::Next::Domain::ErrorCode::NotFound)
            {
                return false;
            }
            if (showErrors)
            {
                const std::string message = outcome.error
                    ? outcome.error->message
                    : std::string{};
                DialogServices::showWarning(
                    this,
                    tr("Save Directory"),
                    message.empty()
                        ? tr("The Native English Teacher directory could not be saved.")
                        : QString::fromUtf8(
                            message.data(),
                            static_cast<qsizetype>(message.size()))
                    );
            }
            return false;
        }

        loadDirectory();
        emit directorySaved();
        return true;
    }

    const ClassMngr::Next::Platform::
        ApplicationServicesGsTeamDirectorySavePort port(m_services);
    if (!port.hasActiveSession())
    {
        return false;
    }

    ClassMngr::Next::Application::GsTeamDirectorySaveRequest request;
    request.rows.reserve(static_cast<std::size_t>(model->rowCount()));
    for (int row = 0; row < model->rowCount(); ++row)
    {
        const int id = model->rowAt(row).persistedId;
        const QString name = cellText(model, row, 0).simplified();
        const QString koreanName = cellText(model, row, 1).simplified();
        const QString birthday = cellText(model, row, 4);

        ClassMngr::Next::Application::GsTeamDirectorySaveRow member;
        if (id > 0)
        {
            member.id = ClassMngr::Next::Domain::GsTeamMemberId(id);
        }
        member.name = name.toStdU16String();
        member.koreanName = koreanName.toStdU16String();
        member.position = cellText(model, row, 2).toStdU16String();
        member.phoneNumber = cellText(model, row, 3).toStdU16String();
        member.birthday = birthday.toStdU16String();
        member.normalizedEnglishNameKey =
            normalizedName(name).toStdU16String();
        member.normalizedKoreanNameKey =
            normalizedName(koreanName).toStdU16String();
        request.rows.push_back(std::move(member));
    }

    request.deletedIds.reserve(
        static_cast<std::size_t>(m_deletedGsTeamMemberIds.size()));
    for (const auto id : m_deletedGsTeamMemberIds)
    {
        request.deletedIds.push_back(id);
    }

    const auto outcome =
        ClassMngr::Next::Application::GsTeamDirectorySaveUseCase::execute(
            request,
            port
            );
    if (!outcome.validation.isValid())
    {
        if (showErrors)
        {
            DialogServices::showWarning(
                this,
                tr("Save Directory"),
                tr("Each GS Team member needs a unique name or Korean name and a valid MM-dd birthday.")
                );
        }
        return false;
    }
    if (!outcome.saved)
    {
        if (outcome.error
            && outcome.error->code
                == ClassMngr::Next::Domain::ErrorCode::NotFound)
        {
            return false;
        }
        if (showErrors)
        {
            const std::string message = outcome.error
                ? outcome.error->message
                : std::string{};
            DialogServices::showWarning(
                this,
                tr("Save Directory"),
                message.empty()
                    ? tr("The GS Team directory could not be saved.")
                    : QString::fromUtf8(
                        message.data(),
                        static_cast<qsizetype>(message.size()))
                );
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

    if (!m_dirty)
    {
        loadDirectory();
    }
}

void StaffDirectoryPage::clearDatabaseState()
{
    m_loading = true;
    static_cast<StaffDirectoryTableModel*>(m_model)->setRows({});
    m_deletedGsTeamMemberIds.clear();
    m_deletedNativeEnglishTeacherIds.clear();
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
    static_cast<StaffDirectoryTableModel*>(m_model)->setHeaders(native
        ? QStringList{tr("Name"), tr("Position"), tr("Phone Number"), tr("Email"), tr("Birthday"), tr("Nationality")}
        : QStringList{tr("Name"), tr("Korean Name"), tr("Position"), tr("Phone Number"), tr("Birthday")});
    m_addButton->setText(tr("Add"));
    m_deleteButton->setText(tr("Delete"));
    m_discardButton->setText(tr("Discard Changes"));
    if (m_koreanKeyboardButton)
    {
        m_koreanKeyboardButton->setToolTip(
            tr("Open Korean / English on-screen keyboard")
            );
        m_koreanKeyboardButton->setAccessibleName(
            tr("Korean Keyboard")
            );
    }
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
    m_deleteButton->setEnabled(
        !m_table->selectionModel()->selectedRows().isEmpty());
    const bool manual = m_saveMode != SaveMode::Automatic;
    m_discardButton->setVisible(manual);
    m_discardButton->setEnabled(manual && m_dirty);
    m_saveButton->setVisible(manual);
    m_saveButton->setEnabled(manual && m_dirty);
    m_saveButton->setText(m_dirty ? tr("Save Changes *") : tr("Save Changes"));
}
