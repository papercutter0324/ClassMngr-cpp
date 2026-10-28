#include "next/ui/class_transfer_workflow_page.h"

#include "data/repositories/class_transfer_legacy_package_adapter.h"
#include "features/classes/services/class_transfer_json_codec.h"
#include "next/application/class_transfer_apply_request.h"
#include "next/application/class_transfer_keys.h"
#include "next/application/class_transfer_package_class_index.h"
#include "next/application/class_transfer_projection.h"

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

[[nodiscard]] QString transferText(const char* source)
{
    return QCoreApplication::translate("ClassTransferWorkflowPage", source);
}

[[nodiscard]] QString utf8Text(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] QString displayName(const std::u16string& value)
{
    return QString::fromStdU16String(value);
}

[[nodiscard]] std::string normalizedUtf8(const std::u16string& value)
{
    return displayName(value).simplified().toCaseFolded().toStdString();
}

[[nodiscard]] QString matchCountText(const std::size_t count)
{
    return transferText("%1 possible match(es)").arg(
        static_cast<qulonglong>(count));
}

} // namespace

class ClassTransferWorkflowPage::ClassSelectionModel final
    : public QAbstractListModel
{
public:
    struct Row final
    {
        Domain::ClassId id;
        QString name;
        bool checked = false;
    };

    explicit ClassSelectionModel(QObject* parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }

    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        const int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0
            || static_cast<std::size_t>(index.row()) >= m_rows.size())
        {
            return {};
        }
        const Row& row = m_rows[static_cast<std::size_t>(index.row())];
        if (role == Qt::DisplayRole)
        {
            return row.name.isEmpty()
                ? transferText("Class %1").arg(QString::fromStdString(row.id.value()))
                : row.name;
        }
        if (role == Qt::CheckStateRole)
        {
            return row.checked ? Qt::Checked : Qt::Unchecked;
        }
        if (role == Qt::ToolTipRole)
        {
            return QString::fromStdString(row.id.value());
        }
        return {};
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return index.isValid()
            ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable
            : Qt::NoItemFlags;
    }

    [[nodiscard]] bool setData(
        const QModelIndex& index,
        const QVariant& value,
        const int role = Qt::EditRole) override
    {
        if (!index.isValid() || role != Qt::CheckStateRole || index.row() < 0
            || static_cast<std::size_t>(index.row()) >= m_rows.size())
        {
            return false;
        }
        Row& row = m_rows[static_cast<std::size_t>(index.row())];
        const bool checked = value.toInt() == Qt::Checked;
        if (row.checked == checked)
        {
            return true;
        }
        row.checked = checked;
        Q_EMIT dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }

    void setSnapshot(const Application::ClassesListSnapshot& snapshot)
    {
        beginResetModel();
        m_rows.clear();
        m_rows.reserve(snapshot.classes.size());
        for (const Application::ClassesListEntry& entry : snapshot.classes)
        {
            m_rows.push_back({entry.classId, displayName(entry.className), false});
        }
        endResetModel();
    }

    void setAllChecked(const bool checked)
    {
        if (m_rows.empty())
        {
            return;
        }
        for (Row& row : m_rows)
        {
            row.checked = checked;
        }
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0),
            {Qt::CheckStateRole});
    }

    void clear()
    {
        beginResetModel();
        m_rows.clear();
        endResetModel();
    }

    [[nodiscard]] std::vector<Domain::ClassId> checkedIds() const
    {
        std::vector<Domain::ClassId> result;
        for (const Row& row : m_rows)
        {
            if (row.checked)
            {
                result.push_back(row.id);
            }
        }
        return result;
    }

    [[nodiscard]] int checkedCount() const noexcept
    {
        return static_cast<int>(std::count_if(
            m_rows.cbegin(), m_rows.cend(), [](const Row& row) {
                return row.checked;
            }));
    }

private:
    std::vector<Row> m_rows;
};

