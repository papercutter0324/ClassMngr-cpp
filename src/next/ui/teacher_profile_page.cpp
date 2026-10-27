#include "next/ui/teacher_profile_page.h"

#include "next/application/positive_decimal_id_validation.h"
#include "next/application/preferred_name_selection_policy.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/on_screen_keyboard.h"

#include <QCalendarWidget>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QEvent>
#include <QFrame>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <string>
#include <utility>

namespace ClassMngr::Next::Ui
{
namespace
{

constexpr int AutosaveDebounceMilliseconds = 750;
constexpr int TeacherNameFieldWidth = 140;
constexpr int TeacherKoreanNameFieldWidth = 208;
constexpr int TeacherPreferredRomanizationFieldWidth = 180;
constexpr int TeacherPreferredNameFieldWidth = 180;
constexpr int TeacherRoomNumberFieldWidth = 115;
constexpr int TeacherPhoneNumberFieldWidth = 160;
constexpr int BirthdayReferenceYear = 2000;
#if defined(Q_OS_MACOS)
constexpr int TeacherKoreanFontPointSize = 16;
#else
constexpr int TeacherKoreanFontPointSize = 15;
#endif

[[nodiscard]] QFont fontWithFamilyFragment(
    QFont font,
    const QString& fragment
    )
{
    for (const QString& family : QFontDatabase::families())
    {
        if (family.contains(fragment, Qt::CaseInsensitive))
        {
            font.setFamily(family);
            break;
        }
    }
    return font;
}

class TeacherProfileSectionCard final : public QFrame
{
public:
    explicit TeacherProfileSectionCard(
        const QString& title,
        QWidget* parent = nullptr
        )
        : QFrame(parent)
    {
        setProperty("role", QStringLiteral("card"));
        setObjectName(QStringLiteral("TeacherSectionCard"));
        m_layout = new QVBoxLayout(this);
        m_layout->setAlignment(Qt::AlignTop);
        m_layout->setContentsMargins(
            UiConstants::Cards::Margin,
            UiConstants::Cards::Margin,
            UiConstants::Cards::Margin,
            UiConstants::Cards::Margin
            );
        m_layout->setSpacing(UiConstants::Cards::Spacing);
        m_titleLabel = new QLabel(title, this);
        m_titleLabel->setObjectName(QStringLiteral("sectionTitle"));
        m_titleLabel->setFont(fontWithFamilyFragment(
            m_titleLabel->font(), QStringLiteral("Inter")
            ));
        m_layout->addWidget(
            m_titleLabel,
            0,
            Qt::AlignLeft | Qt::AlignTop
            );
    }

    void setTitle(const QString& title)
    {
        m_titleLabel->setText(title);
    }

    QVBoxLayout* contentLayout() const
    {
        return m_layout;
    }

private:
    QLabel* m_titleLabel = nullptr;
    QVBoxLayout* m_layout = nullptr;
};

void setMinimumFieldWidth(QWidget* const widget, const int width)
{
    widget->setMinimumWidth(width);
    QSizePolicy policy = widget->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Expanding);
    widget->setSizePolicy(policy);
}

void setFixedFieldWidth(QWidget* const widget, const int width)
{
    widget->setFixedWidth(width);
}

[[nodiscard]] QString pageText(const char* text)
{
    return QCoreApplication::translate("TeacherProfilePage", text);
}

[[nodiscard]] QString stdMessage(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] Domain::TeacherProfileFields comparisonFields(
    Domain::TeacherProfileFields fields
    )
{
    const auto trim = [](std::u16string& value) {
        value = QString::fromStdU16String(value).trimmed().toStdU16String();
    };
    trim(fields.teacherKr);
    trim(fields.teacherEn);
    trim(fields.preferredRomanization);
    trim(fields.preferredName);
    trim(fields.roomNumber);
    trim(fields.birthday);
    trim(fields.phoneNumber);
    trim(fields.wifiName);
    trim(fields.wifiPassword);
    trim(fields.internetType);
    trim(fields.zoomId);
    trim(fields.zoomPassword);
    trim(fields.projectionType);
    trim(fields.notes);
    const QString birthday = QString::fromStdU16String(fields.birthday);
    QDate normalizedBirthday = QDate::fromString(
        QStringLiteral("2000-") + birthday,
        QStringLiteral("yyyy-MM-dd")
        );
    if (!normalizedBirthday.isValid())
    {
        normalizedBirthday = QDate::fromString(
            birthday,
            QStringLiteral("yyyy-MM-dd")
            );
    }
    if (normalizedBirthday.isValid())
    {
        fields.birthday = normalizedBirthday
            .toString(QStringLiteral("MM-dd"))
            .toStdU16String();
    }
    return fields;
}

} // namespace

class TeacherBirthdayDateEdit final : public QLineEdit
{
public:
    explicit TeacherBirthdayDateEdit(QWidget* parent = nullptr)
        : QLineEdit(parent)
    {
        setReadOnly(true);
        setClearButtonEnabled(true);

        m_popup = new QMenu(this);
        auto* const calendarAction = new QWidgetAction(m_popup);
        m_calendar = new QCalendarWidget(m_popup);
        m_calendar->setObjectName(QStringLiteral("teacherBirthdayCalendar"));
        m_calendar->setMinimumDate(QDate(2000, 1, 1));
        m_calendar->setMaximumDate(QDate(2000, 12, 31));
        m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        calendarAction->setDefaultWidget(m_calendar);
        m_popup->addAction(calendarAction);

        connect(m_calendar, &QCalendarWidget::clicked, this, [this](const QDate& date) {
            m_birthday = QDate(2000, date.month(), date.day());
            updateDisplayedBirthday();
            m_popup->hide();
        });
        connect(this, &QLineEdit::textChanged, this, [this](const QString& text) {
            if (!m_updatingText && text.isEmpty())
            {
                m_birthday.reset();
            }
        });
    }

