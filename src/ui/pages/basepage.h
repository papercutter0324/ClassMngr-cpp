#ifndef BASEPAGE_H
#define BASEPAGE_H

#include "ui/constants/options.h"

#include <QString>
#include <QWidget>

class QVBoxLayout;
class QHBoxLayout;
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



protected:

    QVBoxLayout* contentLayout() const;

    QHBoxLayout* bottomLayout() const;



private:

    // =====================================================
    // Layouts
    // =====================================================

    QVBoxLayout* m_mainLayout = nullptr;

    QVBoxLayout* m_contentLayout = nullptr;

    QWidget* m_bottomBar = nullptr;

    QHBoxLayout* m_bottomLayout = nullptr;
};

#endif // BASEPAGE_H
