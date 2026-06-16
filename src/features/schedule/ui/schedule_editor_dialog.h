#pragma once

#include "domain/models/class_info.h"

#include <QDialog>

class ApplicationServices;
class ClickableColorPreview;
class QComboBox;
class QLineEdit;

class ScheduleEditorDialog : public QDialog
{
    Q_OBJECT

public:
    ScheduleEditorDialog(
        ApplicationServices* services,
        int classId,
        QWidget* parent = nullptr
        );

signals:
    void saved(
        int classId
        );

private slots:
    void updateLevelOptions();

    void chooseClassColor();

    void chooseFontColor();

    void saveChanges();

private:
    void buildUi();

    void loadData();

    void rebuildLevelOptions(const QString& preferredLevel = {});

    void updateColorPreviews();

    void setComboText(
        QComboBox* combo,
        const QString& text
        );

private:
    ApplicationServices* m_services = nullptr;
    int m_classId = -1;
    ClassInfo m_cachedInfo;
    QString m_originalGrade;
    QString m_originalLevel;
    QString m_classColor{"#FFFFFF"};
    QString m_fontColor{"#000000"};
    bool m_loadingData = false;

    QLineEdit* m_teacherKrEdit = nullptr;
    QLineEdit* m_roomNumberEdit = nullptr;
    QComboBox* m_gradeCombo = nullptr;
    QComboBox* m_levelCombo = nullptr;
    ClickableColorPreview* m_classColorPreview = nullptr;
    ClickableColorPreview* m_fontColorPreview = nullptr;
};
