#include "class_details_section.h"

#include <QSignalBlocker>
#include <QColorDialog>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtAssert>

#include "core/application_services.h"
#include "config/class_info_config.h"
#include "ui/constants/gui_constants.h"
#include "ui/utils/widget_sizing.h"
#include "utils/colorutils.h"

#include <algorithm>

namespace
{
void appendUnique(
    QStringList& target,
    const QStringList& values
    )
{
    for (const QString& value : values)
    {
        if (!target.contains(value))
        {
            target.append(value);
        }
    }
}

QStringList allLevels()
{
    QStringList values{QString()};

    for (const QString& grade : ClassInfoConfig::Grades)
    {
        appendUnique(
            values,
            ClassInfoConfig::levelsForGrade(grade)
            );
    }

    return values;
}

QStringList allReadingBooks()
{
    QStringList values{QString()};

    for (const QString& grade : ClassInfoConfig::Grades)
    {
        const QStringList levels =
            ClassInfoConfig::levelsForGrade(grade);

        for (const QString& level : levels)
        {
            appendUnique(
                values,
                ClassInfoConfig::readingBooks(
                    grade,
                    level
                    )
                );
        }
    }

    return values;
}

QStringList allEssayBooks()
{
    QStringList values{QString()};

    for (const QString& grade : ClassInfoConfig::Grades)
    {
        const QStringList levels =
            ClassInfoConfig::levelsForGrade(grade);

        for (const QString& level : levels)
        {
            appendUnique(
                values,
                ClassInfoConfig::essayBooks(
                    grade,
                    level
                    )
                );
        }
    }

    return values;
}

void setLabelMinimumWidth(
    QLabel* label
    )
{
    if (!label)
    {
        return;
    }

    label->setMinimumWidth(
        WidgetSizing::labelMinimumWidth(label)
        );
}

int setComboMinimumWidthForTexts(
    QComboBox* combo,
    const QStringList& texts
    )
{
    if (!combo)
    {
        return 0;
    }

    const int minimumWidth =
        WidgetSizing::comboMinimumWidthForTexts(
            combo,
            texts,
            UiConstants::ClassInfo::TextWidthPadding
            );

    combo->setMinimumWidth(
        minimumWidth
        );

    return minimumWidth;
}
}


