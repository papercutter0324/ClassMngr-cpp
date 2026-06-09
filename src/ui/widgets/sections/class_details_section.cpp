#include "class_details_section.h"

#include <QSignalBlocker>
#include <QColorDialog>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtAssert>

#include "core/application_services.h"
#include "config/class_info_config.h"
#include "utils/colorutils.h"


ClassDetailsSection::ClassDetailsSection(
    ApplicationServices* services,
    QWidget* parent
    )
    : QWidget(parent)
    , m_services(services)
{
    Q_ASSERT(m_services);

    m_colorPreview = new QFrame(this);
    m_colorPreview->setFixedSize(40, 24);
    m_colorPreview->setObjectName("colorPreview");

    m_pendingClassColor = "#FFFFFF";
    m_pendingFontColor = "#000000";

    m_gradeCombo = new QComboBox(this);
    m_gradeCombo->addItem(QString());
    m_gradeCombo->addItems(ClassInfoConfig::Grades);

    m_levelCombo = new QComboBox(this);
    m_readingBookCombo = new QComboBox(this);
    m_essayBookCombo = new QComboBox(this);

    auto* colorButton =
        new QPushButton(tr("Choose Color"), this);

    auto* colorLayout =
        new QHBoxLayout;

    colorLayout->addWidget(m_colorPreview);
    colorLayout->addWidget(colorButton);
    colorLayout->addStretch();

    auto* grid =
        new QGridLayout;

    grid->addWidget(new QLabel(tr("Grade"), this), 0, 0);
    grid->addWidget(new QLabel(tr("Level"), this), 0, 1);
    grid->addWidget(m_gradeCombo, 1, 0);
    grid->addWidget(m_levelCombo, 1, 1);

    grid->addWidget(new QLabel(tr("Reading Book"), this), 2, 0);
    grid->addWidget(new QLabel(tr("Essay Book"), this), 2, 1);
    grid->addWidget(m_readingBookCombo, 3, 0);
    grid->addWidget(m_essayBookCombo, 3, 1);

    grid->addWidget(new QLabel(tr("Class Color"), this), 4, 0);
    grid->addLayout(colorLayout, 5, 0, 1, 2);

    auto* layout =
        new QVBoxLayout(this);

    layout->addLayout(grid);

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
