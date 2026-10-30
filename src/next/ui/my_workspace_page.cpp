#include "next/ui/my_workspace_page.h"

#include "features/my_info/data/signature_image_processor.h"
#include "features/my_info/data/typed_signature_renderer.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyle>
#include <QTabBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QBuffer>
#include <QAbstractTableModel>
#include <QDate>
#include <QHeaderView>
#include <QTableView>
#include <QLocale>
#include <QTime>

#include <algorithm>
#include <array>
#include <exception>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int AutosaveDebounceMilliseconds = 750;
constexpr int SignaturePreviewHeight = 120;
constexpr int LegacyCalendarViewportLeft = 44;
constexpr int LegacyCalendarViewportTop = 185;
constexpr int LegacyCalendarViewportRightMargin = 44;
constexpr int LegacyCalendarViewportBottomMargin = 88;

QString text(const char* source)
{
    return QCoreApplication::translate("PersonalDetailsPage", source);
}

QString workspaceText(const char* source)
{
    return QCoreApplication::translate("NextMyWorkspacePage", source);
}

QString navigationText(const char* source)
{
    return QCoreApplication::translate("NextNavigation", source);
}

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString scheduleDayText(const std::string& source)
{
    const QString day = fromUtf8(source);
    static constexpr std::array<const char*, 7> canonicalWeekdays{
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
    };
    for (const char* const canonicalDay : canonicalWeekdays)
    {
        if (day == QString::fromLatin1(canonicalDay))
        {
            return QCoreApplication::translate("ScheduleWidget", canonicalDay);
        }
    }
    return day;
}

class MyWorkspaceScheduleTableModel final : public QAbstractTableModel
{
public:
    explicit MyWorkspaceScheduleTableModel(QObject* parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_visibleRows.size());
    }

    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : 5;
    }

    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        const int role = Qt::DisplayRole
        ) const override
    {
        if (!index.isValid() || index.row() < 0
            || static_cast<std::size_t>(index.row()) >= m_visibleRows.size()
            || role != Qt::DisplayRole)
        {
            return {};
        }
        const auto [rowIndex, cellIndex] =
            m_visibleRows.at(static_cast<std::size_t>(index.row()));
        const Application::ScheduleViewRow& row = m_projection.rows().at(rowIndex);
        const Application::ScheduleViewCell& cell = row.cells.at(cellIndex);
        switch (index.column())
        {
        case 0:
            return scheduleDayText(row.dayLabel);
        case 1:
            return QStringLiteral("%1 - %2")
                .arg(fromUtf8(cell.startText), fromUtf8(cell.endText));
        case 2:
            return fromUtf8(cell.meetingText);
        case 3:
            return fromUtf8(cell.roomText);
        case 4:
            return cell.mode == Application::ScheduleViewMode::Intensive
                ? workspaceText("Intensive")
                : workspaceText("Regular");
        default:
            return {};
        }
    }

    [[nodiscard]] QVariant headerData(
        const int section,
        const Qt::Orientation orientation,
        const int role = Qt::DisplayRole
        ) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        {
            return QAbstractTableModel::headerData(section, orientation, role);
        }
        switch (section)
        {
        case 0: return workspaceText("Day");
        case 1: return workspaceText("Time");
        case 2: return workspaceText("Class / Teacher");
        case 3: return workspaceText("Room");
        case 4: return workspaceText("Schedule Type");
        default: return {};
        }
    }

    void setProjection(Application::ScheduleViewProjection projection)
    {
        beginResetModel();
        m_projection = std::move(projection);
        m_visibleRows.clear();
        m_visibleRows.reserve(m_projection.cellCount());
        for (std::size_t rowIndex = 0;
             rowIndex < m_projection.rows().size();
             ++rowIndex)
        {
            const auto& row = m_projection.rows().at(rowIndex);
            for (std::size_t cellIndex = 0; cellIndex < row.cells.size(); ++cellIndex)
            {
                m_visibleRows.emplace_back(rowIndex, cellIndex);
            }
        }
        endResetModel();
    }

    void clear()
    {
        setProjection({});
    }

    void retranslateDayLabels()
    {
        if (rowCount() > 0)
        {
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0),
                               {Qt::DisplayRole});
        }
    }

private:
    Application::ScheduleViewProjection m_projection;
    std::vector<std::pair<std::size_t, std::size_t>> m_visibleRows;
};

std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

std::string bytes(const QByteArray& value)
{
    return value.isEmpty()
        ? std::string{}
        : std::string(
              value.constData(),
              static_cast<std::size_t>(value.size())
              );
}

TypedSignatureFont typedSignatureFont(
    const Application::PersonalSignatureFontChoice choice
    )
{
    using Choice = Application::PersonalSignatureFontChoice;
    switch (choice)
    {
    case Choice::DancingScript:
        return TypedSignatureFont::DancingScript;
    case Choice::GreatVibes:
        return TypedSignatureFont::GreatVibes;
    case Choice::Caveat:
        return TypedSignatureFont::Caveat;
    case Choice::JustAnotherHand:
    default:
        return TypedSignatureFont::JustAnotherHand;
    }
}

Application::PersonalSignatureFontChoice applicationSignatureFont(
    const TypedSignatureFont font
    )
{
    using Choice = Application::PersonalSignatureFontChoice;
    switch (font)
    {
    case TypedSignatureFont::DancingScript:
        return Choice::DancingScript;
    case TypedSignatureFont::GreatVibes:
        return Choice::GreatVibes;
    case TypedSignatureFont::Caveat:
        return Choice::Caveat;
    case TypedSignatureFont::JustAnotherHand:
    default:
        return Choice::JustAnotherHand;
    }
}

int matchingCampusIndex(
    const QComboBox& combo,
    const QString& storedCampus
    )
{
    const QString normalized = storedCampus.trimmed();
    if (normalized.isEmpty())
    {
        return -1;
    }
    for (int index = 0; index < combo.count(); ++index)
    {
        if (combo.itemData(index).toString().compare(
                normalized, Qt::CaseInsensitive) == 0
            || combo.itemText(index).compare(
                normalized, Qt::CaseInsensitive) == 0)
        {
            return index;
        }
    }
    return -1;
}

} // namespace

MyWorkspacePage::MyWorkspacePage(
    const PageContext& context,
    MyWorkspacePageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
    , m_saveMode(m_actions.saveMode)
    , m_autosaveTimer(new QTimer(this))
{
    m_autosaveTimer->setSingleShot(true);
    m_autosaveTimer->setInterval(AutosaveDebounceMilliseconds);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this] {
        static_cast<void>(saveDetails(false));
    });
    buildUi(context);
    updateSaveStatus(workspaceText("All changes saved."));
}