ClassDetailsSection::ClassDetailsSection(
    ApplicationServices* services,
    QWidget* parent
    )
    : QWidget(parent)
    , m_services(services)
{
    Q_ASSERT(m_services);

    m_colorPreview = new QFrame(this);
    m_colorPreview->setFixedSize(
        UiConstants::ClassInfo::Details::ColorPreviewWidth,
        UiConstants::ClassInfo::Details::ColorPreviewHeight
        );
    m_colorPreview->setObjectName("colorPreview");

    m_pendingClassColor = "#FFFFFF";
    m_pendingFontColor = "#000000";

    m_gradeCombo = new QComboBox(this);
    m_gradeCombo->setMaximumWidth(
        UiConstants::ClassInfo::Details::GradeMaxWidth
        );
    m_gradeCombo->addItem(QString());
    m_gradeCombo->addItems(ClassInfoConfig::Grades);

    m_levelCombo = new QComboBox(this);
    m_studentCountEdit = new QLineEdit(this);
    m_studentCountEdit->setReadOnly(true);
    m_studentCountEdit->setMaximumWidth(
        UiConstants::ClassInfo::Details::StudentCountMaxWidth
        );

    m_readingBookCombo = new QComboBox(this);
    m_essayBookCombo = new QComboBox(this);

    auto* colorButton =
        new QPushButton(tr("Choose Color"), this);

    colorButton->setMinimumWidth(
        std::max(
            colorButton->minimumSizeHint().width(),
            WidgetSizing::textWidth(
                colorButton,
                colorButton->text()
                )
            + UiConstants::ClassInfo::TextWidthPadding
            )
        + UiConstants::ClassInfo::Details::ColorButtonExtraWidth
        );

    auto* colorLayout =
        new QHBoxLayout;

    colorLayout->setSpacing(
        UiConstants::ClassInfo::Details::ColorPreviewButtonSpacing
        );

    colorLayout->addWidget(m_colorPreview);
    colorLayout->addWidget(colorButton);
    colorLayout->addStretch();

    auto* grid =
        new QGridLayout;

    grid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );

    grid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    auto* booksGrid =
        new QGridLayout;

    booksGrid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );

    booksGrid->setVerticalSpacing(
        UiConstants::ClassInfo::Form::VerticalSpacing
        );

    const auto fieldLabel =
        [this](const QString& text)
        {
            auto* label = new QLabel(text, this);

            label->setContentsMargins(
                UiConstants::ClassInfo::Form::LabelIndent,
                0,
                0,
                0
                );

            return label;
        };

    auto* colorLabel = fieldLabel(tr("Color"));
    auto* gradeLabel = fieldLabel(tr("Grade"));
    auto* levelLabel = fieldLabel(tr("Level"));
    auto* studentCountLabel = fieldLabel(tr("# of Students"));
    auto* readingBookLabel = fieldLabel(tr("Reading Book"));
    auto* essayBookLabel = fieldLabel(tr("Essay Book"));

    for (auto* label : {
             colorLabel,
             gradeLabel,
             levelLabel,
             studentCountLabel,
             readingBookLabel,
             essayBookLabel
         })
    {
        setLabelMinimumWidth(label);
    }

    setComboMinimumWidthForTexts(
        m_gradeCombo,
        ClassInfoConfig::Grades
        );

    m_gradeCombo->setMaximumWidth(
        std::max(
            UiConstants::ClassInfo::Details::GradeMaxWidth,
            m_gradeCombo->minimumWidth()
            )
        );

    const int levelMinimumWidth =
        setComboMinimumWidthForTexts(
            m_levelCombo,
            allLevels()
            );

    m_levelCombo->setMinimumWidth(
        levelMinimumWidth
        + UiConstants::ClassInfo::Details::LevelComboExtraWidth
        );

    const int readingBookMinimumWidth =
        setComboMinimumWidthForTexts(
            m_readingBookCombo,
            allReadingBooks()
            );

    const int essayBookMinimumWidth =
        setComboMinimumWidthForTexts(
            m_essayBookCombo,
            allEssayBooks()
            );

    m_essayBookCombo->setMaximumWidth(
        std::max(
            essayBookMinimumWidth,
            readingBookMinimumWidth
            - UiConstants::ClassInfo::Details::EssayBookWidthReduction
            )
        );

    m_studentCountEdit->setMinimumWidth(
        std::min(
            UiConstants::ClassInfo::Details::StudentCountMaxWidth,
            std::max(
                m_studentCountEdit->minimumSizeHint().width(),
                WidgetSizing::textWidth(
                    m_studentCountEdit,
                    QStringLiteral("000")
                    )
                + UiConstants::ClassInfo::TextWidthPadding
                )
            )
        );

    grid->addWidget(colorLabel, 0, 0);
    grid->addWidget(gradeLabel, 0, 1);
    grid->addWidget(levelLabel, 0, 2);
    grid->addWidget(studentCountLabel, 0, 3);

    grid->addLayout(colorLayout, 1, 0);
    grid->addWidget(m_gradeCombo, 1, 1);
    grid->addWidget(m_levelCombo, 1, 2);
    grid->addWidget(m_studentCountEdit, 1, 3);

    booksGrid->addWidget(readingBookLabel, 0, 0);
    booksGrid->addWidget(essayBookLabel, 0, 1);

    booksGrid->addWidget(m_readingBookCombo, 1, 0);
    booksGrid->addWidget(m_essayBookCombo, 1, 1);

    grid->setColumnStretch(
        0,
        UiConstants::ClassInfo::Details::FieldColumnStretch
        );

    grid->setColumnStretch(
        1,
        UiConstants::ClassInfo::Details::FieldColumnStretch
        );

    grid->setColumnStretch(
        2,
        UiConstants::ClassInfo::Details::FieldColumnStretch
        );

    grid->setColumnStretch(
        3,
        UiConstants::ClassInfo::Details::FieldColumnStretch
        );

    grid->setColumnStretch(
        4,
        UiConstants::ClassInfo::Details::FillerColumnStretch
        );

    booksGrid->setColumnStretch(
        0,
        UiConstants::ClassInfo::Details::FieldColumnStretch
        );

    booksGrid->setColumnStretch(
        1,
        UiConstants::ClassInfo::Details::FieldColumnStretch
        );

    booksGrid->setColumnStretch(
        2,
        UiConstants::ClassInfo::Details::FillerColumnStretch
        );

    auto* layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addLayout(grid);
    layout->addSpacing(
        UiConstants::ClassInfo::Form::GroupSpacerHeight
        );
    layout->addLayout(booksGrid);

    updateColorPreview(m_pendingClassColor);
    updateLevelOptions();

    connect(
        m_gradeCombo,
        &QComboBox::currentTextChanged,
        this,
        [this]
        {
            updateLevelOptions();
            emit dataChanged();
        }
        );

    connect(
        m_levelCombo,
        &QComboBox::currentTextChanged,
        this,
        [this]
        {
            updateBookOptions();
            emit dataChanged();
        }
        );

    connect(
        m_readingBookCombo,
        &QComboBox::currentTextChanged,
        this,
        &ClassDetailsSection::dataChanged
        );

    connect(
        m_essayBookCombo,
        &QComboBox::currentTextChanged,
        this,
        &ClassDetailsSection::dataChanged
        );

    connect(
        colorButton,
        &QPushButton::clicked,
        this,
        &ClassDetailsSection::openColorPicker
        );
}

