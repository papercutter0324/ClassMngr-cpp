#include "next/ui/my_classes_page.h"

#include "domain/rules/schedule_value_parser.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

QString text(const char* source)
{
    return QCoreApplication::translate("NextMyClassesPage", source);
}

QString errorText(const std::string& message)
{
    return QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size()));
}

QString fromValue(const std::u16string& value)
{
    return QString::fromStdU16String(value).trimmed();
}

QString dayAbbreviation(const QString& value)
{
    const auto day = ScheduleValueParser::parseWeekday(value.trimmed());
    if (!day || day->text != value.trimmed())
    {
        return value.trimmed();
    }
    switch (day->value)
    {
    case ScheduleValueParser::Weekday::Monday: return text("Mon");
    case ScheduleValueParser::Weekday::Tuesday: return text("Tues");
    case ScheduleValueParser::Weekday::Wednesday: return text("Wed");
    case ScheduleValueParser::Weekday::Thursday: return text("Thurs");
    case ScheduleValueParser::Weekday::Friday: return text("Fri");
    case ScheduleValueParser::Weekday::Saturday: return text("Sat");
    case ScheduleValueParser::Weekday::Sunday: return text("Sun");
    }
    return value.trimmed();
}

QString formatSchedule(
    const std::vector<Application::MyClassesPageScheduleEntry>& schedule
    )
{
    if (schedule.empty())
    {
        return text("N/A");
    }
    struct TimeGroup final
    {
        QString startTime;
        QString endTime;
        QStringList days;
    };
    QList<TimeGroup> groups;
    for (const auto& entry : schedule)
    {
        const QString start = fromValue(entry.startTime);
        const QString end = fromValue(entry.endTime);
        auto group = std::find_if(groups.begin(), groups.end(),
            [&start, &end](const TimeGroup& candidate) {
                return candidate.startTime == start && candidate.endTime == end;
            });
        const QString day = dayAbbreviation(fromValue(entry.day));
        if (group == groups.end())
        {
            groups.push_back({start, end, {day}});
        }
        else
        {
            group->days.push_back(day);
        }
    }
    QStringList labels;
    for (const TimeGroup& group : groups)
    {
        const QString days = group.days.join(QLatin1Char('/'));
        if (group.startTime.isEmpty() && group.endTime.isEmpty())
        {
            labels.push_back(days.isEmpty() ? text("N/A") : days);
            continue;
        }
        labels.push_back(QStringLiteral("%1 %2-%3")
            .arg(days.isEmpty() ? text("N/A") : days,
                 group.startTime.isEmpty() ? text("N/A") : group.startTime,
                 group.endTime.isEmpty() ? text("N/A") : group.endTime));
    }
    return labels.join(QStringLiteral("; "));
}

QString teacherName(
    const Application::MyClassesPageTeacherDetails& teacher
    )
{
    for (const std::u16string* candidate : {
             &teacher.preferredName,
             &teacher.englishName,
             &teacher.preferredRomanization,
             &teacher.koreanName
         })
    {
        const QString name = fromValue(*candidate);
        if (!name.isEmpty())
        {
            return name;
        }
    }
    return text("Unassigned");
}

QLabel* addField(
    QGridLayout* layout,
    const int row,
    const int column,
    const char* labelSource,
    QWidget* parent
    )
{
    auto* const fieldLabel = new QLabel(text(labelSource), parent);
    fieldLabel->setObjectName(QStringLiteral("myClassesFieldLabel_%1_%2")
        .arg(row).arg(column));
    fieldLabel->setProperty("translationSource", QString::fromUtf8(labelSource));
    QFont font = fieldLabel->font();
    font.setWeight(QFont::DemiBold);
    fieldLabel->setFont(font);
    auto* const valueLabel = new QLabel(parent);
    valueLabel->setObjectName(QStringLiteral("myClassesFieldValue_%1_%2")
        .arg(row).arg(column));
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(fieldLabel, row * 2, column);
    layout->addWidget(valueLabel, row * 2 + 1, column);
    return valueLabel;
}

