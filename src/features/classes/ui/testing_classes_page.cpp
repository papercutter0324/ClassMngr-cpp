#include "testing_classes_page.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/fontmanager.h"
#include "core/utils/colorutils.h"
#include "domain/models/classroom.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/clickable_color_preview.h"
#include "ui/shared/widgets/marquee_item_delegate.h"
#include "ui/shared/widgets/no_wheel_combobox.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"

#include <algorithm>
#include <utility>

#include <QColor>
#include <QComboBox>
#include <QFormLayout>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
constexpr int TeacherRoomRole = Qt::UserRole + 1;

QString classListLabel(
    const TestingClass& testingClass
    )
{
    return QStringLiteral("%1 — %2 — %3")
        .arg(
            testingClass.name,
            testingClass.grade,
            testingClass.level
            );
}

int gradeOrder(
    const QString& grade
    )
{
    const int index =
        testingClassGrades().indexOf(
            grade
            );
    return index >= 0
        ? index
        : 1000;
}

int levelOrder(
    const QString& grade,
    const QString& level
    )
{
    const int index =
        testingClassLevelsForGrade(grade)
            .indexOf(level);
    return index >= 0
        ? index
        : 1000;
}
}

TestingClassesPage::TestingClassesPage(
    ApplicationServices* services,
    QWidget* parent
    )
    : BasePage(parent)
    , m_services(services)
    , m_autosave(new AutosaveCoordinator(this))
{
    buildUi();
    m_autosave->bindSaveButton(m_saveButton);
    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool) { saveChanges(); }
        );
    populateTeachers();
    rebuildClassList();
}

void TestingClassesPage::openTestingClass(
    int classId,
    const QString& pendingDay,
    const QString& pendingStartTime
    )
{
    populateTeachers();
    rebuildClassList(classId);

    if (classId > 0)
    {
        loadClass(classId);
        return;
    }

    if (
        !pendingDay.trimmed().isEmpty()
        && !pendingStartTime.trimmed().isEmpty()
        )
    {
        beginNewClass(
            pendingDay,
            pendingStartTime
            );
        return;
    }

    if (m_classList->count() > 0)
    {
        m_classList->setCurrentRow(0);
        loadClass(
            m_classList
                ->currentItem()
                ->data(Qt::UserRole)
                .toInt()
            );
    }
    else
    {
        beginNewClass();
    }
}

void TestingClassesPage::refresh()
{
    BasePage::refresh();

    if (isVisible())
    {
        populateTeachers();
        if (hasUnsavedChanges())
        {
            return;
        }
        rebuildClassList(m_currentClassId);
        if (m_currentClassId > 0)
        {
            loadClass(m_currentClassId);
        }
    }
}

void TestingClassesPage::clearDatabaseState()
{
    m_autosave->setLoading(true);
    m_currentClassId = -1;
    m_savedClass = {};
    m_pendingDay.clear();
    m_pendingStartTime.clear();
    m_editorDirty = false;
    m_classList->clear();
    loadEditorValue({});
    m_rosterEditor->clearDatabaseState();
    m_autosave->setLoading(false);
    m_autosave->markClean();
    updateActions();
}

void TestingClassesPage::retranslateUi()
{
    m_pageHeader->setTitle(tr("Testing Classes"));
    m_pageHeader->setSubtitle(
        tr("Create reusable classes for the weekly Testing layout.")
        );
    m_backButton->setText(tr("Back to Testing Schedule"));
    m_addButton->setText(tr("Add Class"));
    m_deleteButton->setText(tr("Delete Class"));
    m_saveButton->setText(tr("Save Changes"));
    m_tabs->setTabText(0, tr("Details"));
    m_tabs->setTabText(1, tr("Roster"));
    m_tabs->setTabText(2, tr("Notes"));
    m_nameLabel->setText(tr("Class Name"));
    m_gradeLevelLabel->setText(tr("Grade / Level"));
    m_roomLabel->setText(tr("Room"));
    m_teacherLabel->setText(tr("Korean Teacher"));
    m_classColorLabel->setText(tr("Class Color"));
    m_fontColorLabel->setText(tr("Font Color"));
    m_classColorButton->setText(tr("Choose Color"));
    m_fontColorButton->setText(tr("Choose Color"));
    m_nameEdit->setPlaceholderText(
        tr("Shown on schedules and roster printouts")
        );
    m_nameEdit->setToolTip(
        tr("This name identifies the class on the Testing schedule and its roster printout.")
        );
}