PageOperationResult<void> MyWorkspacePage::onActivating()
{
    m_pageActive = true;
    refreshForWorkspaceIdentity();
    return {};
}

bool MyWorkspacePage::hasUnsavedChanges() const noexcept
{
    return m_dirty || Page::hasUnsavedChanges();
}

PageOperationResult<void> MyWorkspacePage::saveChanges()
{
    if (!m_dirty && Page::hasUnsavedChanges())
    {
        setHasUnsavedChanges(false);
        return {};
    }
    return saveDetails(true);
}

PageOperationResult<void> MyWorkspacePage::discardChanges()
{
    if (!m_dirty && Page::hasUnsavedChanges())
    {
        setHasUnsavedChanges(false);
        return {};
    }
    return reloadForDiscard();
}

void MyWorkspacePage::onSuspending() noexcept
{
    m_pageActive = false;
    if (m_embeddedCalendar)
    {
        m_embeddedCalendar->onSuspending();
    }
    clearWorkspaceSummaries();
    m_autosaveTimer->stop();
    if (m_signaturePreviewLabel)
    {
        m_signaturePreviewLabel->setPixmap(QPixmap{});
    }
}

void MyWorkspacePage::refreshForWorkspaceIdentity()
{
    static_cast<void>(synchronizeWorkspaceIdentity());
    if (m_pageActive)
    {
        refreshCurrentWorkspaceTab();
    }
}

void MyWorkspacePage::clearForClosedWorkspace()
{
    m_workspaceIdentity.clear();
    if (m_embeddedCalendar)
    {
        m_embeddedCalendar->clearForClosedWorkspace();
    }
    clearWorkspaceSummaries();
    clearForNewWorkspace();
    m_reloadButton->hide();
    updateSaveStatus(workspaceText("No Teacher Profile is open."));
    updateScheduleStatus(workspaceText("No Teacher Profile is open."));
}

void MyWorkspacePage::reloadPersonalDetails()
{
    const QString identity = m_actions.workspaceIdentity
        ? m_actions.workspaceIdentity()
        : QString{};
    if (identity != m_workspaceIdentity)
    {
        m_workspaceIdentity = identity;
        clearForNewWorkspace();
    }
    static_cast<void>(loadPersonalDetails(false));
}

void MyWorkspacePage::setSaveMode(const Application::SaveMode mode)
{
    if (m_saveMode == mode)
    {
        return;
    }
    m_saveMode = mode;
    if (mode == Application::SaveMode::Manual)
    {
        m_autosaveTimer->stop();
    }
    else if (m_dirty && m_loaded)
    {
        m_autosaveTimer->start();
    }
}

bool MyWorkspacePage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_workspaceTabs && event
        && event->type() == QEvent::Resize)
    {
        updateEmbeddedCalendarViewportInsets();
    }
    if (watched == m_signaturePreviewLabel && event
        && event->type() == QEvent::Resize)
    {
        QTimer::singleShot(0, this, [this] {
            updateSignaturePreview();
        });
    }
    return QWidget::eventFilter(watched, event);
}

void MyWorkspacePage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (!event)
    {
        return;
    }
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    if (event->type() == QEvent::LanguageChange
        || event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange)
    {
        clearWorkspaceSummaries();
        if (m_pageActive)
        {
            refreshCurrentWorkspaceTab();
        }
    }
}

void MyWorkspacePage::retranslateUi()
{
    if (auto* const title = findChild<QLabel*>(QStringLiteral("pageTitle")))
    {
        title->setText(navigationText("My Workspace"));
    }
    if (auto* const subtitle = findChild<QLabel*>(QStringLiteral("pageSubtitle")))
    {
        subtitle->setText(navigationText(
            "Manage your personal details, schedule, and calendar."));
    }
    if (m_workspaceTabBar)
    {
        m_workspaceTabBar->setTabText(0, navigationText("My Details"));
        m_workspaceTabBar->setTabText(1, navigationText("My Schedule"));
        m_workspaceTabBar->setTabText(2, navigationText("Calendar"));
    }
    if (m_scheduleHeading)
    {
        m_scheduleHeading->setText(workspaceText("Weekly Class Schedule"));
    }
    if (m_scheduleSubtitle)
    {
        m_scheduleSubtitle->setText(workspaceText(
            "Read-only regular schedule from registered classes."));
    }
    if (m_scheduleRetryButton)
    {
        m_scheduleRetryButton->setText(workspaceText("Retry"));
    }
    if (m_scheduleStatusLabel)
    {
        m_scheduleStatusLabel->setText(workspaceText("Schedule data is not loaded."));
    }
    if (m_scheduleModel)
    {
        m_scheduleModel->headerDataChanged(Qt::Horizontal, 0, 4);
        if (auto* const scheduleModel =
                dynamic_cast<MyWorkspaceScheduleTableModel*>(m_scheduleModel))
        {
            scheduleModel->retranslateDayLabels();
        }
    }
}

void MyWorkspacePage::onWorkspaceTabChanged(const int index)
{
    if (index < 0 || index >= m_workspaceTabs->count()
        || index == m_previousWorkspaceTab)
    {
        return;
    }
    if (m_previousWorkspaceTab == 1)
    {
        clearScheduleSummary();
    }
    else if (m_previousWorkspaceTab == 2)
    {
        if (m_embeddedCalendar)
        {
            m_embeddedCalendar->onSuspending();
        }
    }
    m_previousWorkspaceTab = index;
    m_workspaceTabs->setCurrentIndex(index);
    if (m_pageActive)
    {
        refreshCurrentWorkspaceTab();
    }
}

void MyWorkspacePage::refreshCurrentWorkspaceTab()
{
    if (!m_workspaceTabs)
    {
        return;
    }
    const bool workspaceChanged = synchronizeWorkspaceIdentity();
    if (m_workspaceTabs->currentIndex() == 0)
    {
        if (workspaceChanged || !m_loaded || !m_dirty)
        {
            static_cast<void>(loadPersonalDetails(false));
        }
    }
    else if (m_workspaceTabs->currentIndex() == 1)
    {
        loadScheduleSummary();
    }
    else if (m_workspaceTabs->currentIndex() == 2)
    {
        if (m_embeddedCalendar)
        {
            static_cast<void>(m_embeddedCalendar->onActivating());
        }
    }
}

bool MyWorkspacePage::synchronizeWorkspaceIdentity()
{
    const QString identity = m_actions.workspaceIdentity
        ? m_actions.workspaceIdentity()
        : QString{};
    if (identity == m_workspaceIdentity)
    {
        return false;
    }
    m_workspaceIdentity = identity;
    clearForNewWorkspace();
    return true;
}