void clearTabs(QTabBar* tabs)
{
    while (tabs->count() > 0)
    {
        tabs->removeTab(tabs->count() - 1);
    }
}

QPlainTextEdit* addReadOnlyText(
    QVBoxLayout* layout,
    const char* labelSource,
    const QString& objectName,
    QWidget* parent,
    const int minimumHeight
    )
{
    auto* const heading = new QLabel(text(labelSource), parent);
    heading->setProperty("translationSource", QString::fromUtf8(labelSource));
    QFont font = heading->font();
    font.setWeight(QFont::DemiBold);
    heading->setFont(font);
    auto* const edit = new QPlainTextEdit(parent);
    edit->setObjectName(objectName);
    edit->setReadOnly(true);
    edit->setMinimumHeight(minimumHeight);
    edit->setMaximumHeight(minimumHeight + 38);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(heading);
    layout->addWidget(edit);
    return edit;
}

QString displayTeacherHeading(
    const Application::MyClassesPageTeacherDetails& teacher
    )
{
    const QString name = teacherName(teacher);
    const QString room = fromValue(teacher.roomNumber);
    return room.isEmpty()
        ? name
        : QStringLiteral("%1 - %2 %3").arg(name, text("Room"), room);
}

QString classTitleForSummary(
    const Application::MyClassesPageClassSummary& summary
    )
{
    const QString grade = fromValue(summary.grade);
    const QString level = fromValue(summary.level);
    if (!grade.isEmpty() && !level.isEmpty())
    {
        return QStringLiteral("%1 - %2").arg(grade, level);
    }
    if (!grade.isEmpty() || !level.isEmpty())
    {
        return grade.isEmpty() ? level : grade;
    }
    const QString name = fromValue(summary.className);
    return name.isEmpty()
        ? text("Class %1").arg(QString::fromStdString(summary.classId.value()))
        : name;
}

} // namespace

