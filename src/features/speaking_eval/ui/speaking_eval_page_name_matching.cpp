#include "speaking_eval_page_p.h"

QList<SpeakingEvalCellEdit> SpeakingEvalPage::nameImportChanges(
    const QStringList& rosterColumns,
    const QList<QStringList>& rosterRows
    ) const
{
    QList<SpeakingEvalCellEdit> changes;

    const int rosterEnglishColumn =
        findColumn(
            rosterColumns,
            QStringLiteral("English")
            );

    const int rosterKoreanColumn =
        findColumn(
            rosterColumns,
            QStringLiteral("Korean")
            );

    if (rosterEnglishColumn < 0 || rosterKoreanColumn < 0)
    {
        return changes;
    }

    const int evaluationEnglishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);

    const int evaluationKoreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    const SpeakingEvalRows evaluationRows =
        m_model->rows();

    const QHash<QString, QList<int>> rowsByNamePair =
        StudentNameUtils::rowsByNamePair(
            evaluationRows,
            evaluationEnglishColumn,
            evaluationKoreanColumn
            );

    QList<int> availableRows;

    for (int row = 0; row < evaluationRows.size(); ++row)
    {
        if (
            evaluationRows[row]
                .value(evaluationEnglishColumn)
                .trimmed()
                .isEmpty()
            && evaluationRows[row]
                   .value(evaluationKoreanColumn)
                   .trimmed()
                   .isEmpty()
            )
        {
            availableRows.append(row);
        }
    }

    const auto appendChange =
        [this, &changes](int row, SpeakingEvalColumn column, const QString& value)
        {
            const QModelIndex index =
                m_model->index(
                    row,
                    SpeakingEval::toInt(column)
                    );

            if (
                !index.isValid()
                || !(m_model->flags(index) & Qt::ItemIsEditable)
                )
            {
                return;
            }

            const QString oldValue =
                index.data(Qt::EditRole).toString();

            if (oldValue == value)
            {
                return;
            }

            changes.append(
                {
                    row,
                    SpeakingEval::toInt(column),
                    oldValue,
                    value
                }
                );
        };

    QSet<QString> importedNamePairs;
    int availableRowIndex = 0;

    for (const QStringList& rosterRow : rosterRows)
    {
        const QString englishName =
            rosterEnglishColumn < rosterRow.size()
                ? rosterRow[rosterEnglishColumn].trimmed()
                : QString();

        const QString koreanName =
            rosterKoreanColumn < rosterRow.size()
                ? rosterRow[rosterKoreanColumn].trimmed()
                : QString();

        const QString namePairKey =
            StudentNameUtils::namePairKey(
                englishName,
                koreanName
                );

        if (
            namePairKey.isEmpty()
            || importedNamePairs.contains(namePairKey)
            || rowsByNamePair.contains(namePairKey)
            )
        {
            continue;
        }

        if (availableRowIndex >= availableRows.size())
        {
            break;
        }

        importedNamePairs.insert(namePairKey);

        const int targetRow =
            availableRows[availableRowIndex++];

        appendChange(
            targetRow,
            SpeakingEvalColumn::EnglishName,
            englishName
            );

        appendChange(
            targetRow,
            SpeakingEvalColumn::KoreanName,
            koreanName
            );

    }

    return changes;
}

void SpeakingEvalPage::handleNameCellChanged(
    const QModelIndex& topLeft,
    const QModelIndex& bottomRight
    )
{
    if (
        m_loadingEvaluation
        || m_importingNames
        || m_resolvingDuplicateName
        || !m_model
        || !m_table
        || !topLeft.isValid()
        || !bottomRight.isValid()
        )
    {
        return;
    }

    m_table->viewport()->update();

    const int englishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);

    const int koreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row)
    {
        for (int column = topLeft.column(); column <= bottomRight.column(); ++column)
        {
            if (column == englishColumn || column == koreanColumn)
            {
                resolveDuplicateName(
                    row,
                    column
                    );
                return;
            }
        }
    }
}