    void setBirthday(const std::u16string& value)
    {
        const QString stored = QString::fromStdU16String(value).trimmed();
        QDate date = QDate::fromString(
            QStringLiteral("2000-") + stored,
            QStringLiteral("yyyy-MM-dd")
            );
        if (!date.isValid())
        {
            date = QDate::fromString(stored, QStringLiteral("yyyy-MM-dd"));
            if (date.isValid())
            {
                date = QDate(BirthdayReferenceYear, date.month(), date.day());
            }
        }
        m_birthday = date.isValid() ? std::optional<QDate>(date) : std::nullopt;
        updateDisplayedBirthday();
    }

    [[nodiscard]] std::u16string birthday() const
    {
        return m_birthday
            ? m_birthday->toString(QStringLiteral("MM-dd")).toStdU16String()
            : std::u16string{};
    }

    void setEmptyText(const QString& text)
    {
        setPlaceholderText(text);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        const bool clearButton = qobject_cast<QToolButton*>(
            childAt(event->position().toPoint())
            ) != nullptr;
        QLineEdit::mousePressEvent(event);
        if (!clearButton && event->button() == Qt::LeftButton)
        {
            showCalendar();
        }
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)
        {
            clear();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }

private:
    void showCalendar()
    {
        const QDate current = m_birthday.value_or(QDate(
            BirthdayReferenceYear,
            QDate::currentDate().month(),
            QDate::currentDate().day()
            ));
        m_calendar->setSelectedDate(current);
        m_calendar->setCurrentPage(2000, current.month());
        m_popup->popup(mapToGlobal(QPoint(0, height())));
    }

    void updateDisplayedBirthday()
    {
        m_updatingText = true;
        setText(m_birthday
            ? QLocale().toString(*m_birthday, QStringLiteral("MMMM d"))
            : QString{});
        m_updatingText = false;
        if (QToolButton* const clearButton = findChild<QToolButton*>())
        {
            clearButton->setEnabled(m_birthday.has_value());
            if (!m_clearButtonConnected)
            {
                connect(clearButton, &QToolButton::clicked, this, [this] {
                    clear();
                });
                m_clearButtonConnected = true;
            }
        }
    }

    QMenu* m_popup = nullptr;
    QCalendarWidget* m_calendar = nullptr;
    std::optional<QDate> m_birthday;
    bool m_updatingText = false;
    bool m_clearButtonConnected = false;
};

TeacherProfilePage::TeacherProfilePage(
    const PageContext& context,
    TeacherProfilePageActions actions,
    QWidget* parent
    )
    : Page(context, parent)
    , m_actions(std::move(actions))
    , m_saveMode(m_actions.saveMode)
{
    setObjectName(QStringLiteral("classMngrNextTeacherProfilePage"));
    buildUi();
    retranslateUi();
    updatePageState();
}

bool TeacherProfilePage::displayProfile(
    const Application::TeacherProfileReadSnapshot& profile
    )
{
    if (!Application::isCanonicalPositiveDecimalId64(
            profile.teacherId.value())
        )
    {
        return false;
    }

    m_saveTimer->stop();
    m_loading = true;
    m_teacherId = profile.teacherId;
    m_savedFields = comparisonFields(profile.fields);
    m_dirty = false;
    m_reloadRequired = false;
    populateForm(profile.fields);
    clearPersistenceStatus();
    m_loading = false;
    updateValidation();
    updatePageState();
    return true;
}

bool TeacherProfilePage::refreshFromStore(QString* error)
{
    if (error)
    {
        error->clear();
    }
    if (!m_teacherId)
    {
        return true;
    }

    const Application::TeacherProfileReadResult loaded = m_actions.read
        ? m_actions.read(*m_teacherId)
        : Application::TeacherProfileReadResult::failure({
              .code = Domain::ErrorCode::NotFound,
              .message = "The teacher profile read action is unavailable.",
              .recoverable = true
          });
    if (!loaded)
    {
        m_reloadRequired = true;
        m_dirty = false;
        setHasUnsavedChanges(false);
        const QString message = QString::fromUtf8(
            loaded.error().message.data(),
            static_cast<qsizetype>(loaded.error().message.size())
            );
        if (error)
        {
            *error = message;
        }
        setPersistenceStatus(
            pageText("The latest teacher profile could not be loaded. Reload this teacher before editing or saving again."),
            true,
            true
            );
        updatePageState();
        return false;
    }
    if (!displayProfile(loaded.value()))
    {
        m_reloadRequired = true;
        m_dirty = false;
        setHasUnsavedChanges(false);
        if (error)
        {
            *error = pageText("The refreshed teacher profile is invalid.");
        }
        setPersistenceStatus(
            pageText("The refreshed teacher profile is invalid. Reload this teacher before editing or saving again."),
            true,
            true
            );
        updatePageState();
        return false;
    }
    return true;
}

std::optional<Domain::TeacherId> TeacherProfilePage::selectedTeacherId() const
{
    return m_teacherId;
}

bool TeacherProfilePage::hasUnsavedChanges() const noexcept
{
    return m_teacherId.has_value() && !m_reloadRequired && m_dirty;
}

PageOperationResult<void> TeacherProfilePage::saveChanges()
{
    return saveProfile(true);
}

PageOperationResult<void> TeacherProfilePage::discardChanges()
{
    if (m_reloadRequired)
    {
        return {};
    }
    if (!m_teacherId)
    {
        return {};
    }

    m_saveTimer->stop();
    m_loading = true;
    populateForm(m_savedFields);
    m_dirty = false;
    clearPersistenceStatus();
    m_loading = false;
    updateValidation();
    updatePageState();
    return {};
}

void TeacherProfilePage::onSuspending() noexcept
{
    m_saveTimer->stop();
}

void TeacherProfilePage::setSaveMode(const Application::SaveMode mode)
{
    m_saveMode = mode;
    updatePageState();
    if (m_saveMode == Application::SaveMode::Automatic
        && hasUnsavedChanges()
        && !m_hasBlockingErrors
        && !m_reloadRequired)
    {
        m_saveTimer->start();
    }
    else
    {
        m_saveTimer->stop();
    }
}