MyClassesPage::MyClassesPage(
    const PageContext& context,
    MyClassesPageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
    , m_model(new MyClassesSummaryModel(this))
{
    buildUi();
}

MyClassesSummaryModel* MyClassesPage::summaryModel() const noexcept
{
    return m_model;
}

QListView* MyClassesPage::classList() const noexcept
{
    return m_classList;
}

QTabBar* MyClassesPage::gradeTabs() const noexcept
{
    return m_gradeTabs;
}

const std::optional<Domain::ClassId>& MyClassesPage::selectedClassId() const noexcept
{
    return m_selectedClassId;
}

const std::optional<Application::MyClassesPageSelectedDetails>&
MyClassesPage::selectedDetails() const noexcept
{
    return m_details;
}

bool MyClassesPage::detailsReleased() const noexcept
{
    return !m_details.has_value()
        && m_teacherNotes->toPlainText().isEmpty()
        && m_classNotes->toPlainText().isEmpty()
        && m_timeFiller->toPlainText().isEmpty();
}

PageOperationResult<void> MyClassesPage::onActivating()
{
    m_active = true;
    const QString identity = m_actions.workspaceIdentity
        ? m_actions.workspaceIdentity()
        : QString{};
    if (m_workspaceIdentity && *m_workspaceIdentity != identity)
    {
        m_selectedClassId.reset();
        m_details.reset();
        clearDetailFields();
    }
    m_workspaceIdentity = identity;
    QString error;
    if (!refreshFromStore(&error))
    {
        setDetailError(error);
        m_loadState->setText(error);
        m_loadState->setProperty("translationSource", QString{});
    }
    return {};
}

void MyClassesPage::onSuspending() noexcept
{
    m_active = false;
    if (const QModelIndex current = m_classList->currentIndex(); current.isValid())
    {
        if (const auto* summary = m_model->summaryForIndex(current))
        {
            m_selectedClassId = summary->classId;
        }
    }
    m_loading = true;
    {
        const QSignalBlocker gradeSignals(m_gradeTabs);
        const QSignalBlocker selectionSignals(m_classList->selectionModel());
        clearTabs(m_gradeTabs);
        m_gradeTabs->hide();
        m_classList->setRootIndex({});
        m_classList->selectionModel()->clear();
        m_classList->setCurrentIndex({});
    }
    m_details.reset();
    m_model->clear();
    clearDetailFields();
    m_loadState->clear();
    m_loadState->setProperty("translationSource", QString{});
    m_detailPanel->hide();
    m_loading = false;
}

bool MyClassesPage::refreshFromStore(QString* error)
{
    if (!m_actions.readSummaries)
    {
        m_loadState->setProperty(
            "translationSource", QStringLiteral("Class summaries are unavailable.")
            );
        if (error)
        {
            *error = text("Class summaries are unavailable.");
        }
        m_loading = true;
        clearTabs(m_gradeTabs);
        m_gradeTabs->hide();
        m_classList->setRootIndex({});
        m_model->clear();
        clearDetailFields();
        m_detailPanel->hide();
        m_loading = false;
        return false;
    }

    m_loading = true;
    m_details.reset();
    clearDetailFields();
    auto loaded = m_actions.readSummaries();
    if (!loaded)
    {
        m_loading = true;
        clearTabs(m_gradeTabs);
        m_gradeTabs->hide();
        m_classList->setRootIndex({});
        m_model->clear();
        const QString message = errorText(loaded.error().message);
        m_loadState->setProperty("translationSource", QString{});
        m_loadState->setText(message);
        setDetailError(message);
        if (error)
        {
            *error = message;
        }
        m_loading = false;
        m_detailPanel->hide();
        return false;
    }

    m_model->setSummaries(std::move(loaded.value()));
    rebuildGradeTabs(m_selectedClassId);
    const QModelIndex restored = m_classList->currentIndex();
    m_selectedClassId = m_model->summaryForIndex(restored)
        ? std::optional<Domain::ClassId>(
              m_model->summaryForIndex(restored)->classId)
        : std::nullopt;
    m_loading = false;

    if (m_model->summaryCount() == 0)
    {
        m_loadState->setText(text("No classes available."));
        m_loadState->setProperty(
            "translationSource", QStringLiteral("No classes available.")
            );
        m_classTitle->setText(text("No classes available."));
        m_classTitle->setProperty(
            "translationSource", QStringLiteral("No classes available.")
            );
        m_detailPanel->hide();
        if (error)
        {
            error->clear();
        }
        return true;
    }
    m_loadState->clear();
    m_loadState->setProperty("translationSource", QString{});
    m_detailPanel->show();
    activateSelectedClass();
    if (error)
    {
        error->clear();
    }
    return true;
}

void MyClassesPage::clearForClosedWorkspace()
{
    clearForWorkspaceChange();
    m_loadState->setText(text("No Teacher Profile is open."));
    m_loadState->setProperty(
        "translationSource", QStringLiteral("No Teacher Profile is open.")
        );
}

void MyClassesPage::clearForWorkspaceChange()
{
    m_workspaceIdentity.reset();
    m_selectedClassId.reset();
    m_details.reset();
    m_loading = true;
    {
        const QSignalBlocker gradeSignals(m_gradeTabs);
        const QSignalBlocker selectionSignals(m_classList->selectionModel());
        clearTabs(m_gradeTabs);
        m_gradeTabs->hide();
        m_classList->setRootIndex({});
        m_classList->selectionModel()->clear();
        m_classList->setCurrentIndex({});
    }
    m_model->clear();
    clearDetailFields();
    m_detailPanel->hide();
    m_loadState->setText(text("Workspace changed."));
    m_loadState->setProperty("translationSource", QStringLiteral("Workspace changed."));
    m_loading = false;
}

void MyClassesPage::changeEvent(QEvent* event)
{
    Page::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
}

void MyClassesPage::buildUi()
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* const scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("myClassesPageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* const content = new QWidget(scroll);
    auto* const contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(36, 34, 44, 20);
    contentLayout->setSpacing(8);
    contentLayout->setAlignment(Qt::AlignTop);

    m_title = new QLabel(content);
    m_title->setObjectName(QStringLiteral("pageTitle"));
    m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont titleFont = m_title->font();
    titleFont.setPointSize(24);
    titleFont.setWeight(QFont::DemiBold);
    m_title->setFont(titleFont);
    m_subtitle = new QLabel(content);
    m_subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    m_subtitle->setWordWrap(true);
    contentLayout->addWidget(m_title);
    contentLayout->addWidget(m_subtitle);
    contentLayout->addSpacing(10);

    m_loadState = new QLabel(content);
    m_loadState->setObjectName(QStringLiteral("myClassesLoadState"));
    m_loadState->setWordWrap(true);
    contentLayout->addWidget(m_loadState);

    m_gradeTabs = new QTabBar(content);
    m_gradeTabs->setObjectName(QStringLiteral("myClassesGradeTabs"));
    m_gradeTabs->setUsesScrollButtons(true);
    m_gradeTabs->setExpanding(false);
    m_gradeTabs->setElideMode(Qt::ElideRight);
    m_gradeTabs->setDrawBase(false);
    m_gradeTabs->hide();
    m_gradeTabs->setStyleSheet(QStringLiteral(
        "QTabBar::tab { background: palette(button); color: palette(button-text); "
        "border: 1px solid palette(mid); border-radius: 8px; "
        "padding: 7px 14px; margin-right: 6px; }"
        "QTabBar::tab:selected { background: palette(highlight); "
        "color: palette(highlighted-text); }"
        ));
    contentLayout->addWidget(m_gradeTabs);

    auto* const classNavigation = new QHBoxLayout;
    classNavigation->setContentsMargins(0, 0, 0, 0);
    classNavigation->setSpacing(4);
    auto* const leftButton = new QToolButton(content);
    leftButton->setObjectName(QStringLiteral("myClassesClassScrollLeft"));
    leftButton->setArrowType(Qt::LeftArrow);
    leftButton->setAccessibleName(text("Scroll class tabs left"));
    auto* const rightButton = new QToolButton(content);
    rightButton->setObjectName(QStringLiteral("myClassesClassScrollRight"));
    rightButton->setArrowType(Qt::RightArrow);
    rightButton->setAccessibleName(text("Scroll class tabs right"));
    m_classList = new QListView(content);
    m_classList->setObjectName(QStringLiteral("myClassesSummaryList"));
    m_classList->setModel(m_model);
    m_classList->setFlow(QListView::LeftToRight);
    m_classList->setWrapping(false);
    m_classList->setResizeMode(QListView::Adjust);
    m_classList->setSpacing(6);
    m_classList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_classList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_classList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_classList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_classList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_classList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_classList->setMinimumHeight(46);
    m_classList->setMaximumHeight(50);
    m_classList->setStyleSheet(QStringLiteral(
        "QListView#myClassesSummaryList { background: transparent; border: none; }"
        "QListView#myClassesSummaryList::item { background: palette(button); "
        "color: palette(button-text); border: 1px solid palette(mid); "
        "border-radius: 8px; padding: 7px 14px; }"
        "QListView#myClassesSummaryList::item:selected { "
        "background: palette(highlight); color: palette(highlighted-text); }"
        ));
    classNavigation->addWidget(leftButton);
    classNavigation->addWidget(m_classList, 1);
    classNavigation->addWidget(rightButton);
    contentLayout->addLayout(classNavigation);
    contentLayout->addSpacing(8);

    m_detailPanel = new QWidget(content);
    m_detailPanel->setObjectName(QStringLiteral("myClassesSelectedDetails"));
    auto* const detailLayout = new QVBoxLayout(m_detailPanel);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(12);

    m_detailError = new QLabel(m_detailPanel);
    m_detailError->setObjectName(QStringLiteral("myClassesDetailError"));
    m_detailError->setWordWrap(true);
    m_detailError->hide();
    detailLayout->addWidget(m_detailError);

    m_teacherTitle = new QLabel(m_detailPanel);
    m_teacherTitle->setObjectName(QStringLiteral("myClassesTeacherTitle"));
    QFont sectionFont = m_teacherTitle->font();
    sectionFont.setPointSize(16);
    sectionFont.setWeight(QFont::DemiBold);
    m_teacherTitle->setFont(sectionFont);
    detailLayout->addWidget(m_teacherTitle);

    auto* const teacherCard = new QFrame(m_detailPanel);
    teacherCard->setObjectName(QStringLiteral("myClassesTeacherCard"));
    teacherCard->setProperty("role", QStringLiteral("card"));
    auto* const teacherLayout = new QVBoxLayout(teacherCard);
    teacherLayout->setContentsMargins(18, 14, 18, 14);
    teacherLayout->setSpacing(8);
    auto* const teacherFields = new QGridLayout;
    teacherFields->setHorizontalSpacing(16);
    teacherFields->setVerticalSpacing(8);
    teacherFields->setColumnStretch(0, 1);
    teacherFields->setColumnStretch(1, 1);
    teacherFields->setColumnStretch(2, 1);
    m_internetType = addField(teacherFields, 0, 0, "Internet Type", teacherCard);
    m_wifiName = addField(teacherFields, 0, 1, "WiFi Name", teacherCard);
    m_wifiPassword = addField(teacherFields, 0, 2, "WiFi Password", teacherCard);
    m_projectionType = addField(teacherFields, 1, 0, "Projection Type", teacherCard);
    m_zoomId = addField(teacherFields, 1, 1, "Zoom ID", teacherCard);
    m_zoomPassword = addField(teacherFields, 1, 2, "Zoom Password", teacherCard);
    teacherLayout->addLayout(teacherFields);
    m_teacherNotes = addReadOnlyText(
        teacherLayout, "Notes", QStringLiteral("myClassesTeacherNotes"),
        teacherCard, 116
        );
    detailLayout->addWidget(teacherCard);

    m_classTitle = new QLabel(m_detailPanel);
    m_classTitle->setObjectName(QStringLiteral("myClassesSelectedClassTitle"));
    QFont classFont = m_classTitle->font();
    classFont.setPointSize(16);
    classFont.setWeight(QFont::DemiBold);
    m_classTitle->setFont(classFont);
    m_classTitle->setWordWrap(true);
    detailLayout->addWidget(m_classTitle);

    auto* const classCard = new QFrame(m_detailPanel);
    classCard->setObjectName(QStringLiteral("myClassesClassCard"));
    classCard->setProperty("role", QStringLiteral("card"));
    auto* const classLayout = new QVBoxLayout(classCard);
    classLayout->setContentsMargins(18, 14, 18, 14);
    classLayout->setSpacing(8);
    m_classFields = new QGridLayout;
    m_classFields->setHorizontalSpacing(18);
    m_classFields->setVerticalSpacing(6);
    m_classFields->setColumnStretch(0, 0);
    m_classFields->setColumnStretch(1, 1);

    auto addClassField = [this](
        const char* labelSource,
        const QString& valueObjectName,
        const int row,
        QLabel** labelOutput
        ) {
        auto* const fieldLabel = new QLabel(text(labelSource), m_detailPanel);
        fieldLabel->setProperty("translationSource", QString::fromUtf8(labelSource));
        fieldLabel->setObjectName(valueObjectName + QStringLiteral("Label"));
        auto* const fieldValue = new QLabel(m_detailPanel);
        fieldValue->setObjectName(valueObjectName);
        fieldValue->setWordWrap(true);
        fieldValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_classFields->addWidget(fieldLabel, row, 0, Qt::AlignTop);
        m_classFields->addWidget(fieldValue, row, 1, Qt::AlignTop);
        if (labelOutput)
        {
            *labelOutput = fieldLabel;
        }
        return fieldValue;
    };
    m_studentCount = addClassField(
        "# of Students", QStringLiteral("myClassesStudentCount"), 0, nullptr);
    m_regularSchedule = addClassField(
        "Regular", QStringLiteral("myClassesRegularSchedule"), 1,
        &m_regularScheduleLabel);
    m_intensiveSchedule = addClassField(
        "Intensive", QStringLiteral("myClassesIntensiveSchedule"), 2,
        &m_intensiveScheduleLabel);
    m_emptySchedule = addClassField(
        "Schedule", QStringLiteral("myClassesEmptySchedule"), 1,
        &m_emptyScheduleLabel);
    classLayout->addLayout(m_classFields);
    m_classNotes = addReadOnlyText(
        classLayout, "Class Notes", QStringLiteral("myClassesClassNotes"),
        classCard, 116
        );
    m_timeFiller = addReadOnlyText(
        classLayout, "Time Filler Activities",
        QStringLiteral("myClassesTimeFillerActivities"), classCard, 76
        );
    detailLayout->addWidget(classCard);
    contentLayout->addWidget(m_detailPanel);
    contentLayout->addStretch(1);

    scroll->setWidget(content);
    root->addWidget(scroll);

    auto* const scrollBar = m_classList->horizontalScrollBar();
    auto updateScrollButtons = [scrollBar, leftButton, rightButton] {
        leftButton->setEnabled(scrollBar->value() > scrollBar->minimum());
        rightButton->setEnabled(scrollBar->value() < scrollBar->maximum());
    };
    connect(leftButton, &QToolButton::clicked, this, [scrollBar] {
        scrollBar->setValue(scrollBar->value() - qMax(80, scrollBar->pageStep() / 2));
    });
    connect(rightButton, &QToolButton::clicked, this, [scrollBar] {
        scrollBar->setValue(scrollBar->value() + qMax(80, scrollBar->pageStep() / 2));
    });
    connect(scrollBar, &QScrollBar::valueChanged, this,
            [updateScrollButtons](int) { updateScrollButtons(); });
    connect(scrollBar, &QScrollBar::rangeChanged, this,
            [updateScrollButtons](int, int) { updateScrollButtons(); });
    connect(m_gradeTabs, &QTabBar::currentChanged,
            this, &MyClassesPage::handleGradeChanged);
    connect(m_classList->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this](const QModelIndex&, const QModelIndex&) {
            if (!m_loading && !m_retranslating && m_active)
            {
                activateSelectedClass();
            }
        });
    retranslateUi();
    clearDetailFields();
    m_detailPanel->hide();
}

