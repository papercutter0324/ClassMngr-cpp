#include "initial_setup_wizard.h"

#include "core/application_services.h"
#include "data/data_service.h"
#include "domain/models/teacher.h"
#include "features/classes/config/class_info_config.h"
#include "features/my_info/data/personal_details_repository.h"
#include "features/my_info/data/signature_image_processor.h"
#include "features/schedule/ui/schedule_import_dialog.h"
#include "features/teacher/ui/teacher_import_dialog.h"
#include "core/utils/colorutils.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/widgets/clickable_color_preview.h"
#include "ui/shared/widgets/sections/class_schedule_section.h"
#include "ui/shared/widgets/text_fit_push_button.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace
{
InitialSetupWizard* setupWizard(const QWizardPage* page)
{
    return qobject_cast<InitialSetupWizard*>(page->window());
}

QLabel* explanatoryLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    return label;
}

bool showMissingFields(
    QWidget* parent,
    const QString& title,
    const QStringList& fields)
{
    if (fields.isEmpty())
    {
        return false;
    }

    QMessageBox::information(
        parent,
        title,
        QCoreApplication::translate(
            "InitialSetupWizard",
            "Complete the following required fields before continuing:\n\n- %1")
            .arg(fields.join(QStringLiteral("\n- "))));
    return true;
}

class ResourcesPage final : public QWizardPage
{
public:
    explicit ResourcesPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Select the Resources You Have"));

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(explanatoryLabel(
            tr("ClassMngr can automatically import your schedule and/or a list of Korean teachers (and Front Desk staff). Please mark below which documents you have. (You can also import/update these at a later time from the menu bar.)"),
            this));

        m_teacherList = new QCheckBox(
            tr("List of Korean Teachers"), this);
        m_teacherList->setObjectName(QStringLiteral("setupHasTeacherList"));
        m_schedule = new QCheckBox(
            tr("My Schedule"), this);
        m_schedule->setObjectName(QStringLiteral("setupHasSchedule"));

        layout->addSpacing(12);
        layout->addWidget(m_teacherList);
        layout->addWidget(m_schedule);
        layout->addStretch();

        registerField(QStringLiteral("hasTeacherList"), m_teacherList);
        registerField(QStringLiteral("hasSchedule"), m_schedule);
    }

    int nextId() const override
    {
        if (m_teacherList->isChecked())
        {
            return InitialSetupWizard::TeacherImportPage;
        }
        if (m_schedule->isChecked())
        {
            return InitialSetupWizard::ScheduleImportPage;
        }
        return InitialSetupWizard::PersonalDetailsPage;
    }

private:
    QCheckBox* m_teacherList = nullptr;
    QCheckBox* m_schedule = nullptr;
};

class TeacherImportWizardPage final : public QWizardPage
{
public:
    explicit TeacherImportWizardPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Import the Korean teacher list"));
        setSubTitle(
            tr("Select the teacher workbook and review which groups and teachers should be imported."));

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(explanatoryLabel(
            tr("The teacher importer will open in a separate window. Return here after the import completes."),
            this));

        auto* importButton = new QPushButton(tr("Import Teachers..."), this);
        importButton->setObjectName(QStringLiteral("setupImportTeachersButton"));
        layout->addWidget(importButton, 0, Qt::AlignLeft);

        m_status = explanatoryLabel(
            tr("No teacher list has been imported yet."), this);
        m_status->setObjectName(QStringLiteral("setupTeacherImportStatus"));
        layout->addWidget(m_status);
        layout->addStretch();