PageOutputCapabilities TestingClassesPage::outputCapabilities() const
{
    if (
        !isDatabaseOpen()
        || !m_tabs
        || m_tabs->currentWidget() != m_rosterEditor
        || !m_rosterEditor
        )
    {
        return {};
    }

    return m_rosterEditor->outputCapabilities();
}

void TestingClassesPage::printCurrentPage()
{
    if (outputCapabilities().printEnabled)
    {
        m_rosterEditor->printCurrentPage();
    }
}

void TestingClassesPage::saveCurrentPageAs()
{
    if (outputCapabilities().saveAsEnabled)
    {
        m_rosterEditor->saveCurrentPageAs();
    }
}

void TestingClassesPage::saveData()
{
    saveChanges();
}

bool TestingClassesPage::saveChanges()
{
    if (
        m_rosterEditor->hasUnsavedChanges()
        && !m_rosterEditor->saveChanges()
        )
    {
        return false;
    }

    if (!m_editorDirty)
    {
        return true;
    }

    auto* scheduleService = m_services ? m_services->scheduleService() : nullptr;
    if (!scheduleService || !scheduleService->isAvailable())
    {
        return false;
    }

    TestingClass testingClass =
        editorValue();
    Result<int> created =
        std::unexpected(QString());

    if (m_currentClassId <= 0)
    {
        created =
            scheduleService->createTestingClass(
                testingClass,
                m_pendingDay,
                m_pendingStartTime
                );
        if (!created)
        {
            QMessageBox::warning(
                this,
                tr("Save Testing Class"),
                created.error()
                );
            return false;
        }

        testingClass.classId = *created;
    }
    else
    {
        testingClass.classId =
            m_currentClassId;
        const Status updated =
            scheduleService->updateTestingClass(
                testingClass
                );
        if (!updated)
        {
            QMessageBox::warning(
                this,
                tr("Save Testing Class"),
                updated.error()
                );
            return false;
        }
    }

    m_currentClassId =
        testingClass.classId;
    m_savedClass =
        testingClass;
    m_pendingDay.clear();
    m_pendingStartTime.clear();
    m_editorDirty = false;
    m_autosave->markClean();

    Classroom classroom(
        testingClass.name,
        testingClass.classId
        );
    m_rosterEditor->loadClass(classroom);
    rebuildClassList(m_currentClassId);
    updateActions();
    emit testingDataChanged();
    return true;
}

bool TestingClassesPage::hasUnsavedChanges() const
{
    return m_editorDirty
        || (
            m_rosterEditor
            && m_rosterEditor->hasUnsavedChanges()
            );
}

void TestingClassesPage::discardChanges()
{
    m_autosave->cancelPendingSave();
    if (m_currentClassId > 0)
    {
        loadClass(m_currentClassId);
    }
    else
    {
        beginNewClass();
    }
}

QString TestingClassesPage::unsavedChangesTitle() const
{
    return tr("Unsaved Testing Class Changes");
}

QString TestingClassesPage::unsavedChangesMessage() const
{
    return tr("This testing class has unsaved changes.");
}

void TestingClassesPage::setSaveMode(
    SaveMode mode
    )
{
    m_autosave->setSaveMode(mode);
    m_rosterEditor->setSaveMode(mode);
    updateActions();
}