void TeacherProfilePage::retranslateUi()
{
    m_titleLabel->setText(pageText("Teacher Information"));
    m_subtitleLabel->setText(pageText("View and manage teacher details."));
    m_noSelectionLabel->setText(
        pageText("Select a teacher to view or edit their profile.")
        );
    static_cast<TeacherProfileSectionCard*>(m_detailsGroup)->setTitle(
        pageText("Teacher Details")
        );
    static_cast<TeacherProfileSectionCard*>(m_connectivityGroup)->setTitle(
        pageText("Connectivity")
        );
    static_cast<TeacherProfileSectionCard*>(m_notesGroup)->setTitle(
        pageText("Notes")
        );
    m_saveButton->setText(pageText("Save Changes"));

    const QHash<QString, QString> labels{
        {QStringLiteral("teacherKr"), pageText("Korean Name")},
        {QStringLiteral("teacherEn"), pageText("English Name")},
        {QStringLiteral("preferredRomanization"), pageText("Preferred Spelling")},
        {QStringLiteral("preferredName"), pageText("Preferred Name")},
        {QStringLiteral("roomNumber"), pageText("Room Number")},
        {QStringLiteral("birthday"), pageText("Birthday")},
        {QStringLiteral("phoneNumber"), pageText("Phone Number")},
        {QStringLiteral("internetType"), pageText("Internet Type")},
        {QStringLiteral("wifiName"), pageText("WiFi Name")},
        {QStringLiteral("wifiPassword"), pageText("WiFi Password")},
        {QStringLiteral("projectionType"), pageText("Projection Type")},
        {QStringLiteral("zoomId"), pageText("Zoom ID")},
        {QStringLiteral("zoomPassword"), pageText("Zoom Password")},
    };
    for (auto it = labels.cbegin(); it != labels.cend(); ++it)
    {
        if (QLabel* const label = m_fieldLabels.value(it.key()))
        {
            label->setText(it.value());
        }
    }
    m_keyboardButton->setToolTip(
        pageText("Open Korean / English on-screen keyboard")
        );
    m_keyboardButton->setAccessibleName(pageText("Korean Keyboard"));
    m_birthdayEdit->setEmptyText(pageText("Not set"));

    for (const QString& field : {
             QStringLiteral("internetType"),
             QStringLiteral("projectionType")
         })
    {
        auto* const combo = qobject_cast<QComboBox*>(m_fields.value(field));
        if (!combo)
        {
            continue;
        }
        const QString currentData = combo->currentData().toString();
        const QString currentText = combo->currentText();
        const QSignalBlocker blocker(combo);
        combo->clear();
        const QStringList values = field == QStringLiteral("internetType")
            ? QStringList{
                  QStringLiteral("WiFi"), QStringLiteral("LAN"),
                  QStringLiteral("Both"), QStringLiteral("N/A")
              }
            : QStringList{
                  QStringLiteral("HDMI"), QStringLiteral("Zoom"),
                  QStringLiteral("Any"), QStringLiteral("N/A")
              };
        for (const QString& value : values)
        {
            const QString label = value == QStringLiteral("Both")
                ? pageText("Both")
                : value == QStringLiteral("Any")
                    ? pageText("Any")
                    : value == QStringLiteral("N/A")
                        ? pageText("N/A") : value;
            combo->addItem(label, value);
        }
        int index = combo->findData(currentData);
        if (index < 0 && !currentText.isEmpty())
        {
            combo->addItem(currentText, currentText);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index < 0 ? 0 : index);
    }
    updatePageState();
}

void TeacherProfilePage::changeEvent(QEvent* event)
{
    Page::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
        showValidation({});
        updateValidation();
    }
    else if (event->type() == QEvent::StyleChange)
    {
        updateKeyboardTriggerTheme();
    }
}

void TeacherProfilePage::updateKeyboardTriggerTheme()
{
    if (!m_keyboardButton)
    {
        return;
    }

    const QString stylesheet = qApp->styleSheet();
    const bool hasExplicitStylesheetTheme = !stylesheet.isEmpty();
    const bool dark = hasExplicitStylesheetTheme
        ? stylesheet.contains(QStringLiteral("#2b2b2b"), Qt::CaseInsensitive)
        : m_keyboardButton->palette()
              .color(QPalette::ButtonText).lightness() > 128;
    m_keyboardButton->setProperty(
        "theme",
        dark ? QStringLiteral("dark") : QStringLiteral("light")
        );
}

