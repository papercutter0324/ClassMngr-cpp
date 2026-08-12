#include "teacher_info_page.h"

#include "core/application_services.h"
#include "app/services/feature_services.h"
#include "core/fontmanager.h"
#include "domain/models/teacher.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/styles/roles.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/sectioncards/teacher_section_card.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/pages/page_header.h"
#include "ui/shared/pages/scrollable_page_body.h"
#include "core/utils/sidebar_node_naming.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDate>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QFont>
#include <QGridLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <optional>

namespace
{

constexpr int TeacherNameFieldWidth = 140;
constexpr int TeacherPreferredRomanizationFieldWidth = 180;
constexpr int TeacherPreferredNameFieldWidth = 180;
constexpr int TeacherRoomNumberFieldWidth = 115;
constexpr int TeacherPhoneNumberFieldWidth = 160;
constexpr int BirthdayReferenceYear = 2000;

QDate emptyBirthdayDate()
{
    return QDate(1999, 12, 31);
}

QDate birthdayDateFromStorage(const QString& birthday)
{
    const QDate parsed = QDate::fromString(
        QStringLiteral("%1-%2")
            .arg(BirthdayReferenceYear)
            .arg(birthday.trimmed()),
        QStringLiteral("yyyy-MM-dd")
        );

    return parsed.isValid()
        ? parsed
        : emptyBirthdayDate();
}

QString birthdayStorageFromDate(const QDate& date)
{
    if (date == emptyBirthdayDate())
    {
        return QString();
    }

    const QDate normalized(
        BirthdayReferenceYear,
        date.month(),
        date.day()
        );

    return normalized.isValid()
        ? normalized.toString(QStringLiteral("MM-dd"))
        : QString();
}

class BirthdayDateEdit final : public QLineEdit
{
public:
    explicit BirthdayDateEdit(QWidget* parent = nullptr)
        : QLineEdit(parent)
    {
        setReadOnly(true);
        setClearButtonEnabled(true);

        m_popup = new QMenu(this);
        auto* calendarAction = new QWidgetAction(m_popup);
        m_calendar = new QCalendarWidget(m_popup);
        m_calendar->setMinimumDate(
            QDate(BirthdayReferenceYear, 1, 1)
            );
        m_calendar->setMaximumDate(
            QDate(BirthdayReferenceYear, 12, 31)
            );
        m_calendar->setVerticalHeaderFormat(
            QCalendarWidget::NoVerticalHeader
            );
        calendarAction->setDefaultWidget(m_calendar);
        m_popup->addAction(calendarAction);

        connect(
            m_calendar,
            &QCalendarWidget::clicked,
            this,
            [this](const QDate& date)
            {
                m_birthday = date;
                updateDisplayedBirthday();
                m_popup->hide();
            }
            );

        connect(
            this,
            &QLineEdit::textChanged,
            this,
            [this](const QString& text)
            {
                if (!m_updatingText
                    &&
                    text.isEmpty()
                    )
                {
                    m_birthday.reset();
                }
            }
            );
    }

    void setBirthday(const QString& birthday)
    {
        const QDate date = birthdayDateFromStorage(birthday);

        if (date == emptyBirthdayDate())
        {
            m_birthday.reset();
        }
        else
        {
            m_birthday = date;
        }

        updateDisplayedBirthday();
    }

    QString birthday() const
    {
        return m_birthday
            ? birthdayStorageFromDate(*m_birthday)
            : QString();
    }

    void setEmptyText(const QString& text)
    {
        setPlaceholderText(text);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (
            qobject_cast<QToolButton*>(
                childAt(event->position().toPoint())
                )
            )
        {
            QLineEdit::mousePressEvent(event);
            return;
        }

        QLineEdit::mousePressEvent(event);
        showCalendar();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (
            event->key() == Qt::Key_Backspace
            || event->key() == Qt::Key_Delete
            )
        {
            clear();
            return;
        }

        QLineEdit::keyPressEvent(event);
    }

private:
    void showCalendar()
    {
        const QDate date = m_birthday.value_or(
            QDate(
                BirthdayReferenceYear,
                QDate::currentDate().month(),
                QDate::currentDate().day()
                )
            );

        m_calendar->setSelectedDate(date);
        m_calendar->setCurrentPage(
            BirthdayReferenceYear,
            date.month()
            );
        m_popup->popup(
            mapToGlobal(QPoint(0, height()))
            );
    }