void TestingClassesPage::buildUi()
{
    contentLayout()->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        0
        );
    contentLayout()->setSpacing(
        UiConstants::Pages::Spacing
        );

    auto* header =
        new QHBoxLayout;
    m_pageHeader = new PageHeader(
        tr("Testing Classes"),
        tr("Create reusable classes for the weekly Testing layout."),
        this
        );
    header->addWidget(m_pageHeader, 1);

    m_backButton =
        new TextFitPushButton(
            tr("Back to Testing Schedule"),
            this
            );
    m_backButton->setObjectName(
        QStringLiteral("testingClassesBackButton")
        );
    header->addWidget(m_backButton);
    contentLayout()->addLayout(header);

    auto* splitter =
        new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(
        QStringLiteral("testingClassesSplitter")
        );
    splitter->setChildrenCollapsible(false);

    auto* navigation =
        new QWidget(splitter);
    navigation->setObjectName(
        QStringLiteral("testingClassesNavigation")
        );
    auto* navigationLayout =
        new QVBoxLayout(navigation);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    m_classList =
        new QListWidget(navigation);
    m_classList->setObjectName(
        QStringLiteral("testingClassesList")
        );
    m_classList->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded
        );
    m_classList->setHorizontalScrollMode(
        QAbstractItemView::ScrollPerPixel
        );
    m_classList->setTextElideMode(Qt::ElideNone);
    m_classList->setWordWrap(false);
    m_classList->setMinimumWidth(0);
    m_classList->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Expanding
        );
    m_classList->setItemDelegate(
        new MarqueeItemDelegate(
            m_classList,
            m_classList
            )
        );
    navigationLayout->addWidget(m_classList, 1);

    auto* listButtons =
        new QVBoxLayout;
    m_addButton =
        new TextFitPushButton(
            tr("Add Class"),
            navigation
            );
    m_deleteButton =
        new TextFitPushButton(
            tr("Delete Class"),
            navigation
            );
    m_addButton->setObjectName(
        QStringLiteral("testingClassesAddButton")
        );
    m_deleteButton->setObjectName(
        QStringLiteral("testingClassesDeleteButton")
        );
    listButtons->addWidget(m_addButton);
    listButtons->addWidget(m_deleteButton);
    navigationLayout->addLayout(listButtons);

    m_tabs =
        new QTabWidget(splitter);
    m_tabs->setObjectName(
        QStringLiteral("testingClassesTabs")
        );

    auto* detailsPage =
        new QWidget(m_tabs);
    auto* detailsLayout =
        new QVBoxLayout(detailsPage);
    auto* form =
        new QFormLayout;
    form->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow
        );
    m_nameEdit =
        new QLineEdit(detailsPage);
    m_nameEdit->setObjectName(
        QStringLiteral("testingClassNameEdit")
        );
    m_nameEdit->setText(
        QStringLiteral("Testing Class")
        );
    m_nameEdit->setPlaceholderText(
        tr("Shown on schedules and roster printouts")
        );
    m_nameEdit->setToolTip(
        tr("This name identifies the class on the Testing schedule and its roster printout.")
        );
    m_gradeCombo =
        new NoWheelComboBox(detailsPage);
    m_gradeCombo->setObjectName(
        QStringLiteral("testingClassGradeCombo")
        );
    m_gradeCombo->addItems(
        testingClassGrades()
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_gradeCombo,
        WidgetSizing::comboMinimumWidthForTexts(
            m_gradeCombo,
            testingClassGrades(),
            UiConstants::ClassInfo::TextWidthPadding
            ),
        QSizePolicy::Fixed,
        true
        );
    m_levelCombo =
        new NoWheelComboBox(detailsPage);
    m_levelCombo->setObjectName(
        QStringLiteral("testingClassLevelCombo")
        );
    QStringList allTestingLevels =
        testingClassLevelsForGrade(
            QStringLiteral("M1")
            );
    for (
        const QString& level :
        testingClassLevelsForGrade(
            QStringLiteral("M2")
            )
        )
    {
        if (!allTestingLevels.contains(level))
        {
            allTestingLevels.append(level);
        }
    }
    WidgetSizing::installTextAwareFieldWidth(
        m_levelCombo,
        WidgetSizing::comboMinimumWidthForTexts(
            m_levelCombo,
            allTestingLevels,
            UiConstants::ClassInfo::TextWidthPadding
            )
        );
    m_roomEdit =
        new QLineEdit(detailsPage);
    m_roomEdit->setObjectName(
        QStringLiteral("testingClassRoomEdit")
        );
    m_teacherCombo =
        new NoWheelComboBox(detailsPage);
    m_teacherCombo->setObjectName(
        QStringLiteral("testingClassTeacherCombo")
        );
    m_classColorPreview =
        new ClickableColorPreview(detailsPage);
    m_classColorPreview->setObjectName(
        QStringLiteral("testingClassColorPreview")
        );
    m_classColorPreview->setFixedSize(28, 28);
    m_classColorButton =
        new TextFitPushButton(
            tr("Choose Color"),
            detailsPage
            );
    m_classColorButton->setObjectName(
        QStringLiteral("testingClassColorButton")
        );
    auto* classColorField =
        new QWidget(detailsPage);
    auto* classColorLayout =
        new QHBoxLayout(classColorField);
    classColorLayout->setContentsMargins(0, 0, 0, 0);
    classColorLayout->setSpacing(8);
    classColorLayout->addWidget(m_classColorPreview);
    classColorLayout->addWidget(m_classColorButton);
    classColorLayout->addStretch();

    m_fontColorPreview =
        new ClickableColorPreview(detailsPage);
    m_fontColorPreview->setObjectName(
        QStringLiteral("testingClassFontColorPreview")
        );
    m_fontColorPreview->setFixedSize(28, 28);
    m_fontColorButton =
        new TextFitPushButton(
            tr("Choose Color"),
            detailsPage
            );
    m_fontColorButton->setObjectName(
        QStringLiteral("testingClassFontColorButton")
        );
    auto* fontColorField =
        new QWidget(detailsPage);
    auto* fontColorLayout =
        new QHBoxLayout(fontColorField);
    fontColorLayout->setContentsMargins(0, 0, 0, 0);
    fontColorLayout->setSpacing(8);
    fontColorLayout->addWidget(m_fontColorPreview);
    fontColorLayout->addWidget(m_fontColorButton);
    fontColorLayout->addStretch();

    auto* gradeLevelField =
        new QWidget(detailsPage);
    auto* gradeLevelLayout =
        new QHBoxLayout(gradeLevelField);
    gradeLevelLayout->setContentsMargins(0, 0, 0, 0);
    gradeLevelLayout->setSpacing(8);
    gradeLevelLayout->addWidget(m_gradeCombo);
    gradeLevelLayout->addWidget(m_levelCombo, 1);

    m_nameLabel =
        new QLabel(tr("Class Name"), detailsPage);
    m_gradeLevelLabel =
        new QLabel(tr("Grade / Level"), detailsPage);
    m_roomLabel =
        new QLabel(tr("Room"), detailsPage);
    m_teacherLabel =
        new QLabel(tr("Korean Teacher"), detailsPage);
    m_classColorLabel =
        new QLabel(tr("Class Color"), detailsPage);
    m_fontColorLabel =
        new QLabel(tr("Font Color"), detailsPage);

    form->addRow(m_teacherLabel, m_teacherCombo);
    form->addRow(m_roomLabel, m_roomEdit);
    form->addRow(m_nameLabel, m_nameEdit);
    form->addRow(m_gradeLevelLabel, gradeLevelField);
    form->addRow(m_classColorLabel, classColorField);
    form->addRow(m_fontColorLabel, fontColorField);
    detailsLayout->addLayout(form);

    m_rosterEditor =
        new RosterEditorWidget(
            m_services,
            true,
            m_tabs
            );
    m_rosterEditor->setTestingClassMode(true);
    connect(
        m_rosterEditor,
        &BasePage::outputCapabilitiesChanged,
        this,
        &BasePage::outputCapabilitiesChanged
        );
    connect(
        m_rosterEditor,
        &RosterEditorWidget::unsavedChangesChanged,
        this,
        [this](bool) { updateActions(); }
        );

    auto* notesPage =
        new QWidget(m_tabs);
    auto* notesLayout =
        new QVBoxLayout(notesPage);
    m_notesEdit =
        new QTextEdit(notesPage);
    m_notesEdit->setObjectName(
        QStringLiteral("testingClassNotesEdit")
        );
    m_notesEdit->setPlaceholderText(
        tr("Instructions, accommodations, and other notes")
        );
    notesLayout->addWidget(m_notesEdit);

    m_tabs->addTab(detailsPage, tr("Details"));
    m_tabs->addTab(m_rosterEditor, tr("Roster"));
    m_tabs->addTab(notesPage, tr("Notes"));
    connect(
        m_tabs,
        &QTabWidget::currentChanged,
        this,
        [this](int)
        {
            emit outputCapabilitiesChanged();
        }
        );

    splitter->addWidget(navigation);
    splitter->addWidget(m_tabs);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 760});
    contentLayout()->addWidget(splitter, 1);

    m_saveButton =
        new TextFitPushButton(
            tr("Save Changes"),
            this
            );
    m_saveButton->setObjectName(
        QStringLiteral("testingClassesSaveButton")
        );
    bottomLayout()->addStretch(1);
    bottomLayout()->addWidget(m_saveButton);

    connect(
        m_backButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            emit returnToScheduleRequested();
        }
        );
    connect(
        m_addButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            if (!hasUnsavedChanges() || saveChanges())
            {
                beginNewClass();
            }
        }
        );
    connect(
        m_deleteButton,
        &QPushButton::clicked,
        this,
        &TestingClassesPage::deleteCurrentClass
        );
    connect(
        m_gradeCombo,
        &QComboBox::currentTextChanged,
        this,
        [this]()
        {
            updateLevelOptions();
            markDirty();
        }
        );
    for (QObject* editor : {
             static_cast<QObject*>(m_nameEdit),
             static_cast<QObject*>(m_roomEdit)
         })
    {
        auto* lineEdit =
            qobject_cast<QLineEdit*>(editor);
        connect(
            lineEdit,
            &QLineEdit::textChanged,
            this,
            &TestingClassesPage::markDirty
            );
    }
    connect(
        m_levelCombo,
        &QComboBox::currentTextChanged,
        this,
        &TestingClassesPage::markDirty
        );
    connect(
        m_teacherCombo,
        &QComboBox::currentIndexChanged,
        this,
        [this]()
        {
            if (m_autosave->isLoading())
            {
                return;
            }

            m_roomEdit->setText(
                m_teacherCombo
                    ->currentData(TeacherRoomRole)
                    .toString()
                    .trimmed()
                );
            markDirty();
        }
        );
    connect(
        m_notesEdit,
        &QTextEdit::textChanged,
        this,
        &TestingClassesPage::markDirty
        );
    connect(
        m_classColorButton,
        &QPushButton::clicked,
        this,
        &TestingClassesPage::chooseClassColor
        );
    connect(
        m_fontColorButton,
        &QPushButton::clicked,
        this,
        &TestingClassesPage::chooseFontColor
        );
    connect(
        m_classColorPreview,
        &ClickableColorPreview::clicked,
        this,
        &TestingClassesPage::chooseClassColor
        );
    connect(
        m_fontColorPreview,
        &ClickableColorPreview::clicked,
        this,
        &TestingClassesPage::chooseFontColor
        );
    connect(
        m_classList,
        &QListWidget::currentItemChanged,
        this,
        [this](QListWidgetItem* current, QListWidgetItem* previous)
        {
            if (m_autosave->isLoading() || !current)
            {
                return;
            }
            if (hasUnsavedChanges() && !saveChanges())
            {
                const QSignalBlocker blocker(m_classList);
                m_classList->setCurrentItem(previous);
                return;
            }
            loadClass(
                current->data(Qt::UserRole).toInt()
                );
        }
        );
    updateLevelOptions();
    updateColorButtons();
    updateActions();
}

