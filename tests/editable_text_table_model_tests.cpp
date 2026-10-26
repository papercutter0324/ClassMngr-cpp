#include "ui/shared/models/editable_text_table_model.h"

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QTableView>
#include <QtTest>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
struct TestRow final
{
    int id = -1;
    QStringList cells;
    std::shared_ptr<int> generation;
};

class TestModel final : public ClassMngr::Ui::EditableTextTableModel<TestRow>
{
public:
    explicit TestModel(const int columns)
        : EditableTextTableModel(columns, &TestRow::cells, Qt::AlignCenter)
    {
    }

    QModelIndex uncheckedIndex(const int row, const int column) const
    {
        return createIndex(row, column);
    }
};

TestRow rowWithId(const int id, const int columns)
{
    TestRow row;
    row.id = id;
    for (int column = 0; column < columns; ++column)
    {
        row.cells.push_back(QStringLiteral("%1:%2").arg(id).arg(column));
    }
    return row;
}
}

class EditableTextTableModelTests final : public QObject
{
    Q_OBJECT

private slots:
    void schemasExposeRolesHeadersAndMetadata_data();
    void schemasExposeRolesHeadersAndMetadata();
    void editsRejectInvalidRequestsAndNotifyChangedCells();
    void rowOperationsSignalAndPreserveMetadata();
    void resetReplacesThePreviousGenerationAndClearsRows();
    void windowRowsExposeMetadataAndRejectInvalidReplacement();
    void windowedTableViewBoundsResidentRowsAcrossLargeLogicalSource();
};

void EditableTextTableModelTests::schemasExposeRolesHeadersAndMetadata_data()
{
    QTest::addColumn<int>("columns");
    QTest::newRow("five-columns") << 5;
    QTest::newRow("six-columns") << 6;
}

void EditableTextTableModelTests::schemasExposeRolesHeadersAndMetadata()
{
    QFETCH(int, columns);
    TestModel model(columns);
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRows({rowWithId(77, columns)});
    QVERIFY(!model.isWindowed());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.logicalRowOffset(), qint64(0));
    QCOMPARE(model.totalRowCount(), qint64(1));
    QCOMPARE(model.windowCapacity(), 0);
    QCOMPARE(model.columnCount(), columns);
    QCOMPARE(model.rowCount(model.index(0, 0)), 0);
    QCOMPARE(model.columnCount(model.index(0, 0)), 0);
    QCOMPARE(model.rowAt(0).id, 77);

    QStringList headers;
    for (int column = 0; column < columns; ++column)
    {
        const QModelIndex index = model.index(0, column);
        QCOMPARE(index.data(Qt::DisplayRole).toString(), model.rowAt(0).cells.at(column));
        QCOMPARE(index.data(Qt::EditRole), index.data(Qt::DisplayRole));
        QCOMPARE(index.data(Qt::TextAlignmentRole).toInt(), int(Qt::AlignCenter));
        QVERIFY(!index.data(Qt::DecorationRole).isValid());
        QCOMPARE(model.flags(index), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        headers.push_back(QStringLiteral("Column %1").arg(column));
    }
    QSignalSpy headerChanges(&model, &QAbstractItemModel::headerDataChanged);
    model.setHeaders(headers);
    QCOMPARE(headerChanges.size(), 1);
    QCOMPARE(headerChanges.at(0).at(0).value<Qt::Orientation>(), Qt::Horizontal);
    QCOMPARE(headerChanges.at(0).at(1).toInt(), 0);
    QCOMPARE(headerChanges.at(0).at(2).toInt(), columns - 1);
    for (int column = 0; column < columns; ++column)
    {
        QCOMPARE(model.headerData(column, Qt::Horizontal).toString(), headers.at(column));
    }
    QCOMPARE(model.headerData(0, Qt::Vertical).toInt(), 1);
    QVERIFY(!model.headerData(columns, Qt::Horizontal).isValid());
    QVERIFY(!model.headerData(-1, Qt::Horizontal).isValid());
    QVERIFY(!model.headerData(1, Qt::Vertical).isValid());
    QVERIFY(!model.headerData(0, Qt::Horizontal, Qt::EditRole).isValid());
    model.setHeaders(headers);
    QCOMPARE(headerChanges.size(), 1);
    model.setHeaders({});
    QCOMPARE(headerChanges.size(), 2);
    QCOMPARE(headerChanges.at(1).at(2).toInt(), columns - 1);
    QCOMPARE(model.headerData(columns - 1, Qt::Horizontal).toInt(), columns);
}

