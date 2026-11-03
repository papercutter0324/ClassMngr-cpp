#pragma once

#include "core/settingsmanager.h"
#include "next/application/roster_row_display_policy.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>

namespace Ui
{

class RosterRowPreferencesGroup final : public QGroupBox
{
    Q_DECLARE_TR_FUNCTIONS(RosterRowPreferencesGroup)

public:
    explicit RosterRowPreferencesGroup(QWidget* parent = nullptr)
        : QGroupBox(parent)
    {
        setObjectName(QStringLiteral("preferencesRosterRowsGroup"));
        auto* const form = new QFormLayout(this);

        m_maximumRows = new QSpinBox(this);
        m_maximumRows->setObjectName(
            QStringLiteral("preferencesMaximumRosterRows"));
        m_maximumRows->setRange(
            ClassMngr::Next::Application::MinimumMaximumRosterRows,
            ClassMngr::Next::Application::MaximumRosterRows);
        m_maximumRows->setToolTip(tr(
            "Set the maximum number of rows for Class Roster and Speaking Evaluation."));
        form->addRow(tr("Maximum Rows:"), m_maximumRows);

        m_displayMode = new QComboBox(this);
        m_displayMode->setObjectName(
            QStringLiteral("preferencesRosterRowDisplayMode"));
        m_displayMode->addItem(tr("Display All Rows"), 0);
        m_displayMode->addItem(tr("Display 5 Extra Rows"), 5);
        m_displayMode->addItem(tr("Display 10 Extra Rows"), 10);
        m_displayMode->addItem(tr("Display 15 Extra Rows"), 15);
        m_displayMode->addItem(tr("Display 20 Extra Rows"), 20);
        m_displayMode->addItem(tr("Display 25 Extra Rows"), 25);
        m_displayMode->setToolTip(tr(
            "Show all rows, or show blank rows after the last row with data."));
        form->addRow(tr("Rows to Display:"), m_displayMode);

        auto& settings = SettingsManager::instance();
        m_savedMaximumRows = settings.maximumRosterRows();
        m_savedDisplayMode = settings.rosterRowDisplayMode();
        m_maximumRows->setValue(m_savedMaximumRows);
        m_displayMode->setCurrentIndex(
            m_displayMode->findData(m_savedDisplayMode));
        retranslate();

        connect(
            m_maximumRows,
            &QSpinBox::valueChanged,
            this,
            [this](const int candidateRows)
            {
                saveMaximumRows(candidateRows);
            });
        connect(
            m_displayMode,
            &QComboBox::currentIndexChanged,
            this,
            [this](const int index)
            {
                saveDisplayMode(index);
            });
    }

protected:
    void changeEvent(QEvent* event) override
    {
        QGroupBox::changeEvent(event);
        if (event && event->type() == QEvent::LanguageChange)
        {
            retranslate();
        }
    }

private:
    void retranslate()
    {
        setTitle(tr("Roster and Speaking Evaluation"));
        if (auto* const form = qobject_cast<QFormLayout*>(layout()))
        {
            if (QLabel* const maximumLabel = qobject_cast<QLabel*>(
                    form->labelForField(m_maximumRows)))
            {
                maximumLabel->setText(tr("Maximum Rows:"));
            }
            if (QLabel* const displayLabel = qobject_cast<QLabel*>(
                    form->labelForField(m_displayMode)))
            {
                displayLabel->setText(tr("Rows to Display:"));
            }
        }
        m_maximumRows->setToolTip(tr(
            "Set the maximum number of rows for Class Roster and Speaking Evaluation."));
        m_displayMode->setToolTip(tr(
            "Show all rows, or show blank rows after the last row with data."));
        static constexpr const char* labels[]{
            "Display All Rows",
            "Display 5 Extra Rows",
            "Display 10 Extra Rows",
            "Display 15 Extra Rows",
            "Display 20 Extra Rows",
            "Display 25 Extra Rows"
        };
        for (int index = 0; index < m_displayMode->count(); ++index)
        {
            m_displayMode->setItemText(index, tr(labels[index]));
        }
    }

    void restoreMaximumRows()
    {
        const QSignalBlocker blocker(m_maximumRows);
        m_maximumRows->setValue(m_savedMaximumRows);
    }

    void saveMaximumRows(const int candidateRows)
    {
        if (ClassMngr::Next::Application::shouldConfirmMaximumRosterRowsChange(
                m_savedMaximumRows, candidateRows))
        {
            const auto answer = QMessageBox::question(
                this,
                tr("Confirm Maximum Rows"),
                tr("Is this number correct?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes)
            {
                restoreMaximumRows();
                return;
            }
        }

        if (!SettingsManager::instance().writeMaximumRosterRowsPreference(
                candidateRows))
        {
            restoreMaximumRows();
            QMessageBox::warning(
                this,
                tr("Unable to Save Preference"),
                tr("The maximum row setting could not be saved."));
            return;
        }
        m_savedMaximumRows = candidateRows;
    }

    void saveDisplayMode(const int index)
    {
        if (index < 0)
        {
            return;
        }
        const int candidateMode = m_displayMode->itemData(index).toInt();
        if (!SettingsManager::instance().writeRosterRowDisplayModePreference(
                candidateMode))
        {
            const QSignalBlocker blocker(m_displayMode);
            m_displayMode->setCurrentIndex(
                m_displayMode->findData(m_savedDisplayMode));
            QMessageBox::warning(
                this,
                tr("Unable to Save Preference"),
                tr("The row display setting could not be saved."));
            return;
        }
        m_savedDisplayMode = candidateMode;
    }

    QSpinBox* m_maximumRows = nullptr;
    QComboBox* m_displayMode = nullptr;
    int m_savedMaximumRows =
        ClassMngr::Next::Application::DefaultMaximumRosterRows;
    int m_savedDisplayMode = static_cast<int>(
        ClassMngr::Next::Application::RosterRowDisplayMode::AllRows);
};

} // namespace Ui