        connect(importButton, &QPushButton::clicked, this, [this]()
        {
            auto* setup = setupWizard(this);
            if (!setup || !setup->dataService())
            {
                return;
            }

            TeacherImportDialog dialog(this);
            if (dialog.exec() != QDialog::Accepted)
            {
                return;
            }

            const Result<TeacherImportSummary> imported =
                setup->dataService()->importTeachers(dialog.importPlan());
            if (!imported)
            {
                QMessageBox::warning(
                    this, tr("Import Teachers"), imported.error());
                return;
            }

            m_imported = true;
            const TeacherImportSummary& summary = *imported;
            m_status->setText(
                tr("Import complete: %1 Korean teachers created, %2 updated, and %3 unchanged.")
                    .arg(summary.koreanTeachers.created)
                    .arg(summary.koreanTeachers.updated)
                    .arg(summary.koreanTeachers.unchanged));
            emit completeChanged();
        });
    }

    bool isComplete() const override
    {
        return true;
    }

    bool validatePage() override
    {
        return !showMissingFields(
            this,
            tr("Import Teachers"),
            m_imported
                ? QStringList{}
                : QStringList{tr("Korean Teacher List Import")});
    }

    int nextId() const override
    {
        const auto* setup = setupWizard(this);
        return setup && setup->wantsScheduleImport()
            ? InitialSetupWizard::ScheduleImportPage
            : InitialSetupWizard::PersonalDetailsPage;
    }

private:
    QLabel* m_status = nullptr;
    bool m_imported = false;
};

class ScheduleImportWizardPage final : public QWizardPage
{
public:
    explicit ScheduleImportWizardPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Import your schedule"));
        setSubTitle(
            tr("Schedules can be imported even when you did not import a separate Korean teacher list."));

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(explanatoryLabel(
            tr("Choose whether the workbook contains your regular or intensive schedule, then review the detected classes before importing."),
            this));

        auto* importButton = new QPushButton(tr("Import Schedule..."), this);
        importButton->setObjectName(QStringLiteral("setupImportScheduleButton"));
        layout->addWidget(importButton, 0, Qt::AlignLeft);

        m_status = explanatoryLabel(
            tr("No schedule has been imported yet."), this);
        m_status->setObjectName(QStringLiteral("setupScheduleImportStatus"));
        layout->addWidget(m_status);
        layout->addStretch();

        connect(importButton, &QPushButton::clicked, this, [this]()
        {
            auto* setup = setupWizard(this);
            if (!setup || !setup->services())
            {
                return;
            }

            ScheduleImportDialog dialog(setup->services(), this);
            if (dialog.exec() != QDialog::Accepted)
            {
                return;
            }

            m_imported = true;
            m_status->setText(tr("Schedule import complete."));
            emit completeChanged();
        });
    }

    bool isComplete() const override
    {
        return true;
    }

    bool validatePage() override
    {
        return !showMissingFields(
            this,
            tr("Import Schedule"),
            m_imported
                ? QStringList{}
                : QStringList{tr("Schedule Import")});
    }

    int nextId() const override
    {
        return InitialSetupWizard::PersonalDetailsPage;
    }

private:
    QLabel* m_status = nullptr;
    bool m_imported = false;
};

class PersonalDetailsWizardPage final : public QWizardPage
{
public:
    explicit PersonalDetailsWizardPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Add Your Information"));

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(explanatoryLabel(
            tr("Your Name (Required) - Used for Sub Prep, Speaking Evaluations, etc."),
            this));

        m_name = new QLineEdit(this);
        m_name->setObjectName(QStringLiteral("setupUserName"));
        layout->addWidget(m_name);
        layout->addSpacing(16);

        auto* signatureHeading = new QHBoxLayout;
        auto* signatureLabel = explanatoryLabel(
            tr("Your Signature (Optional) - Used for Speaking Evaulations"),
            this);
        auto* browse = new QPushButton(tr("Choose Signature"), this);
        browse->setObjectName(QStringLiteral("setupSignatureButton"));
        signatureHeading->addWidget(signatureLabel, 1);
        signatureHeading->addWidget(browse, 0, Qt::AlignRight);
        layout->addLayout(signatureHeading);