void EditableTextTableModelTests::editsRejectInvalidRequestsAndNotifyChangedCells()
{
    TestModel model(5);
    model.setRows({rowWithId(10, 5)});
    TestModel other(5);
    other.setRows({rowWithId(20, 5)});
    QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
    const QModelIndex index = model.index(0, 2);
    for (const QModelIndex invalid : {
             QModelIndex(), model.uncheckedIndex(-1, 0), model.uncheckedIndex(1, 0),
             model.uncheckedIndex(0, -1), model.uncheckedIndex(0, 5), other.index(0, 0)})
    {
        QVERIFY(!model.data(invalid, Qt::DisplayRole).isValid());
        QCOMPARE(model.flags(invalid), Qt::NoItemFlags);
        QVERIFY(!model.setData(invalid, QStringLiteral("rejected")));
    }
    QVERIFY(!model.setData(index, QStringLiteral("rejected"), Qt::DisplayRole));
    QVERIFY(model.setData(index, index.data(Qt::EditRole)));
    QCOMPARE(changes.size(), 0);
    QVERIFY(model.setData(index, QStringLiteral("edited")));
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.at(0).at(0).value<QModelIndex>(), index);
    QCOMPARE(changes.at(0).at(1).value<QModelIndex>(), index);
    QCOMPARE(changes.at(0).at(2).value<QList<int>>(), (QList<int>{Qt::DisplayRole, Qt::EditRole}));
    QCOMPARE(index.data(Qt::DisplayRole).toString(), QStringLiteral("edited"));
    QCOMPARE(model.rowAt(0).id, 10);
    model.setEditingEnabled(false);
    QVERIFY(!(model.flags(index) & Qt::ItemIsEditable));
    QVERIFY(!model.setData(index, QStringLiteral("locked")));
    QVERIFY(!model.insertRows(0, 1));
    QVERIFY(!model.removeRows(0, 1));
    QCOMPARE(changes.size(), 1);
    model.setEditingEnabled(true);
    QVERIFY(model.flags(index) & Qt::ItemIsEditable);
}

void EditableTextTableModelTests::rowOperationsSignalAndPreserveMetadata()
{
    TestModel model(6);
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRows({rowWithId(10, 6), rowWithId(20, 6)});
    QPersistentModelIndex second(model.index(1, 0));
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QVERIFY(model.insertRows(1, 2));
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(inserted.size(), 1);
    QCOMPARE(inserted.at(0).at(1).toInt(), 1);
    QCOMPARE(inserted.at(0).at(2).toInt(), 2);
    QCOMPARE(second.row(), 3);
    QCOMPARE(model.rowAt(second.row()).id, 20);
    QCOMPARE(model.rowAt(1).id, -1);
    QCOMPARE(model.rowAt(1).cells, QStringList(6, QString()));
    QVERIFY(model.removeRows(1, 2));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(removed.size(), 1);
    QCOMPARE(removed.at(0).at(1).toInt(), 1);
    QCOMPARE(removed.at(0).at(2).toInt(), 2);
    QCOMPARE(second.row(), 1);
    QCOMPARE(model.rowAt(second.row()).id, 20);
    QVERIFY(!model.insertRows(-1, 1));
    QVERIFY(!model.insertRows(3, 1));
    QVERIFY(!model.insertRows(0, 0));
    QVERIFY(!model.insertRows(0, std::numeric_limits<int>::max()));
    QVERIFY(!model.insertRows(0, 1, model.index(0, 0)));
    QVERIFY(!model.removeRows(-1, 1));
    QVERIFY(!model.removeRows(2, 1));
    QVERIFY(!model.removeRows(0, 0));
    QVERIFY(!model.removeRows(1, std::numeric_limits<int>::max()));
    QVERIFY(!model.removeRows(0, 1, model.index(0, 0)));
    QCOMPARE(inserted.size(), 1);
    QCOMPARE(removed.size(), 1);
}