class ClassTransferWorkflowPage::ReviewModel final
    : public QAbstractTableModel
{
public:
    enum class Kind
    {
        Classes,
        Teachers
    };

    struct Option final
    {
        QString label;
        Application::ClassTransferReviewClassAction classAction =
            Application::ClassTransferReviewClassAction::Invalid;
        Application::ClassTransferReviewTeacherAction teacherAction =
            Application::ClassTransferReviewTeacherAction::Invalid;
        std::optional<Domain::ClassId> classTarget;
        std::optional<Domain::TeacherId> teacherTarget;
    };

    struct Row final
    {
        int packageIndex = -1;
        QString key;
        QString name;
        std::vector<Domain::ClassId> matchingClasses;
        std::vector<Domain::TeacherId> matchingTeachers;
        int selectedOption = 0;
    };

    explicit ReviewModel(const Kind kind, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_kind(kind)
    {
    }

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
    }

    [[nodiscard]] int columnCount(
        const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 3;
    }

    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        const int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole || index.row() < 0
            || static_cast<std::size_t>(index.row()) >= m_rows.size())
        {
            return {};
        }
        const Row& row = m_rows[static_cast<std::size_t>(index.row())];
        switch (index.column())
        {
        case 0: return row.name;
        case 1:
            return matchCountText(m_kind == Kind::Classes
                ? row.matchingClasses.size() : row.matchingTeachers.size());
        case 2:
        {
            const std::vector<Option> choices = optionsFor(index.row());
            return row.selectedOption >= 0
                    && static_cast<std::size_t>(row.selectedOption) < choices.size()
                ? choices[static_cast<std::size_t>(row.selectedOption)].label
                : transferText("Choose a resolution…");
        }
        default: return {};
        }
    }

    [[nodiscard]] QVariant headerData(
        const int section,
        const Qt::Orientation orientation,
        const int role = Qt::DisplayRole) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        {
            return {};
        }
        switch (section)
        {
        case 0: return m_kind == Kind::Classes
                ? transferText("Package class") : transferText("Package teacher");
        case 1: return transferText("Matches");
        case 2: return transferText("Resolution");
        default: return {};
        }
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                               : Qt::NoItemFlags;
    }

    void setClasses(
        const std::vector<Application::ClassExportClass>& packageClasses,
        const std::vector<Application::ClassTransferReviewClassCandidate>& candidates
        )
    {
        beginResetModel();
        m_rows.clear();
        m_rows.reserve(packageClasses.size());
        for (std::size_t packageIndex = 0;
             packageIndex < packageClasses.size(); ++packageIndex)
        {
            const auto found = std::find_if(candidates.cbegin(), candidates.cend(),
                [packageIndex](const auto& candidate) {
                    return candidate.packageClassIndex.rawValue()
                        == static_cast<int>(packageIndex);
                });
            Row row;
            row.packageIndex = static_cast<int>(packageIndex);
            row.key = QString::fromStdU16String(
                packageClasses[packageIndex].key.value());
            row.name = displayName(packageClasses[packageIndex].name);
            if (row.name.trimmed().isEmpty())
            {
                row.name = row.key;
            }
            if (found != candidates.cend())
            {
                row.matchingClasses = found->matchingClassIds;
            }
            m_rows.push_back(std::move(row));
        }
        endResetModel();
    }

    void setTeachers(
        const std::vector<Application::ClassExportTeacher>& packageTeachers,
        const std::vector<Application::ClassTransferReviewTeacherCandidate>& candidates
        )
    {
        beginResetModel();
        m_rows.clear();
        m_rows.reserve(packageTeachers.size());
        for (const Application::ClassExportTeacher& teacher : packageTeachers)
        {
            const QString key = QString::fromStdU16String(teacher.key.value());
            const auto found = std::find_if(candidates.cbegin(), candidates.cend(),
                [&teacher](const auto& candidate) {
                    return candidate.teacherKey.value()
                        == QString::fromStdU16String(teacher.key.value()).toStdString();
                });
            Row row;
            row.key = key;
            row.name = displayName(teacher.teacher.preferredName);
            if (row.name.trimmed().isEmpty())
            {
                row.name = displayName(teacher.teacher.teacherEn);
            }
            if (row.name.trimmed().isEmpty())
            {
                row.name = displayName(teacher.teacher.teacherKr);
            }
            if (row.name.trimmed().isEmpty())
            {
                row.name = key;
            }
            if (found != candidates.cend())
            {
                row.matchingTeachers = found->matchingTeacherIds;
            }
            m_rows.push_back(std::move(row));
        }
        endResetModel();
    }

    [[nodiscard]] std::vector<Option> optionsFor(const int rowIndex) const
    {
        if (rowIndex < 0 || static_cast<std::size_t>(rowIndex) >= m_rows.size())
        {
            return {};
        }
        const Row& row = m_rows[static_cast<std::size_t>(rowIndex)];
        std::vector<Option> options;
        if (m_kind == Kind::Classes)
        {
            options.push_back({
                row.matchingClasses.empty()
                    ? transferText("Create new class")
                    : transferText("Create another class"),
                Application::ClassTransferReviewClassAction::Create});
            for (const Domain::ClassId& id : row.matchingClasses)
            {
                options.push_back({
                    transferText("Replace class %1").arg(
                        QString::fromStdString(id.value())),
                    Application::ClassTransferReviewClassAction::Replace,
                    Application::ClassTransferReviewTeacherAction::Invalid,
                    id});
            }
            options.push_back({transferText("Skip"),
                Application::ClassTransferReviewClassAction::Skip});
            return options;
        }

        if (row.matchingTeachers.size() > 1)
        {
            options.push_back({transferText("Choose a resolution…"),
                Application::ClassTransferReviewClassAction::Invalid,
                Application::ClassTransferReviewTeacherAction::Unselected});
        }
        if (row.matchingTeachers.empty() || row.matchingTeachers.size() > 1)
        {
            options.push_back({transferText("Create new teacher"),
                Application::ClassTransferReviewClassAction::Invalid,
                Application::ClassTransferReviewTeacherAction::Create});
        }
        for (const Domain::TeacherId& id : row.matchingTeachers)
        {
            const QString destination = QString::fromStdString(id.value());
            options.push_back({transferText("Keep local teacher %1").arg(destination),
                Application::ClassTransferReviewClassAction::Invalid,
                Application::ClassTransferReviewTeacherAction::KeepExisting,
                std::nullopt, id});
            options.push_back({transferText("Replace local teacher %1").arg(destination),
                Application::ClassTransferReviewClassAction::Invalid,
                Application::ClassTransferReviewTeacherAction::ReplaceExisting,
                std::nullopt, id});
        }
        return options;
    }

    [[nodiscard]] int selectedOption(const int row) const
    {
        if (row < 0 || static_cast<std::size_t>(row) >= m_rows.size())
        {
            return -1;
        }
        return m_rows[static_cast<std::size_t>(row)].selectedOption;
    }

    void setSelectedOption(const int row, const int option)
    {
        if (row < 0 || static_cast<std::size_t>(row) >= m_rows.size())
        {
            return;
        }
        Row& target = m_rows[static_cast<std::size_t>(row)];
        const auto choices = optionsFor(row);
        if (option < 0 || static_cast<std::size_t>(option) >= choices.size())
        {
            return;
        }
        target.selectedOption = option;
        Q_EMIT dataChanged(index(row, 2), index(row, 2), {Qt::DisplayRole});
    }

    [[nodiscard]] bool isClasses() const noexcept
    {
        return m_kind == Kind::Classes;
    }

    [[nodiscard]] int matchCountTotal() const noexcept
    {
        int result = 0;
        for (const Row& row : m_rows)
        {
            result += static_cast<int>(m_kind == Kind::Classes
                ? row.matchingClasses.size() : row.matchingTeachers.size());
        }
        return result;
    }

    [[nodiscard]] Application::ClassTransferApplyRequest applyChoices() const
    {
        Application::ClassTransferApplyRequest result;
        for (const Row& row : m_rows)
        {
            const auto choices = optionsFor(static_cast<int>(&row - m_rows.data()));
            if (row.selectedOption < 0
                || static_cast<std::size_t>(row.selectedOption) >= choices.size())
            {
                continue;
            }
            const Option& choice = choices[static_cast<std::size_t>(row.selectedOption)];
            if (m_kind == Kind::Classes)
            {
                result.classes.push_back({
                    Application::ClassTransferPackageClassIndex{row.packageIndex},
                    choice.classAction,
                    choice.classTarget});
            }
            else
            {
                result.teachers.push_back({
                    Application::ClassTransferTeacherKey(row.key.toStdString()),
                    choice.teacherAction,
                    choice.teacherTarget});
            }
        }
        return result;
    }

    void clear()
    {
        beginResetModel();
        m_rows.clear();
        endResetModel();
    }