        m_signaturePreview = new QLabel(this);
        m_signaturePreview->setObjectName(QStringLiteral("signatureImagePreview"));
        m_signaturePreview->setAlignment(Qt::AlignCenter);
        m_signaturePreview->setWordWrap(true);
        m_signaturePreview->setFixedHeight(220);
        m_signaturePreview->setSizePolicy(
            QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(m_signaturePreview);
        layout->addStretch();

        updateSignaturePreview();

        connect(m_name, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
        connect(browse, &QPushButton::clicked, this, [this]()
        {
            const QString path = QFileDialog::getOpenFileName(
                this,
                tr("Choose Signature Image"),
                QString(),
                tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
            if (path.isEmpty())
            {
                return;
            }

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
            {
                QMessageBox::warning(
                    this,
                    tr("Signature Image"),
                    tr("The selected signature image could not be opened."));
                return;
            }

            const QByteArray prepared =
                SignatureImage::prepareForEmbedding(file.readAll());
            if (prepared.isEmpty())
            {
                QMessageBox::warning(
                    this,
                    tr("Signature Image"),
                    tr("The selected file is not a supported signature image."));
                return;
            }

            m_signature = prepared;
            updateSignaturePreview();
        });
    }

    void initializePage() override
    {
        auto* setup = setupWizard(this);
        if (!setup || !setup->dataService())
        {
            return;
        }

        const PersonalDetails details =
            PersonalDetailsRepository(setup->dataService()).load();
        if (m_name->text().trimmed().isEmpty())
        {
            m_name->setText(details.name);
        }
        if (m_signature.isEmpty() && !details.signatureImage.isEmpty())
        {
            m_signature = details.signatureImage;
            updateSignaturePreview();
        }
    }

    bool validatePage() override
    {
        if (showMissingFields(
                this,
                tr("Your Information"),
                m_name->text().trimmed().isEmpty()
                    ? QStringList{tr("Your Name")}
                    : QStringList{}))
        {
            return false;
        }

        auto* setup = setupWizard(this);
        if (!setup || !setup->dataService())
        {
            return false;
        }

        PersonalDetails details =
            PersonalDetailsRepository(setup->dataService()).load();
        details.name = m_name->text().trimmed();
        if (!m_signature.isEmpty())
        {
            details.signatureImage = m_signature;
        }

        if (!PersonalDetailsRepository(setup->dataService()).save(details))
        {
            QMessageBox::warning(
                this, tr("Initial Setup"),
                tr("Your personal information could not be saved."));
            return false;
        }
        return true;
    }

    int nextId() const override
    {
        const auto* setup = setupWizard(this);
        if (!setup)
        {
            return -1;
        }
        if (setup->wantsScheduleImport())
        {
            return InitialSetupWizard::CompletionPage;
        }
        if (!setup->dataService()
            || setup->dataService()->getAllTeachers().isEmpty())
        {
            return InitialSetupWizard::TeacherEntryPage;
        }
        return InitialSetupWizard::ClassDetailsPage;
    }

private:
    void updateSignaturePreview()
    {
        if (m_signature.isEmpty())
        {
            m_signaturePreview->setPixmap(QPixmap());
            m_signaturePreview->setText(
                tr("Preview\nPNG and JPEG images with transparent backgrounds work best.\nOther supported formats will be attempted to be converted."));
            return;
        }

        QPixmap signature;
        if (!signature.loadFromData(m_signature))
        {
            m_signature.clear();
            updateSignaturePreview();
            return;
        }

        const QSize previewSize =
            m_signaturePreview->contentsRect()
                .adjusted(16, 16, -16, -16)
                .size();
        m_signaturePreview->setText(QString());
        m_signaturePreview->setPixmap(signature.scaled(
            previewSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    }

    QLineEdit* m_name = nullptr;
    QLabel* m_signaturePreview = nullptr;
    QByteArray m_signature;
};

class TeacherEntryWizardPage final : public QWizardPage
{
public:
    explicit TeacherEntryWizardPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Add Korean Teacher(s)"));

        auto* outer = new QVBoxLayout(this);
        auto* scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget(scroll);
        auto* layout = new QVBoxLayout(content);

        auto* requiredGroup = new QGroupBox(tr("Required Information"), content);
        auto* required = new QFormLayout(requiredGroup);
        m_englishName = addLine(required, tr("English Name *"), "setupTeacherEnglishName");
        m_koreanName = addLine(required, tr("Korean Name *"), "setupTeacherKoreanName");
        m_spelling = addLine(required, tr("Preferred Spelling *"), "setupTeacherPreferredSpelling");
        m_room = addLine(required, tr("Room Number"), "setupTeacherRoom");
        layout->addWidget(requiredGroup);

        auto* note = explanatoryLabel(
            tr("* Enter values for at least 2 of these fields so ClassMngr can reliably identify and display the teacher."),
            content);
        layout->addWidget(note);
        layout->addSpacing(16);

        auto* optionalGroup = new QGroupBox(tr("Optional Details"), content);
        auto* optional = new QGridLayout(optionalGroup);
        m_preferredName = new QComboBox(optionalGroup);
        m_preferredName->setObjectName(QStringLiteral("setupTeacherPreferredName"));
        m_wifiName = new QLineEdit(optionalGroup);
        m_wifiPassword = new QLineEdit(optionalGroup);
        m_zoomId = new QLineEdit(optionalGroup);
        m_zoomPassword = new QLineEdit(optionalGroup);
        m_internet = new QComboBox(optionalGroup);
        m_internet->addItems({QStringLiteral("WiFi"), QStringLiteral("LAN"), QStringLiteral("Both"), QStringLiteral("N/A")});
        m_projection = new QComboBox(optionalGroup);
        m_projection->addItems({QStringLiteral("HDMI"), QStringLiteral("Zoom"), QStringLiteral("Any"), QStringLiteral("N/A")});
        optional->setHorizontalSpacing(16);
        optional->addWidget(new QLabel(tr("Preferred Name"), optionalGroup), 0, 0);
        optional->addWidget(m_preferredName, 0, 1);
        optional->setColumnStretch(1, 1);
        optional->setColumnStretch(3, 1);
        addGridRow(optional, 1, tr("Internet Type"), m_internet, tr("Projection Type"), m_projection);
        addGridRow(optional, 2, tr("WiFi Name"), m_wifiName, tr("WiFi Password"), m_wifiPassword);
        addGridRow(optional, 3, tr("Zoom ID"), m_zoomId, tr("Zoom Password"), m_zoomPassword);
        layout->addWidget(optionalGroup);
        layout->addStretch();
        scroll->setWidget(content);
        outer->addWidget(scroll);

        m_addAnother = new QPushButton(tr("Add Another Teacher"), this);
        m_addAnother->setObjectName(QStringLiteral("setupAddAnotherTeacherButton"));
        outer->addWidget(m_addAnother, 0, Qt::AlignRight);

        connect(m_addAnother, &QPushButton::clicked, this, [this]()
        {
            if (saveCurrentTeacher(true))
            {
                clearFields();
                m_englishName->setFocus();
            }
        });
        connect(m_englishName, &QLineEdit::textChanged,
                this, [this]() { updatePreferredNameChoices(); });
        connect(m_spelling, &QLineEdit::textChanged,
                this, [this]() { updatePreferredNameChoices(); });
    }

    bool validatePage() override
    {
        if (currentFieldsEmpty())
        {
            auto* setup = setupWizard(this);
            if (setup && setup->dataService()
                && !setup->dataService()->getAllTeachers().isEmpty())
            {
                return true;
            }
        }
        return saveCurrentTeacher(false);
    }

    int nextId() const override
    {
        return InitialSetupWizard::ClassDetailsPage;
    }

private:
    static QLineEdit* addLine(
        QFormLayout* layout,
        const QString& label,
        const char* objectName)
    {
        auto* edit = new QLineEdit;
        edit->setObjectName(QString::fromLatin1(objectName));
        layout->addRow(label, edit);
        return edit;
    }

    static void addGridRow(
        QGridLayout* layout,
        int row,
        const QString& firstLabel,
        QWidget* first,
        const QString& secondLabel,
        QWidget* second)
    {
        layout->addWidget(new QLabel(firstLabel), row, 0);
        layout->addWidget(first, row, 1);
        layout->addWidget(new QLabel(secondLabel), row, 2);
        layout->addWidget(second, row, 3);
    }

    bool currentFieldsEmpty() const
    {
        return m_englishName->text().trimmed().isEmpty()
            && m_koreanName->text().trimmed().isEmpty()
            && m_spelling->text().trimmed().isEmpty()
            && m_room->text().trimmed().isEmpty();
    }

    void updatePreferredNameChoices()
    {
        Teacher teacher;
        teacher.teacherEn = m_englishName->text();
        teacher.preferredRomanization = m_spelling->text();
        const QStringList choices = teacher.preferredNameChoices();
        const QString selected = m_preferredName->currentText();

        QSignalBlocker blocker(m_preferredName);
        m_preferredName->clear();
        m_preferredName->addItems(choices);
        m_preferredName->setEnabled(!choices.isEmpty());

        const int selectedIndex = choices.indexOf(selected);
        m_preferredName->setCurrentIndex(
            selectedIndex >= 0 ? selectedIndex : (choices.isEmpty() ? -1 : 0));
    }

    bool saveCurrentTeacher(bool addingAnother)
    {
        const int namesEntered =
            !m_englishName->text().trimmed().isEmpty()
            + !m_koreanName->text().trimmed().isEmpty()
            + !m_spelling->text().trimmed().isEmpty();

        if (namesEntered < 2)
        {
            QStringList missingNames;
            if (m_englishName->text().trimmed().isEmpty())
            {
                missingNames.append(tr("English Name"));
            }
            if (m_koreanName->text().trimmed().isEmpty())
            {
                missingNames.append(tr("Korean Name"));
            }
            if (m_spelling->text().trimmed().isEmpty())
            {
                missingNames.append(tr("Preferred Spelling"));
            }

            QMessageBox::information(
                this,
                tr("Teacher Information"),
                tr("Complete at least %1 of the following fields before continuing:\n\n- %2")
                    .arg(2 - namesEntered)
                    .arg(missingNames.join(QStringLiteral("\n- "))));
            return false;
        }

        auto* setup = setupWizard(this);
        if (!setup || !setup->dataService())
        {
            return false;
        }

        Teacher teacher;
        teacher.teacherEn = m_englishName->text().trimmed();
        teacher.teacherKr = m_koreanName->text().trimmed();
        teacher.preferredRomanization = m_spelling->text().trimmed();
        teacher.preferredName = m_preferredName->currentText().trimmed();
        if (teacher.preferredName.isEmpty())
        {
            teacher.preferredName = !teacher.teacherEn.isEmpty()
                ? teacher.teacherEn
                : (!teacher.preferredRomanization.isEmpty()
                    ? teacher.preferredRomanization
                    : teacher.teacherKr);
        }
        teacher.roomNumber = m_room->text().trimmed();
        teacher.internetType = m_internet->currentText();
        teacher.wifiName = m_wifiName->text().trimmed();
        teacher.wifiPassword = m_wifiPassword->text();
        teacher.projectionType = m_projection->currentText();
        teacher.zoomId = m_zoomId->text().trimmed();
        teacher.zoomPassword = m_zoomPassword->text();

        if (setup->dataService()->createTeacher(teacher) <= 0)
        {
            QMessageBox::warning(
                this, tr("Teacher Information"),
                tr("The teacher could not be saved."));
            return false;
        }

        if (addingAnother)
        {
            QMessageBox::information(
                this, tr("Teacher Information"),
                tr("Teacher saved. You can enter another teacher or continue to the next step."));
        }
        return true;
    }

    void clearFields()
    {
        for (QLineEdit* edit : {
                 m_englishName, m_koreanName, m_spelling, m_room,
                 m_wifiName, m_wifiPassword,
                 m_zoomId, m_zoomPassword})
        {
            edit->clear();
        }
        updatePreferredNameChoices();
        m_internet->setCurrentIndex(0);
        m_projection->setCurrentIndex(0);
    }

    QLineEdit* m_englishName = nullptr;
    QLineEdit* m_koreanName = nullptr;
    QLineEdit* m_spelling = nullptr;
    QLineEdit* m_room = nullptr;
    QComboBox* m_preferredName = nullptr;
    QLineEdit* m_wifiName = nullptr;
    QLineEdit* m_wifiPassword = nullptr;
    QLineEdit* m_zoomId = nullptr;
    QLineEdit* m_zoomPassword = nullptr;
    QComboBox* m_internet = nullptr;
    QComboBox* m_projection = nullptr;
    QPushButton* m_addAnother = nullptr;
};

class ClassDetailsWizardPage final : public QWizardPage
{
public:
    explicit ClassDetailsWizardPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Create Your First Class"));

        auto* pageLayout = new QVBoxLayout(this);
        auto* layout = new QFormLayout;
        m_teacher = new QComboBox(this);
        m_teacher->setObjectName(QStringLiteral("setupClassTeacher"));
        layout->addRow(tr("Korean Teacher"), m_teacher);

        auto* colorRow = new QWidget(this);
        auto* colorLayout = new QHBoxLayout(colorRow);
        colorLayout->setContentsMargins(0, 0, 0, 0);
        m_colorPreview = new ClickableColorPreview(colorRow);
        m_colorPreview->setFixedSize(
            UiConstants::ClassInfo::Details::ColorPreviewWidth,
            UiConstants::ClassInfo::Details::ColorPreviewHeight);
        m_colorPreview->setObjectName(QStringLiteral("setupClassColorPreview"));
        m_colorButton = new TextFitPushButton(tr("Choose Color"), colorRow);
        m_colorButton->setObjectName(QStringLiteral("setupClassColorButton"));
        colorLayout->setSpacing(
            UiConstants::ClassInfo::Details::ColorPreviewButtonSpacing);
        colorLayout->addWidget(m_colorPreview);
        colorLayout->addWidget(m_colorButton);
        colorLayout->addStretch();
        layout->addRow(tr("Class Color"), colorRow);

        m_grade = new QComboBox(this);
        m_grade->setObjectName(QStringLiteral("setupClassGrade"));
        m_grade->addItems(ClassInfoConfig::Grades);
        layout->addRow(tr("Grade"), m_grade);

        m_level = new QComboBox(this);
        m_level->setObjectName(QStringLiteral("setupClassLevel"));
        layout->addRow(tr("Level"), m_level);

        m_readingBook = new QComboBox(this);
        m_readingBook->setObjectName(QStringLiteral("setupReadingBook"));
        layout->addRow(tr("Reading Book *"), m_readingBook);

        m_essayBook = new QComboBox(this);
        m_essayBook->setObjectName(QStringLiteral("setupEssayBook"));
        layout->addRow(tr("Essay Book *"), m_essayBook);
        pageLayout->addLayout(layout);
        pageLayout->addWidget(
            explanatoryLabel(tr("* Optional Fields"), this),
            0,
            Qt::AlignRight);
        pageLayout->addStretch();

        connect(m_grade, &QComboBox::currentTextChanged,
                this, [this]() { updateLevels(); });
        connect(m_level, &QComboBox::currentTextChanged,
                this, [this]() { updateBooks(); });
        connect(m_colorButton, &QPushButton::clicked, this, [this]()
        {
            auto* setup = setupWizard(this);
            const QColor color = ColorUtils::getColor(
                QColor(m_color),
                this,
                tr("Choose Class Color"),
                setup ? setup->dataService() : nullptr);
            if (!color.isValid())
            {
                return;
            }
            m_color = color.name(QColor::HexRgb).toUpper();
            m_colorChosen = true;
            updateColorPreview();
        });
        connect(m_colorPreview, &ClickableColorPreview::clicked,
                m_colorButton, &QPushButton::click);

        updateLevels();
        updateColorPreview();
    }

    void initializePage() override
    {
        auto* setup = setupWizard(this);
        m_teacher->clear();
        if (!setup || !setup->dataService())
        {
            return;
        }

        const QList<Teacher> teachers = setup->dataService()->getAllTeachers();
        if (teachers.size() > 1)
        {
            m_teacher->addItem(tr("Select a teacher..."), -1);
        }
        for (const Teacher& teacher : teachers)
        {
            m_teacher->addItem(teacher.preferredDisplayName(), teacher.id);
        }
        if (teachers.size() == 1)
        {
            m_teacher->setCurrentIndex(0);
        }
    }

    bool validatePage() override
    {
        QStringList missingFields;
        if (m_teacher->currentData().toInt() <= 0)
        {
            missingFields.append(tr("Korean Teacher"));
        }
        if (!m_colorChosen)
        {
            missingFields.append(tr("Class Color"));
        }
        if (m_grade->currentText().isEmpty())
        {
            missingFields.append(tr("Grade"));
        }
        if (m_level->currentText().isEmpty())
        {
            missingFields.append(tr("Level"));
        }
        if (showMissingFields(
                this, tr("Class Information"), missingFields))
        {
            return false;
        }

        auto* setup = setupWizard(this);
        if (!setup)
        {
            return false;
        }

        ClassInfo info = setup->classDraft();
        info.teacherId = m_teacher->currentData().toInt();
        info.classColor = m_color;
        info.fontColor = ColorUtils::getContrastingFontColor(QColor(m_color));
        info.classGrade = m_grade->currentText();
        info.classLevel = m_level->currentText();
        info.readingBook = m_readingBook->currentText();
        info.essayBook = m_essayBook->currentText();
        setup->setClassDraft(info);
        return true;
    }

    int nextId() const override
    {
        return InitialSetupWizard::ClassTimesPage;
    }

private:
    void updateColorPreview()
    {
        m_colorPreview->setStyleSheet(
            QStringLiteral(
                "background-color:%1;"
                "border:1px solid gray;"
                "border-radius:4px;")
                .arg(m_color));
    }

    void updateLevels()
    {
        m_level->clear();
        m_level->addItems(ClassInfoConfig::levelsForGrade(m_grade->currentText()));
        updateBooks();
    }

    void updateBooks()
    {
        const QString grade = m_grade->currentText();
        const QString level = m_level->currentText();
        m_readingBook->clear();
        m_readingBook->addItems(ClassInfoConfig::readingBooks(grade, level));
        m_essayBook->clear();
        m_essayBook->addItems(ClassInfoConfig::essayBooks(grade, level));
    }

    QComboBox* m_teacher = nullptr;
    QPushButton* m_colorButton = nullptr;
    ClickableColorPreview* m_colorPreview = nullptr;
    QComboBox* m_grade = nullptr;
    QComboBox* m_level = nullptr;
    QComboBox* m_readingBook = nullptr;
    QComboBox* m_essayBook = nullptr;
    QString m_color = QStringLiteral("#FFFFFF");
    bool m_colorChosen = false;
};

class ClassTimesWizardPage final : public QWizardPage
{
public:
    explicit ClassTimesWizardPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Enter Class Times"));

        auto* layout = new QVBoxLayout(this);
        m_schedules = new ClassScheduleSection(this);
        m_schedules->setObjectName(QStringLiteral("setupClassSchedules"));
        layout->addWidget(m_schedules);
        m_schedules->loadSchedules(
            QList<ClassTime>{ClassTime{}}, QList<ClassTime>{});
    }

    bool validatePage() override
    {
        const QList<ClassTime> regular = m_schedules->regularTimes();
        const QList<ClassTime> intensive = m_schedules->intensiveTimes();
        if (showMissingFields(
                this,
                tr("Class Times"),
                regular.isEmpty() && intensive.isEmpty()
                    ? QStringList{tr("At Least One Regular or Intensive Class Time")}
                    : QStringList{}))
        {
            return false;
        }

        auto* setup = setupWizard(this);
        if (!setup || !setup->dataService())
        {
            return false;
        }

        ClassInfo info = setup->classDraft();
        info.classTimes = regular;
        info.intensiveTimes = intensive;

        int classId = setup->createdClassId();
        if (classId <= 0)
        {
            classId = setup->dataService()->createClass(QString());
            setup->setCreatedClassId(classId);
        }
        if (classId <= 0)
        {
            QMessageBox::warning(
                this, tr("Create Class"),
                tr("The class could not be created."));
            return false;
        }

        info.classId = classId;
        if (!setup->dataService()->saveClassInfo(info))
        {
            QMessageBox::warning(
                this, tr("Create Class"),
                tr("The class information could not be saved."));
            return false;
        }

        setup->setClassDraft(info);
        return true;
    }

    int nextId() const override
    {
        return InitialSetupWizard::CompletionPage;
    }

private:
    ClassScheduleSection* m_schedules = nullptr;
};

class CompletionWizardPage final : public QWizardPage
{
public:
    explicit CompletionWizardPage(QWidget* parent = nullptr)
        : QWizardPage(parent)
    {
        setTitle(tr("Setup complete"));
        setSubTitle(tr("ClassMngr is ready to use."));
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(explanatoryLabel(
            tr("You can update your information, complete optional teacher details, add more teachers and classes, or import additional data at any time."),
            this));
        layout->addStretch();
        setFinalPage(true);
    }