void TeacherProfilePage::buildUi()
{
    auto* const root = new QVBoxLayout(this);
    root->setContentsMargins(11, 11, 11, 11);
    root->setSpacing(0);

    auto* const scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("teacherProfileScrollArea"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(scroll, 1);

    auto* const scrollContents = new QWidget(scroll);
    auto* const contentLayout = new QVBoxLayout(scrollContents);
    contentLayout->setContentsMargins(
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin,
        UiConstants::Pages::Margin
        );
    contentLayout->setSpacing(UiConstants::Pages::Spacing);
    contentLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(scrollContents);

    m_pageHeader = new QWidget(scrollContents);
    auto* const headerLayout = new QVBoxLayout(m_pageHeader);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(UiConstants::Pages::HeaderSpacing);
    auto* const titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(UiConstants::Pages::HeaderSpacing);
    m_titleLabel = new QLabel(m_pageHeader);
    m_titleLabel->setObjectName(QStringLiteral("pageTitle"));
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(UiConstants::Pages::TitleFontSize);
    titleFont.setWeight(QFont::Bold);
    titleFont = fontWithFamilyFragment(titleFont, QStringLiteral("Inter"));
    m_titleLabel->setFont(titleFont);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch();
    m_keyboardButton = new QPushButton(m_pageHeader);
    m_keyboardButton->setObjectName(
        QStringLiteral("teacherProfileKoreanKeyboardButton")
        );
    m_keyboardButton->setMinimumSize(44, 40);
    m_keyboardButton->setMaximumWidth(52);
    titleRow->addWidget(m_keyboardButton);
    headerLayout->addLayout(titleRow);
    m_subtitleLabel = new QLabel(m_pageHeader);
    m_subtitleLabel->setObjectName(QStringLiteral("pageSubtitle"));
    m_subtitleLabel->setWordWrap(true);
    QFont subtitleFont = m_subtitleLabel->font();
    subtitleFont.setPointSize(UiConstants::Pages::SubtitleFontSize);
    subtitleFont = fontWithFamilyFragment(
        subtitleFont, QStringLiteral("Inter")
        );
    m_subtitleLabel->setFont(subtitleFont);
    headerLayout->addWidget(m_subtitleLabel);
    contentLayout->addWidget(m_pageHeader);
    contentLayout->addSpacing(UiConstants::Pages::HeaderContentSpacing);

    m_noSelectionLabel = new QLabel(scrollContents);
    m_noSelectionLabel->setObjectName(
        QStringLiteral("teacherProfileNoSelectionState")
        );
    m_noSelectionLabel->setWordWrap(true);
    contentLayout->addWidget(m_noSelectionLabel);

    m_formContainer = new QWidget(scrollContents);
    m_formContainer->setObjectName(QStringLiteral("teacherProfileForm"));
    auto* const formLayout = new QVBoxLayout(m_formContainer);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(UiConstants::Pages::Spacing);

    auto* const details = new TeacherProfileSectionCard({}, m_formContainer);
    details->setObjectName(QStringLiteral("teacherProfileDetailsCard"));
    m_detailsGroup = details;
    auto* const detailsGrid = new QGridLayout;
    detailsGrid->setHorizontalSpacing(16);
    detailsGrid->setVerticalSpacing(4);
    addLineField(*detailsGrid, QStringLiteral("teacherEn"), {},
                 QStringLiteral("teacherEnEdit"), 0, 0);
    addLineField(*detailsGrid, QStringLiteral("teacherKr"), {},
                 QStringLiteral("teacherKrEdit"), 0, 1);
    addLineField(*detailsGrid, QStringLiteral("preferredRomanization"), {},
                 QStringLiteral("preferredRomanizationEdit"), 0, 2);
    addComboField(*detailsGrid, QStringLiteral("preferredName"), {}, {}, false,
                  0, 3);
    addLineField(*detailsGrid, QStringLiteral("roomNumber"), {},
                 QStringLiteral("roomNumberEdit"), 4, 0);
    addBirthdayField(*detailsGrid, 4, 1);
    addLineField(*detailsGrid, QStringLiteral("phoneNumber"), {},
                 QStringLiteral("phoneNumberEdit"), 4, 2);
    detailsGrid->addItem(
        new QSpacerItem(
            0,
            UiConstants::ClassInfo::Form::GroupSpacerHeight,
            QSizePolicy::Minimum,
            QSizePolicy::Fixed
            ),
        3,
        0,
        1,
        4
        );
    detailsGrid->setColumnStretch(0, 0);
    detailsGrid->setColumnStretch(1, 0);
    detailsGrid->setColumnStretch(2, 0);
    detailsGrid->setColumnStretch(3, 0);
    detailsGrid->setColumnStretch(4, 1);
    static_cast<TeacherProfileSectionCard*>(m_detailsGroup)
        ->contentLayout()->addLayout(detailsGrid);
    formLayout->addWidget(details);

    auto* const connectivity = new TeacherProfileSectionCard({}, m_formContainer);
    connectivity->setObjectName(
        QStringLiteral("teacherProfileConnectivityCard")
        );
    m_connectivityGroup = connectivity;
    auto* const connectivityGrid = new QGridLayout;
    connectivityGrid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );
    connectivityGrid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );
    addComboField(*connectivityGrid, QStringLiteral("internetType"), {},
                  {QStringLiteral("WiFi"), QStringLiteral("LAN"),
                   QStringLiteral("Both"), QStringLiteral("N/A")}, false,
                  0, 0);
    addLineField(*connectivityGrid, QStringLiteral("wifiName"), {},
                 QStringLiteral("wifiNameEdit"), 0, 1);
    addLineField(*connectivityGrid, QStringLiteral("wifiPassword"), {},
                 QStringLiteral("wifiPasswordEdit"), 0, 2);
    addComboField(*connectivityGrid, QStringLiteral("projectionType"), {},
                  {QStringLiteral("HDMI"), QStringLiteral("Zoom"),
                   QStringLiteral("Any"), QStringLiteral("N/A")}, false,
                  4, 0);
    addLineField(*connectivityGrid, QStringLiteral("zoomId"), {},
                 QStringLiteral("zoomIdEdit"), 4, 1);
    addLineField(*connectivityGrid, QStringLiteral("zoomPassword"), {},
                 QStringLiteral("zoomPasswordEdit"), 4, 2);
    connectivityGrid->addItem(
        new QSpacerItem(
            0,
            UiConstants::ClassInfo::Form::GroupSpacerHeight,
            QSizePolicy::Minimum,
            QSizePolicy::Fixed
            ),
        3,
        0,
        1,
        4
        );
    connectivityGrid->setColumnStretch(
        0, UiConstants::ClassInfo::Teacher::ColumnStretch);
    connectivityGrid->setColumnStretch(
        1, UiConstants::ClassInfo::Teacher::ColumnStretch);
    connectivityGrid->setColumnStretch(
        2, UiConstants::ClassInfo::Teacher::ColumnStretch);
    connectivityGrid->setColumnStretch(
        3, UiConstants::ClassInfo::Teacher::FillerColumnStretch);
    static_cast<TeacherProfileSectionCard*>(m_connectivityGroup)
        ->contentLayout()->addLayout(connectivityGrid);
    formLayout->addWidget(connectivity);

    auto* const notes = new TeacherProfileSectionCard({}, m_formContainer);
    notes->setObjectName(QStringLiteral("teacherProfileNotesCard"));
    m_notesGroup = notes;
    addNotesField(*static_cast<TeacherProfileSectionCard*>(m_notesGroup)
                       ->contentLayout());
    formLayout->addWidget(notes);
    contentLayout->addWidget(m_formContainer);

    auto* const footer = new QWidget(this);
    auto* const footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(16, 8, 16, 8);
    footerLayout->setSpacing(8);
    m_statusLabel = new QLabel(footer);
    m_statusLabel->setObjectName(QStringLiteral("teacherProfileSaveStatus"));
    m_statusLabel->setWordWrap(true);
    footerLayout->addWidget(m_statusLabel, 1);
    m_saveButton = new QPushButton(footer);
    m_saveButton->setObjectName(QStringLiteral("teacherProfileSaveButton"));
    footerLayout->addWidget(m_saveButton);
    root->addWidget(footer);

    m_onScreenKeyboard = new OnScreenKeyboard(this);
    const auto iconFor = [this](const QString& logicalId) {
        const auto resourceId = Resources::ResourceId::fromString(logicalId);
        if (!resourceId)
        {
            return QIcon{};
        }
        const auto request = Resources::ResourceRequest::create(
            *resourceId,
            Resources::ResourceScopeKind::Page,
            QStringLiteral("teacher-profile-keyboard-icons")
            );
        if (!request)
        {
            return QIcon{};
        }
        auto image = resourceContext().resourceLoader().loadImage(*request);
        if (!image || image->get().isNull())
        {
            return QIcon{};
        }
        return QIcon(QPixmap::fromImage(image->get()));
    };
    const QIcon lightKeyboardIcon = iconFor(
        QStringLiteral("resource:assets/icons/keyboard_light.svg")
        );
    const QIcon darkKeyboardIcon = iconFor(
        QStringLiteral("resource:assets/icons/keyboard_dark.svg")
        );
    if (!lightKeyboardIcon.isNull() && !darkKeyboardIcon.isNull())
    {
        m_onScreenKeyboard->setTriggerIcons(
            lightKeyboardIcon,
            darkKeyboardIcon
            );
    }
    updateKeyboardTriggerTheme();
    m_onScreenKeyboard->setTriggerButton(m_keyboardButton);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(AutosaveDebounceMilliseconds);
    connect(m_saveTimer, &QTimer::timeout, this, [this] {
        static_cast<void>(saveProfile(false));
    });
    connect(m_saveButton, &QPushButton::clicked, this, [this] {
        static_cast<void>(saveProfile(true));
    });
    connect(m_keyboardButton, &QPushButton::clicked, this, [this] {
        m_onScreenKeyboard->showForFocusScope(m_formContainer);
    });
}