    void updateDisplayedBirthday()
    {
        m_updatingText = true;
        setText(
            m_birthday
                ? QLocale().toString(*m_birthday, QStringLiteral("MMMM d"))
                : QString()
            );
        m_updatingText = false;
    }

    QMenu* m_popup = nullptr;
    QCalendarWidget* m_calendar = nullptr;
    std::optional<QDate> m_birthday;
    bool m_updatingText = false;
};

int findComboText(
    QComboBox* combo,
    const QString& value
    )
{
    if (!combo)
    {
        return -1;
    }

    const QString trimmed =
        value.trimmed();

    for (int index = 0; index < combo->count(); ++index)
    {
        const QString storedValue = combo->itemData(index).toString();
        if ((!storedValue.isEmpty()
             && storedValue.compare(trimmed, Qt::CaseInsensitive) == 0)
            || combo->itemText(index).compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return index;
        }
    }

    return -1;
}

void setComboTextWithFallback(
    QComboBox* combo,
    const QString& value,
    const QString& fallback
    )
{
    int index =
        findComboText(combo, value);

    if (index < 0)
    {
        index = findComboText(combo, fallback);
    }

    if (index >= 0)
    {
        combo->setCurrentIndex(index);
    }
}

} // namespace


TeacherInfoPage::TeacherInfoPage(
    ApplicationServices* services,
    QWidget* parent
)
    : BasePage(parent),
      m_services(services),
      m_autosave(new AutosaveCoordinator(this))
{
    setProperty("role", UiRoles::TeacherInfo);

    buildUi();
    m_autosave->bindSaveButton(m_saveButton);
    connect(
        m_autosave,
        &AutosaveCoordinator::saveRequested,
        this,
        [this](bool) { saveTeacherInternal(); }
        );
}

