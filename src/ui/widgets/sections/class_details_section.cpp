#include "class_details_section.h"

#include <QSignalBlocker>
#include <QColorDialog>
#include <QComboBox>
#include <QFrame>
#include <QtAssert>

#include "core/application_services.h"
#include "models/class_info/class_info_config.h"
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

    updateColorPreview(m_pendingClassColor);

    // build UI here (or call _build_ui())
}

void ClassDetailsSection::updateLevelOptions()
{
    const QSignalBlocker blocker(m_levelCombo);

    m_levelCombo->clear();

    const QString grade =
        m_gradeCombo->currentText();

    const QStringList levels =
        ClassInfoConfig::levelsForGrade(grade);

    m_levelCombo->addItems(levels);

    if (!levels.isEmpty())
    {
        updateBookOptions();
    }
}

void ClassDetailsSection::updateBookOptions()
{
    const QSignalBlocker readingBlocker(
        m_readingBookCombo
        );

    const QSignalBlocker essayBlocker(
        m_essayBookCombo
        );


    m_readingBookCombo->clear();
    m_essayBookCombo->clear();

    const QString grade =
        m_gradeCombo->currentText();

    const QString level =
        m_levelCombo->currentText();

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

    m_readingBookCombo->addItems(readingBooks);
    m_essayBookCombo->addItems(essayBooks);
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