void TestingClassesPage::populateTeachers()
{
    const int selectedId =
        m_teacherCombo->currentData().toInt();
    const QSignalBlocker blocker(m_teacherCombo);
    m_teacherCombo->clear();
    m_teacherCombo->addItem(
        tr("None"),
        -1
        );
    m_teacherCombo->setItemData(
        0,
        QString(),
        TeacherRoomRole
        );

    auto* teacherService = m_services ? m_services->teacherService() : nullptr;
    if (teacherService && teacherService->isAvailable())
    {
        for (const Teacher& teacher : teacherService->teachers())
        {
            const QString label =
                teacher.teacherKr.trimmed();
            if (label.isEmpty())
            {
                continue;
            }
            m_teacherCombo->addItem(label, teacher.id);
            m_teacherCombo->setItemData(
                m_teacherCombo->count() - 1,
                teacher.roomNumber.trimmed(),
                TeacherRoomRole
                );
        }
    }

    const int selectedIndex =
        m_teacherCombo->findData(selectedId);
    m_teacherCombo->setCurrentIndex(
        selectedIndex >= 0
            ? selectedIndex
            : 0
        );
}

void TestingClassesPage::updateLevelOptions()
{
    const QString selected =
        m_levelCombo->currentText();
    const QSignalBlocker blocker(m_levelCombo);
    m_levelCombo->clear();
    m_levelCombo->addItems(
        testingClassLevelsForGrade(
            m_gradeCombo->currentText()
            )
        );

    const int index =
        m_levelCombo->findText(selected);
    if (index >= 0)
    {
        m_levelCombo->setCurrentIndex(index);
    }
}

