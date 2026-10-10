#include "sub_prep_class_information_list_model.h"

#include "features/classes/config/class_info_config.h"
#include "ui/shared/widgets/navigation_pill_style.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStyle>
#include <QStyleOption>
#include <QHash>
#include <QWheelEvent>
#include <QVariant>

#include <algorithm>
#include <string>
#include <utility>

namespace
{
constexpr int UnknownOrder = 1'000;

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

int gradeOrder(
    const QString& grade
    )
{
    const int index = ClassInfoConfig::Grades.indexOf(grade);
    return index >= 0 ? index : UnknownOrder;
}

int levelOrder(
    const QString& grade,
    const QString& level
    )
{
    const int index = ClassInfoConfig::levelsForGrade(grade).indexOf(level);
    return index >= 0 ? index : UnknownOrder;
}

QString classLabel(
    const ClassMngr::Next::Application::ClassSummary& summary
    )
{
    const QString level = fromUtf8(summary.level).trimmed();
    if (!level.isEmpty())
    {
        return level;
    }

    return fromUtf8(summary.displayLabel).trimmed();
}

QString baseClassLabel(
    const ClassMngr::Next::Application::ClassSummary& summary
    )
{
    const QString navigationLabel =
        fromUtf8(summary.navigationLabel).trimmed();
    if (!navigationLabel.isEmpty())
    {
        return navigationLabel;
    }

    const QString name = classLabel(summary);
    const QString meeting = fromUtf8(summary.meetingText).trimmed();
    if (name.isEmpty())
    {
        return meeting;
    }
    if (meeting.isEmpty())
    {
        return name;
    }

    return QStringLiteral("%1 • %2").arg(name, meeting);
}

QString expandedClassLabel(
    const QString& baseLabel,
    const QString& teacherLabel
    )
{
    return teacherLabel.isEmpty()
        ? baseLabel
        : QStringLiteral("%1 • %2").arg(baseLabel, teacherLabel);
}
}

SubPrepClassInformationListModel::SubPrepClassInformationListModel(
    QObject* parent
    )
    : QAbstractListModel(parent)
{
}

int SubPrepClassInformationListModel::rowCount(
    const QModelIndex& parent
    ) const
{
    return parent.isValid()
        ? 0
        : static_cast<int>(m_visibleRows.size());
}

QVariant SubPrepClassInformationListModel::data(
    const QModelIndex& index,
    int role
    ) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }

    const auto* summary = summaryAt(index.row());
    if (!summary)
    {
        return {};
    }

    switch (role)
    {
    case Qt::DisplayRole:
        return m_displayLabels.value(index.row());
    case Qt::ToolTipRole:
        return QStringLiteral("%1 (%2)")
            .arg(
                fromUtf8(summary->displayLabel),
                fromUtf8(summary->id.value())
                );
    case ClassIdRole:
        return fromUtf8(summary->id.value());
    case GradeRole:
        return fromUtf8(summary->grade);
    case LevelRole:
        return fromUtf8(summary->level);
    case MeetingTextRole:
        return fromUtf8(summary->meetingText);
    case TeacherDisplayNameRole:
        return teacherDisplayName(*summary);
    case StudentCountRole:
        return static_cast<qulonglong>(summary->studentCount);
    default:
        return {};
    }
}

QHash<int, QByteArray> SubPrepClassInformationListModel::roleNames() const
{
    return {
        {ClassIdRole, "classId"},
        {GradeRole, "grade"},
        {LevelRole, "level"},
        {MeetingTextRole, "meetingText"},
        {TeacherDisplayNameRole, "teacherDisplayName"},
        {StudentCountRole, "studentCount"}
    };
}