void MyClassesPage::retranslateUi()
{
    m_title->setText(text("Class Information"));
    m_subtitle->setText(text("Review teacher, class, and roster details."));
    m_classList->setAccessibleName(text("Class summaries"));
    m_classList->setToolTip(text("Select a class to view its details."));
    for (QLabel* const label : findChildren<QLabel*>())
    {
        const QString source = label->property("translationSource").toString();
        if (!source.isEmpty())
        {
            label->setText(text(source.toUtf8().constData()));
        }
    }
    const QString loadStateSource =
        m_loadState->property("translationSource").toString();
    if (!loadStateSource.isEmpty())
    {
        m_loadState->setText(text(loadStateSource.toUtf8().constData()));
    }
    std::optional<Domain::ClassId> selected;
    if (const QModelIndex current = m_classList->currentIndex(); current.isValid())
    {
        if (const auto* summary = m_model->summaryForIndex(current))
        {
            selected = summary->classId;
        }
    }
    m_retranslating = true;
    m_model->retranslate();
    rebuildGradeTabs(selected);
    m_retranslating = false;
    if (m_details)
    {
        showSelectedDetails(*m_details);
    }
    else if (const auto* const summary =
            m_model->summaryForIndex(m_classList->currentIndex()))
    {
        m_classTitle->setText(classTitleForSummary(*summary));
    }
    else
    {
        const QString source = m_classTitle->property("translationSource").toString();
        if (!source.isEmpty())
        {
            m_classTitle->setText(text(source.toUtf8().constData()));
        }
    }
}

