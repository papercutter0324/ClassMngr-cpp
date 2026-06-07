#include "teacher_info_section.h"
#include "models/teacher_model.h"

#include <QComboBox>
#include <QLineEdit>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>

TeacherInfoSection::TeacherInfoSection(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* grid = new QGridLayout();

    m_teacherKrCombo = new QComboBox(this);
    m_teacherEnCombo = new QComboBox(this);

    m_roomNumberEdit = new QLineEdit(this);
    m_wifiNameEdit = new QLineEdit(this);
    m_wifiPasswordEdit = new QLineEdit(this);
    m_zoomIdEdit = new QLineEdit(this);
    m_zoomPasswordEdit = new QLineEdit(this);

    for (auto* w : {
             m_roomNumberEdit,
             m_wifiNameEdit,
             m_wifiPasswordEdit,
             m_zoomIdEdit,
             m_zoomPasswordEdit
         }) {
        w->setReadOnly(true);
    }

    grid->addWidget(new QLabel("Korean"), 0, 0);
    grid->addWidget(new QLabel("English"), 0, 1);

    grid->addWidget(m_teacherKrCombo, 1, 0);
    grid->addWidget(m_teacherEnCombo, 1, 1);

    grid->addWidget(m_roomNumberEdit, 1, 2);

    layout->addLayout(grid);

    connect(m_teacherKrCombo, &QComboBox::currentIndexChanged,
            this, &TeacherInfoSection::onTeacherIndexChanged);

    connect(m_teacherEnCombo, &QComboBox::currentIndexChanged,
            this, &TeacherInfoSection::onTeacherIndexChanged);
}

void TeacherInfoSection::setTeacherModel(TeacherModel* model)
{
    m_model = model;

    m_teacherKrCombo->setModel(model);
    m_teacherEnCombo->setModel(model);

    m_teacherKrCombo->setModelColumn(TeacherModel::KrRole);
    m_teacherEnCombo->setModelColumn(TeacherModel::EnRole);
}

void TeacherInfoSection::onTeacherIndexChanged(int index)
{
    if (!m_model)
        return;

    applyTeacher(index);
}

void TeacherInfoSection::applyTeacher(int index)
{
    const auto& t = m_model->teacherAt(index);

    m_teacherKrCombo->blockSignals(true);
    m_teacherEnCombo->blockSignals(true);

    m_teacherKrCombo->setCurrentIndex(index);
    m_teacherEnCombo->setCurrentIndex(index);

    m_teacherKrCombo->blockSignals(false);
    m_teacherEnCombo->blockSignals(false);

    m_roomNumberEdit->setText(t.roomNumber);
    m_wifiNameEdit->setText(t.wifiName);
    m_wifiPasswordEdit->setText(t.wifiPassword);
    m_zoomIdEdit->setText(t.zoomId);
    m_zoomPasswordEdit->setText(t.zoomPassword);
}

void TeacherInfoSection::clearTeacher()
{
    m_teacherKrCombo->blockSignals(true);
    m_teacherEnCombo->blockSignals(true);

    m_teacherKrCombo->setCurrentIndex(-1);
    m_teacherEnCombo->setCurrentIndex(-1);

    m_teacherKrCombo->blockSignals(false);
    m_teacherEnCombo->blockSignals(false);

    m_roomNumberEdit->clear();
    m_wifiNameEdit->clear();
    m_wifiPasswordEdit->clear();
    m_zoomIdEdit->clear();
    m_zoomPasswordEdit->clear();
}