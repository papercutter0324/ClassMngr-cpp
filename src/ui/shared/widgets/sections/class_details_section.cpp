#include "class_details_section.h"

#include "ui/shared/widgets/text_fit_push_button.h"

#include <QSignalBlocker>
#include <QComboBox>
#include "ui/shared/widgets/no_wheel_combobox.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMargins>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtAssert>

#include "core/application_services.h"
#include "features/classes/config/class_info_config.h"
#include "ui/shared/constants/gui_constants.h"
#include "ui/shared/utils/widget_sizing.h"
#include "ui/shared/widgets/clickable_color_preview.h"
#include "core/utils/colorutils.h"

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

int installComboWidthForTexts(
    QComboBox* combo,
    const QStringList& texts,
    int preferredWidth = 0,
    int extraWidth = 0
    )
{
    if (!combo)
    {
        return 0;
    }

    const int minimumWidth =
        std::max(
            preferredWidth,
            WidgetSizing::comboMinimumWidthForTexts(
                combo,
                texts,
                UiConstants::ClassInfo::TextWidthPadding
                )
            + extraWidth
            );

    WidgetSizing::installTextAwareFieldWidth(
        combo,
        minimumWidth
        );

    return minimumWidth;
}

int studentCountMinimumWidth(
    QLineEdit* edit
    )
{
    if (!edit)
    {
        return 0;
    }

    const QMargins margins =
        edit->textMargins();

    return WidgetSizing::textWidth(
        edit,
        QStringLiteral("000")
        )
        + margins.left()
        + margins.right()
        + WidgetSizing::LineEditTextPadding;
}