void SubPrepClassInformationListModel::setProjection(
    ClassMngr::Next::Application::ClassSummaryProjection projection
    )
{
    const QString previousGrade = m_currentGrade;
    beginResetModel();
    m_projection = std::move(projection);

    m_grades.clear();
    for (const auto& summary : m_projection.classes())
    {
        const QString grade = fromUtf8(summary.grade).trimmed();
        if (!m_grades.contains(grade))
        {
            m_grades.append(grade);
        }
    }

    std::sort(
        m_grades.begin(),
        m_grades.end(),
        [](const QString& left, const QString& right)
        {
            const int leftOrder = gradeOrder(left);
            const int rightOrder = gradeOrder(right);
            if (leftOrder != rightOrder)
            {
                return leftOrder < rightOrder;
            }
            if (leftOrder != UnknownOrder)
            {
                return false;
            }
            return QString::localeAwareCompare(left, right) < 0;
        }
        );

    m_currentGrade = m_grades.contains(previousGrade)
        ? previousGrade
        : m_grades.value(0);
    rebuildVisibleRows();
    endResetModel();
}

void SubPrepClassInformationListModel::setCurrentGrade(
    const QString& grade
    )
{
    if (grade == m_currentGrade || !m_grades.contains(grade))
    {
        return;
    }

    beginResetModel();
    m_currentGrade = grade;
    rebuildVisibleRows();
    endResetModel();
}

const QStringList& SubPrepClassInformationListModel::grades() const noexcept
{
    return m_grades;
}

const QString& SubPrepClassInformationListModel::currentGrade() const noexcept
{
    return m_currentGrade;
}

const ClassMngr::Next::Application::ClassSummaryProjection&
SubPrepClassInformationListModel::projection() const noexcept
{
    return m_projection;
}

const ClassMngr::Next::Application::ClassSummary*
SubPrepClassInformationListModel::summaryAt(int row) const noexcept
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_visibleRows.size())
    {
        return nullptr;
    }

    const std::size_t projectionIndex =
        m_visibleRows[static_cast<std::size_t>(row)];
    const auto& summaries = m_projection.classes();
    return projectionIndex < summaries.size()
        ? &summaries[projectionIndex]
        : nullptr;
}

std::optional<ClassMngr::Next::Domain::ClassId>
SubPrepClassInformationListModel::classIdAt(int row) const
{
    const auto* summary = summaryAt(row);
    return summary
        ? std::optional<ClassMngr::Next::Domain::ClassId>(summary->id)
        : std::nullopt;
}

int SubPrepClassInformationListModel::rowForClassId(
    const ClassMngr::Next::Domain::ClassId& classId
    ) const noexcept
{
    for (std::size_t row = 0; row < m_visibleRows.size(); ++row)
    {
        const auto* summary = summaryAt(static_cast<int>(row));
        if (summary && summary->id == classId)
        {
            return static_cast<int>(row);
        }
    }

    return -1;
}

void SubPrepClassInformationListModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    const auto& summaries = m_projection.classes();
    for (std::size_t index = 0; index < summaries.size(); ++index)
    {
        if (fromUtf8(summaries[index].grade).trimmed() == m_currentGrade)
        {
            m_visibleRows.push_back(index);
        }
    }

    std::stable_sort(
        m_visibleRows.begin(),
        m_visibleRows.end(),
        [&summaries](std::size_t leftIndex, std::size_t rightIndex)
        {
            const auto& left = summaries[leftIndex];
            const auto& right = summaries[rightIndex];
            const QString grade = fromUtf8(left.grade).trimmed();
            const int leftLevel = levelOrder(grade, fromUtf8(left.level).trimmed());
            const int rightLevel = levelOrder(grade, fromUtf8(right.level).trimmed());
            if (leftLevel != rightLevel)
            {
                return leftLevel < rightLevel;
            }
            if (left.navigationFirstDayOrder
                != right.navigationFirstDayOrder)
            {
                return left.navigationFirstDayOrder
                    < right.navigationFirstDayOrder;
            }
            if (left.navigationFirstTimeOrder
                != right.navigationFirstTimeOrder)
            {
                return left.navigationFirstTimeOrder
                    < right.navigationFirstTimeOrder;
            }

            const int labelComparison = QString::localeAwareCompare(
                baseClassLabel(left),
                baseClassLabel(right)
                );
            if (labelComparison != 0)
            {
                return labelComparison < 0;
            }
            return left.id < right.id;
        }
        );

    QStringList baseLabels;
    baseLabels.reserve(static_cast<qsizetype>(m_visibleRows.size()));
    QHash<QString, int> counts;
    for (const std::size_t index : m_visibleRows)
    {
        const QString label = baseClassLabel(summaries[index]);
        baseLabels.append(label);
        ++counts[label];
    }

    m_displayLabels.clear();
    m_displayLabels.reserve(baseLabels.size());
    for (std::size_t row = 0; row < m_visibleRows.size(); ++row)
    {
        const auto& summary = summaries[m_visibleRows[row]];
        QString label = baseLabels.at(static_cast<qsizetype>(row));
        if (counts.value(label) <= 1)
        {
            m_displayLabels.append(label);
            continue;
        }

        const QString teacher =
            fromUtf8(summary.navigationTeacherLabel).trimmed().isEmpty()
                ? teacherDisplayName(summary).trimmed()
                : fromUtf8(summary.navigationTeacherLabel).trimmed();
        const QString expandedLabel = expandedClassLabel(label, teacher);
        bool stillDuplicated = teacher.isEmpty();
        if (!stillDuplicated)
        {
            for (std::size_t otherRow = 0;
                 otherRow < m_visibleRows.size();
                 ++otherRow)
            {
                if (otherRow == row
                    || baseLabels.at(static_cast<qsizetype>(otherRow)) != label)
                {
                    continue;
                }

                const auto& otherSummary =
                    summaries[m_visibleRows[otherRow]];
                const QString otherNavigationTeacher =
                    fromUtf8(otherSummary.navigationTeacherLabel).trimmed();
                const QString otherTeacher = otherNavigationTeacher.isEmpty()
                    ? teacherDisplayName(otherSummary).trimmed()
                    : otherNavigationTeacher;
                if (expandedClassLabel(label, otherTeacher) == expandedLabel)
                {
                    stillDuplicated = true;
                    break;
                }
            }
        }
        if (stillDuplicated)
        {
            label = QStringLiteral("%1 #%2")
                .arg(expandedLabel, fromUtf8(summary.id.value()));
        }
        else
        {
            label = expandedLabel;
        }
        m_displayLabels.append(label);
    }
}

QString SubPrepClassInformationListModel::teacherDisplayName(
    const ClassMngr::Next::Application::ClassSummary& summary
    ) const
{
    if (!summary.teacherId.has_value())
    {
        return {};
    }

    const auto teacher = m_projection.findTeacher(*summary.teacherId);
    const QString legacyTeacher = fromUtf8(summary.navigationTeacherLabel).trimmed();
    return !legacyTeacher.isEmpty()
        ? legacyTeacher
        : teacher.has_value() ? fromUtf8(teacher->displayName) : QString();
}

SubPrepClassInformationTabSelector::SubPrepClassInformationTabSelector(
    QWidget* parent
    )
    : QWidget(parent)
{
    setObjectName(QStringLiteral("subPrepClassTabStrip"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    updateTabMetrics();
}

void SubPrepClassInformationTabSelector::setModel(
    SubPrepClassInformationListModel* model
    )
{
    if (m_model == model)
    {
        return;
    }

    if (m_model)
    {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    m_currentRow = -1;
    m_horizontalScrollOffset = 0;
    if (m_model)
    {
        connect(
            m_model,
            &QAbstractItemModel::modelReset,
            this,
            [this]()
            {
                updateTabMetrics();
            }
            );
        connect(
            m_model,
            &QAbstractItemModel::dataChanged,
            this,
            [this]()
            {
                updateTabMetrics();
            }
            );
        connect(
            m_model,
            &QAbstractItemModel::rowsInserted,
            this,
            [this]()
            {
                updateTabMetrics();
            }
            );
        connect(
            m_model,
            &QAbstractItemModel::rowsRemoved,
            this,
            [this]()
            {
                updateTabMetrics();
            }
            );
    }
    updateTabMetrics();
}

SubPrepClassInformationListModel*
SubPrepClassInformationTabSelector::model() const noexcept
{
    return m_model;
}

int SubPrepClassInformationTabSelector::currentRow() const noexcept
{
    return m_currentRow;
}

int SubPrepClassInformationTabSelector::horizontalScrollOffset() const noexcept
{
    return m_horizontalScrollOffset;
}

QRect SubPrepClassInformationTabSelector::tabRectForRow(int row) const noexcept
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_tabPositions.size())
    {
        return {};
    }

    const int startX = m_hasOverflow ? arrowButtonWidth() : 0;
    return QRect(
        startX + m_tabPositions[static_cast<std::size_t>(row)]
            - m_horizontalScrollOffset,
        0,
        m_tabWidth,
        m_tabHeight
        );
}

void SubPrepClassInformationTabSelector::setCurrentRow(int row)
{
    const int rowCount = m_model ? m_model->rowCount() : 0;
    if (row < 0 || row >= rowCount)
    {
        row = -1;
    }
    if (m_currentRow == row)
    {
        ensureCurrentVisible();
        update();
        return;
    }

    m_currentRow = row;
    ensureCurrentVisible();
    update();
    emit currentChanged(m_currentRow);
}

void SubPrepClassInformationTabSelector::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    updateTabMetrics();
}