void MyWorkspacePage::loadScheduleSummary()
{
    clearScheduleSummary();
    updateScheduleStatus(workspaceText("Loading schedule..."));
    Application::MyWorkspaceScheduleReadResult loaded = m_actions.readSchedule
        ? m_actions.readSchedule()
        : Application::MyWorkspaceScheduleReadResult::failure({
              .code = Domain::ErrorCode::NotFound,
              .message = "The active workspace schedule repository is unavailable.",
              .recoverable = true
          });
    if (!loaded)
    {
        updateScheduleStatus(
            workspaceText("The schedule could not be loaded. Retry this tab."),
            true
            );
        m_scheduleStatusLabel->setToolTip(fromUtf8(loaded.error().message));
        return;
    }
    auto* const model = dynamic_cast<MyWorkspaceScheduleTableModel*>(m_scheduleModel);
    if (!model)
    {
        updateScheduleStatus(
            workspaceText("The schedule could not be loaded. Retry this tab."),
            true
            );
        return;
    }
    model->setProjection(std::move(loaded.value()));
    if (model->rowCount() == 0)
    {
        updateScheduleStatus(workspaceText("There are no schedule entries."));
    }
    else
    {
        updateScheduleStatus(QString{});
        m_scheduleTable->resizeRowsToContents();
    }
}

void MyWorkspacePage::clearScheduleSummary()
{
    if (auto* const model = dynamic_cast<MyWorkspaceScheduleTableModel*>(m_scheduleModel))
    {
        model->clear();
    }
    if (m_scheduleStatusLabel)
    {
        m_scheduleStatusLabel->setToolTip(QString{});
    }
    updateScheduleStatus(QString{});
}

void MyWorkspacePage::clearWorkspaceSummaries()
{
    clearScheduleSummary();
}

void MyWorkspacePage::updateScheduleStatus(
    const QString& message,
    const bool error
    )
{
    if (!m_scheduleStatusLabel)
    {
        return;
    }
    m_scheduleStatusLabel->setText(message);
    m_scheduleStatusLabel->setProperty("loadError", error);
    if (m_scheduleStatusLabel->style())
    {
        m_scheduleStatusLabel->style()->unpolish(m_scheduleStatusLabel);
        m_scheduleStatusLabel->style()->polish(m_scheduleStatusLabel);
    }
    if (m_scheduleStatusLabel->parentWidget())
    {
        m_scheduleStatusLabel->parentWidget()->setVisible(!message.isEmpty());
    }
    if (m_scheduleRetryButton)
    {
        m_scheduleRetryButton->setVisible(error);
    }
}

void MyWorkspacePage::updateEmbeddedCalendarViewportInsets()
{
    if (!m_workspaceTabs || !m_embeddedCalendarLayout)
    {
        return;
    }

    const QPoint topLeft = m_workspaceTabs->mapTo(this, QPoint{});
    const int right = topLeft.x() + m_workspaceTabs->width();
    const int bottom = topLeft.y() + m_workspaceTabs->height();
    const int leftInset = qMax(0, LegacyCalendarViewportLeft - topLeft.x());
    const int topInset = qMax(0, LegacyCalendarViewportTop - topLeft.y());
    const int rightInset = qMax(
        0, right - (width() - LegacyCalendarViewportRightMargin));
    const int targetBottom = height() - LegacyCalendarViewportBottomMargin;
    const int bottomInset = qMax(0, bottom - targetBottom);
    m_embeddedCalendarLayout->setContentsMargins(
        leftInset, topInset, rightInset, bottomInset);
}