private:
    Kind m_kind;
    std::vector<Row> m_rows;
};

ClassTransferWorkflowPage::ClassTransferWorkflowPage(
    const PageContext& context,
    ClassTransferWorkflowPageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
    , m_classSelectionModel(new ClassSelectionModel(this))
    , m_classReviewModel(new ReviewModel(ReviewModel::Kind::Classes, this))
    , m_teacherReviewModel(new ReviewModel(ReviewModel::Kind::Teachers, this))
{
    buildUi();
    retranslateUi();
}

QListView* ClassTransferWorkflowPage::exportClassList() const noexcept
{
    return m_exportClassList;
}

QTabWidget* ClassTransferWorkflowPage::reviewTabs() const noexcept
{
    return m_reviewTabs;
}

QTableView* ClassTransferWorkflowPage::classReviewTable() const noexcept
{
    return m_classReviewTable;
}

QTableView* ClassTransferWorkflowPage::teacherReviewTable() const noexcept
{
    return m_teacherReviewTable;
}

QComboBox* ClassTransferWorkflowPage::resolutionChoice() const noexcept
{
    return m_resolutionChoice;
}

QPushButton* ClassTransferWorkflowPage::exportButton() const noexcept
{
    return m_exportButton;
}

QPushButton* ClassTransferWorkflowPage::importButton() const noexcept
{
    return m_importButton;
}

QPushButton* ClassTransferWorkflowPage::refreshPreviewButton() const noexcept
{
    return m_refreshPreviewButton;
}

QPushButton* ClassTransferWorkflowPage::applyButton() const noexcept
{
    return m_applyButton;
}

QLabel* ClassTransferWorkflowPage::statusLabel() const noexcept
{
    return m_statusLabel;
}

int ClassTransferWorkflowPage::exportedClassChoiceCount() const noexcept
{
    return m_classSelectionModel->checkedCount();
}

bool ClassTransferWorkflowPage::hasRetainedPackage() const noexcept
{
    return m_package.has_value();
}

QString ClassTransferWorkflowPage::reviewedWorkspacePath() const
{
    return m_reviewWorkspacePath.value_or(QString{});
}

void ClassTransferWorkflowPage::buildUi()
{
    auto* const layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(10);

    auto* const title = new QLabel(this);
    title->setObjectName(QStringLiteral("classTransferTitle"));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* const subtitle = new QLabel(this);
    subtitle->setObjectName(QStringLiteral("classTransferSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto* const buttons = new QHBoxLayout;
    m_exportButton = new QPushButton(this);
    m_exportButton->setObjectName(QStringLiteral("classTransferExport"));
    m_importButton = new QPushButton(this);
    m_importButton->setObjectName(QStringLiteral("classTransferImport"));
    m_refreshPreviewButton = new QPushButton(this);
    m_refreshPreviewButton->setObjectName(QStringLiteral("classTransferRefreshPreview"));
    m_applyButton = new QPushButton(this);
    m_applyButton->setObjectName(QStringLiteral("classTransferApply"));
    m_cancelReviewButton = new QPushButton(this);
    m_cancelReviewButton->setObjectName(QStringLiteral("classTransferCancelReview"));
    buttons->addWidget(m_exportButton);
    buttons->addWidget(m_importButton);
    buttons->addWidget(m_refreshPreviewButton);
    buttons->addWidget(m_applyButton);
    buttons->addWidget(m_cancelReviewButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    auto* const splitter = new QSplitter(Qt::Horizontal, this);
    m_exportClassList = new QListView(splitter);
    m_exportClassList->setObjectName(QStringLiteral("classTransferExportClassList"));
    m_exportClassList->setModel(m_classSelectionModel);
    m_exportClassList->setMinimumWidth(260);

    auto* const reviewPanel = new QWidget(splitter);
    auto* const reviewLayout = new QVBoxLayout(reviewPanel);
    reviewLayout->setContentsMargins(0, 0, 0, 0);
    m_reviewSummaryLabel = new QLabel(reviewPanel);
    m_reviewSummaryLabel->setObjectName(QStringLiteral("classTransferReviewSummary"));
    m_reviewSummaryLabel->setWordWrap(true);
    reviewLayout->addWidget(m_reviewSummaryLabel);

    m_reviewTabs = new QTabWidget(reviewPanel);
    m_reviewTabs->setObjectName(QStringLiteral("classTransferReviewTabs"));
    m_classReviewTable = new QTableView(m_reviewTabs);
    m_classReviewTable->setObjectName(QStringLiteral("classTransferClassReviewTable"));
    m_classReviewTable->setModel(m_classReviewModel);
    m_classReviewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_classReviewTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_classReviewTable->setAlternatingRowColors(true);
    m_classReviewTable->horizontalHeader()->setStretchLastSection(true);
    m_classReviewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_teacherReviewTable = new QTableView(m_reviewTabs);
    m_teacherReviewTable->setObjectName(QStringLiteral("classTransferTeacherReviewTable"));
    m_teacherReviewTable->setModel(m_teacherReviewModel);
    m_teacherReviewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_teacherReviewTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_teacherReviewTable->setAlternatingRowColors(true);
    m_teacherReviewTable->horizontalHeader()->setStretchLastSection(true);
    m_teacherReviewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_reviewTabs->addTab(m_classReviewTable, QString{});
    m_reviewTabs->addTab(m_teacherReviewTable, QString{});
    reviewLayout->addWidget(m_reviewTabs, 1);

    m_resolutionChoice = new QComboBox(reviewPanel);
    m_resolutionChoice->setObjectName(QStringLiteral("classTransferResolutionChoice"));
    reviewLayout->addWidget(m_resolutionChoice);
    splitter->addWidget(m_exportClassList);
    splitter->addWidget(reviewPanel);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("classTransferStatus"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_exportButton, &QPushButton::clicked,
        this, &ClassTransferWorkflowPage::chooseExportPath);
    connect(m_importButton, &QPushButton::clicked,
        this, &ClassTransferWorkflowPage::chooseImportPath);
    connect(m_refreshPreviewButton, &QPushButton::clicked,
        this, &ClassTransferWorkflowPage::refreshPreview);
    connect(m_applyButton, &QPushButton::clicked,
        this, [this] { static_cast<void>(applyImport()); });
    connect(m_cancelReviewButton, &QPushButton::clicked,
        this, &ClassTransferWorkflowPage::cancelImportReview);
    connect(m_resolutionChoice,
        qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](const int option) {
            ReviewModel* const model = m_reviewTabs->currentIndex() == 0
                ? m_classReviewModel : m_teacherReviewModel;
            QTableView* const view = m_reviewTabs->currentIndex() == 0
                ? m_classReviewTable : m_teacherReviewTable;
            const int row = view->currentIndex().row();
            model->setSelectedOption(row, option);
            refreshApplyState();
        });
    connect(m_reviewTabs, &QTabWidget::currentChanged,
        this, [this] { refreshReviewChoices(); });
    connect(m_classReviewTable->selectionModel(),
        &QItemSelectionModel::currentChanged, this,
        [this] { refreshReviewChoices(); });
    connect(m_teacherReviewTable->selectionModel(),
        &QItemSelectionModel::currentChanged, this,
        [this] { refreshReviewChoices(); });
}

void ClassTransferWorkflowPage::retranslateUi()
{
    if (auto* const title = findChild<QLabel*>(QStringLiteral("classTransferTitle")))
    {
        title->setText(transferText("Class Transfer"));
    }
    if (auto* const subtitle = findChild<QLabel*>(QStringLiteral("classTransferSubtitle")))
    {
        subtitle->setText(transferText(
            "Export selected classes to a JSON package or review a package before importing."));
    }
    m_exportButton->setText(transferText("Export Selected Classes"));
    m_importButton->setText(transferText("Import JSON Package"));
    m_refreshPreviewButton->setText(transferText("Refresh Matches"));
    m_applyButton->setText(transferText("Apply Import"));
    m_cancelReviewButton->setText(transferText("Cancel Review"));
    m_reviewTabs->setTabText(0, transferText("Classes"));
    m_reviewTabs->setTabText(1, transferText("Teachers"));
    refreshReviewChoices();
}

std::optional<QString> ClassTransferWorkflowPage::activePath() const
{
    return m_actions.activeWorkspacePath
        ? m_actions.activeWorkspacePath() : std::nullopt;
}

PageOperationResult<void> ClassTransferWorkflowPage::onActivating()
{
    const std::optional<QString> path = activePath();
    if (!path)
    {
        clearForClosedWorkspace();
        setStatus(transferText("Open a workspace to use Class Transfer."), false);
        return {};
    }
    if (m_loadedWorkspacePath != path)
    {
        clearForWorkspaceChange();
        m_loadedWorkspacePath = path;
        if (!refreshClasses())
        {
            return {};
        }
    }
    else if (m_reviewWorkspacePath && m_reviewWorkspacePath != path)
    {
        clearReview();
        setStatus(transferText("The workspace changed. Load the package again to review it."), true);
    }
    return {};
}

void ClassTransferWorkflowPage::onSuspending() noexcept
{
}

bool ClassTransferWorkflowPage::refreshClasses()
{
    const auto path = activePath();
    if (!path || !m_actions.readClasses)
    {
        m_classSelectionModel->clear();
        m_loadedWorkspacePath.reset();
        return false;
    }
    const auto loaded = m_actions.readClasses(*path);
    if (!loaded)
    {
        m_classSelectionModel->clear();
        m_loadedWorkspacePath.reset();
        setStatus(utf8Text(loaded.error().message), true);
        return false;
    }
    m_classSelectionModel->setSnapshot(loaded.value());
    m_loadedWorkspacePath = path;
    setStatus(transferText("Loaded %1 classes for transfer.").arg(
        static_cast<qulonglong>(loaded.value().classes.size())), false);
    return true;
}

void ClassTransferWorkflowPage::chooseExportPath()
{
    if (exportedClassChoiceCount() == 0)
    {
        setStatus(transferText("Select at least one class to export."), true);
        return;
    }
    if (!m_actions.chooseExportFile)
    {
        setStatus(transferText("The export file picker is unavailable."), true);
        return;
    }
    const auto selection = m_actions.chooseExportFile();
    if (!selection)
    {
        setStatus(utf8Text(selection.error().message), true);
        return;
    }
    if (!selection.value())
    {
        setStatus(transferText("Export cancelled."), false);
        return;
    }
    static_cast<void>(exportSelectedTo(*selection.value()));
}

void ClassTransferWorkflowPage::chooseImportPath()
{
    if (!m_actions.chooseImportFile)
    {
        setStatus(transferText("The import file picker is unavailable."), true);
        return;
    }
    const auto selection = m_actions.chooseImportFile();
    if (!selection)
    {
        setStatus(utf8Text(selection.error().message), true);
        return;
    }
    if (!selection.value())
    {
        setStatus(transferText("Import cancelled."), false);
        return;
    }
    static_cast<void>(loadImportFile(*selection.value()));
}

bool ClassTransferWorkflowPage::exportSelectedTo(const QString& outputPath)
{
    const auto path = activePath();
    if (!path || !m_actions.exportClasses)
    {
        setStatus(transferText("The active workspace is unavailable."), true);
        return false;
    }
    if (m_loadedWorkspacePath != path)
    {
        clearForWorkspaceChange();
        m_loadedWorkspacePath = path;
        static_cast<void>(refreshClasses());
        setStatus(transferText(
            "The workspace changed. Select classes again before exporting."), true);
        return false;
    }
    const std::vector<Domain::ClassId> selected = m_classSelectionModel->checkedIds();
    if (selected.empty())
    {
        setStatus(transferText("Select at least one class to export."), true);
        return false;
    }
    if (outputPath.trimmed().isEmpty())
    {
        setStatus(transferText("Select an export path."), true);
        return false;
    }
    QString normalizedPath = outputPath;
    if (!normalizedPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
    {
        normalizedPath += QStringLiteral(".json");
    }
    Application::ClassTransferExportRequest request{selected};
    const auto package = m_actions.exportClasses(*path, request);
    if (!package)
    {
        setStatus(utf8Text(package.error().message), true);
        return false;
    }
    const Status saved = ClassTransferJsonCodec::saveFile(
        QFileInfo(normalizedPath).absoluteFilePath(), package.value());
    if (!saved)
    {
        setStatus(saved.error(), true);
        return false;
    }
    setStatus(transferText("Exported %1 class(es) to %2.")
        .arg(QString::number(static_cast<qulonglong>(selected.size())),
            QDir::toNativeSeparators(QFileInfo(normalizedPath).absoluteFilePath())), false);
    return true;
}

bool ClassTransferWorkflowPage::loadImportFile(const QString& inputPath)
{
    clearReview();
    const auto path = activePath();
    if (!path)
    {
        setStatus(transferText("The active workspace is unavailable."), true);
        return false;
    }
    if (m_loadedWorkspacePath != path)
    {
        clearForWorkspaceChange();
        m_loadedWorkspacePath = path;
        static_cast<void>(refreshClasses());
    }
    m_reviewWorkspacePath = path;
    std::optional<Application::ClassTransferPortablePackage> parsedPackage;
    {
        const auto legacy = ClassTransferJsonCodec::loadFile(inputPath);
        if (!legacy)
        {
            clearReview();
            setStatus(legacy.error(), true);
            return false;
        }
        parsedPackage = Data::Legacy::toPortableClassTransferPackage(
            std::move(*legacy));
    }
    m_package = std::move(parsedPackage);
    refreshPreview();
    return m_preview.has_value();
}

Application::ClassTransferPreviewRequest
ClassTransferWorkflowPage::previewRequest() const
{
    Application::ClassTransferPreviewRequest request;
    if (!m_package)
    {
        return request;
    }
    request.teachers.reserve(m_package->teachers.size());
    for (const Application::ClassExportTeacher& teacher : m_package->teachers)
    {
        request.teachers.push_back({
            Application::ClassTransferTeacherKey(
                QString::fromStdU16String(teacher.key.value()).toStdString()),
            {
                normalizedUtf8(teacher.teacher.teacherEn),
                normalizedUtf8(teacher.teacher.teacherKr)
            }
        });
    }
    request.classes.reserve(m_package->classes.size());
    for (std::size_t index = 0; index < m_package->classes.size(); ++index)
    {
        const Application::ClassExportClass& transferClass =
            m_package->classes[index];
        request.classes.push_back({
            Application::ClassTransferPackageClassIndex{
                static_cast<int>(index)},
            Application::ClassTransferTeacherKey(
                QString::fromStdU16String(transferClass.teacherKey.value())
                    .toStdString()),
            normalizedUtf8(transferClass.info.classGrade),
            normalizedUtf8(transferClass.info.classLevel),
            Application::ClassTransferClassKey(
                QString::fromStdU16String(transferClass.key.value()).toStdString())
        });
    }
    return request;
}

void ClassTransferWorkflowPage::refreshPreview()
{
    if (!m_package || !m_reviewWorkspacePath || !m_actions.previewImport)
    {
        setStatus(transferText("Load a package before refreshing matches."), true);
        return;
    }
    if (!ensureReviewPathIsCurrent())
    {
        return;
    }
    const auto preview = m_actions.previewImport(
        *m_reviewWorkspacePath, previewRequest());
    if (!preview)
    {
        m_preview.reset();
        m_classReviewModel->clear();
        m_teacherReviewModel->clear();
        m_reviewSummaryLabel->clear();
        setStatus(utf8Text(preview.error().message), true);
        refreshReviewChoices();
        refreshApplyState();
        return;
    }
    m_preview = preview.value();
    m_classReviewModel->setClasses(m_package->classes, m_preview->classes);
    m_teacherReviewModel->setTeachers(m_package->teachers, m_preview->teachers);
    m_reviewSummaryLabel->setText(transferText(
        "%1 class(es), %2 teacher(s); select a row to choose its action.")
        .arg(static_cast<qulonglong>(m_package->classes.size()))
        .arg(static_cast<qulonglong>(m_package->teachers.size())));
    if (m_classReviewModel->rowCount() > 0)
    {
        m_classReviewTable->selectRow(0);
    }
    else if (m_teacherReviewModel->rowCount() > 0)
    {
        m_reviewTabs->setCurrentIndex(1);
        m_teacherReviewTable->selectRow(0);
    }
    setStatus(transferText("Package matches are ready for review."), false);
    refreshReviewChoices();
    refreshApplyState();
}

Application::ClassTransferApplyCandidates
ClassTransferWorkflowPage::applyCandidates() const
{
    Application::ClassTransferApplyCandidates result;
    if (m_preview)
    {
        result.classes = m_preview->classes;
        result.teachers = m_preview->teachers;
    }
    return result;
}

Application::ClassTransferApplyRequest
ClassTransferWorkflowPage::applyRequest() const
{
    Application::ClassTransferApplyRequest result =
        m_classReviewModel->applyChoices();
    const Application::ClassTransferApplyRequest teachers =
        m_teacherReviewModel->applyChoices();
    result.teachers = teachers.teachers;
    return result;
}

void ClassTransferWorkflowPage::refreshReviewChoices()
{
    ReviewModel* const model = m_reviewTabs->currentIndex() == 0
        ? m_classReviewModel : m_teacherReviewModel;
    QTableView* const view = m_reviewTabs->currentIndex() == 0
        ? m_classReviewTable : m_teacherReviewTable;
    const int row = view->currentIndex().row();
    const QSignalBlocker blocker(m_resolutionChoice);
    m_resolutionChoice->clear();
    const auto choices = model->optionsFor(row);
    for (const ReviewModel::Option& choice : choices)
    {
        m_resolutionChoice->addItem(choice.label);
    }
    const int selected = model->selectedOption(row);
    if (selected >= 0 && selected < m_resolutionChoice->count())
    {
        m_resolutionChoice->setCurrentIndex(selected);
    }
    m_resolutionChoice->setEnabled(row >= 0 && !choices.empty());
    m_refreshPreviewButton->setEnabled(m_package.has_value());
    m_cancelReviewButton->setEnabled(m_package.has_value());
}

void ClassTransferWorkflowPage::refreshApplyState()
{
    const bool ready = m_package.has_value() && m_preview.has_value();
    bool valid = false;
    QString validationMessage;
    if (ready)
    {
        const auto decision = Application::validateClassTransferApplyRequest(
            applyRequest(), applyCandidates());
        valid = decision.accepted();
        if (!valid && !decision.issues.empty())
        {
            validationMessage = transferText(
                "Resolve every required class and teacher choice before applying.");
        }
    }
    m_applyButton->setEnabled(valid);
    if (!validationMessage.isEmpty())
    {
        m_reviewSummaryLabel->setText(validationMessage);
    }
    else if (!ready)
    {
        m_reviewSummaryLabel->clear();
    }
}

bool ClassTransferWorkflowPage::ensureReviewPathIsCurrent()
{
    if (m_reviewWorkspacePath == activePath())
    {
        return true;
    }
    clearReview();
    if (const auto path = activePath())
    {
        clearForWorkspaceChange();
        m_loadedWorkspacePath = path;
        static_cast<void>(refreshClasses());
    }
    setStatus(transferText(
        "The active workspace changed. Load the package again before continuing."), true);
    return false;
}

bool ClassTransferWorkflowPage::applyImport()
{
    if (!m_package || !m_preview || !m_reviewWorkspacePath
        || !m_actions.applyImport || !ensureReviewPathIsCurrent())
    {
        return false;
    }
    const Application::ClassTransferApplyRequest choices = applyRequest();
    const auto decision = Application::validateClassTransferApplyRequest(
        choices, applyCandidates());
    if (!decision.accepted())
    {
        setStatus(transferText(
            "Resolve every required class and teacher choice before applying."), true);
        return false;
    }

    const auto applied = m_actions.applyImport(*m_reviewWorkspacePath,
        Application::ClassTransferApplyCommand{*m_package, choices});
    if (!applied)
    {
        setStatus(utf8Text(applied.error().message), true);
        return false;
    }

    const Application::ClassTransferApplySummary summary = applied.value();
    const int created = static_cast<int>(summary.createdClassIds.size());
    const int replaced = static_cast<int>(summary.replacedClassIds.size());
    const int skipped = summary.skippedClassCount.rawValue();
    std::optional<Domain::ClassId> firstAffected;
    if (!summary.createdClassIds.empty())
    {
        firstAffected = summary.createdClassIds.front();
    }
    else if (!summary.replacedClassIds.empty())
    {
        firstAffected = summary.replacedClassIds.front();
    }
    clearReview();
    static_cast<void>(refreshClasses());
    setStatus(transferText("Import complete: %1 created, %2 replaced, %3 skipped.")
        .arg(created).arg(replaced).arg(skipped), false);
    if (firstAffected && m_actions.selectImportedClass)
    {
        m_actions.selectImportedClass(*firstAffected);
    }
    return true;
}

void ClassTransferWorkflowPage::cancelImportReview()
{
    clearReview();
    setStatus(transferText("Package review cancelled."), false);
}

void ClassTransferWorkflowPage::clearReview()
{
    m_package.reset();
    m_preview.reset();
    m_reviewWorkspacePath.reset();
    m_classReviewModel->clear();
    m_teacherReviewModel->clear();
    m_reviewSummaryLabel->clear();
    m_applyButton->setEnabled(false);
    m_refreshPreviewButton->setEnabled(false);
    m_cancelReviewButton->setEnabled(false);
    refreshReviewChoices();
}

void ClassTransferWorkflowPage::clearForWorkspaceChange()
{
    clearReview();
    m_classSelectionModel->clear();
    m_loadedWorkspacePath.reset();
}

void ClassTransferWorkflowPage::clearForClosedWorkspace()
{
    clearForWorkspaceChange();
    setStatus(transferText("No workspace is open."), false);
}

void ClassTransferWorkflowPage::setStatus(const QString& message, const bool error)
{
    m_statusLabel->setText(message);
    m_statusLabel->setProperty("classTransferError", error);
}

} // namespace ClassMngr::Next::Ui