void TestingClassesPage::rebuildClassList(
    int preferredClassId
    )
{
    auto* scheduleService = m_services ? m_services->scheduleService() : nullptr;
    const QSignalBlocker blocker(m_classList);
    m_classList->clear();
    if (!scheduleService || !scheduleService->isAvailable())
    {
        updateActions();
        return;
    }

    const Result<QList<TestingClass>> loaded =
        scheduleService->testingClasses();
    if (!loaded)
    {
        QMessageBox::warning(
            this,
            tr("Testing Classes"),
            loaded.error()
            );
        return;
    }

    QList<TestingClass> testingClasses =
        *loaded;
    std::sort(
        testingClasses.begin(),
        testingClasses.end(),
        [](const TestingClass& left, const TestingClass& right)
        {
            const int leftGrade =
                gradeOrder(left.grade);
            const int rightGrade =
                gradeOrder(right.grade);
            if (leftGrade != rightGrade)
            {
                return leftGrade < rightGrade;
            }
            const int leftLevel =
                levelOrder(
                    left.grade,
                    left.level
                    );
            const int rightLevel =
                levelOrder(
                    right.grade,
                    right.level
                    );
            if (leftLevel != rightLevel)
            {
                return leftLevel < rightLevel;
            }
            const int levelComparison =
                QString::localeAwareCompare(
                    left.level,
                    right.level
                    );
            if (levelComparison != 0)
            {
                return levelComparison < 0;
            }
            return QString::localeAwareCompare(
                left.name,
                right.name
                ) < 0;
        }
        );

    for (const TestingClass& testingClass : std::as_const(testingClasses))
    {
        auto* item =
            new QListWidgetItem(
                classListLabel(testingClass),
                m_classList
                );
        item->setData(
            Qt::UserRole,
            testingClass.classId
            );
        if (testingClass.classId == preferredClassId)
        {
            m_classList->setCurrentItem(item);
        }
    }
    updateActions();
}