void SubPrepClassInformationTabSelector::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Left)
    {
        setCurrentRow(std::max(0, m_currentRow - 1));
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right)
    {
        const int rowCount = m_model ? m_model->rowCount() : 0;
        setCurrentRow(std::min(rowCount - 1, m_currentRow + 1));
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SubPrepClassInformationTabSelector::mouseMoveEvent(QMouseEvent* event)
{
    const int hoveredRow = rowAt(event->pos());
    const bool hoveredLeftArrow = m_hasOverflow
        && event->pos().x() < arrowButtonWidth();
    const bool hoveredRightArrow = m_hasOverflow
        && event->pos().x() >= width() - arrowButtonWidth();
    if (m_hoveredRow != hoveredRow
        || m_hoveredLeftArrow != hoveredLeftArrow
        || m_hoveredRightArrow != hoveredRightArrow)
    {
        m_hoveredRow = hoveredRow;
        m_hoveredLeftArrow = hoveredLeftArrow;
        m_hoveredRightArrow = hoveredRightArrow;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void SubPrepClassInformationTabSelector::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void SubPrepClassInformationTabSelector::mouseReleaseEvent(
    QMouseEvent* event
    )
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (m_hasOverflow && event->pos().x() < arrowButtonWidth())
    {
        scrollRows(-1);
    }
    else if (m_hasOverflow
             && event->pos().x() >= width() - arrowButtonWidth())
    {
        scrollRows(1);
    }
    else
    {
        const int row = rowAt(event->pos());
        if (row >= 0)
        {
            setCurrentRow(row);
        }
    }
    setFocus(Qt::MouseFocusReason);
    event->accept();
}

void SubPrepClassInformationTabSelector::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    if (!m_model || m_model->rowCount() == 0 || m_tabWidth <= 0)
    {
        return;
    }

    const int startX = m_hasOverflow ? arrowButtonWidth() : 0;
    const int viewportWidth = contentViewportWidth();
    const bool darkPalette =
        palette().color(QPalette::Window).lightness() < 128;
    NavigationPillStyle::Colors tabColors;
    tabColors.fill = darkPalette
        ? QColor("#303030")
        : QColor("#deded8");
    tabColors.border = darkPalette
        ? QColor("#454545")
        : QColor("#c5c7c3");
    tabColors.hover = darkPalette
        ? QColor("#31363b")
        : QColor("#f5f5f5");
    tabColors.selected = QColor("#3daee9");
    tabColors.text = darkPalette
        ? QColor("#f0f0f0")
        : QColor("#27313a");
    if (m_hasOverflow)
    {
        const auto paintArrow = [this, &painter, darkPalette](
                                   const QRect& bounds,
                                   const bool left,
                                   const bool hovered,
                                   const bool enabled
                               )
        {
            const bool dark = darkPalette;
            const QColor background = !enabled
                ? (dark ? QColor("#292c2f") : QColor("#e1dfda"))
                : hovered
                    ? (dark ? QColor("#4b535a") : QColor("#e5eef5"))
                    : (dark ? QColor("#3b3f43") : QColor("#f5f3ee"));
            const QColor border = !enabled
                ? (dark ? QColor("#4b5055") : QColor("#bdc2c4"))
                : hovered
                    ? (dark ? QColor("#3daee9") : QColor("#2d7ca3"))
                    : (dark ? QColor("#737a80") : QColor("#8c969c"));
            const QColor foreground = !enabled
                ? (dark ? QColor("#8f969c") : QColor("#92999d"))
                : (dark ? QColor("#ffffff") : QColor("#27313a"));

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(background);
            painter.setPen(QPen(border, 1.0));
            painter.drawRoundedRect(
                QRectF(bounds).adjusted(0.5, 0.5, -0.5, -0.5),
                6.0,
                6.0
                );
            painter.restore();

            QPalette arrowPalette = palette();
            for (const QPalette::ColorGroup group : {
                     QPalette::Active,
                     QPalette::Inactive
                 })
            {
                arrowPalette.setColor(
                    group,
                    QPalette::ButtonText,
                    foreground
                    );
                arrowPalette.setColor(
                    group,
                    QPalette::WindowText,
                    foreground
                    );
            }
            arrowPalette.setColor(
                QPalette::Disabled,
                QPalette::ButtonText,
                dark ? QColor("#8f969c") : QColor("#92999d")
                );
            arrowPalette.setColor(
                QPalette::Disabled,
                QPalette::WindowText,
                dark ? QColor("#8f969c") : QColor("#92999d")
                );

            QStyleOption arrow;
            arrow.initFrom(this);
            arrow.rect = bounds;
            arrow.palette = arrowPalette;
            if (!enabled)
            {
                arrow.state &= ~QStyle::State_Enabled;
            }
            if (hovered)
            {
                arrow.state |= QStyle::State_MouseOver;
            }
            style()->drawPrimitive(
                left ? QStyle::PE_IndicatorArrowLeft
                     : QStyle::PE_IndicatorArrowRight,
                &arrow,
                &painter,
                this
                );
        };
        paintArrow(
            QRect(0, 0, arrowButtonWidth(), m_tabHeight),
            true,
            m_hoveredLeftArrow,
            m_horizontalScrollOffset > 0
            );
        paintArrow(
            QRect(width() - arrowButtonWidth(), 0, arrowButtonWidth(), m_tabHeight),
            false,
            m_hoveredRightArrow,
            m_horizontalScrollOffset < maximumScrollOffset()
            );
    }

    painter.save();
    painter.setClipRect(QRect(
        startX,
        0,
        viewportWidth,
        height()
        ));
    const int visibleLeft = m_horizontalScrollOffset;
    const int visibleRight = visibleLeft + viewportWidth;
    for (int row = 0; row < m_model->rowCount(); ++row)
    {
        const auto rowIndex = static_cast<std::size_t>(row);
        const int tabLeft = m_tabPositions[rowIndex];
        const int tabRight = tabLeft + m_tabWidth;
        if (tabRight <= visibleLeft || tabLeft >= visibleRight)
        {
            continue;
        }
        const QRect bounds = tabRectForRow(row);
        const QString text = m_model->data(
            m_model->index(row, 0),
            Qt::DisplayRole
            ).toString();
        NavigationPillStyle::paint(
            &painter,
            bounds,
            text,
            {},
            font(),
            Qt::ElideRight,
            style(),
            this,
            palette(),
            {
                isEnabled(),
                row == m_currentRow,
                row == m_hoveredRow,
                isActiveWindow()
            },
            tabColors
            );
    }
    painter.restore();
}

void SubPrepClassInformationTabSelector::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateTabMetrics();
}