void SpeakingEvalPage::resolveDuplicateName(
    int row,
    int editedColumn
    )
{
    if (!m_model || !m_table)
    {
        return;
    }

    const int englishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);

    const int koreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    if (editedColumn != englishColumn && editedColumn != koreanColumn)
    {
        return;
    }

    const QList<int> duplicateRows =
        m_model->duplicateNameRows(row);

    if (duplicateRows.isEmpty())
    {
        return;
    }

    QStringList duplicateRowLabels;

    for (int duplicateRow : duplicateRows)
    {
        duplicateRowLabels.append(
            QString::number(duplicateRow + 1)
            );
    }

    const QString suggestedName =
        m_model->suggestedKoreanNameWithSuffix(row);

    const QList<QStringList> rosterCandidates =
        unmatchedRosterNamePairs();

    QMessageBox dialog(this);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(
        tr("Duplicate Student Name")
        );
    dialog.setText(
        tr("This English/Korean name combination already exists.")
        );
    dialog.setInformativeText(
        tr("Duplicate row(s): %1").arg(
            duplicateRowLabels.join(QStringLiteral(", "))
            )
        );

    QPushButton* suffixButton =
        dialog.addButton(
            suggestedName.isEmpty()
                ? tr("No Suffix Available")
                : tr("Use %1").arg(suggestedName),
            QMessageBox::AcceptRole
            );

    suffixButton->setEnabled(
        !suggestedName.isEmpty()
        );

    QPushButton* clearButton =
        dialog.addButton(
            tr("Clear Edited Cell"),
            QMessageBox::DestructiveRole
            );

    QPushButton* locateButton =
        dialog.addButton(
            tr("Locate Duplicate"),
            QMessageBox::ActionRole
            );

    QPushButton* matchButton = nullptr;

    if (!rosterCandidates.isEmpty())
    {
        matchButton =
            dialog.addButton(
                tr("Use Roster Match..."),
                QMessageBox::ActionRole
                );
    }

    QPushButton* keepButton =
        dialog.addButton(
            tr("Keep As-Is"),
            QMessageBox::RejectRole
            );

    if (!suggestedName.isEmpty())
    {
        dialog.setDefaultButton(suffixButton);
    }
    else
    {
        dialog.setDefaultButton(clearButton);
    }

    dialog.exec();

    QAbstractButton* clickedButton =
        dialog.clickedButton();

    const auto applySingleChange =
        [this, row](int column, const QString& newValue, const QString& description)
        {
            const QModelIndex index =
                m_model->index(
                    row,
                    column
                    );

            if (!index.isValid())
            {
                return;
            }

            const QString oldValue =
                index.data(Qt::EditRole).toString();

            if (oldValue == newValue)
            {
                return;
            }

            m_resolvingDuplicateName = true;
            m_table->applyChanges(
                {
                    {
                        row,
                        column,
                        oldValue,
                        newValue
                    }
                },
                description
                );
            m_resolvingDuplicateName = false;
        };

    if (clickedButton == suffixButton && !suggestedName.isEmpty())
    {
        applySingleChange(
            koreanColumn,
            suggestedName,
            tr("Resolve Duplicate Name")
            );

        selectEvaluationCell(
            row,
            SpeakingEvalColumn::KoreanName
            );
    }
    else if (clickedButton == clearButton)
    {
        applySingleChange(
            editedColumn,
            QString(),
            tr("Clear Duplicate Name")
            );

        selectEvaluationCell(
            row,
            SpeakingEval::columnFromInt(editedColumn)
            );
    }
    else if (clickedButton == locateButton)
    {
        selectEvaluationCell(
            duplicateRows.first(),
            SpeakingEvalColumn::EnglishName
            );
    }
    else if (matchButton && clickedButton == matchButton)
    {
        QStringList options;

        for (const QStringList& candidate : rosterCandidates)
        {
            options.append(
                tr("%1 / %2")
                    .arg(candidate.value(0))
                    .arg(candidate.value(1))
                );
        }

        bool accepted = false;

        const QString selected =
            QInputDialog::getItem(
                this,
                tr("Match Roster Student"),
                tr("Student:"),
                options,
                0,
                false,
                &accepted
                );

        const int selectedIndex =
            options.indexOf(selected);

        if (accepted && selectedIndex >= 0)
        {
            const QStringList candidate =
                rosterCandidates[selectedIndex];

            QList<SpeakingEvalCellEdit> changes;

            const QModelIndex englishIndex =
                m_model->index(
                    row,
                    englishColumn
                    );

            const QModelIndex koreanIndex =
                m_model->index(
                    row,
                    koreanColumn
                    );

            const QString oldEnglish =
                englishIndex.data(Qt::EditRole).toString();

            const QString oldKorean =
                koreanIndex.data(Qt::EditRole).toString();

            if (oldEnglish != candidate.value(0))
            {
                changes.append(
                    {
                        row,
                        englishColumn,
                        oldEnglish,
                        candidate.value(0)
                    }
                    );
            }

            if (oldKorean != candidate.value(1))
            {
                changes.append(
                    {
                        row,
                        koreanColumn,
                        oldKorean,
                        candidate.value(1)
                    }
                    );
            }

            m_resolvingDuplicateName = true;
            m_table->applyChanges(
                changes,
                tr("Match Roster Name")
                );
            m_resolvingDuplicateName = false;

            selectEvaluationCell(
                row,
                SpeakingEvalColumn::EnglishName
                );
        }
    }
    else
    {
        Q_UNUSED(keepButton);
    }

    m_table->viewport()->update();
    updateActions();
}

QList<QStringList> SpeakingEvalPage::unmatchedRosterNamePairs() const
{
    QList<QStringList> candidates;

    if (
        !m_services
        || !m_services->dataService()
        || !m_model
        || m_classroom.id <= 0
        )
    {
        return candidates;
    }

    const Roster roster =
        m_services
            ->dataService()
            ->loadRoster(
                m_classroom.id
                );

    const int englishColumn =
        findColumn(
            roster.columns,
            QStringLiteral("English")
            );

    const int koreanColumn =
        findColumn(
            roster.columns,
            QStringLiteral("Korean")
            );

    if (englishColumn < 0 || koreanColumn < 0)
    {
        return candidates;
    }

    QSet<QString> seen;

    for (const QStringList& row : roster.rows)
    {
        const QString englishName =
            englishColumn < row.size()
                ? row[englishColumn].trimmed()
                : QString();

        const QString koreanName =
            koreanColumn < row.size()
                ? row[koreanColumn].trimmed()
                : QString();

        if (
            englishName.isEmpty()
            || koreanName.isEmpty()
            || m_model->containsNamePair(
                englishName,
                koreanName
                )
            )
        {
            continue;
        }

        const QString key =
            englishName
            + QChar(0x001F)
            + koreanName;

        if (seen.contains(key))
        {
            continue;
        }

        seen.insert(key);
        candidates.append(
            {
                englishName,
                koreanName
            }
            );
    }

    return candidates;
}

void SpeakingEvalPage::selectEvaluationCell(
    int row,
    SpeakingEvalColumn column
    )
{
    if (!m_model || !m_table)
    {
        return;
    }

    const QModelIndex index =
        m_model->index(
            row,
            SpeakingEval::toInt(column)
            );

    if (!index.isValid())
    {
        return;
    }

    m_table->setCurrentIndex(index);
    m_table->scrollTo(index);
}