void TestingClassesPage::loadClass(
    int classId
    )
{
    m_autosave->setLoading(true);
    auto* scheduleService = m_services ? m_services->scheduleService() : nullptr;
    if (!scheduleService || !scheduleService->isAvailable() || classId <= 0)
    {
        return;
    }

    const Result<TestingClass> loaded =
        scheduleService->testingClass(classId);
    if (!loaded)
    {
        QMessageBox::warning(
            this,
            tr("Testing Classes"),
            loaded.error()
            );
        return;
    }

    m_currentClassId = classId;
    m_savedClass = *loaded;
    m_pendingDay.clear();
    m_pendingStartTime.clear();
    loadEditorValue(*loaded);
    m_rosterEditor->loadClass(
        Classroom(
            loaded->name,
            loaded->classId
            )
        );
    m_editorDirty = false;
    m_autosave->setLoading(false);
    m_autosave->markClean();
    updateActions();
}

void TestingClassesPage::beginNewClass(
    const QString& pendingDay,
    const QString& pendingStartTime
    )
{
    m_autosave->setLoading(true);
    m_currentClassId = -1;
    m_savedClass = {};
    m_savedClass.name =
        QStringLiteral("Testing Class");
    m_savedClass.grade =
        QStringLiteral("M1");
    m_savedClass.classColor = QStringLiteral("#FFFFFF");
    m_savedClass.fontColor = QStringLiteral("#000000");
    m_pendingDay = pendingDay;
    m_pendingStartTime = pendingStartTime;
    {
        const QSignalBlocker blocker(m_classList);
        m_classList->clearSelection();
        m_classList->setCurrentItem(nullptr);
    }
    loadEditorValue(m_savedClass);
    m_rosterEditor->loadClass({});
    m_editorDirty = false;
    m_autosave->setLoading(false);
    m_autosave->markClean();
    updateActions();
    m_nameEdit->setFocus();
}