void TeacherProfilePage::addLineField(
    QGridLayout& layout,
    const QString& field,
    const QString& label,
    const QString& objectName,
    const int labelRow,
    const int column
    )
{
    auto* const input = new QLineEdit(m_formContainer);
    input->setObjectName(objectName);
    if (field == QStringLiteral("teacherKr"))
    {
        QFont koreanFont = fontWithFamilyFragment(
            input->font(), QStringLiteral("Pretendard")
            );
        koreanFont.setPointSize(TeacherKoreanFontPointSize);
        input->setFont(koreanFont);
    }

    int preferredWidth = UiConstants::ClassInfo::Teacher::FieldMinWidth;
    bool fixedWidth = false;
    if (field == QStringLiteral("teacherKr")
        || field == QStringLiteral("teacherEn"))
    {
        preferredWidth = field == QStringLiteral("teacherKr")
            ? TeacherKoreanNameFieldWidth : TeacherNameFieldWidth;
        fixedWidth = true;
    }
    else if (field == QStringLiteral("preferredRomanization"))
    {
        preferredWidth = TeacherPreferredRomanizationFieldWidth;
        fixedWidth = true;
    }
    else if (field == QStringLiteral("roomNumber"))
    {
        preferredWidth = TeacherRoomNumberFieldWidth;
        fixedWidth = true;
    }
    else if (field == QStringLiteral("phoneNumber"))
    {
        preferredWidth = TeacherPhoneNumberFieldWidth;
        fixedWidth = true;
    }
    fixedWidth
        ? setFixedFieldWidth(input, preferredWidth)
        : setMinimumFieldWidth(input, preferredWidth);
    m_fields.insert(field, input);

    auto* const fieldLabel = new QLabel(label, m_formContainer);
    fieldLabel->setObjectName(field + QStringLiteral("Label"));
    fieldLabel->setContentsMargins(4, 0, 0, 0);
    m_fieldLabels.insert(field, fieldLabel);
    layout.addWidget(fieldLabel, labelRow, column);
    layout.addWidget(input, labelRow + 1, column);
    layout.addWidget(
        createValidationLabel(field),
        labelRow + 2,
        column
        );

    connect(input, &QLineEdit::textChanged, this, [this, field] {
        if (field == QStringLiteral("teacherEn")
            || field == QStringLiteral("preferredRomanization"))
        {
            updatePreferredNameChoices();
        }
        handleFieldChanged();
    });
}

void TeacherProfilePage::addComboField(
    QGridLayout& layout,
    const QString& field,
    const QString& label,
    const QStringList& values,
    const bool editable,
    const int labelRow,
    const int column
    )
{
    auto* const combo = new QComboBox(m_formContainer);
    combo->setObjectName(field + QStringLiteral("Combo"));
    combo->setEditable(editable);
    for (const QString& value : values)
    {
        combo->addItem(value, value);
    }
    m_fields.insert(field, combo);

    auto* const fieldLabel = new QLabel(label, m_formContainer);
    fieldLabel->setObjectName(field + QStringLiteral("Label"));
    fieldLabel->setContentsMargins(4, 0, 0, 0);
    m_fieldLabels.insert(field, fieldLabel);
    const bool profileDetails = field == QStringLiteral("preferredName");
    if (profileDetails)
    {
        setFixedFieldWidth(combo, TeacherPreferredNameFieldWidth);
    }
    else
    {
        setMinimumFieldWidth(
            combo,
            UiConstants::ClassInfo::Teacher::FieldMinWidth
            );
    }
    layout.addWidget(fieldLabel, labelRow, column);
    layout.addWidget(combo, labelRow + 1, column);
    layout.addWidget(
        createValidationLabel(field),
        labelRow + 2,
        column
        );
    connect(combo, &QComboBox::currentTextChanged, this, [this] {
        handleFieldChanged();
    });
}