    int nextId() const override
    {
        return -1;
    }
};
} // namespace

InitialSetupWizard::InitialSetupWizard(
    ApplicationServices* services,
    QWidget* parent)
    : QWizard(parent)
    , m_services(services)
{
    setObjectName(QStringLiteral("initialSetupWizard"));
    setWindowTitle(tr("Initial Setup"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnLastPage, true);
    setButtonText(QWizard::NextButton, tr("Continue"));
    setMinimumSize(760, 620);

    setPage(ResourcesPage, new ::ResourcesPage(this));
    setPage(TeacherImportPage, new ::TeacherImportWizardPage(this));
    setPage(ScheduleImportPage, new ::ScheduleImportWizardPage(this));
    setPage(PersonalDetailsPage, new ::PersonalDetailsWizardPage(this));
    setPage(TeacherEntryPage, new ::TeacherEntryWizardPage(this));
    setPage(ClassDetailsPage, new ::ClassDetailsWizardPage(this));
    setPage(ClassTimesPage, new ::ClassTimesWizardPage(this));
    setPage(CompletionPage, new ::CompletionWizardPage(this));
    setStartId(ResourcesPage);

    connect(this, &QWizard::currentIdChanged, this, [this](int pageId)
    {
        constexpr int standardWidth = 760;
        constexpr int classTimesWidth = 1040;
        const int targetWidth = pageId == ClassTimesPage
            ? classTimesWidth
            : standardWidth;
        setMinimumWidth(targetWidth);
        resize(targetWidth, height());
    });
}

ApplicationServices* InitialSetupWizard::services() const
{
    return m_services;
}

DataService* InitialSetupWizard::dataService() const
{
    return m_services ? m_services->dataService() : nullptr;
}

bool InitialSetupWizard::wantsTeacherImport() const
{
    return field(QStringLiteral("hasTeacherList")).toBool();
}

bool InitialSetupWizard::wantsScheduleImport() const
{
    return field(QStringLiteral("hasSchedule")).toBool();
}

void InitialSetupWizard::setClassDraft(const ClassInfo& info)
{
    m_classDraft = info;
}

ClassInfo InitialSetupWizard::classDraft() const
{
    return m_classDraft;
}

void InitialSetupWizard::setCreatedClassId(int classId)
{
    m_createdClassId = classId;
}

int InitialSetupWizard::createdClassId() const
{
    return m_createdClassId;
}