TestingClass TestingClassesPage::editorValue() const
{
    TestingClass testingClass;
    testingClass.classId = m_currentClassId;
    testingClass.name = m_nameEdit->text().trimmed();
    testingClass.grade = m_gradeCombo->currentText().trimmed();
    testingClass.level = m_levelCombo->currentText().trimmed();
    testingClass.room = m_roomEdit->text().trimmed();
    testingClass.teacherId = m_teacherCombo->currentData().toInt();
    testingClass.classColor = m_savedClass.classColor;
    testingClass.fontColor = m_savedClass.fontColor;
    testingClass.notes = m_notesEdit->toPlainText();
    return testingClass;
}

void TestingClassesPage::loadEditorValue(
    const TestingClass& testingClass
    )
{
    const QSignalBlocker nameBlocker(m_nameEdit);
    const QSignalBlocker gradeBlocker(m_gradeCombo);
    const QSignalBlocker levelBlocker(m_levelCombo);
    const QSignalBlocker roomBlocker(m_roomEdit);
    const QSignalBlocker teacherBlocker(m_teacherCombo);
    const QSignalBlocker notesBlocker(m_notesEdit);

    m_nameEdit->setText(testingClass.name);
    m_gradeCombo->setCurrentText(testingClass.grade);
    updateLevelOptions();
    m_levelCombo->setCurrentText(testingClass.level);
    m_roomEdit->setText(testingClass.room);

    const int teacherIndex =
        m_teacherCombo->findData(
            testingClass.teacherId
            );
    m_teacherCombo->setCurrentIndex(
        teacherIndex >= 0
            ? teacherIndex
            : 0
        );
    m_notesEdit->setPlainText(testingClass.notes);
    m_savedClass.classColor =
        testingClass.classColor.trimmed().isEmpty()
            ? QStringLiteral("#FFFFFF")
            : testingClass.classColor;
    m_savedClass.fontColor =
        testingClass.fontColor.trimmed().isEmpty()
            ? QStringLiteral("#000000")
            : testingClass.fontColor;
    updateColorButtons();
}