void TeacherProfilePage::addBirthdayField(
    QGridLayout& layout,
    const int labelRow,
    const int column
    )
{
    m_birthdayEdit = new TeacherBirthdayDateEdit(m_formContainer);
    m_birthdayEdit->setObjectName(QStringLiteral("birthdayEdit"));
    setFixedFieldWidth(m_birthdayEdit, TeacherKoreanNameFieldWidth);
    m_fields.insert(QStringLiteral("birthday"), m_birthdayEdit);

    auto* const fieldLabel = new QLabel(m_formContainer);
    fieldLabel->setObjectName(QStringLiteral("birthdayLabel"));
    fieldLabel->setContentsMargins(4, 0, 0, 0);
    m_fieldLabels.insert(QStringLiteral("birthday"), fieldLabel);
    layout.addWidget(fieldLabel, labelRow, column);
    layout.addWidget(m_birthdayEdit, labelRow + 1, column);
    layout.addWidget(
        createValidationLabel(QStringLiteral("birthday")),
        labelRow + 2,
        column
        );
    connect(m_birthdayEdit, &QLineEdit::textChanged, this, [this] {
        handleFieldChanged();
    });
}

void TeacherProfilePage::addNotesField(QVBoxLayout& layout)
{
    auto* const input = new QPlainTextEdit(m_formContainer);
    input->setObjectName(QStringLiteral("teacherNotesEdit"));
    input->setMinimumHeight(180);
    m_fields.insert(QStringLiteral("notes"), input);
    layout.addWidget(fieldContainer(QStringLiteral("notes"), input));
    connect(input, &QPlainTextEdit::textChanged, this, [this] {
        handleFieldChanged();
    });
}

QWidget* TeacherProfilePage::fieldContainer(
    const QString& field,
    QWidget* input
    )
{
    auto* const container = new QWidget(m_formContainer);
    auto* const layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(input);
    layout->addWidget(createValidationLabel(field));
    return container;
}

QLabel* TeacherProfilePage::createValidationLabel(const QString& field)
{
    auto* const validation = new QLabel(m_formContainer);
    validation->setObjectName(field + QStringLiteral("ValidationMessage"));
    validation->setWordWrap(true);
    validation->hide();
    m_validationLabels.insert(field, validation);
    return validation;
}

void TeacherProfilePage::updatePreferredNameChoices()
{
    auto* const combo = qobject_cast<QComboBox*>(
        m_fields.value(QStringLiteral("preferredName"))
        );
    if (!combo)
    {
        return;
    }
    const QString current = combo->currentText();
    const QString english = qobject_cast<QLineEdit*>(
        m_fields.value(QStringLiteral("teacherEn"))
        )->text().trimmed();
    const QString romanization = qobject_cast<QLineEdit*>(
        m_fields.value(QStringLiteral("preferredRomanization"))
        )->text().trimmed();
    QStringList choices;
    for (const QString& value : {english, romanization})
    {
        if (!value.isEmpty() && !choices.contains(value))
        {
            choices.append(value);
        }
    }
    std::vector<std::u16string> orderedChoices;
    orderedChoices.reserve(static_cast<std::size_t>(choices.size()));
    for (const QString& choice : choices)
    {
        orderedChoices.push_back(choice.toStdU16String());
    }
    const std::optional<std::size_t> selected =
        Application::selectPreferredNameChoiceIndex(
            orderedChoices,
            m_savedFields.preferredName,
            current.toStdU16String(),
            m_loading
                ? Application::PreferredNameSelectionContext::Load
                : Application::PreferredNameSelectionContext::Edit
            );

    const QSignalBlocker blocker(combo);
    combo->clear();
    for (const QString& value : choices)
    {
        combo->addItem(value, value);
    }
    combo->setEnabled(!choices.isEmpty());
    combo->setCurrentIndex(selected
        ? static_cast<int>(*selected) : -1);
}

void TeacherProfilePage::handleFieldChanged()
{
    if (m_loading || !m_teacherId || m_reloadRequired)
    {
        return;
    }
    clearPersistenceStatus();
    updateValidation();
    m_dirty = fieldsDifferFromSaved();
    updatePageState();
    if (m_saveMode == Application::SaveMode::Automatic
        && m_dirty
        && !m_hasBlockingErrors)
    {
        m_saveTimer->start();
    }
    else
    {
        m_saveTimer->stop();
    }
}

void TeacherProfilePage::updateValidation()
{
    if (!m_teacherId || !m_actions.validate)
    {
        showValidation({});
        m_hasBlockingErrors = false;
        return;
    }
    try
    {
        const Domain::TeacherProfile profile{
            .id = *m_teacherId,
            .fields = fieldsFromForm()
        };
        const Application::TeacherProfileValidationResult result =
            m_actions.validate(profile);
        showValidation(result.issues);
        m_hasBlockingErrors = std::any_of(
            result.issues.begin(), result.issues.end(),
            [](const Application::TeacherProfileValidationIssue& issue) {
                return issue.severity
                    != Application::TeacherProfileValidationSeverity::Warning;
            }
            );
    }
    catch (...)
    {
        Application::TeacherProfileValidationIssue issue{
            .code = Application::TeacherProfileValidationCode(
                "teacher.validation.unavailable"
                ),
            .field = Application::TeacherProfileValidationField(""),
            .severity = Application::TeacherProfileValidationSeverity::Error
        };
        showValidation({issue});
        m_hasBlockingErrors = true;
    }
}

void TeacherProfilePage::showValidation(
    const std::vector<Application::TeacherProfileValidationIssue>& issues
    )
{
    for (auto it = m_validationLabels.begin(); it != m_validationLabels.end(); ++it)
    {
        it.value()->clear();
        it.value()->hide();
        it.value()->setProperty("formValidationSeverity", QVariant());
        it.value()->setStyleSheet({});
    }

    QHash<QString, QStringList> messages;
    QHash<QString, bool> errors;
    bool hasWarning = false;
    bool hasError = false;
    for (const Application::TeacherProfileValidationIssue& issue : issues)
    {
        const QString field = QString::fromStdString(issue.field.value());
        hasWarning = hasWarning
            || issue.severity == Application::TeacherProfileValidationSeverity::Warning;
        hasError = hasError
            || issue.severity != Application::TeacherProfileValidationSeverity::Warning;
        if (m_validationLabels.contains(field))
        {
            messages[field].append(validationMessage(issue));
            errors[field] = errors.value(field)
                || issue.severity != Application::TeacherProfileValidationSeverity::Warning;
        }
    }
    for (auto it = messages.cbegin(); it != messages.cend(); ++it)
    {
        QLabel* const label = m_validationLabels.value(it.key());
        if (!label)
        {
            continue;
        }
        label->setText(it.value().join(QStringLiteral("\n")));
        const bool error = errors.value(it.key());
        label->setProperty("formValidationSeverity", error
            ? QStringLiteral("error") : QStringLiteral("warning"));
        label->setStyleSheet(error
            ? QStringLiteral("color: #b3261e;")
            : QStringLiteral("color: #8a5a00;"));
        label->show();
    }

    if (m_validationStatusActive)
    {
        m_statusLabel->clear();
        m_validationStatusActive = false;
    }
    if (!m_teacherId || !issues.size())
    {
        return;
    }
    if (hasError)
    {
        m_statusLabel->setText(pageText("Fix the highlighted errors before saving."));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #b3261e;"));
        m_validationStatusActive = true;
    }
    else if (hasWarning)
    {
        m_statusLabel->setText(pageText("Review the highlighted warnings. You can still save."));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #8a5a00;"));
        m_validationStatusActive = true;
    }
}