void MyClassesPage::activateSelectedClass()
{
    const QModelIndex current = m_classList->currentIndex();
    const auto* const summary = m_model->summaryForIndex(current);
    if (!summary)
    {
        m_selectedClassId.reset();
        m_details.reset();
        clearDetailFields();
        return;
    }

    m_selectedClassId = summary->classId;
    m_details.reset();
    clearDetailFields();
    m_classTitle->setText(classTitleForSummary(*summary));
    m_classTitle->setProperty("translationSource", QString{});
    if (!m_actions.readSelectedDetails)
    {
        setDetailError(text("Selected class details are unavailable."));
        return;
    }
    const auto details = m_actions.readSelectedDetails(summary->classId);
    if (!details)
    {
        setDetailError(errorText(details.error().message));
        return;
    }
    m_details = details.value();
    showSelectedDetails(*m_details);
}

void MyClassesPage::showSelectedDetails(
    const Application::MyClassesPageSelectedDetails& details
    )
{
    const QString grade = fromValue(details.grade);
    const QString level = fromValue(details.level);
    const QString className = fromValue(details.className);
    QString title;
    if (!grade.isEmpty() && !level.isEmpty())
    {
        title = QStringLiteral("%1 - %2").arg(grade, level);
    }
    else if (!grade.isEmpty() || !level.isEmpty())
    {
        title = grade.isEmpty() ? level : grade;
    }
    else
    {
        title = className.isEmpty()
            ? text("Class %1").arg(QString::fromStdString(details.classId.value()))
            : className;
    }
    m_classTitle->setText(title);
    m_classTitle->setProperty("translationSource", QString{});
    m_studentCount->setText(details.studentCount
        ? QString::number(*details.studentCount)
        : text("Unavailable"));
    m_regularSchedule->setText(formatSchedule(details.regularSchedule));
    m_intensiveSchedule->setText(formatSchedule(details.intensiveSchedule));
    const bool hasRegularSchedule = !details.regularSchedule.empty();
    const bool hasIntensiveSchedule = !details.intensiveSchedule.empty();
    m_regularScheduleLabel->setVisible(hasRegularSchedule);
    m_regularSchedule->setVisible(hasRegularSchedule);
    m_intensiveScheduleLabel->setVisible(hasIntensiveSchedule);
    m_intensiveSchedule->setVisible(hasIntensiveSchedule);
    m_emptyScheduleLabel->setVisible(
        !hasRegularSchedule && !hasIntensiveSchedule);
    m_emptySchedule->setVisible(!hasRegularSchedule && !hasIntensiveSchedule);
    m_emptySchedule->setText(text("N/A"));
    m_classNotes->setPlainText(fromValue(details.classNotes));
    m_timeFiller->setPlainText(fromValue(details.timeFillerActivities).isEmpty()
        ? text("N/A")
        : fromValue(details.timeFillerActivities));

    if (!details.teacherId || !details.teacher)
    {
        m_teacherTitle->setText(text("Unassigned"));
        return;
    }
    const auto& teacher = *details.teacher;
    m_teacherTitle->setText(displayTeacherHeading(teacher));
    m_internetType->setText(fromValue(teacher.internetType));
    m_wifiName->setText(fromValue(teacher.wifiName));
    m_wifiPassword->setText(fromValue(teacher.wifiPassword));
    m_projectionType->setText(fromValue(teacher.projectionType));
    m_zoomId->setText(fromValue(teacher.zoomId));
    m_zoomPassword->setText(fromValue(teacher.zoomPassword));
    m_teacherNotes->setPlainText(fromValue(teacher.notes));
}

