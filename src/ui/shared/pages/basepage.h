#ifndef BASEPAGE_H
#define BASEPAGE_H

#include "core/result.h"
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

struct PageOutputCapabilities
{
    bool printEnabled = false;
    bool saveAsEnabled = false;
};

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

    [[nodiscard]] virtual Status prepareForActivation();
    virtual void releaseFeatureResources();

    [[nodiscard]] virtual PageOutputCapabilities
        outputCapabilities() const;

    virtual void printCurrentPage();

    virtual void saveCurrentPageAs();

    void setDatabaseOpen(
        bool databaseOpen
        );



signals:

    void initialSetupRequested();

    void openDatabaseRequested();

    void newDatabaseRequested();

    void outputCapabilitiesChanged();



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

    [[nodiscard]] bool isDatabaseOpen() const;



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

    QLabel* m_initialSetupDescription = nullptr;

    QLabel* m_newDatabaseDescription = nullptr;

    QLabel* m_openDatabaseDescription = nullptr;

    QPushButton* m_openDatabaseButton = nullptr;

    QPushButton* m_newDatabaseButton = nullptr;

    QPushButton* m_initialSetupButton = nullptr;

    bool m_noDatabaseBannerEnabled = false;

    bool m_noDatabaseBannerFontUpdateQueued = false;

    bool m_databaseOpen = false;

    void updateNoDatabaseBannerGeometry();

    void updateNoDatabaseBannerLayout();

    void updateNoDatabaseBannerButtonWidths();

    void updateNoDatabaseTitleFont();

    void scheduleNoDatabaseBannerFontUpdate();
};

#endif // BASEPAGE_H