QString TeacherProfilePage::validationMessage(
    const Application::TeacherProfileValidationIssue& issue
    ) const
{
    const QString code = QString::fromStdString(issue.code.value());
    if (code.endsWith(QStringLiteral(".required")))
    {
        return pageText("This field is required.");
    }
    if (code == QStringLiteral("validation.length.out_of_bounds"))
    {
        int maximum = 0;
        for (const Application::TeacherProfileValidationArgument& argument
             : issue.arguments.values())
        {
            if (argument.key.value() == "maximum")
            {
                maximum = QString::fromStdU16String(argument.value).toInt();
            }
        }
        if (maximum > 0)
        {
            return pageText("Enter no more than %1 characters.").arg(maximum);
        }
    }
    if (code.contains(QStringLiteral("enum"))
        || code.contains(QStringLiteral("invalid_choice"))
        || code.contains(QStringLiteral("not_allowed")))
    {
        return pageText("Choose a listed value.");
    }
    if (issue.severity == Application::TeacherProfileValidationSeverity::Warning)
    {
        return pageText("Review this value.");
    }
    if (code == QStringLiteral("teacher.validation.unavailable"))
    {
        return pageText("Validation is temporarily unavailable.");
    }
    return pageText("Enter a valid value.");
}

Domain::TeacherProfileFields TeacherProfilePage::fieldsFromForm() const
{
    const auto lineText = [this](const QString& key) {
        const auto* const line = qobject_cast<QLineEdit*>(m_fields.value(key));
        return line ? line->text().trimmed().toStdU16String() : std::u16string{};
    };
    const auto comboText = [this](const QString& key) {
        const auto* const combo = qobject_cast<QComboBox*>(m_fields.value(key));
        return combo ? combo->currentText().trimmed().toStdU16String()
                     : std::u16string{};
    };
    const auto notes = qobject_cast<QPlainTextEdit*>(
        m_fields.value(QStringLiteral("notes"))
        );
    return {
        .teacherKr = lineText(QStringLiteral("teacherKr")),
        .teacherEn = lineText(QStringLiteral("teacherEn")),
        .preferredRomanization = lineText(QStringLiteral("preferredRomanization")),
        .preferredName = comboText(QStringLiteral("preferredName")),
        .roomNumber = lineText(QStringLiteral("roomNumber")),
        .birthday = m_birthdayEdit
            ? m_birthdayEdit->birthday() : std::u16string{},
        .phoneNumber = lineText(QStringLiteral("phoneNumber")),
        .wifiName = lineText(QStringLiteral("wifiName")),
        .wifiPassword = lineText(QStringLiteral("wifiPassword")),
        .internetType = comboText(QStringLiteral("internetType")),
        .zoomId = lineText(QStringLiteral("zoomId")),
        .zoomPassword = lineText(QStringLiteral("zoomPassword")),
        .projectionType = comboText(QStringLiteral("projectionType")),
        .notes = notes ? notes->toPlainText().trimmed().toStdU16String()
                       : std::u16string{}
    };
}

void TeacherProfilePage::populateForm(
    const Domain::TeacherProfileFields& fields
    )
{
    const auto setLine = [this](const QString& key, const std::u16string& value) {
        if (auto* const line = qobject_cast<QLineEdit*>(m_fields.value(key)))
        {
            line->setText(QString::fromStdU16String(value));
        }
    };
    setLine(QStringLiteral("teacherKr"), fields.teacherKr);
    setLine(QStringLiteral("teacherEn"), fields.teacherEn);
    setLine(QStringLiteral("preferredRomanization"), fields.preferredRomanization);
    setLine(QStringLiteral("roomNumber"), fields.roomNumber);
    m_birthdayEdit->setBirthday(fields.birthday);
    setLine(QStringLiteral("phoneNumber"), fields.phoneNumber);
    setLine(QStringLiteral("wifiName"), fields.wifiName);
    setLine(QStringLiteral("wifiPassword"), fields.wifiPassword);
    setLine(QStringLiteral("zoomId"), fields.zoomId);
    setLine(QStringLiteral("zoomPassword"), fields.zoomPassword);
    if (auto* const preferred = qobject_cast<QComboBox*>(
            m_fields.value(QStringLiteral("preferredName"))))
    {
        updatePreferredNameChoices();
        preferred->setCurrentText(QString::fromStdU16String(fields.preferredName));
    }
    for (const auto& [key, value] : {
             std::pair{QStringLiteral("internetType"), fields.internetType},
             std::pair{QStringLiteral("projectionType"), fields.projectionType}
         })
    {
        auto* const combo = qobject_cast<QComboBox*>(m_fields.value(key));
        if (!combo)
        {
            continue;
        }
        const QString text = QString::fromStdU16String(value);
        int index = combo->findData(text);
        if (index < 0 && !text.isEmpty())
        {
            combo->addItem(text, text);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index < 0 ? 0 : index);
    }
    if (auto* const notes = qobject_cast<QPlainTextEdit*>(
            m_fields.value(QStringLiteral("notes"))))
    {
        notes->setPlainText(QString::fromStdU16String(fields.notes));
    }
}