void MyWorkspacePage::buildUi(const PageContext& context)
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(36, 34, 44, 12);
    root->setSpacing(6);

    auto* const title = new QLabel(navigationText("My Workspace"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont titleFont = title->font();
    titleFont.setPointSize(24);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    auto* const subtitle = new QLabel(
        navigationText("Manage your personal details, schedule, and calendar."),
        this
        );
    subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    subtitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont subtitleFont = subtitle->font();
    subtitleFont.setPointSize(12);
    subtitle->setFont(subtitleFont);
    subtitle->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addSpacing(26);

    auto* const sectionNavigation = new QWidget(this);
    sectionNavigation->setObjectName(QStringLiteral("nextMyWorkspaceTabs"));
    auto* const sectionNavigationLayout = new QHBoxLayout(sectionNavigation);
    sectionNavigationLayout->setContentsMargins(0, 0, 0, 0);
    sectionNavigationLayout->setSpacing(0);
    sectionNavigationLayout->addStretch(1);
    m_workspaceTabBar = new QTabBar(sectionNavigation);
    m_workspaceTabBar->setObjectName(QStringLiteral("nextMyWorkspaceTabBar"));
    m_workspaceTabBar->setExpanding(false);
    m_workspaceTabBar->setDrawBase(false);
    m_workspaceTabBar->addTab(navigationText("My Details"));
    m_workspaceTabBar->addTab(navigationText("My Schedule"));
    m_workspaceTabBar->addTab(navigationText("Calendar"));
    sectionNavigationLayout->addWidget(m_workspaceTabBar);
    sectionNavigationLayout->addStretch(1);
    root->addWidget(sectionNavigation);

    m_workspaceTabs = new QStackedWidget(this);
    m_workspaceTabs->setObjectName(QStringLiteral("nextMyWorkspaceTabPages"));
    m_workspaceTabs->installEventFilter(this);
    root->addWidget(m_workspaceTabs, 1);

    auto* const detailsTab = new QWidget(m_workspaceTabs);
    detailsTab->setObjectName(QStringLiteral("nextMyWorkspaceDetailsTab"));
    auto* const detailsLayout = new QVBoxLayout(detailsTab);
    detailsLayout->setContentsMargins(0, 69, 0, 4);
    detailsLayout->setSpacing(10);

    auto* const scroll = new QScrollArea(detailsTab);
    scroll->setObjectName(QStringLiteral("nextMyWorkspaceDetailsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* const content = new QWidget(scroll);
    auto* const contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(32, 0, 16, 0);
    contentLayout->setSpacing(10);

    auto* const informationHeading = new QLabel(text("My Information"), content);
    informationHeading->setObjectName(QStringLiteral("nextMyWorkspaceInformationHeading"));
    informationHeading->setAlignment(Qt::AlignCenter);
    QFont headingFont = informationHeading->font();
    headingFont.setPointSize(20);
    headingFont.setWeight(QFont::DemiBold);
    informationHeading->setFont(headingFont);
    contentLayout->addWidget(informationHeading);

    auto* const informationCard = new QFrame(content);
    informationCard->setObjectName(QStringLiteral("nextMyWorkspaceInformationCard"));
    informationCard->setProperty("role", QStringLiteral("card"));
    informationCard->setFrameShape(QFrame::StyledPanel);
    auto* const informationGrid = new QGridLayout(informationCard);
    informationGrid->setContentsMargins(20, 4, 20, 18);
    informationGrid->setHorizontalSpacing(16);
    informationGrid->setVerticalSpacing(6);

    const std::array<const char*, 5> labels{
        "My Name", "My Campus", "Zoom Login ID", "Zoom Password", "Zoom"
    };
    for (int column = 0; column < static_cast<int>(labels.size()); ++column)
    {
        auto* const label = new QLabel(text(labels.at(static_cast<std::size_t>(column))),
                                       informationCard);
        label->setObjectName(QStringLiteral("nextMyWorkspaceFieldLabel%1").arg(column));
        informationGrid->addWidget(label, 0, column);
    }
    m_nameEdit = new QLineEdit(informationCard);
    m_nameEdit->setObjectName(QStringLiteral("nextMyWorkspaceName"));
    m_nameEdit->setClearButtonEnabled(true);
    m_nameEdit->setMinimumWidth(190);
    m_campusCombo = new QComboBox(informationCard);
    m_campusCombo->setObjectName(QStringLiteral("nextMyWorkspaceCampus"));
    m_campusCombo->setMinimumWidth(190);
    m_zoomLoginIdEdit = new QLineEdit(informationCard);
    m_zoomLoginIdEdit->setObjectName(QStringLiteral("nextMyWorkspaceZoomLoginId"));
    m_zoomLoginIdEdit->setMinimumWidth(190);
    m_zoomPasswordEdit = new QLineEdit(informationCard);
    m_zoomPasswordEdit->setObjectName(QStringLiteral("nextMyWorkspaceZoomPassword"));
    m_zoomPasswordEdit->setMinimumWidth(190);
    m_zoomNotAvailableCheck = new QCheckBox(text("N/A"), informationCard);
    m_zoomNotAvailableCheck->setObjectName(QStringLiteral("nextMyWorkspaceZoomNotAvailable"));
    const std::array<QWidget*, 5> fields{
        m_nameEdit, m_campusCombo, m_zoomLoginIdEdit, m_zoomPasswordEdit,
        m_zoomNotAvailableCheck
    };
    for (int column = 0; column < static_cast<int>(fields.size()); ++column)
    {
        informationGrid->addWidget(fields.at(static_cast<std::size_t>(column)), 1, column);
        informationGrid->setColumnStretch(column, 1);
    }
    contentLayout->addWidget(informationCard);
    contentLayout->addSpacing(49);

    auto* const signatureHeading = new QLabel(text("Signature"), content);
    signatureHeading->setObjectName(QStringLiteral("nextMyWorkspaceSignatureHeading"));
    signatureHeading->setAlignment(Qt::AlignCenter);
    signatureHeading->setFont(headingFont);
    contentLayout->addWidget(signatureHeading);
    auto* const signatureCard = new QFrame(content);
    signatureCard->setObjectName(QStringLiteral("nextMyWorkspaceSignatureCard"));
    signatureCard->setProperty("role", QStringLiteral("card"));
    signatureCard->setFrameShape(QFrame::StyledPanel);
    auto* const signatureLayout = new QVBoxLayout(signatureCard);
    signatureLayout->setContentsMargins(20, 20, 20, 12);
    signatureLayout->setSpacing(15);
    auto* const instructions = new QLabel(
        text("Choose an image file or type your signature."), signatureCard);
    instructions->setObjectName(QStringLiteral("nextMyWorkspaceSignatureInstructions"));
    instructions->setWordWrap(true);
    signatureLayout->addWidget(instructions);

    auto* const modeLayout = new QHBoxLayout;
    m_imageModeButton = new QPushButton(text("Image"), signatureCard);
    m_imageModeButton->setObjectName(QStringLiteral("nextMyWorkspaceSignatureImageMode"));
    m_imageModeButton->setCheckable(true);
    m_typeModeButton = new QPushButton(text("Type"), signatureCard);
    m_typeModeButton->setObjectName(QStringLiteral("nextMyWorkspaceSignatureTypeMode"));
    m_typeModeButton->setCheckable(true);
    auto* const modes = new QButtonGroup(this);
    modes->setExclusive(true);
    modes->addButton(m_imageModeButton);
    modes->addButton(m_typeModeButton);
    modeLayout->addWidget(m_imageModeButton);
    modeLayout->addWidget(m_typeModeButton);
    modeLayout->addStretch(1);
    signatureLayout->addLayout(modeLayout);

    m_signaturePreviewLabel = new QLabel(signatureCard);
    m_signaturePreviewLabel->setObjectName(QStringLiteral("nextMyWorkspaceSignaturePreview"));
    m_signaturePreviewLabel->setAlignment(Qt::AlignCenter);
    m_signaturePreviewLabel->setFixedHeight(SignaturePreviewHeight);
    m_signaturePreviewLabel->setFrameShape(QFrame::StyledPanel);
    m_signaturePreviewLabel->installEventFilter(this);
    signatureLayout->addWidget(m_signaturePreviewLabel);

    m_imageControls = new QWidget(signatureCard);
    auto* const imageActions = new QHBoxLayout(m_imageControls);
    imageActions->setContentsMargins(0, 0, 0, 0);
    imageActions->addStretch(1);
    m_chooseImageButton = new QPushButton(text("Add Signature Image..."), m_imageControls);
    m_chooseImageButton->setObjectName(QStringLiteral("nextMyWorkspaceChooseSignatureImage"));
    m_removeImageButton = new QPushButton(text("Remove"), m_imageControls);
    m_removeImageButton->setObjectName(QStringLiteral("nextMyWorkspaceRemoveSignatureImage"));
    imageActions->addWidget(m_chooseImageButton);
    imageActions->addWidget(m_removeImageButton);
    signatureLayout->addWidget(m_imageControls);

    m_typedControls = new QWidget(signatureCard);
    m_typedControls->setObjectName(QStringLiteral("nextMyWorkspaceTypedSignatureControls"));
    auto* const typedLayout = new QVBoxLayout(m_typedControls);
    typedLayout->setContentsMargins(0, 0, 0, 0);
    typedLayout->setSpacing(6);
    auto* const typedSignatureLabel = new QLabel(
        text("Type your signature"), m_typedControls);
    typedSignatureLabel->setObjectName(
        QStringLiteral("nextMyWorkspaceTypedSignatureLabel"));
    typedLayout->addWidget(typedSignatureLabel);
    m_typedSignatureEdit = new QLineEdit(m_typedControls);
    m_typedSignatureEdit->setObjectName(QStringLiteral("nextMyWorkspaceTypedSignature"));
    m_typedSignatureEdit->setPlaceholderText(text("Type your name"));
    typedLayout->addWidget(m_typedSignatureEdit);
    auto* const fontSelectionLabel = new QLabel(
        text("Choose a style"), m_typedControls);
    fontSelectionLabel->setObjectName(
        QStringLiteral("nextMyWorkspaceSignatureFontLabel"));
    typedLayout->addWidget(fontSelectionLabel);
    auto* const fontGrid = new QGridLayout;
    fontGrid->setContentsMargins(0, 0, 0, 0);
    fontGrid->setHorizontalSpacing(8);
    fontGrid->setVerticalSpacing(8);
    const std::array<TypedSignatureFont, 4> fonts{
        TypedSignatureFont::JustAnotherHand,
        TypedSignatureFont::DancingScript,
        TypedSignatureFont::GreatVibes,
        TypedSignatureFont::Caveat
    };
    for (int index = 0; index < static_cast<int>(fonts.size()); ++index)
    {
        auto* const option = new QFrame(m_typedControls);
        option->setObjectName(QStringLiteral("nextMyWorkspaceFontOption%1").arg(index));
        option->setFrameShape(QFrame::StyledPanel);
        auto* const optionLayout = new QVBoxLayout(option);
        optionLayout->setContentsMargins(8, 6, 8, 6);
        auto* const fontName = new QLabel(
            TypedSignature::displayName(fonts.at(static_cast<std::size_t>(index))),
            option);
        fontName->setObjectName(
            QStringLiteral("nextMyWorkspaceFontName%1").arg(index));
        auto* const preview = new QLabel(text("Your Signature"), option);
        preview->setObjectName(QStringLiteral("nextMyWorkspaceFontPreview%1").arg(index));
        preview->setAlignment(Qt::AlignCenter);
        preview->setMinimumHeight(48);
        preview->setFont(TypedSignature::fontFor(fonts.at(static_cast<std::size_t>(index)), 30));
        auto* const button = new QPushButton(text("Use this font"), option);
        button->setObjectName(QStringLiteral("nextMyWorkspaceFontButton%1").arg(index));
        button->setCheckable(true);
        optionLayout->addWidget(fontName);
        optionLayout->addWidget(preview);
        optionLayout->addWidget(button);
        fontGrid->addWidget(option, index / 2, index % 2);
        m_fontPreviewLabels.push_back(preview);
        m_fontButtons.push_back(button);
        connect(button, &QPushButton::clicked, this, [this, index] {
            m_details.typedSignatureFont = applicationSignatureFont(
                static_cast<TypedSignatureFont>(index));
            updateTypedSignatureImage();
            markDirty();
        });
    }
    typedLayout->addLayout(fontGrid);
    signatureLayout->addWidget(m_typedControls);
    contentLayout->addWidget(signatureCard);
    contentLayout->addStretch(1);
    scroll->setWidget(content);
    detailsLayout->addWidget(scroll, 1);

    auto* const footer = new QWidget(detailsTab);
    footer->setObjectName(QStringLiteral("nextMyWorkspaceStatusFooter"));
    m_statusFooter = footer;
    auto* const footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 4, 0, 0);
    footerLayout->setSpacing(8);
    m_statusLabel = new QLabel(footer);
    m_statusLabel->setObjectName(QStringLiteral("nextMyWorkspaceSaveStatus"));
    m_statusLabel->setWordWrap(true);
    footerLayout->addWidget(m_statusLabel, 1);
    m_reloadButton = new QPushButton(workspaceText("Reload My Details"), footer);
    m_reloadButton->setObjectName(QStringLiteral("nextMyWorkspaceReloadDetails"));
    m_reloadButton->hide();
    footerLayout->addWidget(m_reloadButton);
    detailsLayout->addWidget(footer);
    footer->hide();

    m_workspaceTabs->addWidget(detailsTab);

    auto* const scheduleTab = new QWidget(m_workspaceTabs);
    scheduleTab->setObjectName(QStringLiteral("nextMyWorkspaceScheduleTab"));
    auto* const scheduleLayout = new QVBoxLayout(scheduleTab);
    scheduleLayout->setContentsMargins(32, 28, 16, 8);
    scheduleLayout->setSpacing(8);
    m_scheduleHeading = new QLabel(
        workspaceText("Weekly Class Schedule"), scheduleTab
        );
    m_scheduleHeading->setObjectName(QStringLiteral("nextMyWorkspaceScheduleHeading"));
    m_scheduleHeading->setAlignment(Qt::AlignCenter);
    QFont scheduleHeadingFont = headingFont;
    scheduleHeadingFont.setPointSize(20);
    m_scheduleHeading->setFont(scheduleHeadingFont);
    scheduleLayout->addWidget(m_scheduleHeading);
    m_scheduleSubtitle = new QLabel(
        workspaceText("Read-only regular schedule from registered classes."),
        scheduleTab
        );
    m_scheduleSubtitle->setObjectName(QStringLiteral("nextMyWorkspaceScheduleSubtitle"));
    m_scheduleSubtitle->setAlignment(Qt::AlignCenter);
    m_scheduleSubtitle->setWordWrap(true);
    scheduleLayout->addWidget(m_scheduleSubtitle);

    auto* const scheduleCard = new QFrame(scheduleTab);
    scheduleCard->setObjectName(QStringLiteral("nextMyWorkspaceScheduleCard"));
    scheduleCard->setProperty("role", QStringLiteral("card"));
    scheduleCard->setFrameShape(QFrame::StyledPanel);
    auto* const scheduleCardLayout = new QVBoxLayout(scheduleCard);
    scheduleCardLayout->setContentsMargins(12, 12, 12, 8);
    scheduleCardLayout->setSpacing(8);
    m_scheduleTable = new QTableView(scheduleCard);
    m_scheduleTable->setObjectName(QStringLiteral("nextMyWorkspaceScheduleTable"));
    m_scheduleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_scheduleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_scheduleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_scheduleTable->setAlternatingRowColors(true);
    m_scheduleTable->verticalHeader()->hide();
    m_scheduleTable->horizontalHeader()->setStretchLastSection(true);
    m_scheduleTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    m_scheduleTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_scheduleTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch);
    m_scheduleTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    m_scheduleTable->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);
    auto* const scheduleModel = new MyWorkspaceScheduleTableModel(m_scheduleTable);
    m_scheduleModel = scheduleModel;
    m_scheduleTable->setModel(scheduleModel);
    scheduleCardLayout->addWidget(m_scheduleTable, 1);
    auto* const scheduleStatusRow = new QWidget(scheduleCard);
    auto* const scheduleStatusLayout = new QHBoxLayout(scheduleStatusRow);
    scheduleStatusLayout->setContentsMargins(0, 0, 0, 0);
    scheduleStatusLayout->setSpacing(8);
    m_scheduleStatusLabel = new QLabel(
        workspaceText("Schedule data is not loaded."), scheduleStatusRow
        );
    m_scheduleStatusLabel->setObjectName(
        QStringLiteral("nextMyWorkspaceScheduleStatus"));
    m_scheduleStatusLabel->setWordWrap(true);
    scheduleStatusLayout->addWidget(m_scheduleStatusLabel, 1);
    m_scheduleRetryButton = new QPushButton(
        workspaceText("Retry"), scheduleStatusRow
        );
    m_scheduleRetryButton->setObjectName(
        QStringLiteral("nextMyWorkspaceScheduleRetry"));
    m_scheduleRetryButton->hide();
    scheduleStatusLayout->addWidget(m_scheduleRetryButton);
    scheduleCardLayout->addWidget(scheduleStatusRow);
    scheduleLayout->addWidget(scheduleCard, 1);
    m_workspaceTabs->addWidget(scheduleTab);

    auto* const calendarTab = new QWidget(m_workspaceTabs);
    calendarTab->setObjectName(QStringLiteral("nextMyWorkspaceCalendarTab"));
    m_embeddedCalendarLayout = new QVBoxLayout(calendarTab);
    m_embeddedCalendarLayout->setContentsMargins(8, 0, 0, 0);
    m_embeddedCalendarScrollArea = new QScrollArea(calendarTab);
    m_embeddedCalendarScrollArea->setObjectName(
        QStringLiteral("nextMyWorkspaceCalendarScrollArea"));
    m_embeddedCalendarScrollArea->setWidgetResizable(true);
    m_embeddedCalendarScrollArea->setFrameShape(QFrame::NoFrame);
    m_embeddedCalendar = new CalendarPage(
        context,
        m_actions.createCalendarActions
            ? m_actions.createCalendarActions(context)
            : CalendarPageActions{},
        true);
    m_embeddedCalendar->setObjectName(
        QStringLiteral("nextMyWorkspaceEmbeddedCalendar"));
    m_embeddedCalendarScrollArea->setWidget(m_embeddedCalendar);
    m_embeddedCalendarLayout->addWidget(m_embeddedCalendarScrollArea, 1);
    m_workspaceTabs->addWidget(calendarTab);

    m_workspaceTabBar->setCurrentIndex(1);
    m_workspaceTabs->setCurrentIndex(1);
    updateEmbeddedCalendarViewportInsets();
    m_previousWorkspaceTab = 1;
    connect(m_workspaceTabBar, &QTabBar::currentChanged,
            this, &MyWorkspacePage::onWorkspaceTabChanged);
    connect(m_scheduleRetryButton, &QPushButton::clicked,
            this, &MyWorkspacePage::loadScheduleSummary);
    connect(m_reloadButton, &QPushButton::clicked,
            this, &MyWorkspacePage::reloadPersonalDetails);
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this] {
        syncDetailsFromControls();
        markDirty();
    });
    connect(m_campusCombo, &QComboBox::currentTextChanged, this, [this] {
        syncDetailsFromControls();
        markDirty();
    });
    connect(m_zoomLoginIdEdit, &QLineEdit::textChanged, this, [this] {
        syncDetailsFromControls();
        markDirty();
    });
    connect(m_zoomPasswordEdit, &QLineEdit::textChanged, this, [this] {
        syncDetailsFromControls();
        markDirty();
    });
    connect(m_zoomNotAvailableCheck, &QCheckBox::toggled, this, [this] {
        updateZoomControls();
        syncDetailsFromControls();
        markDirty();
    });
    connect(m_imageModeButton, &QPushButton::clicked, this, [this] {
        if (m_details.signatureMode != Application::PersonalSignatureMode::Image)
        {
            m_details.signatureMode = Application::PersonalSignatureMode::Image;
            updateSignatureControls();
            updateSignaturePreview();
            markDirty();
        }
    });
    connect(m_typeModeButton, &QPushButton::clicked, this, [this] {
        if (m_details.signatureMode != Application::PersonalSignatureMode::Type)
        {
            m_details.signatureMode = Application::PersonalSignatureMode::Type;
            updateTypedSignatureImage();
            updateSignatureControls();
            updateSignaturePreview();
            markDirty();
        }
    });
    connect(m_typedSignatureEdit, &QLineEdit::textChanged, this, [this] {
        m_details.typedSignatureText = toUtf8(m_typedSignatureEdit->text());
        if (m_details.signatureMode == Application::PersonalSignatureMode::Type)
        {
            updateTypedSignatureImage();
        }
        markDirty();
    });
    connect(m_chooseImageButton, &QPushButton::clicked, this, [this] {
        if (!m_actions.chooseSignatureImage)
        {
            return;
        }
        const auto selected = m_actions.chooseSignatureImage();
        if (!selected)
        {
            reportWarning(text("Signature Image Error"),
                          fromUtf8(selected.error().message));
            return;
        }
        if (!selected.value().has_value())
        {
            return;
        }

        QByteArray raw = *selected.value();
        QBuffer buffer(&raw);
        if (!buffer.open(QIODevice::ReadOnly))
        {
            reportWarning(text("Signature Image Error"),
                          workspaceText("The signature image could not be prepared."));
            return;
        }
        QImageReader reader(&buffer);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        const QByteArray prepared = SignatureImage::prepareForEmbedding(image);
        if (prepared.isEmpty())
        {
            reportWarning(text("Unsupported Signature Image"),
                          text("The selected file is not a supported image."));
            return;
        }
        m_details.signatureImage = Domain::PersonalSignatureImageSaveBytes(bytes(prepared));
        m_details.signatureMode = Application::PersonalSignatureMode::Image;
        updateSignatureControls();
        updateSignaturePreview();
        markDirty();
    });
    connect(m_removeImageButton, &QPushButton::clicked, this, [this] {
        if (m_details.signatureImage.empty())
        {
            return;
        }
        m_details.signatureImage = Domain::PersonalSignatureImageSaveBytes{};
        updateSignatureControls();
        updateSignaturePreview();
        markDirty();
    });
    updateSignatureControls();
    updateZoomControls();
}