void ClassDetailsSection::loadInfo(
    const QString& grade,
    const QString& level,
    const QString& readingBook,
    const QString& essayBook,
    const QString& classColor,
    const QString& fontColor
    )
{
    const QSignalBlocker gradeBlocker(m_gradeCombo);
    const QSignalBlocker levelBlocker(m_levelCombo);
    const QSignalBlocker readingBlocker(m_readingBookCombo);
    const QSignalBlocker essayBlocker(m_essayBookCombo);

    m_pendingClassColor =
        classColor.isEmpty()
            ? QString("#FFFFFF")
            : classColor;

    m_pendingFontColor =
        fontColor.isEmpty()
            ? ColorUtils::getContrastingFontColor(m_pendingClassColor)
            : fontColor;

    const int gradeIndex =
        m_gradeCombo->findText(grade);

    m_gradeCombo->setCurrentIndex(
        gradeIndex >= 0
            ? gradeIndex
            : 0
        );

    updateLevelOptions();

    const int levelIndex =
        m_levelCombo->findText(level);

    m_levelCombo->setCurrentIndex(
        levelIndex >= 0
            ? levelIndex
            : 0
        );

    updateBookOptions();

    const int readingIndex =
        m_readingBookCombo->findText(readingBook);

    if (readingIndex >= 0)
    {
        m_readingBookCombo->setCurrentIndex(readingIndex);
    }

    const int essayIndex =
        m_essayBookCombo->findText(essayBook);

    if (essayIndex >= 0)
    {
        m_essayBookCombo->setCurrentIndex(essayIndex);
    }

    updateColorPreview(m_pendingClassColor);
}

QString ClassDetailsSection::grade() const
{
    return m_gradeCombo->currentText();
}

QString ClassDetailsSection::level() const
{
    return m_levelCombo->currentText();
}

QString ClassDetailsSection::readingBook() const
{
    return m_readingBookCombo->currentText();
}

QString ClassDetailsSection::essayBook() const
{
    return m_essayBookCombo->currentText();
}

QString ClassDetailsSection::classColor() const
{
    return m_pendingClassColor;
}

QString ClassDetailsSection::fontColor() const
{
    return m_pendingFontColor;
}

void ClassDetailsSection::updateLevelOptions()
{
    const QSignalBlocker blocker(m_levelCombo);

    const QString previousLevel =
        m_levelCombo->currentText();

    m_levelCombo->clear();
    m_levelCombo->addItem(QString());

    const QString grade =
        m_gradeCombo->currentText();

    const QStringList levels =
        ClassInfoConfig::levelsForGrade(grade);

    m_levelCombo->addItems(levels);

    const int levelIndex =
        m_levelCombo->findText(previousLevel);

    if (levelIndex >= 0)
        m_levelCombo->setCurrentIndex(levelIndex);

    updateBookOptions();
}

void ClassDetailsSection::updateBookOptions()
{
    const QSignalBlocker readingBlocker(
        m_readingBookCombo
        );

    const QSignalBlocker essayBlocker(
        m_essayBookCombo
        );


    const QString previousReadingBook =
        m_readingBookCombo->currentText();

    const QString previousEssayBook =
        m_essayBookCombo->currentText();

    m_readingBookCombo->clear();
    m_essayBookCombo->clear();
    m_readingBookCombo->addItem(QString());
    m_essayBookCombo->addItem(QString());

    const QString grade =
        m_gradeCombo->currentText();

    const QString level =
        m_levelCombo->currentText();

    if (grade.isEmpty() || level.isEmpty())
    {
        return;
    }

    const QStringList readingBooks =
        ClassInfoConfig::readingBooks(
            grade,
            level
            );

    const QStringList essayBooks =
        ClassInfoConfig::essayBooks(
            grade,
            level
            );

    for (const QString& book : readingBooks)
    {
        if (m_readingBookCombo->findText(book) < 0)
        {
            m_readingBookCombo->addItem(book);
        }
    }

    for (const QString& book : essayBooks)
    {
        if (m_essayBookCombo->findText(book) < 0)
        {
            m_essayBookCombo->addItem(book);
        }
    }

    const int readingIndex =
        m_readingBookCombo->findText(previousReadingBook);

    if (readingIndex >= 0)
        m_readingBookCombo->setCurrentIndex(readingIndex);

    const int essayIndex =
        m_essayBookCombo->findText(previousEssayBook);

    if (essayIndex >= 0)
        m_essayBookCombo->setCurrentIndex(essayIndex);
}

void ClassDetailsSection::openColorPicker()
{
    QColor currentColor(m_pendingClassColor);

    if (!currentColor.isValid())
        currentColor = QColor("#FFFFFF");

    QColor color = QColorDialog::getColor(
        currentColor,
        this,
        tr("Select Class Color")
        );

    if (!color.isValid())
        return;

    m_pendingClassColor = color.name();

    m_pendingFontColor =
        ColorUtils::getContrastingFontColor(
            m_pendingClassColor
            );

    updateColorPreview(m_pendingClassColor);

    ColorUtils::saveCustomColors(
        m_services->dataService()
        );

    emit dataChanged();
}

void ClassDetailsSection::updateColorPreview(
    const QString& color
    )
{
    if (!m_colorPreview)
    {
        return;
    }

    m_colorPreview->setStyleSheet(
        QString(
            "background-color:%1;"
            "border:1px solid gray;"
            "border-radius:4px;"
            ).arg(color)
        );
}