void MyClassesPage::clearDetailFields()
{
    if (m_classTitle)
    {
        m_classTitle->setText(text("Select a class."));
        m_classTitle->setProperty(
            "translationSource", QStringLiteral("Select a class.")
            );
        m_detailError->clear();
        m_detailError->hide();
        m_teacherTitle->clear();
        m_studentCount->clear();
        m_regularSchedule->clear();
        m_intensiveSchedule->clear();
        m_regularScheduleLabel->hide();
        m_regularSchedule->hide();
        m_intensiveScheduleLabel->hide();
        m_intensiveSchedule->hide();
        m_emptyScheduleLabel->hide();
        m_emptySchedule->hide();
        m_internetType->clear();
        m_wifiName->clear();
        m_wifiPassword->clear();
        m_projectionType->clear();
        m_zoomId->clear();
        m_zoomPassword->clear();
        m_teacherNotes->clear();
        m_classNotes->clear();
        m_timeFiller->clear();
    }
}

void MyClassesPage::setDetailError(const QString& error)
{
    m_detailError->setText(error);
    m_detailError->setVisible(!error.isEmpty());
}

QModelIndex MyClassesPage::firstClassIndexForGrade(const int gradeRow) const
{
    if (gradeRow < 0 || gradeRow >= m_model->rowCount())
    {
        return {};
    }
    const QModelIndex gradeIndex = m_model->index(gradeRow, 0);
    return m_model->rowCount(gradeIndex) > 0
        ? m_model->index(0, 0, gradeIndex)
        : QModelIndex{};
}

