#pragma once

#include "ui/shared/pages/basepage.h"

class ApplicationServices;
class QLabel;
class QScrollArea;
class NavigationTabWidget;
class QTextEdit;
class QVBoxLayout;
class QWidget;

class MyClassesPage : public BasePage
{
    Q_OBJECT

public:
    explicit MyClassesPage(
        ApplicationServices* services,
        QWidget* parent = nullptr
        );

    void refresh() override;
    void clearDatabaseState() override;
    void retranslateUi() override;

private:
    void buildUi();
    void refreshGeneratedContent();
    void rebuildClassInformation();
    void clearClassInformation();

    QLabel* createFieldLabel(
        const QString& text,
        QWidget* parent
        ) const;
    QTextEdit* createTextEdit(
        int minimumLines,
        bool readOnly,
        QWidget* parent
        ) const;

    ApplicationServices* m_services = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollContentLayout = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QWidget* m_classInformationContent = nullptr;
    QVBoxLayout* m_classInformationLayout = nullptr;
    NavigationTabWidget* m_classInformationTabs = nullptr;
    int m_selectedClassId = -1;
};
