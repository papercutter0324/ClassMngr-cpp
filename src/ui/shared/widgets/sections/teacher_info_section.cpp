#include "teacher_info_section.h"
#include "features/teacher/ui/teacher_model.h"
#include "ui/shared/constants/gui_constants.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QVBoxLayout>

TeacherInfoSection::TeacherInfoSection(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_grid = new QGridLayout();

    m_grid->setHorizontalSpacing(
        UiConstants::ClassInfo::Form::HorizontalSpacing
        );

    m_grid->setVerticalSpacing(
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

    m_teacherKrCombo = new QComboBox(this);
    m_teacherEnCombo = new QComboBox(this);
    m_model = new TeacherModel(this);

    m_roomNumberEdit = new QLineEdit(this);
    m_internetTypeEdit = new QLineEdit(this);
    m_wifiNameEdit = new QLineEdit(this);
    m_wifiPasswordEdit = new QLineEdit(this);
    m_projectionTypeEdit = new QLineEdit(this);
    m_zoomIdEdit = new QLineEdit(this);
    m_zoomPasswordEdit = new QLineEdit(this);

    for (auto* w : {
             m_roomNumberEdit,
             m_internetTypeEdit,
             m_wifiNameEdit,
             m_wifiPasswordEdit,
             m_projectionTypeEdit,
             m_zoomIdEdit,
             m_zoomPasswordEdit
         }) {
        w->setReadOnly(true);
    }

    m_grid->addWidget(fieldLabel(tr("Korean")), 0, 0, Qt::AlignLeft);
    m_grid->addWidget(fieldLabel(tr("English")), 0, 1, Qt::AlignLeft);
    m_grid->addWidget(fieldLabel(tr("Room")), 0, 2, Qt::AlignLeft);

    m_grid->addWidget(m_teacherKrCombo, 1, 0, Qt::AlignLeft);
    m_grid->addWidget(m_teacherEnCombo, 1, 1, Qt::AlignLeft);

    m_grid->addWidget(m_roomNumberEdit, 1, 2, Qt::AlignLeft);

    m_grid->addItem(
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

    m_grid->addWidget(fieldLabel(tr("Internet Type")), 3, 0, Qt::AlignLeft);
    m_grid->addWidget(fieldLabel(tr("WiFi Name")), 3, 1, Qt::AlignLeft);
    m_grid->addWidget(fieldLabel(tr("WiFi Password")), 3, 2, Qt::AlignLeft);
    m_grid->addWidget(m_internetTypeEdit, 4, 0, Qt::AlignLeft);
    m_grid->addWidget(m_wifiNameEdit, 4, 1, Qt::AlignLeft);
    m_grid->addWidget(m_wifiPasswordEdit, 4, 2, Qt::AlignLeft);

    m_grid->addItem(
        new QSpacerItem(
            0,
            UiConstants::ClassInfo::Form::GroupSpacerHeight,
            QSizePolicy::Minimum,
            QSizePolicy::Fixed
            ),
        5,
        0,
        1,
        4
        );

    m_grid->addWidget(fieldLabel(tr("Projection Type")), 6, 0, Qt::AlignLeft);
    m_grid->addWidget(fieldLabel(tr("Zoom ID")), 6, 1, Qt::AlignLeft);
    m_grid->addWidget(fieldLabel(tr("Zoom Password")), 6, 2, Qt::AlignLeft);
    m_grid->addWidget(m_projectionTypeEdit, 7, 0, Qt::AlignLeft);
    m_grid->addWidget(m_zoomIdEdit, 7, 1, Qt::AlignLeft);
    m_grid->addWidget(m_zoomPasswordEdit, 7, 2, Qt::AlignLeft);

    m_grid->setColumnStretch(
        0,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    m_grid->setColumnStretch(
        1,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );

    m_grid->setColumnStretch(
        2,
        UiConstants::ClassInfo::Teacher::ColumnStretch
        );
    m_grid->setColumnStretch(
        3,
        UiConstants::ClassInfo::Teacher::FillerColumnStretch
        );

    layout->addLayout(m_grid);

    applyFieldWidths();

    rebuildTeacherCombos();

    connect(m_teacherKrCombo, &QComboBox::currentIndexChanged,
            this, &TeacherInfoSection::onTeacherIndexChanged);

    connect(m_teacherEnCombo, &QComboBox::currentIndexChanged,
            this, &TeacherInfoSection::onTeacherIndexChanged);
}

void TeacherInfoSection::setTeacherModel(TeacherModel* model)
{
    if (!model)
    {
        return;
    }

    m_model = model;
    rebuildTeacherCombos();
}

void TeacherInfoSection::setTeachers(const QList<Teacher>& teachers)
{
    const int previousTeacherId =
        teacherId();

    m_model->setTeachers(teachers);

    rebuildTeacherCombos();
    selectTeacher(previousTeacherId);
}

void TeacherInfoSection::selectTeacher(int teacherId)
{
    const int comboIndex =
        m_teacherKrCombo->findData(teacherId);

    if (comboIndex < 0)
    {
        clearTeacher();
        return;
    }

    applyTeacher(comboIndex - 1);
}

int TeacherInfoSection::teacherId() const
{
    return m_selectedTeacherId;
}

void TeacherInfoSection::onTeacherIndexChanged(int index)
{
    if (!m_model)
        return;

    if (index <= 0)
    {
        clearTeacher();
        emit dataChanged();
        return;
    }

    applyTeacher(index - 1);
    emit dataChanged();
}

void TeacherInfoSection::rebuildTeacherCombos()
{
    const QSignalBlocker krBlocker(m_teacherKrCombo);
    const QSignalBlocker enBlocker(m_teacherEnCombo);

    m_teacherKrCombo->clear();
    m_teacherEnCombo->clear();

    m_teacherKrCombo->addItem(QString(), -1);
    m_teacherEnCombo->addItem(QString(), -1);

    if (!m_model)
    {
        return;
    }

    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        const Teacher& teacher =
            m_model->teacherAt(row);

        m_teacherKrCombo->addItem(
            teacher.teacherKr,
            teacher.id
            );

        m_teacherEnCombo->addItem(
            teacher.teacherEn,
            teacher.id
            );
    }
}

void TeacherInfoSection::applyTeacher(int index)
{
    const auto& t = m_model->teacherAt(index);

    if (t.id < 0)
    {
        clearTeacher();
        return;
    }

    m_teacherKrCombo->blockSignals(true);
    m_teacherEnCombo->blockSignals(true);

    m_teacherKrCombo->setCurrentIndex(index + 1);
    m_teacherEnCombo->setCurrentIndex(index + 1);

    m_teacherKrCombo->blockSignals(false);
    m_teacherEnCombo->blockSignals(false);

    m_selectedTeacherId = t.id;

    m_roomNumberEdit->setText(t.roomNumber);
    m_internetTypeEdit->setText(t.internetType);
    m_wifiNameEdit->setText(t.wifiName);
    m_wifiPasswordEdit->setText(t.wifiPassword);
    m_projectionTypeEdit->setText(t.projectionType);
    m_zoomIdEdit->setText(t.zoomId);
    m_zoomPasswordEdit->setText(t.zoomPassword);
}

void TeacherInfoSection::clearTeacher()
{
    m_teacherKrCombo->blockSignals(true);
    m_teacherEnCombo->blockSignals(true);

    m_teacherKrCombo->setCurrentIndex(0);
    m_teacherEnCombo->setCurrentIndex(0);

    m_teacherKrCombo->blockSignals(false);
    m_teacherEnCombo->blockSignals(false);

    m_selectedTeacherId = -1;

    m_roomNumberEdit->clear();
    m_internetTypeEdit->clear();
    m_wifiNameEdit->clear();
    m_wifiPasswordEdit->clear();
    m_projectionTypeEdit->clear();
    m_zoomIdEdit->clear();
    m_zoomPasswordEdit->clear();
}

void TeacherInfoSection::applyFieldWidths()
{
    const auto applyWidth =
        [](QWidget* widget)
        {
            if (!widget)
            {
                return;
            }

            widget->setMinimumWidth(
                UiConstants::ClassInfo::Teacher::FieldMinWidth
                );
            widget->setMaximumWidth(
                UiConstants::ClassInfo::Teacher::FieldMaxWidth
                );
            widget->setSizePolicy(
                QSizePolicy::Maximum,
                QSizePolicy::Preferred
                );
        };

    for (auto* widget : {
             static_cast<QWidget*>(m_teacherKrCombo),
             static_cast<QWidget*>(m_teacherEnCombo),
             static_cast<QWidget*>(m_roomNumberEdit),
             static_cast<QWidget*>(m_internetTypeEdit),
             static_cast<QWidget*>(m_wifiNameEdit),
             static_cast<QWidget*>(m_wifiPasswordEdit),
             static_cast<QWidget*>(m_projectionTypeEdit),
             static_cast<QWidget*>(m_zoomIdEdit),
             static_cast<QWidget*>(m_zoomPasswordEdit)
         })
    {
        applyWidth(widget);
    }
}