int essayBookMinimumWidth(
    QComboBox* combo
    )
{
    return WidgetSizing::comboMinimumWidthForTexts(
        combo,
        { QStringLiteral("000") },
        UiConstants::ClassInfo::TextWidthPadding
        );
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

    m_colorPreview = new ClickableColorPreview(this);
    m_colorPreview->setFixedSize(
        UiConstants::ClassInfo::Details::ColorPreviewWidth,
        UiConstants::ClassInfo::Details::ColorPreviewHeight
        );
    m_colorPreview->setObjectName("colorPreview");

    m_pendingClassColor = "#FFFFFF";
    m_pendingFontColor = "#000000";

    m_gradeCombo = new NoWheelComboBox(this);
    m_gradeCombo->setObjectName(QStringLiteral("classGradeCombo"));
    m_gradeCombo->addItem(QString());
    m_gradeCombo->addItems(ClassInfoConfig::Grades);

    m_levelCombo = new NoWheelComboBox(this);
    m_levelCombo->setObjectName(QStringLiteral("classLevelCombo"));
    m_studentCountEdit = new QLineEdit(this);
    m_studentCountEdit->setReadOnly(true);

    m_readingBookCombo = new NoWheelComboBox(this);
    m_readingBookCombo->setObjectName(QStringLiteral("classReadingBookCombo"));
    m_essayBookCombo = new NoWheelComboBox(this);
    m_essayBookCombo->setObjectName(QStringLiteral("classEssayBookCombo"));

    m_colorButton =
        new TextFitPushButton(this);

    m_colorButton->setText(
        tr("Choose Color")
        );
    m_colorButton->setObjectName(QStringLiteral("classColorButton"));
    updateColorButtonWidth();

    auto* colorLayout =
        new QHBoxLayout;

    colorLayout->setSpacing(
        UiConstants::ClassInfo::Details::ColorPreviewButtonSpacing
        );

    colorLayout->addWidget(m_colorPreview);
    colorLayout->addWidget(m_colorButton);

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

    m_colorLabel = fieldLabel(tr("Color"));
    m_gradeLabel = fieldLabel(tr("Grade"));
    m_levelLabel = fieldLabel(tr("Level"));
    m_studentCountLabel = fieldLabel(tr("# of Students"));
    m_readingBookLabel = fieldLabel(tr("Reading Book"));
    m_essayBookLabel = fieldLabel(tr("Essay Book"));

    updateLabelMinimumWidths();

    installComboWidthForTexts(
        m_gradeCombo,
        ClassInfoConfig::Grades,
        UiConstants::ClassInfo::Details::GradeMaxWidth
        );

    installComboWidthForTexts(
        m_levelCombo,
        allLevels(),
        0,
        UiConstants::ClassInfo::Details::LevelComboExtraWidth
        );

    installComboWidthForTexts(
        m_readingBookCombo,
        allReadingBooks()
        );

    WidgetSizing::installTextAwareFieldWidth(
        m_essayBookCombo,
        essayBookMinimumWidth(m_essayBookCombo),
        QSizePolicy::Fixed,
        true
        );

    WidgetSizing::installTextAwareFieldWidth(
        m_studentCountEdit,
        studentCountMinimumWidth(m_studentCountEdit),
        QSizePolicy::Fixed,
        true
        );

    grid->addWidget(m_colorLabel, 0, 0);
    grid->addWidget(m_gradeLabel, 0, 1);
    grid->addWidget(m_levelLabel, 0, 2);
    grid->addWidget(m_studentCountLabel, 0, 3);

    grid->addLayout(colorLayout, 1, 0);
    grid->addWidget(m_gradeCombo, 1, 1);
    grid->addWidget(m_levelCombo, 1, 2);
    grid->addWidget(m_studentCountEdit, 1, 3);

    booksGrid->addWidget(m_readingBookLabel, 0, 0);
    booksGrid->addWidget(m_essayBookLabel, 0, 1);

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
    rebuildLevelOptions(QString());
    rebuildBookOptions(QString(), QString());

    connect(
        m_gradeCombo,
        &QComboBox::currentTextChanged,
        this,
        &ClassDetailsSection::updateLevelOptions
        );

    connect(
        m_levelCombo,
        &QComboBox::currentTextChanged,
        this,
        &ClassDetailsSection::updateBookOptions
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
        m_colorButton,
        &QPushButton::clicked,
        this,
        &ClassDetailsSection::openColorPicker
        );

    connect(
        m_colorPreview,
        &ClickableColorPreview::clicked,
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
    const QString& fontColor,
    int studentCount
    )
{
    m_loadingInfo = true;

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

    rebuildLevelOptions(level);
    rebuildBookOptions(readingBook, essayBook);

    updateColorPreview(m_pendingClassColor);

    m_studentCountEdit->setText(
        QString::number(
            studentCount
            )
            .trimmed()
        );
    WidgetSizing::updateTextAwareFieldWidth(
        m_studentCountEdit,
        studentCountMinimumWidth(m_studentCountEdit),
        true
        );

    m_loadingInfo = false;
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

QComboBox* ClassDetailsSection::gradeEditor() const
{
    return m_gradeCombo;
}

QComboBox* ClassDetailsSection::levelEditor() const
{
    return m_levelCombo;
}

QComboBox* ClassDetailsSection::readingBookEditor() const
{
    return m_readingBookCombo;
}

QComboBox* ClassDetailsSection::essayBookEditor() const
{
    return m_essayBookCombo;
}

QPushButton* ClassDetailsSection::colorEditor() const
{
    return m_colorButton;
}

void ClassDetailsSection::retranslateUi()
{
    if (m_colorButton)
    {
        m_colorButton->setText(
            tr("Choose Color")
            );
    }

    if (m_colorLabel)
    {
        m_colorLabel->setText(
            tr("Color")
            );
    }

    if (m_gradeLabel)
    {
        m_gradeLabel->setText(
            tr("Grade")
            );
    }

    if (m_levelLabel)
    {
        m_levelLabel->setText(
            tr("Level")
            );
    }

    if (m_studentCountLabel)
    {
        m_studentCountLabel->setText(
            tr("# of Students")
            );
    }

    if (m_readingBookLabel)
    {
        m_readingBookLabel->setText(
            tr("Reading Book")
            );
    }

    if (m_essayBookLabel)
    {
        m_essayBookLabel->setText(
            tr("Essay Book")
            );
    }

    updateColorButtonWidth();
    updateLabelMinimumWidths();
}

void ClassDetailsSection::updateLevelOptions()
{
    if (m_loadingInfo)
    {
        return;
    }

    rebuildLevelOptions(QString());
    rebuildBookOptions(QString(), QString());

    emit dataChanged();
}

void ClassDetailsSection::updateBookOptions()
{
    if (m_loadingInfo)
    {
        return;
    }

    rebuildBookOptions(QString(), QString());

    emit dataChanged();
}

void ClassDetailsSection::rebuildLevelOptions(
    const QString& preferredLevel
    )
{
    const QSignalBlocker blocker(m_levelCombo);

    m_levelCombo->clear();
    m_levelCombo->addItem(QString());

    const QString grade =
        m_gradeCombo->currentText();

    const QStringList levels =
        ClassInfoConfig::levelsForGrade(grade);

    m_levelCombo->addItems(levels);

    const int levelIndex =
        m_levelCombo->findText(preferredLevel);

    if (levelIndex >= 0)
        m_levelCombo->setCurrentIndex(levelIndex);
}

void ClassDetailsSection::rebuildBookOptions(
    const QString& preferredReadingBook,
    const QString& preferredEssayBook
    )
{
    const QSignalBlocker readingBlocker(
        m_readingBookCombo
        );

    const QSignalBlocker essayBlocker(
        m_essayBookCombo
        );


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
        m_readingBookCombo->findText(preferredReadingBook);

    if (readingIndex >= 0)
        m_readingBookCombo->setCurrentIndex(readingIndex);

    const int essayIndex =
        m_essayBookCombo->findText(preferredEssayBook);

    if (essayIndex >= 0)
        m_essayBookCombo->setCurrentIndex(essayIndex);

    WidgetSizing::updateTextAwareFieldWidth(
        m_essayBookCombo,
        essayBookMinimumWidth(m_essayBookCombo),
        true
        );
}

void ClassDetailsSection::openColorPicker()
{
    QColor currentColor(m_pendingClassColor);

    if (!currentColor.isValid())
        currentColor = QColor("#FFFFFF");

    QColor color = ColorUtils::getColor(
        currentColor,
        this,
        tr("Select Class Color"),
        m_services
            ? m_services->settingsService()
            : nullptr
        );

    if (!color.isValid())
        return;

    m_pendingClassColor = color.name();

    m_pendingFontColor =
        ColorUtils::getContrastingFontColor(
            m_pendingClassColor
            );

    updateColorPreview(m_pendingClassColor);

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

void ClassDetailsSection::updateLabelMinimumWidths()
{
    for (auto* label : {
             m_colorLabel,
             m_gradeLabel,
             m_levelLabel,
             m_studentCountLabel,
             m_readingBookLabel,
             m_essayBookLabel
         })
    {
        setLabelMinimumWidth(label);
    }
}

void ClassDetailsSection::updateColorButtonWidth()
{
    if (!m_colorButton)
    {
        return;
    }

    m_colorButton->setMinimumWidth(
        std::max(
            m_colorButton->minimumSizeHint().width(),
            WidgetSizing::textWidth(
                m_colorButton,
                m_colorButton->text()
                )
            + UiConstants::ClassInfo::TextWidthPadding
            )
        + UiConstants::ClassInfo::Details::ColorButtonExtraWidth
        );
}
