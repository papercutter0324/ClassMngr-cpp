#ifndef BASEPAGE_H
#define BASEPAGE_H

#include "ui/shared/constants/options.h"

#include <QMargins>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class QHBoxLayout;
class QFrame;
class QEvent;
class QLabel;
class QPushButton;
class QResizeEvent;
class QWidget;

class BasePage : public QWidget
{
    Q_OBJECT

public:
    explicit BasePage(
        QWidget* parent = nullptr
        );

    virtual ~BasePage() override = default;



    // =====================================================
    // Persistence
    // =====================================================

    virtual void saveData();

    virtual bool saveChanges();

    virtual bool hasUnsavedChanges() const;

    virtual void discardChanges();

    virtual QString unsavedChangesTitle() const;

    virtual QString unsavedChangesMessage() const;

    virtual void setSaveMode(
        SaveMode mode
        );



    // =====================================================
    // Refresh
    // =====================================================

    virtual void refresh();
    virtual void retranslateUi();

    virtual void clearDatabaseState();

    void setDatabaseOpen(
        bool databaseOpen
        );



signals:

    void openDatabaseRequested();

    void newDatabaseRequested();



protected:

    void changeEvent(
        QEvent* event
        ) override;

    QVBoxLayout* contentLayout() const;

    QHBoxLayout* bottomLayout() const;

    void setPageLayoutMargins(
        const QMargins& margins
        );

    void setBottomBarVisible(
        bool visible
        );

    void resizeEvent(
        QResizeEvent* event
        ) override;



private:

    // =====================================================
    // Layouts
    // =====================================================

    QVBoxLayout* m_mainLayout = nullptr;

    QMargins m_defaultMainLayoutMargins;

    QVBoxLayout* m_contentLayout = nullptr;

    QWidget* m_bottomBar = nullptr;

    QHBoxLayout* m_bottomLayout = nullptr;

    QFrame* m_noDatabaseBanner = nullptr;

    QLabel* m_noDatabaseTitle = nullptr;

    QLabel* m_noDatabaseMessage = nullptr;

    QLabel* m_noDatabaseStepOne = nullptr;

    QLabel* m_noDatabaseStepTwo = nullptr;

    QLabel* m_noDatabaseStepThree = nullptr;

    QLabel* m_noDatabaseNextSteps = nullptr;

    QPushButton* m_openDatabaseButton = nullptr;

    QPushButton* m_newDatabaseButton = nullptr;

    bool m_noDatabaseBannerEnabled = false;

    void updateNoDatabaseBannerGeometry();

    void updateNoDatabaseBannerLayout();
};

#endif // BASEPAGE_H