void MyClassesPage::rebuildGradeTabs(
    const std::optional<Domain::ClassId>& preferredClass
    )
{
    const QSignalBlocker gradeSignals(m_gradeTabs);
    const QSignalBlocker selectionSignals(m_classList->selectionModel());
    clearTabs(m_gradeTabs);
    if (!m_model->isGrouped())
    {
        m_gradeTabs->hide();
        m_classList->setRootIndex({});
        QModelIndex selected;
        if (preferredClass)
        {
            selected = m_model->indexForClassId(*preferredClass);
        }
        if (!selected.isValid() && m_model->rowCount() > 0)
        {
            selected = m_model->index(0, 0);
        }
        m_classList->selectionModel()->clear();
        m_classList->setCurrentIndex(selected);
        if (selected.isValid())
        {
            m_classList->selectionModel()->select(
                selected,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_classList->scrollTo(selected);
        }
        return;
    }
    m_gradeTabs->show();
    int preferredGrade = -1;
    if (preferredClass)
    {
        const QModelIndex preferredIndex = m_model->indexForClassId(*preferredClass);
        if (preferredIndex.isValid() && preferredIndex.parent().isValid())
        {
            preferredGrade = preferredIndex.parent().row();
        }
    }
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        const QModelIndex gradeIndex = m_model->index(row, 0);
        m_gradeTabs->addTab(m_model->data(gradeIndex, Qt::DisplayRole).toString());
    }
    const int grade = preferredGrade >= 0
        ? preferredGrade
        : (m_gradeTabs->count() > 0 ? 0 : -1);
    m_gradeTabs->setCurrentIndex(grade);
    rebuildClassList(preferredClass);
}