bool MyWorkspacePage::loadPersonalDetails(const bool resetForNewIdentity)
{
    if (resetForNewIdentity)
    {
        clearForNewWorkspace();
    }
    m_autosaveTimer->stop();
    const auto loaded = m_actions.readDetails
        ? m_actions.readDetails()
        : Application::PersonalDetailsReadResult::failure({
              .code = Domain::ErrorCode::NotFound,
              .message = "The active workspace store is unavailable.",
              .recoverable = true
          });
    if (!loaded)
    {
        m_loaded = false;
        const QString message = workspaceText(
            "Your personal details could not be loaded. The displayed controls are preserved; reload before saving.");
        updateSaveStatus(message, true);
        m_reloadButton->show();
        reportWarning(text("Personal Details Could Not Be Loaded"),
                      fromUtf8(loaded.error().message));
        return false;
    }

    m_details = loaded.value();
    std::vector<Application::MyInfoCampusMetadata> campuses;
    try
    {
        if (m_actions.loadCampuses)
        {
            campuses = m_actions.loadCampuses(resourceContext());
        }
    }
    catch (...)
    {
        campuses.clear();
    }
    {
        const QSignalBlocker nameBlocker(m_nameEdit);
        const QSignalBlocker campusBlocker(m_campusCombo);
        const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
        const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);
        const QSignalBlocker notAvailableBlocker(m_zoomNotAvailableCheck);
        const QSignalBlocker typedBlocker(m_typedSignatureEdit);
        m_loading = true;
        m_campusCombo->clear();
        for (const auto& campus : campuses)
        {
            const QString display = fromUtf8(campus.displayName).trimmed();
            const QString identifier = fromUtf8(campus.id.value());
            if (!display.isEmpty())
            {
                m_campusCombo->addItem(display, identifier);
            }
        }
        applyDetailsToControls(m_details);
        m_loading = false;
    }

    m_loaded = true;
    m_dirty = false;
    setHasUnsavedChanges(false);
    m_reloadButton->hide();
    updateZoomControls();
    updateSignatureControls();
    updateSignaturePreview();
    updateSaveStatus(workspaceText("All changes saved."));

    const QString storedCampus = fromUtf8(loaded.value().campus.value());
    const QString selectedCampus = m_campusCombo->currentText();
    if (m_campusCombo->currentIndex() >= 0
        && selectedCampus.compare(storedCampus.trimmed(), Qt::CaseInsensitive) != 0)
    {
        m_details.campus = Application::CurrentCampusPreferenceValue(
            toUtf8(selectedCampus));
        const auto corrected = m_actions.saveDetails
            ? m_actions.saveDetails(m_details)
            : Application::PersonalDetailsSaveResult::failure({
                  .code = Domain::ErrorCode::NotFound,
                  .message = "The active workspace store is unavailable.",
                  .recoverable = true
              });
        if (!corrected)
        {
            reportWarning(text("Unable to Save Preference"),
                          fromUtf8(corrected.error().message));
        }
    }
    return true;
}