void TeacherInfoPage::buildUi()
{
    // =====================================================
    // Scroll Area
    // =====================================================

    m_pageBody = new ScrollablePageBody(this);
    auto* scrollContainer = m_pageBody->contentWidget();
    auto* scrollLayout = m_pageBody->contentLayout();
    contentLayout()->addWidget(m_pageBody);

    // =====================================================
    // Header
    // =====================================================

    m_pageHeader = new PageHeader(
        tr("Teacher Information"),
        tr("View and manage teacher details."),
        scrollContainer
        );
    scrollLayout->addWidget(m_pageHeader);
    scrollLayout->addSpacing(
        UiConstants::Pages::HeaderContentSpacing
        );

    // =====================================================
    // Teacher Details
    // =====================================================

    m_detailsCard =
        new TeacherSectionCard(tr("Teacher Details"));

    auto* detailsGrid = new QGridLayout;

    detailsGrid->setHorizontalSpacing(16);
    detailsGrid->setVerticalSpacing(4);

    m_teacherKrEdit = new QLineEdit;
    m_teacherKrEdit->setObjectName(
        QStringLiteral("teacherKrEdit")
        );
    m_teacherKrEdit->setFont(
        FontManager::getKoreanFont()
        );
    m_teacherEnEdit = new QLineEdit;
    m_teacherEnEdit->setObjectName(
        QStringLiteral("teacherEnEdit")
        );
    m_preferredRomanizationEdit = new QLineEdit;
    m_preferredRomanizationEdit->setObjectName(
        QStringLiteral("preferredRomanizationEdit")
        );
    m_preferredNameCombo = new NoWheelComboBox;
    m_preferredNameCombo->setObjectName(
        QStringLiteral("preferredNameCombo")
        );
    m_roomNumberEdit = new QLineEdit;
    m_roomNumberEdit->setObjectName(
        QStringLiteral("roomNumberEdit")
        );
    m_birthdayEdit = new BirthdayDateEdit;
    m_birthdayEdit->setObjectName(
        QStringLiteral("birthdayEdit")
        );
    static_cast<BirthdayDateEdit*>(m_birthdayEdit)->setEmptyText(
        tr("Not set")
        );
    m_phoneNumberEdit = new QLineEdit;
    m_phoneNumberEdit->setObjectName(
        QStringLiteral("phoneNumberEdit")
        );

    WidgetSizing::installTextAwareFieldWidth(
        m_teacherKrEdit,
        TeacherNameFieldWidth,
        QSizePolicy::Maximum
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_teacherEnEdit,
        TeacherNameFieldWidth,
        QSizePolicy::Maximum
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_preferredRomanizationEdit,
        TeacherPreferredRomanizationFieldWidth,
        QSizePolicy::Maximum
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_preferredNameCombo,
        TeacherPreferredNameFieldWidth,
        QSizePolicy::Maximum
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_roomNumberEdit,
        TeacherRoomNumberFieldWidth,
        QSizePolicy::Maximum
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_birthdayEdit,
        TeacherNameFieldWidth,
        QSizePolicy::Maximum
        );
    WidgetSizing::installTextAwareFieldWidth(
        m_phoneNumberEdit,
        TeacherPhoneNumberFieldWidth,
        QSizePolicy::Maximum
        );

    matchBirthdayWidthToKoreanName();

    m_teacherKrLabel =
        createFieldLabel(tr("Korean Name"));

    m_teacherEnLabel =
        createFieldLabel(tr("English Name"));

    m_preferredRomanizationLabel =
        createFieldLabel(tr("Preferred Spelling"));

    m_preferredNameLabel =
        createFieldLabel(tr("Preferred Name"));

    m_roomNumberLabel =
        createFieldLabel(tr("Room Number"));

    m_birthdayLabel =
        createFieldLabel(tr("Birthday"));

    m_phoneNumberLabel =
        createFieldLabel(tr("Phone Number"));

    detailsGrid->addWidget(
        m_teacherEnLabel, 0, 0);

    detailsGrid->addWidget(
        m_teacherKrLabel, 0, 1);

    detailsGrid->addWidget(
        m_preferredRomanizationLabel, 0, 2);

    detailsGrid->addWidget(
        m_preferredNameLabel, 0, 3);

    detailsGrid->addWidget(m_teacherEnEdit, 1, 0, Qt::AlignLeft);
    detailsGrid->addWidget(m_teacherKrEdit, 1, 1, Qt::AlignLeft);
    detailsGrid->addWidget(
        m_preferredRomanizationEdit, 1, 2, Qt::AlignLeft);
    detailsGrid->addWidget(
        m_preferredNameCombo, 1, 3, Qt::AlignLeft);

    detailsGrid->addItem(
        new QSpacerItem(
            0,
            UiConstants::ClassInfo::Form::GroupSpacerHeight,
            QSizePolicy::Minimum,
            QSizePolicy::Fixed
            ),
        2,
        0,
        1,
        4
        );

    detailsGrid->addWidget(
        m_roomNumberLabel, 3, 0);

    detailsGrid->addWidget(
        m_birthdayLabel, 3, 1);

    detailsGrid->addWidget(
        m_phoneNumberLabel, 3, 2);

    detailsGrid->addWidget(m_roomNumberEdit, 4, 0, Qt::AlignLeft);
    detailsGrid->addWidget(m_birthdayEdit, 4, 1, Qt::AlignLeft);
    detailsGrid->addWidget(m_phoneNumberEdit, 4, 2, Qt::AlignLeft);

    detailsGrid->setColumnStretch(0, 0);
    detailsGrid->setColumnStretch(1, 0);
    detailsGrid->setColumnStretch(2, 0);
    detailsGrid->setColumnStretch(3, 0);
    detailsGrid->setColumnStretch(4, 1);

    m_detailsCard->contentLayout()->addLayout(detailsGrid);

    scrollLayout->addWidget(m_detailsCard);

    // =====================================================
    // Connectivity
    // =====================================================

    m_connectivityCard =
        new TeacherSectionCard(tr("Connectivity"));

    auto* connectivityGrid = new QGridLayout;

    connectivityGrid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );

    connectivityGrid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    m_wifiNameEdit = new QLineEdit;
    m_wifiPasswordEdit = new QLineEdit;
    m_internetTypeCombo = new NoWheelComboBox;
    m_internetTypeCombo->addItem(
        QStringLiteral("WiFi"),
        QStringLiteral("WiFi")
        );
    m_internetTypeCombo->addItem(
        QStringLiteral("LAN"),
        QStringLiteral("LAN")
        );
    m_internetTypeCombo->addItem(
        tr("Both"),
        QStringLiteral("Both")
        );
    m_internetTypeCombo->addItem(
        tr("N/A"),
        QStringLiteral("N/A")
        );

    m_zoomIdEdit = new QLineEdit;
    m_zoomPasswordEdit = new QLineEdit;
    m_projectionTypeCombo = new NoWheelComboBox;
    m_projectionTypeCombo->addItem(
        QStringLiteral("HDMI"),
        QStringLiteral("HDMI")
        );
    m_projectionTypeCombo->addItem(
        QStringLiteral("Zoom"),
        QStringLiteral("Zoom")
        );
    m_projectionTypeCombo->addItem(
        tr("Any"),
        QStringLiteral("Any")
        );
    m_projectionTypeCombo->addItem(
        tr("N/A"),
        QStringLiteral("N/A")
        );

    m_internetTypeLabel =
        createFieldLabel(tr("Internet Type"));

    m_wifiNameLabel =
        createFieldLabel(tr("WiFi Name"));

    m_wifiPasswordLabel =
        createFieldLabel(tr("WiFi Password"));

    connectivityGrid->addWidget(
        m_internetTypeLabel, 0, 0, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_wifiNameLabel, 0, 1, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_wifiPasswordLabel, 0, 2, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_internetTypeCombo, 1, 0);
    connectivityGrid->addWidget(
        m_wifiNameEdit, 1, 1);
    connectivityGrid->addWidget(
        m_wifiPasswordEdit, 1, 2);

    connectivityGrid->addItem(
        new QSpacerItem(
            0,
            UiConstants::ClassInfo::Form::GroupSpacerHeight,
            QSizePolicy::Minimum,
            QSizePolicy::Fixed
            ),
        2,
        0,
        1,
        4
        );

    m_projectionTypeLabel =
        createFieldLabel(tr("Projection Type"));

    m_zoomIdLabel =
        createFieldLabel(tr("Zoom ID"));

    m_zoomPasswordLabel =
        createFieldLabel(tr("Zoom Password"));

    connectivityGrid->addWidget(
        m_projectionTypeLabel, 3, 0, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_zoomIdLabel, 3, 1, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_zoomPasswordLabel, 3, 2, Qt::AlignLeft);

    connectivityGrid->addWidget(
        m_projectionTypeCombo, 4, 0);
    connectivityGrid->addWidget(
        m_zoomIdEdit, 4, 1);
    connectivityGrid->addWidget(
        m_zoomPasswordEdit, 4, 2);

    for (auto* widget : {
             static_cast<QWidget*>(m_internetTypeCombo),
             static_cast<QWidget*>(m_wifiNameEdit),
             static_cast<QWidget*>(m_wifiPasswordEdit),
             static_cast<QWidget*>(m_projectionTypeCombo),
             static_cast<QWidget*>(m_zoomIdEdit),
             static_cast<QWidget*>(m_zoomPasswordEdit)
         })
    {
        WidgetSizing::installTextAwareFieldWidth(
            widget,
            UiConstants::ClassInfo::Teacher::FieldMinWidth
            );
    }

    connectivityGrid->setColumnStretch(
        0,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    connectivityGrid->setColumnStretch(
        1,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    connectivityGrid->setColumnStretch(
        2,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    connectivityGrid->setColumnStretch(
        3,
        UiConstants::ClassInfo::Teacher::FillerColumnStretch
        );

    m_connectivityCard->contentLayout()->addLayout(
        connectivityGrid);

    scrollLayout->addWidget(m_connectivityCard);

    // =====================================================
    // Notes
    // =====================================================

    m_notesCard =
        new TeacherSectionCard(tr("Notes"));

    m_notesEdit = new QTextEdit;
    m_notesEdit->setMinimumHeight(180);

    m_notesCard->contentLayout()->addWidget(m_notesEdit);

    scrollLayout->addWidget(m_notesCard);

    // =====================================================
    // Footer
    // =====================================================

    m_saveButton =
        new TextFitPushButton(tr("Save Changes"));

    for (auto* edit : {
             m_teacherKrEdit,
             m_roomNumberEdit,
             m_birthdayEdit,
             m_phoneNumberEdit,
             m_wifiNameEdit,
             m_wifiPasswordEdit,
             m_zoomIdEdit,
             m_zoomPasswordEdit
         })
    {
        connect(
            edit,
            &QLineEdit::textChanged,
            this,
            &TeacherInfoPage::handleFieldChanged
        );
    }

    const auto handlePreferredNameInputChanged = [this]
    {
        updatePreferredNameChoices(true);
        handleFieldChanged();
    };

    connect(
        m_teacherEnEdit,
        &QLineEdit::textChanged,
        this,
        handlePreferredNameInputChanged
        );

    connect(
        m_preferredRomanizationEdit,
        &QLineEdit::textChanged,
        this,
        handlePreferredNameInputChanged
        );

    connect(
        m_teacherKrEdit,
        &QLineEdit::textChanged,
        this,
        &TeacherInfoPage::matchBirthdayWidthToKoreanName
        );

    connect(
        m_birthdayEdit,
        &QLineEdit::textChanged,
        this,
        &TeacherInfoPage::matchBirthdayWidthToKoreanName
        );

    connect(
        m_notesEdit,
        &QTextEdit::textChanged,
        this,
        &TeacherInfoPage::handleFieldChanged
        );

    connect(
        m_internetTypeCombo,
        &QComboBox::currentTextChanged,
        this,
        &TeacherInfoPage::handleFieldChanged
        );

    connect(
        m_projectionTypeCombo,
        &QComboBox::currentTextChanged,
        this,
        &TeacherInfoPage::handleFieldChanged
        );

    connect(
        m_preferredNameCombo,
        &QComboBox::currentTextChanged,
        this,
        &TeacherInfoPage::handleFieldChanged
        );

    bottomLayout()->addStretch();
    bottomLayout()->addWidget(m_saveButton);

    updateActions();
}

QLabel* TeacherInfoPage::createFieldLabel(
    const QString& text)
{
    auto* label = new QLabel(text);
    label->setContentsMargins(
        UiConstants::ClassInfo::Form::LabelIndent,
        0,
        0,
        0
        );

    return label;
}

void TeacherInfoPage::loadTeacher(
    const Teacher& teacher)
{
    m_autosave->setLoading(true);

    m_teacher = teacher;

    QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(teacher);

    m_pageHeader->setTitle(
        tr("Teacher Information for %1")
            .arg(displayName));

    m_teacherKrEdit->setText(
        teacher.teacherKr);

    m_teacherEnEdit->setText(
        teacher.teacherEn);

    m_preferredRomanizationEdit->setText(
        teacher.preferredRomanization);

    updatePreferredNameChoices(false);

    m_roomNumberEdit->setText(
        teacher.roomNumber);

    static_cast<BirthdayDateEdit*>(m_birthdayEdit)->setBirthday(
        teacher.birthday
        );

    m_phoneNumberEdit->setText(
        teacher.phoneNumber);

    setComboTextWithFallback(
        m_internetTypeCombo,
        teacher.internetType,
        QStringLiteral("WiFi")
        );

    m_wifiNameEdit->setText(
        teacher.wifiName);

    m_wifiPasswordEdit->setText(
        teacher.wifiPassword);

    setComboTextWithFallback(
        m_projectionTypeCombo,
        teacher.projectionType,
        QStringLiteral("HDMI")
        );

    m_zoomIdEdit->setText(
        teacher.zoomId);

    m_zoomPasswordEdit->setText(
        teacher.zoomPassword);

    m_notesEdit->setPlainText(
        teacher.notes);

    updateFieldWidths();

    m_autosave->setLoading(false);
    clearDirty();
}

void TeacherInfoPage::saveData()
{
    saveTeacher();
}

void TeacherInfoPage::showEvent(QShowEvent* event)
{
    BasePage::showEvent(event);

    QTimer::singleShot(
        0,
        this,
        &TeacherInfoPage::matchBirthdayWidthToKoreanName
        );
}

bool TeacherInfoPage::saveChanges()
{
    if (!m_autosave->isDirty())
    {
        return true;
    }

    return saveTeacherInternal();
}

bool TeacherInfoPage::hasUnsavedChanges() const
{
    return m_autosave->isDirty();
}

void TeacherInfoPage::discardChanges()
{
    m_autosave->cancelPendingSave();

    loadTeacher(m_teacher);
}

QString TeacherInfoPage::unsavedChangesTitle() const
{
    return tr("Unsaved Teacher Changes");
}

QString TeacherInfoPage::unsavedChangesMessage() const
{
    return tr("This teacher has unsaved changes.");
}

void TeacherInfoPage::setSaveMode(
    SaveMode mode
    )
{
    m_autosave->setSaveMode(mode);
}

void TeacherInfoPage::saveTeacher()
{
    saveTeacherInternal();
}

void TeacherInfoPage::handleFieldChanged()
{
    if (m_autosave->isLoading())
    {
        return;
    }

    m_autosave->setDirty(formDiffersFromTeacher());
}

Teacher TeacherInfoPage::teacherFromForm() const
{
    Teacher updated = m_teacher;

    updated.teacherKr = m_teacherKrEdit->text().trimmed();
    updated.teacherEn = m_teacherEnEdit->text().trimmed();
    updated.preferredRomanization =
        m_preferredRomanizationEdit->text().trimmed();
    updated.preferredName =
        m_preferredNameCombo->currentText().trimmed();

    updated.roomNumber = m_roomNumberEdit->text().trimmed();
    updated.birthday =
        static_cast<BirthdayDateEdit*>(m_birthdayEdit)->birthday();
    updated.phoneNumber = m_phoneNumberEdit->text().trimmed();

    updated.internetType =
        m_internetTypeCombo->currentData().toString().trimmed();
    updated.wifiName = m_wifiNameEdit->text().trimmed();
    updated.wifiPassword = m_wifiPasswordEdit->text().trimmed();

    updated.projectionType =
        m_projectionTypeCombo->currentData().toString().trimmed();
    updated.zoomId = m_zoomIdEdit->text().trimmed();
    updated.zoomPassword = m_zoomPasswordEdit->text().trimmed();

    updated.notes = m_notesEdit->toPlainText().trimmed();

    return updated;
}

bool TeacherInfoPage::formDiffersFromTeacher() const
{
    const Teacher updated =
        teacherFromForm();

    return updated.teacherKr != m_teacher.teacherKr.trimmed()
        || updated.teacherEn != m_teacher.teacherEn.trimmed()
        || updated.preferredRomanization
            != m_teacher.preferredRomanization.trimmed()
        || updated.preferredName
            != m_teacher.preferredName.trimmed()
        || updated.roomNumber != m_teacher.roomNumber.trimmed()
        || updated.birthday != m_teacher.birthday.trimmed()
        || updated.phoneNumber != m_teacher.phoneNumber.trimmed()
        || updated.internetType != m_teacher.internetType.trimmed()
        || updated.wifiName != m_teacher.wifiName.trimmed()
        || updated.wifiPassword != m_teacher.wifiPassword.trimmed()
        || updated.projectionType != m_teacher.projectionType.trimmed()
        || updated.zoomId != m_teacher.zoomId.trimmed()
        || updated.zoomPassword != m_teacher.zoomPassword.trimmed()
        || updated.notes != m_teacher.notes.trimmed();
}

bool TeacherInfoPage::saveTeacherInternal()
{
    if (
        !m_services
        || !m_services->teacherService()
        || m_teacher.id <= 0
        )
    {
        return false;
    }

    m_autosave->cancelPendingSave();

    auto* teacherService = m_services->teacherService();

    const Teacher updated =
        teacherFromForm();

    teacherService->update(updated);

    m_teacher =
        teacherService->teacher(
            m_teacher.id
            );

    const QString displayName =
        SidebarNodeNaming::formatTeacherDisplayName(m_teacher);

    m_pageHeader->setTitle(
        tr("Teacher Information for %1")
            .arg(displayName)
        );

    clearDirty();

    emit teacherSaved(
        m_teacher.id
        );

    return !m_autosave->isDirty();
}

void TeacherInfoPage::clearDirty()
{
    m_autosave->markClean();
    updateActions();
}

void TeacherInfoPage::updateActions()
{
    m_autosave->setSaveAvailable(m_teacher.id > 0);
    m_autosave->setSaveMode(m_autosave->saveMode());
}

void TeacherInfoPage::updateFieldWidths()
{
    WidgetSizing::updateTextAwareFieldWidth(
        m_teacherKrEdit,
        TeacherNameFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_teacherEnEdit,
        TeacherNameFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_preferredRomanizationEdit,
        TeacherPreferredRomanizationFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_preferredNameCombo,
        TeacherPreferredNameFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_roomNumberEdit,
        TeacherRoomNumberFieldWidth
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_birthdayEdit,
        TeacherNameFieldWidth
        );
    matchBirthdayWidthToKoreanName();
    WidgetSizing::updateTextAwareFieldWidth(
        m_phoneNumberEdit,
        TeacherPhoneNumberFieldWidth
        );

    for (auto* widget : {
             static_cast<QWidget*>(m_internetTypeCombo),
             static_cast<QWidget*>(m_wifiNameEdit),
             static_cast<QWidget*>(m_wifiPasswordEdit),
             static_cast<QWidget*>(m_projectionTypeCombo),
             static_cast<QWidget*>(m_zoomIdEdit),
             static_cast<QWidget*>(m_zoomPasswordEdit)
         })
    {
        WidgetSizing::updateTextAwareFieldWidth(
            widget,
            UiConstants::ClassInfo::Teacher::FieldMinWidth
            );
    }
}

void TeacherInfoPage::matchBirthdayWidthToKoreanName()
{
    if (!m_teacherKrEdit || !m_birthdayEdit)
    {
        return;
    }

    const int koreanNameWidth =
        m_teacherKrEdit->width() > 0
            ? m_teacherKrEdit->width()
            : std::max(
                m_teacherKrEdit->minimumWidth(),
                m_teacherKrEdit->sizeHint().width()
                );

    m_birthdayEdit->setFixedWidth(koreanNameWidth);
}

void TeacherInfoPage::updatePreferredNameChoices(
    bool promptForSelection
    )
{
    if (!m_preferredNameCombo)
    {
        return;
    }

    const int previousChoiceCount =
        m_preferredNameCombo->count();
    const QString previousSelection =
        m_preferredNameCombo->currentText();

    Teacher teacher;
    teacher.teacherEn = m_teacherEnEdit->text();
    teacher.preferredRomanization =
        m_preferredRomanizationEdit->text();

    const QStringList choices =
        teacher.preferredNameChoices();

    QSignalBlocker blocker(m_preferredNameCombo);
    m_preferredNameCombo->clear();
    m_preferredNameCombo->addItems(choices);
    m_preferredNameCombo->setEnabled(!choices.isEmpty());

    int selectedIndex =
        m_autosave->isLoading()
            ? choices.indexOf(m_teacher.preferredName.trimmed())
            : choices.indexOf(previousSelection);

    if (selectedIndex < 0)
    {
        selectedIndex =
            choices.indexOf(m_teacher.preferredName.trimmed());
    }

    if (selectedIndex < 0 && !choices.isEmpty())
    {
        selectedIndex = 0;
    }

    m_preferredNameCombo->setCurrentIndex(selectedIndex);

    if (
        !m_autosave->isLoading()
        && promptForSelection
        && previousChoiceCount == 1
        && choices.size() == 2
        )
    {
        bool accepted = false;
        const QString selectedName =
            QInputDialog::getItem(
                this,
                tr("Select Preferred Name"),
                tr("Choose the name to display for this teacher."),
                choices,
                selectedIndex,
                false,
                &accepted
                );

        if (accepted)
        {
            m_preferredNameCombo->setCurrentText(selectedName);
        }
    }
}

Teacher TeacherInfoPage::teacher() const
{
    return m_teacher;
}

void TeacherInfoPage::refresh()
{
    BasePage::refresh();
}

void TeacherInfoPage::clearDatabaseState()
{
    loadTeacher({});
    retranslateUi();
}

void TeacherInfoPage::retranslateUi()
{
    if (m_pageHeader)
    {
        if (m_teacher.id > 0)
        {
            m_pageHeader->setTitle(
                tr("Teacher Information for %1")
                    .arg(
                        SidebarNodeNaming::formatTeacherDisplayName(
                            m_teacher
                            )
                        )
                );
        }
        else
        {
            m_pageHeader->setTitle(
                tr("Teacher Information")
                );
        }
    }

    if (m_pageHeader)
    {
        m_pageHeader->setSubtitle(
            tr("View and manage teacher details.")
            );
    }

    if (m_detailsCard)
    {
        m_detailsCard->setTitle(
            tr("Teacher Details")
            );
    }

    if (m_connectivityCard)
    {
        m_connectivityCard->setTitle(
            tr("Connectivity")
            );
    }

    if (m_notesCard)
    {
        m_notesCard->setTitle(
            tr("Notes")
            );
    }

    if (m_teacherKrLabel)
    {
        m_teacherKrLabel->setText(
            tr("Korean Name")
            );
    }

    if (m_teacherEnLabel)
    {
        m_teacherEnLabel->setText(
            tr("English Name")
            );
    }

    if (m_roomNumberLabel)
    {
        m_roomNumberLabel->setText(
            tr("Room Number")
            );
    }

    if (m_preferredRomanizationLabel)
    {
        m_preferredRomanizationLabel->setText(
            tr("Preferred Spelling")
            );
    }

    if (m_preferredNameLabel)
    {
        m_preferredNameLabel->setText(
            tr("Preferred Name")
            );
    }

    if (m_birthdayLabel)
    {
        m_birthdayLabel->setText(
            tr("Birthday")
            );
    }

    if (m_birthdayEdit)
    {
        static_cast<BirthdayDateEdit*>(m_birthdayEdit)->setEmptyText(
            tr("Not set")
            );
    }

    if (m_phoneNumberLabel)
    {
        m_phoneNumberLabel->setText(
            tr("Phone Number")
            );
    }

    if (m_internetTypeLabel)
    {
        m_internetTypeLabel->setText(
            tr("Internet Type")
            );
    }

    if (m_wifiNameLabel)
    {
        m_wifiNameLabel->setText(
            tr("WiFi Name")
            );
    }

    if (m_wifiPasswordLabel)
    {
        m_wifiPasswordLabel->setText(
            tr("WiFi Password")
            );
    }

    if (m_projectionTypeLabel)
    {
        m_projectionTypeLabel->setText(
            tr("Projection Type")
            );
    }

    if (m_internetTypeCombo)
    {
        m_internetTypeCombo->setItemText(
            findComboText(m_internetTypeCombo, QStringLiteral("Both")),
            tr("Both")
            );
        m_internetTypeCombo->setItemText(
            findComboText(m_internetTypeCombo, QStringLiteral("N/A")),
            tr("N/A")
            );
    }

    if (m_projectionTypeCombo)
    {
        m_projectionTypeCombo->setItemText(
            findComboText(m_projectionTypeCombo, QStringLiteral("Any")),
            tr("Any")
            );
        m_projectionTypeCombo->setItemText(
            findComboText(m_projectionTypeCombo, QStringLiteral("N/A")),
            tr("N/A")
            );
    }

    if (m_zoomIdLabel)
    {
        m_zoomIdLabel->setText(
            tr("Zoom ID")
            );
    }

    if (m_zoomPasswordLabel)
    {
        m_zoomPasswordLabel->setText(
            tr("Zoom Password")
            );
    }

    updateActions();
}