void SubPrepClassInformationTabSelector::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().x() != 0
        ? event->angleDelta().x()
        : event->angleDelta().y();
    if (delta != 0 && m_hasOverflow)
    {
        scrollRows(delta < 0 ? 1 : -1);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void SubPrepClassInformationTabSelector::updateTabMetrics()
{
    const int previousCurrentRow = m_currentRow;
    m_tabPositions.clear();
    m_contentWidth = 0;
    m_tabWidth = 0;
    m_tabHeight = NavigationPillStyle::controlHeight(fontMetrics());
    if (m_model)
    {
        const int rowCount = m_model->rowCount();
        m_tabPositions.reserve(static_cast<std::size_t>(rowCount));
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const QString label = m_model->data(
                m_model->index(row, 0),
                Qt::DisplayRole
                ).toString();
            const int tabWidth = NavigationPillStyle::sizeHint(
                    fontMetrics(),
                    {},
                    label,
                    {}
                    ).width();
            m_tabWidth = std::max(m_tabWidth, tabWidth);
        }
        for (int row = 0; row < rowCount; ++row)
        {
            m_tabPositions.push_back(
                row * (m_tabWidth + NavigationPillStyle::Gap)
                );
        }
        if (rowCount > 0)
        {
            m_contentWidth = rowCount * m_tabWidth
                + (rowCount - 1) * NavigationPillStyle::Gap;
        }
        if (m_currentRow >= m_model->rowCount())
        {
            m_currentRow = -1;
        }
    }
    setFixedHeight(m_tabHeight + NavigationPillStyle::RowBottomSpacing);
    m_hasOverflow = m_model
        && m_model->rowCount() > 1
        && m_contentWidth > std::max(
            0,
            width() - 2 * arrowButtonWidth()
            );
    ensureCurrentVisible();
    if (previousCurrentRow != m_currentRow)
    {
        emit currentChanged(m_currentRow);
    }
    updateGeometry();
    update();
}