void MyWorkspacePage::applyDetailsToControls(
    const Application::PersonalDetailsSaveRequest& details
    )
{
    m_nameEdit->setText(fromUtf8(details.name));
    const QString campus = fromUtf8(details.campus.value());
    int campusIndex = matchingCampusIndex(*m_campusCombo, campus);
    if (campusIndex < 0 && !campus.trimmed().isEmpty())
    {
        // The legacy chooser corrects deleted or renamed campuses to its
        // first current entry and immediately attempts to persist that value.
        campusIndex = 0;
    }
    if (campusIndex < 0 && m_campusCombo->count() > 0)
    {
        campusIndex = 0;
    }
    m_campusCombo->setCurrentIndex(campusIndex);
    const QString login = fromUtf8(details.zoomLoginId);
    const QString password = fromUtf8(details.zoomPassword);
    m_zoomLoginIdEdit->setText(login.trimmed().isEmpty() ? text("N/A") : login);
    m_zoomPasswordEdit->setText(password.trimmed().isEmpty() ? text("N/A") : password);
    m_zoomNotAvailableCheck->setChecked(details.zoomNotAvailable);
    m_typedSignatureEdit->setText(fromUtf8(details.typedSignatureText));
}

void MyWorkspacePage::syncDetailsFromControls()
{
    if (m_loading)
    {
        return;
    }
    m_details.name = toUtf8(m_nameEdit->text());
    m_details.campus = Application::CurrentCampusPreferenceValue(
        toUtf8(m_campusCombo->currentText()));
    m_details.zoomLoginId = toUtf8(m_zoomLoginIdEdit->text());
    m_details.zoomPassword = toUtf8(m_zoomPasswordEdit->text());
    m_details.zoomNotAvailable = m_zoomNotAvailableCheck->isChecked();
    m_details.typedSignatureText = toUtf8(m_typedSignatureEdit->text());
}