void MyClassesPage::rebuildClassList(
    const std::optional<Domain::ClassId>& preferredClass
    )
{
    const QSignalBlocker selectionSignals(m_classList->selectionModel());
    if (!m_model->isGrouped())
    {
        m_classList->setRootIndex({});
        QModelIndex selected;
        if (preferredClass)
        {
            selected = m_model->indexForClassId(*preferredClass);
        }
        if (!selected.isValid() && m_model->rowCount() > 0)
        {
            selected = m_model->index(0, 0);
        }
        m_classList->selectionModel()->clear();
        m_classList->setCurrentIndex(selected);
        if (selected.isValid())
        {
            m_classList->selectionModel()->select(
                selected,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_classList->scrollTo(selected);
        }
        return;
    }
    const int gradeRow = m_gradeTabs->currentIndex();
    if (gradeRow < 0 || gradeRow >= m_model->rowCount())
    {
        m_classList->setRootIndex({});
        m_classList->selectionModel()->clear();
        m_classList->setCurrentIndex({});
        return;
    }
    const QModelIndex gradeIndex = m_model->index(gradeRow, 0);
    m_classList->setRootIndex(gradeIndex);
    QModelIndex selected;
    if (preferredClass)
    {
        const QModelIndex candidate = m_model->indexForClassId(*preferredClass);
        if (candidate.isValid() && candidate.parent() == gradeIndex)
        {
            selected = candidate;
        }
    }
    if (!selected.isValid())
    {
        selected = firstClassIndexForGrade(gradeRow);
    }
    m_classList->selectionModel()->clear();
    m_classList->setCurrentIndex(selected);
    if (selected.isValid())
    {
        m_classList->selectionModel()->select(
            selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_classList->scrollTo(selected);
    }
}

void MyClassesPage::handleGradeChanged(const int index)
{
    if (!m_model->isGrouped() || m_loading
        || index < 0 || index >= m_model->rowCount())
    {
        return;
    }
    rebuildClassList(std::nullopt);
    if (!m_retranslating && m_active)
    {
        activateSelectedClass();
    }
}

} // namespace ClassMngr::Next::Ui