void TestingClassesPage::markDirty()
{
    if (m_autosave->isLoading())
    {
        return;
    }
    m_editorDirty = true;
    updateActions();
}

void TestingClassesPage::updateActions()
{
    const bool hasClass =
        m_currentClassId > 0;
    m_deleteButton->setEnabled(hasClass);
    m_tabs->setTabEnabled(1, hasClass);
    const TestingClass testingClass = editorValue();
    m_autosave->setValid(
        !testingClass.name.isEmpty()
        && !testingClass.grade.isEmpty()
        && !testingClass.level.isEmpty()
        && !testingClass.room.isEmpty()
        );
    m_autosave->setSaveAvailable(true);
    m_autosave->setManualSaveRequired(m_currentClassId <= 0);
    m_autosave->setDirty(
        m_editorDirty
        || (
            m_rosterEditor
            && m_rosterEditor->hasUnsavedChanges()
            ),
        m_editorDirty
        );
    m_autosave->setSaveMode(m_autosave->saveMode());
}

void TestingClassesPage::updateColorButtons()
{
    m_classColorPreview->setStyleSheet(
        QStringLiteral(
            "background:%1;"
            "border:1px solid #888;"
            "border-radius:6px;"
            )
            .arg(m_savedClass.classColor)
        );
    m_fontColorPreview->setStyleSheet(
        QStringLiteral(
            "background:%1;"
            "border:1px solid #888;"
            "border-radius:6px;"
            )
            .arg(m_savedClass.fontColor)
        );
}

void TestingClassesPage::chooseClassColor()
{
    const QColor color =
        ColorUtils::getColor(
            QColor(m_savedClass.classColor),
            this,
            tr("Choose Testing Class Color"),
            m_services
                ? m_services->settingsService()
                : nullptr
            );
    if (!color.isValid())
    {
        return;
    }
    m_savedClass.classColor = color.name();
    m_savedClass.fontColor =
        ColorUtils::getContrastingFontColor(color);
    updateColorButtons();
    markDirty();
}

void TestingClassesPage::chooseFontColor()
{
    const QColor color =
        ColorUtils::getColor(
            QColor(m_savedClass.fontColor),
            this,
            tr("Choose Testing Class Font Color"),
            m_services
                ? m_services->settingsService()
                : nullptr
            );
    if (!color.isValid())
    {
        return;
    }
    m_savedClass.fontColor = color.name();
    updateColorButtons();
    markDirty();
}

void TestingClassesPage::deleteCurrentClass()
{
    if (m_currentClassId <= 0)
    {
        return;
    }

    const auto answer =
        QMessageBox::warning(
            this,
            tr("Delete Testing Class?"),
            tr("This permanently deletes the testing class, its roster, notes, and every schedule assignment."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
            );
    if (answer != QMessageBox::Yes)
    {
        return;
    }

    auto* scheduleService = m_services ? m_services->scheduleService() : nullptr;
    if (!scheduleService)
    {
        return;
    }

    const Status deleted =
        scheduleService->deleteTestingClass(
            m_currentClassId
            );
    if (!deleted)
    {
        QMessageBox::warning(
            this,
            tr("Delete Testing Class"),
            deleted.error()
            );
        return;
    }

    m_currentClassId = -1;
    rebuildClassList();
    if (m_classList->count() > 0)
    {
        m_classList->setCurrentRow(0);
        loadClass(
            m_classList
                ->currentItem()
                ->data(Qt::UserRole)
                .toInt()
            );
    }
    else
    {
        beginNewClass();
    }
    emit testingDataChanged();
}