void MyWorkspacePage::markDirty()
{
    if (m_loading)
    {
        return;
    }
    m_dirty = true;
    setHasUnsavedChanges(true);
    updateSaveStatus(workspaceText("Unsaved changes."));
    if (m_saveMode == Application::SaveMode::Automatic && m_loaded)
    {
        m_autosaveTimer->start();
    }
}

PageOperationResult<void> MyWorkspacePage::saveDetails(const bool interactive)
{
    m_autosaveTimer->stop();
    if (!m_loaded)
    {
        const QString message = workspaceText(
            "Reload My Details successfully before saving.");
        updateSaveStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }
    if (!m_dirty)
    {
        return {};
    }

    syncDetailsFromControls();
    Application::PersonalDetailsSaveRequest request = m_details;
    if (!request.zoomNotAvailable)
    {
        if (m_zoomLoginIdEdit->text().trimmed().isEmpty())
        {
            request.zoomLoginId = toUtf8(text("N/A"));
        }
        if (m_zoomPasswordEdit->text().trimmed().isEmpty())
        {
            request.zoomPassword = toUtf8(text("N/A"));
        }
    }
    if (request.signatureMode == Application::PersonalSignatureMode::Type)
    {
        request.signatureImage = Domain::PersonalSignatureImageSaveBytes(
            bytes(TypedSignature::renderForEmbedding(
                fromUtf8(request.typedSignatureText),
                typedSignatureFont(request.typedSignatureFont)
                )));
    }

    updateSaveStatus(workspaceText("Saving changes..."));
    const auto saved = m_actions.saveDetails
        ? m_actions.saveDetails(request)
        : Application::PersonalDetailsSaveResult::failure({
              .code = Domain::ErrorCode::NotFound,
              .message = "The active workspace store is unavailable.",
              .recoverable = true
          });
    if (!saved)
    {
        const QString message = workspaceText(
            "Your personal details could not be saved. Your changes are still here.");
        if (interactive)
        {
            updateSaveStatus(QString{}, false);
            reportWarning(text("My Details Could Not Be Saved"),
                          fromUtf8(saved.error().message));
        }
        else
        {
            updateSaveStatus(message, true);
        }
        return std::unexpected(PageOperationError{
            fromUtf8(saved.error().message).isEmpty()
                ? message
                : fromUtf8(saved.error().message)
        });
    }

    m_details = std::move(request);
    if (!m_details.zoomNotAvailable)
    {
        const QSignalBlocker loginBlocker(m_zoomLoginIdEdit);
        const QSignalBlocker passwordBlocker(m_zoomPasswordEdit);
        if (m_zoomLoginIdEdit->text().trimmed().isEmpty())
        {
            m_zoomLoginIdEdit->setText(text("N/A"));
        }
        if (m_zoomPasswordEdit->text().trimmed().isEmpty())
        {
            m_zoomPasswordEdit->setText(text("N/A"));
        }
    }
    m_dirty = false;
    setHasUnsavedChanges(false);
    m_reloadButton->hide();
    updateSignatureControls();
    updateSignaturePreview();
    updateSaveStatus(workspaceText("All changes saved."));
    return {};
}