void TeacherProfilePage::updatePageState()
{
    const bool selected = m_teacherId.has_value();
    m_noSelectionLabel->setVisible(!selected);
    m_formContainer->setVisible(selected);
    if (selected)
    {
        const auto* const english = qobject_cast<QLineEdit*>(
            m_fields.value(QStringLiteral("teacherEn"))
            );
        const auto* const korean = qobject_cast<QLineEdit*>(
            m_fields.value(QStringLiteral("teacherKr"))
            );
        const QString name = !english->text().trimmed().isEmpty()
            ? english->text().trimmed() : korean->text().trimmed();
        m_titleLabel->setText(name.isEmpty()
            ? pageText("Teacher Information")
            : pageText("Teacher Information for %1").arg(name));
    }
    const bool editable = selected && !m_reloadRequired;
    for (QWidget* const widget : m_fields)
    {
        widget->setEnabled(editable);
    }
    m_keyboardButton->setEnabled(editable);
    m_saveButton->setVisible(m_saveMode == Application::SaveMode::Manual);
    m_saveButton->setEnabled(
        editable && hasUnsavedChanges() && !m_hasBlockingErrors
        );
}

bool TeacherProfilePage::fieldsDifferFromSaved() const
{
    return m_teacherId.has_value()
        && comparisonFields(fieldsFromForm()) != m_savedFields;
}

PageOperationResult<void> TeacherProfilePage::saveProfile(
    const bool interactive
    )
{
    m_saveTimer->stop();
    if (!m_teacherId)
    {
        return std::unexpected(PageOperationError{
            pageText("Select a teacher before saving.")
        });
    }
    if (m_reloadRequired)
    {
        return std::unexpected(PageOperationError{
            pageText("Reload this teacher before editing or saving again.")
        });
    }
    if (!hasUnsavedChanges())
    {
        return {};
    }

    updateValidation();
    if (m_hasBlockingErrors)
    {
        return std::unexpected(PageOperationError{
            pageText("Fix the highlighted errors before saving.")
        });
    }
    if (!m_actions.edit)
    {
        const QString message = pageText("Teacher profile editing is unavailable.");
        setPersistenceStatus(message, true);
        return std::unexpected(PageOperationError{message});
    }

    const Domain::TeacherProfileFields submitted = fieldsFromForm();
    const Application::TeacherProfileEditRequest request{
        .id = m_teacherId,
        .fields = submitted
    };
    Application::TeacherProfileEditResult result = std::unexpected(
        Application::TeacherProfileEditFailure{
            .kind = Application::TeacherProfileEditFailureKind::UpdateFailed,
            .cause = {
                .code = Domain::ErrorCode::Technical,
                .message = "Teacher profile edit did not return a result.",
                .recoverable = false
            }
        }
        );
    try
    {
        result = m_actions.edit(request);
    }
    catch (...)
    {
        result = std::unexpected(Application::TeacherProfileEditFailure{
            .kind = Application::TeacherProfileEditFailureKind::UpdateFailed,
            .cause = {
                .code = Domain::ErrorCode::Technical,
                .message = "Teacher profile editing failed unexpectedly.",
                .recoverable = false
            }
        });
    }
    if (!result)
    {
        const Application::TeacherProfileEditFailure& failure = result.error();
        if (failure.kind
            == Application::TeacherProfileEditFailureKind::ValidationFailed)
        {
            showValidation(failure.validationIssues);
            m_hasBlockingErrors = std::any_of(
                failure.validationIssues.begin(), failure.validationIssues.end(),
                [](const Application::TeacherProfileValidationIssue& issue) {
                    return issue.severity
                        != Application::TeacherProfileValidationSeverity::Warning;
                }
                );
        }
        if (failure.kind == Application::TeacherProfileEditFailureKind::ReloadFailed
            && failure.writeSucceeded)
        {
            const Domain::TeacherProfile validationProfile{
                .id = *m_teacherId,
                .fields = submitted
            };
            Domain::TeacherProfileFields persisted = submitted;
            if (m_actions.validate)
            {
                try
                {
                    persisted = m_actions.validate(validationProfile).normalizedFields;
                }
                catch (...)
                {
                    // The submitted fields remain visible and editing is
                    // locked because the database write already succeeded.
                }
            }
            m_savedFields = comparisonFields(std::move(persisted));
            m_reloadRequired = true;
            m_dirty = false;
            setPersistenceStatus(
                pageText(
                    "Changes were saved, but the latest teacher profile could not be reloaded. Navigate away and reopen this teacher before editing or saving again."
                    ),
                true,
                true
                );
        }
        else
        {
            const bool automaticFailure = !interactive;
            setPersistenceStatus(
                automaticFailure
                    ? pageText("Your changes could not be saved automatically. Your draft is still here.")
                    : pageText("The teacher profile could not be saved."),
                true
                );
        }

        updatePageState();
        if (interactive)
        {
            QMessageBox::warning(
                this,
                pageText("Save Teacher Profile"),
                m_statusLabel->text() + QStringLiteral("\n")
                    + stdMessage(failure.cause.message)
                );
        }
        return std::unexpected(PageOperationError{
            stdMessage(failure.cause.message)
        });
    }

    m_teacherId = result->id;
    m_savedFields = comparisonFields(result->fields);
    m_reloadRequired = false;
    m_dirty = false;
    m_loading = true;
    populateForm(result->fields);
    m_loading = false;
    clearPersistenceStatus();
    updateValidation();
    updatePageState();
    return {};
}

void TeacherProfilePage::setPersistenceStatus(
    QString text,
    const bool error,
    const bool reloadRequired
    )
{
    m_validationStatusActive = false;
    m_statusLabel->setText(std::move(text));
    m_statusLabel->setStyleSheet(error
        ? QStringLiteral("color: #b3261e;") : QString{});
    if (reloadRequired)
    {
        m_statusLabel->setProperty("teacherProfileReloadRequired", true);
    }
}

void TeacherProfilePage::clearPersistenceStatus()
{
    if (m_reloadRequired)
    {
        return;
    }
    m_statusLabel->clear();
    m_statusLabel->setStyleSheet({});
    m_statusLabel->setProperty("teacherProfileReloadRequired", false);
    m_validationStatusActive = false;
}

} // namespace ClassMngr::Next::Ui