void SubPrepClassInformationTabSelector::ensureCurrentVisible()
{
    if (!m_model
        || m_model->rowCount() == 0
        || m_currentRow < 0
        || m_currentRow >= m_model->rowCount()
        || !m_hasOverflow)
    {
        m_horizontalScrollOffset = 0;
        return;
    }

    const auto row = static_cast<std::size_t>(m_currentRow);
    const int tabLeft = m_tabPositions[row];
    const int tabRight = tabLeft + m_tabWidth - 1;
    const int visibleLeft = m_horizontalScrollOffset;
    const int visibleRight = visibleLeft + contentViewportWidth() - 1;
    if (tabLeft < visibleLeft)
    {
        m_horizontalScrollOffset = tabLeft;
    }
    else if (tabRight > visibleRight)
    {
        m_horizontalScrollOffset = tabRight - contentViewportWidth() + 1;
    }
    m_horizontalScrollOffset = std::clamp(
        m_horizontalScrollOffset,
        0,
        maximumScrollOffset()
        );
}

int SubPrepClassInformationTabSelector::contentViewportWidth() const noexcept
{
    return std::max(
        0,
        width() - (m_hasOverflow ? 2 * arrowButtonWidth() : 0)
        );
}

int SubPrepClassInformationTabSelector::maximumScrollOffset() const noexcept
{
    return std::max(0, m_contentWidth - contentViewportWidth());
}

int SubPrepClassInformationTabSelector::arrowButtonWidth() const noexcept
{
    return 28;
}

int SubPrepClassInformationTabSelector::rowAt(
    const QPoint& point
    ) const noexcept
{
    if (!m_model || m_tabWidth <= 0)
    {
        return -1;
    }
    const int startX = m_hasOverflow ? arrowButtonWidth() : 0;
    const int x = point.x() - startX + m_horizontalScrollOffset;
    if (x < 0 || (m_hasOverflow && point.x() >= width() - arrowButtonWidth()))
    {
        return -1;
    }

    const auto position = std::upper_bound(
        m_tabPositions.cbegin(),
        m_tabPositions.cend(),
        x
        );
    if (position == m_tabPositions.cbegin())
    {
        return -1;
    }
    const auto row = static_cast<std::size_t>(
        std::distance(m_tabPositions.cbegin(), position) - 1
        );
    return x < m_tabPositions[row] + m_tabWidth
        ? static_cast<int>(row)
        : -1;
}

void SubPrepClassInformationTabSelector::scrollRows(int delta)
{
    if (!m_model || m_model->rowCount() == 0)
    {
        return;
    }
    if (!m_hasOverflow || m_tabWidth <= 0)
    {
        return;
    }

    const int step = m_tabWidth + NavigationPillStyle::Gap;
    int target = m_horizontalScrollOffset;
    if (delta > 0)
    {
        target = ((target / step) + 1) * step;
    }
    else
    {
        target = target % step == 0
            ? target - step
            : (target / step) * step;
    }

    m_horizontalScrollOffset = std::clamp(
        target,
        0,
        maximumScrollOffset()
        );
    update();
}