void EditableTextTableModelTests::resetReplacesThePreviousGenerationAndClearsRows()
{
    TestModel model(5);
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    auto generation = std::make_shared<int>(1);
    const std::weak_ptr<int> priorGeneration = generation;
    TestRow old = rowWithId(10, 5);
    old.generation = std::move(generation);
    model.setRows({std::move(old)});
    QPersistentModelIndex oldIndex(model.index(0, 0));
    model.setRows({rowWithId(20, 5)});
    QVERIFY(priorGeneration.expired());
    QVERIFY(!oldIndex.isValid());
    QCOMPARE(model.rowAt(0).id, 20);
    QCOMPARE(resets.size(), 2);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, model.setRows({rowWithId(30, 4)}));
    QCOMPARE(model.rowAt(0).id, 20);
    QCOMPARE(resets.size(), 2);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, model.setHeaders(QStringList(6, QStringLiteral("extra"))));
    model.setRows({});
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 5);
    QCOMPARE(resets.size(), 3);
    QVERIFY(!model.index(0, 0).isValid());
    QVERIFY(!model.removeRows(0, 1));
}

void EditableTextTableModelTests::windowRowsExposeMetadataAndRejectInvalidReplacement()
{
    TestModel model(5);
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);

    auto priorGeneration = std::make_shared<int>(10);
    const std::weak_ptr<int> priorGenerationRef = priorGeneration;
    TestRow priorRow = rowWithId(10, 5);
    priorRow.generation = std::move(priorGeneration);
    model.setWindowRows({std::move(priorRow)}, 12, 40, 2);
    QPersistentModelIndex priorIndex(model.index(0, 0));
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.isWindowed());
    QCOMPARE(model.logicalRowOffset(), qint64(12));
    QCOMPARE(model.totalRowCount(), qint64(40));
    QCOMPARE(model.windowCapacity(), 2);
    QCOMPARE(model.rowAt(0).id, 10);
    QCOMPARE(resets.size(), 1);

    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        model.setWindowRows({rowWithId(20, 5)}, 0, 1, 0));
    QVERIFY_THROWS_EXCEPTION(
        std::length_error,
        model.setWindowRows(
            {rowWithId(20, 5), rowWithId(21, 5), rowWithId(22, 5)}, 0, 3, 2));
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        model.setWindowRows({rowWithId(25, 5)}, 13, 40, 3));
    QVERIFY(priorIndex.isValid());
    QVERIFY(!priorGenerationRef.expired());
    QCOMPARE(model.rowAt(0).id, 10);
    QCOMPARE(model.logicalRowOffset(), qint64(12));
    QCOMPARE(model.totalRowCount(), qint64(40));
    QCOMPARE(model.windowCapacity(), 2);
    QCOMPARE(resets.size(), 1);
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        model.setWindowRows({rowWithId(20, 5)}, -1, 40, 2));
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        model.setWindowRows({rowWithId(20, 5)}, 0, -1, 2));
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        model.setWindowRows({rowWithId(20, 5)}, 41, 40, 2));
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        model.setWindowRows({rowWithId(20, 5)}, 40, 40, 2));
    QVERIFY_THROWS_EXCEPTION(
        std::length_error,
        model.setWindowRows(
            {rowWithId(20, 5)}, std::numeric_limits<qint64>::max(),
            std::numeric_limits<qint64>::max(), 2));
    QVERIFY_THROWS_EXCEPTION(
        std::invalid_argument,
        model.setWindowRows({rowWithId(20, 4)}, 0, 1, 2));

    QCOMPARE(resets.size(), 1);
    QVERIFY(priorIndex.isValid());
    QVERIFY(!priorGenerationRef.expired());
    QCOMPARE(model.logicalRowOffset(), qint64(12));
    QCOMPARE(model.totalRowCount(), qint64(40));
    QCOMPARE(model.windowCapacity(), 2);
    QCOMPARE(model.rowAt(0).id, 10);

    model.setWindowRows({rowWithId(30, 5), rowWithId(31, 5)}, 14, 40, 2);
    QVERIFY(priorGenerationRef.expired());
    QVERIFY(!priorIndex.isValid());
    QCOMPARE(resets.size(), 2);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.logicalRowOffset(), qint64(14));
    QCOMPARE(model.totalRowCount(), qint64(40));
    QVERIFY(!model.insertRows(0, 1));
    QVERIFY(!model.removeRows(0, 1));
    QCOMPARE(model.rowCount(), 2);

    model.setRows({rowWithId(50, 5)});
    QVERIFY(!model.isWindowed());
    QCOMPARE(model.logicalRowOffset(), qint64(0));
    QCOMPARE(model.totalRowCount(), qint64(1));
    QCOMPARE(model.windowCapacity(), 0);
    QVERIFY(model.insertRows(1, 1));
    QVERIFY(model.removeRows(1, 1));

    const qint64 wideOffset =
        static_cast<qint64>(std::numeric_limits<int>::max()) + 42;
    const qint64 wideTotal = wideOffset + 1;
    model.setWindowRows({rowWithId(60, 5)}, wideOffset, wideTotal, 3);
    QVERIFY(model.isWindowed());
    QCOMPARE(model.logicalRowOffset(), wideOffset);
    QCOMPARE(model.totalRowCount(), wideTotal);
    QCOMPARE(model.windowCapacity(), 3);
}