PageOperationResult<void> MyWorkspacePage::reloadForDiscard()
{
    m_autosaveTimer->stop();
    if (!loadPersonalDetails(false))
    {
        return std::unexpected(PageOperationError{
            workspaceText(
                "Your personal details could not be reloaded. Your current form is preserved.")
        });
    }
    return {};
}

void MyWorkspacePage::updateSignatureControls()
{
    const bool imageMode =
        m_details.signatureMode == Application::PersonalSignatureMode::Image;
    m_imageModeButton->setChecked(imageMode);
    m_typeModeButton->setChecked(!imageMode);
    m_imageControls->setVisible(imageMode);
    m_typedControls->setVisible(!imageMode);
    const bool hasImage = !m_details.signatureImage.empty();
    m_chooseImageButton->setText(text(
        hasImage ? "Replace Signature..." : "Add Signature Image..."));
    m_removeImageButton->setEnabled(hasImage);
    const int selectedFont = static_cast<int>(m_details.typedSignatureFont);
    for (int index = 0; index < static_cast<int>(m_fontButtons.size()); ++index)
    {
        const bool selected = index == selectedFont;
        m_fontButtons.at(static_cast<std::size_t>(index))->setChecked(selected);
        m_fontButtons.at(static_cast<std::size_t>(index))->setText(
            text(selected ? "Selected" : "Use this font"));
        if (index < static_cast<int>(m_fontPreviewLabels.size()))
        {
            m_fontPreviewLabels.at(static_cast<std::size_t>(index))->setText(
                m_typedSignatureEdit->text().trimmed().isEmpty()
                    ? text("Your Signature")
                    : m_typedSignatureEdit->text());
        }
    }
}

void MyWorkspacePage::updateSignaturePreview()
{
    if (m_details.signatureMode == Application::PersonalSignatureMode::Type)
    {
        const QString signature = m_typedSignatureEdit->text().trimmed().isEmpty()
            ? text("Your Signature")
            : m_typedSignatureEdit->text();
        const QSize previewSize = m_signaturePreviewLabel->contentsRect()
            .adjusted(16, 16, -16, -16).size();
        const QImage preview = TypedSignature::render(
            signature,
            typedSignatureFont(m_details.typedSignatureFont),
            previewSize
            );
        m_signaturePreviewLabel->setText(QString{});
        m_signaturePreviewLabel->setPixmap(QPixmap::fromImage(preview));
        return;
    }

    m_signaturePreviewLabel->setFont(font());
    if (m_details.signatureImage.empty())
    {
        m_signaturePreviewLabel->setPixmap(QPixmap{});
        m_signaturePreviewLabel->setText(text("No signature image added"));
        return;
    }
    const std::string& encoded = m_details.signatureImage.value();
    QPixmap pixmap;
    if (!pixmap.loadFromData(
            reinterpret_cast<const uchar*>(encoded.data()),
            static_cast<uint>(encoded.size())))
    {
        m_signaturePreviewLabel->setPixmap(QPixmap{});
        m_signaturePreviewLabel->setText(text("The signature image could not be displayed."));
        return;
    }
    const QSize previewSize = m_signaturePreviewLabel->contentsRect()
        .adjusted(16, 16, -16, -16).size();
    m_signaturePreviewLabel->setText(QString{});
    m_signaturePreviewLabel->setPixmap(pixmap.scaled(
        previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MyWorkspacePage::updateTypedSignatureImage()
{
    m_details.signatureImage = Domain::PersonalSignatureImageSaveBytes(
        bytes(TypedSignature::renderForEmbedding(
            m_typedSignatureEdit->text(),
            typedSignatureFont(m_details.typedSignatureFont)
            )));
    updateSignatureControls();
    updateSignaturePreview();
}

void MyWorkspacePage::updateZoomControls()
{
    const bool enabled = !m_zoomNotAvailableCheck->isChecked();
    m_zoomLoginIdEdit->setEnabled(enabled);
    m_zoomPasswordEdit->setEnabled(enabled);
}

void MyWorkspacePage::updateSaveStatus(const QString& message, const bool error)
{
    m_statusLabel->setText(message);
    m_statusLabel->setProperty("saveError", error);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    if (error && !m_loaded)
    {
        m_reloadButton->show();
    }
    m_statusFooter->setVisible(
        error && (!m_loaded || m_saveMode == Application::SaveMode::Automatic));
}

void MyWorkspacePage::clearForNewWorkspace()
{
    clearWorkspaceSummaries();
    m_autosaveTimer->stop();
    m_loading = true;
    m_details = {};
    m_loaded = false;
    m_dirty = false;
    setHasUnsavedChanges(false);
    m_nameEdit->clear();
    m_campusCombo->clear();
    m_zoomLoginIdEdit->clear();
    m_zoomPasswordEdit->clear();
    m_zoomNotAvailableCheck->setChecked(true);
    m_typedSignatureEdit->clear();
    m_loading = false;
    m_reloadButton->show();
    updateZoomControls();
    updateSignatureControls();
    updateSignaturePreview();
    updateSaveStatus(workspaceText("Loading personal details..."));
}

void MyWorkspacePage::reportWarning(
    const QString& title,
    const QString& message
    )
{
    if (m_actions.reportWarning)
    {
        m_actions.reportWarning(title, message);
    }
}

} // namespace ClassMngr::Next::Ui