void EditableTextTableModelTests::
    windowedTableViewBoundsResidentRowsAcrossLargeLogicalSource()
{
    constexpr qint64 logicalRowCount = 16387;
    constexpr int capacity = 64;
    TestModel model(5);
    QAbstractItemModelTester tester(
        &model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QTableView view;
    view.setModel(&model);
    view.resize(640, 360);
    view.show();
    QApplication::processEvents();
    QCOMPARE(view.model(), &model);

    std::weak_ptr<int> previousGeneration;
    int maximumResidentRows = 0;
    for (qint64 offset = 0; offset < logicalRowCount; offset += capacity)
    {
        const int count = static_cast<int>(
            logicalRowCount - offset < capacity ? logicalRowCount - offset : capacity);
        auto generation = std::make_shared<int>(static_cast<int>(offset));
        QVector<TestRow> rows;
        rows.reserve(count);
        for (int localRow = 0; localRow < count; ++localRow)
        {
            TestRow row = rowWithId(static_cast<int>(offset) + localRow, 5);
            row.generation = generation;
            rows.push_back(std::move(row));
        }

        model.setWindowRows(std::move(rows), offset, logicalRowCount, capacity);
        QVERIFY(model.rowCount() <= capacity);
        QCOMPARE(view.model()->rowCount(), model.rowCount());
        QCOMPARE(model.logicalRowOffset(), offset);
        QCOMPARE(model.totalRowCount(), logicalRowCount);
        QCOMPARE(model.windowCapacity(), capacity);
        QCOMPARE(model.rowAt(0).id, static_cast<int>(offset));
        maximumResidentRows = std::max(maximumResidentRows, model.rowCount());

        if (offset > 0)
        {
            QVERIFY(previousGeneration.expired());
        }
        previousGeneration = generation;
    }

    QCOMPARE(model.rowCount(), 3);
    QVERIFY(maximumResidentRows <= capacity);
    QVERIFY(!previousGeneration.expired());
    model.setRows({});
    QVERIFY(previousGeneration.expired());
    QCOMPARE(view.model()->rowCount(), 0);
}

QTEST_MAIN(EditableTextTableModelTests)

#include "editable_text_table_model_tests.moc"
