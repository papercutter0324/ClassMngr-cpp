// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // These two objects are injected by the owning QQuickWidget.
    // qmllint disable unqualified
    property var eventProvider: calendarEventProvider
    property var termProvider: academicCalendarProvider
    // qmllint enable unqualified

    property date shownDate: new Date()
    property color toolbarColor: "#536f8a"
    property color toolbarTextColor: "#fbfaf7"
    property color textColor: "#27313a"
    property color mutedTextColor: "#66727a"
    property color inactiveTextColor: "transparent"
    property color calendarBackground: "#f5f3ee"
    property color headerBackground: "#e2e1dc"
    property color cellBackground: calendarBackground
    property color gridLineColor: "#cbc9c2"
    property color accentColor: toolbarColor
    property color accentTextColor: toolbarTextColor
    property var eventTypeColors: ({})
    property var eventTypeTextColors: ({})
    property int baseFontPixelSize: 14

    readonly property int minimumWeekColumnWidth: 54
    readonly property int weekColumnHorizontalPadding: 16
    readonly property int weekColumnWidth: Math.max(
                                             minimumWeekColumnWidth,
                                             Math.ceil(elemHeaderMetrics.advanceWidth)
                                             + weekColumnHorizontalPadding
                                             + sectionDividerWidth)
    readonly property int dayHeaderHeight: 40
    readonly property int sectionDividerWidth: 3

    readonly property int termRevision: termProvider ? termProvider.revision : 0
    readonly property var calendarLocale: Qt.locale()

    readonly property int visibleRowCount: monthRowCount(
                                               shownDate.getFullYear(),
                                               shownDate.getMonth(),
                                               calendarLocale.firstDayOfWeek)

    readonly property var monthCells: monthCellModel(
                                         shownDate.getFullYear(),
                                         shownDate.getMonth(),
                                         calendarLocale.firstDayOfWeek,
                                         visibleRowCount)

    readonly property bool atFirstMonth: shownDate.getFullYear() < 2026
                                         || (shownDate.getFullYear() === 2026
                                             && shownDate.getMonth() === 0)

    readonly property var academicRows: academicRowsForRevision(root.termRevision)

    function academicRowsForRevision(revision) {
        if (!termProvider)
            return []

        const hasProviderRevision = revision >= 0
        if (!hasProviderRevision)
            return []

        const rows =
            termProvider.weekRows(
                root.shownDate.getFullYear(),
                root.shownDate.getMonth(),
                root.calendarLocale.firstDayOfWeek)

        const visibleRows = []

        for (let index = 0;
             index < root.visibleRowCount && index < rows.length;
             ++index) {
            visibleRows.push(rows[index])
        }

        return visibleRows
    }

    function monthTitleForRevision(revision) {
        if (revision >= 0 && root.termProvider) {
            return root.termProvider.monthTitle(
                root.shownDate.getFullYear(),
                root.shownDate.getMonth())
        }

        return root.shownDate.toLocaleString(Qt.locale(), "MMMM yyyy")
    }

    signal dayActivated(int year, int month, int day)
    signal eventActivated(int eventId)
    signal configureRequested(int year, int month)
    signal displayedMonthChanged(int year, int month)

    function moveMonth(delta) {
        const next = new Date(
            shownDate.getFullYear(),
            shownDate.getMonth() + delta,
            1)

        if (next.getFullYear() < 2026)
            shownDate = new Date(2026, 0, 1)
        else
            shownDate = next
    }

    function monthRowCount(year, month, firstDayOfWeek) {
        const first = new Date(year, month, 1)
        const last = new Date(year, month + 1, 0)
        const firstOffset = (first.getDay() - firstDayOfWeek + 7) % 7

        return Math.ceil((firstOffset + last.getDate()) / 7)
    }

    function monthCellModel(year, month, firstDayOfWeek, rowCount) {
        const first = new Date(year, month, 1)
        const firstOffset = (first.getDay() - firstDayOfWeek + 7) % 7
        const cells = []
        const today = new Date()

        for (let index = 0; index < rowCount * 7; ++index) {
            const date = new Date(year, month, 1 - firstOffset + index)

            cells.push({
                year: date.getFullYear(),
                month: date.getMonth(),
                day: date.getDate(),
                today: date.getFullYear() === today.getFullYear()
                       && date.getMonth() === today.getMonth()
                       && date.getDate() === today.getDate()
            })
        }

        return cells
    }

    function integerSegmentSize(totalSize, segmentCount, segmentIndex) {
        const snappedSize = Math.floor(totalSize)
        const baseSize = Math.floor(snappedSize / segmentCount)
        const remainder = snappedSize % segmentCount

        return baseSize + (segmentIndex < remainder ? 1 : 0)
    }

    TextMetrics {
        id: elemHeaderMetrics

        font.pixelSize: root.baseFontPixelSize
        text: qsTr("Elem")
    }

    Component.onCompleted: {
        if (shownDate.getFullYear() < 2026)
            shownDate = new Date(2026, 0, 1)

        root.displayedMonthChanged(
            shownDate.getFullYear(),
            shownDate.getMonth() + 1)
    }

    onShownDateChanged: {
        root.displayedMonthChanged(
            shownDate.getFullYear(),
            shownDate.getMonth() + 1)
    }

    Rectangle {
        anchors.fill: parent
        color: root.headerBackground
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
            id: toolbar

            implicitHeight: 48
            Layout.fillWidth: true
            font.pixelSize: root.baseFontPixelSize

            palette.buttonText: root.toolbarTextColor
            palette.windowText: root.toolbarTextColor

            background: Rectangle {
                color: root.toolbarColor
            }

            RowLayout {
                anchors.fill: parent
                spacing: 8

                ToolButton {
                    text: "\u2039"
                    enabled: !root.atFirstMonth
                    font.pixelSize: root.baseFontPixelSize
                    palette.buttonText: root.toolbarTextColor
                    onClicked: root.moveMonth(-1)

                    background: Rectangle {
                        color: "transparent"
                    }
                }

                Label {
                    text: root.monthTitleForRevision(root.termRevision)

                    color: root.toolbarTextColor
                    font.pixelSize: toolbar.font.pixelSize * 1.12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight

                    Layout.fillWidth: true
                }

                ToolButton {
                    text: "\u203a"
                    font.pixelSize: root.baseFontPixelSize
                    palette.buttonText: root.toolbarTextColor
                    onClicked: root.moveMonth(1)

                    background: Rectangle {
                        color: "transparent"
                    }
                }

                ToolButton {
                    id: settingsButton

                    text: "\u2699"
                    font.pixelSize: toolbar.font.pixelSize * 1.65
                    palette.buttonText: root.toolbarTextColor

                    Layout.preferredWidth: 48
                    Layout.fillHeight: true

                    Accessible.name: qsTr("Configure academic terms")
                    ToolTip.text: qsTr("Configure academic terms")
                    ToolTip.visible: hovered

                    onClicked: root.configureRequested(
                                   root.shownDate.getFullYear(),
                                   root.shownDate.getMonth() + 1)

                    background: Rectangle {
                        color: settingsButton.hovered ? "#33ffffff" : "#1affffff"
                        border.color: "#66ffffff"
                        border.width: 1
                        radius: 7
                        anchors.fill: parent
                        anchors.margins: 4
                    }
                }
            }
        }

        GridLayout {
            columns: 3
            rowSpacing: 0
            columnSpacing: 0
            clip: true

            Layout.fillWidth: true
            Layout.fillHeight: true

            // =========================================================
            // Header row
            // =========================================================

            Rectangle {
                color: root.headerBackground

                Layout.column: 0
                Layout.row: 0
                Layout.minimumWidth: root.weekColumnWidth
                Layout.preferredWidth: root.weekColumnWidth
                Layout.preferredHeight: root.dayHeaderHeight

                Label {
                    anchors.fill: parent
                    text: qsTr("Elem")
                    color: root.mutedTextColor
                    font.pixelSize: root.baseFontPixelSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    ToolTip.text: qsTr("Elementary academic week")
                    ToolTip.visible: elementaryHeaderMouse.containsMouse
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: root.sectionDividerWidth
                    color: root.gridLineColor
                }

                MouseArea {
                    id: elementaryHeaderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            DayOfWeekRow {
                id: dayOfWeekRow

                locale: root.calendarLocale
                font.bold: false
                font.pixelSize: root.baseFontPixelSize

                background: Rectangle {
                    color: root.headerBackground
                }

                delegate: Label {
                    required property var model

                    text: model.shortName
                    color: root.mutedTextColor
                    font: dayOfWeekRow.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                Layout.column: 1
                Layout.row: 0
                Layout.fillWidth: true
                Layout.preferredHeight: root.dayHeaderHeight
            }

            Rectangle {
                color: root.headerBackground

                Layout.column: 2
                Layout.row: 0
                Layout.minimumWidth: root.weekColumnWidth
                Layout.preferredWidth: root.weekColumnWidth
                Layout.preferredHeight: root.dayHeaderHeight

                Label {
                    anchors.fill: parent
                    text: qsTr("MS")
                    color: root.mutedTextColor
                    font.pixelSize: root.baseFontPixelSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    ToolTip.text: qsTr("Middle School academic week")
                    ToolTip.visible: middleHeaderMouse.containsMouse
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: root.sectionDividerWidth
                    color: root.gridLineColor
                }

                MouseArea {
                    id: middleHeaderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            // =========================================================
            // Calendar body row
            // =========================================================

            Item {
                Layout.column: 0
                Layout.row: 1
                Layout.minimumWidth: root.weekColumnWidth
                Layout.preferredWidth: root.weekColumnWidth
                Layout.preferredHeight: 1
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: root.calendarBackground
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: root.sectionDividerWidth
                    color: root.gridLineColor
                }

                Column {
                    anchors.fill: parent

                    Repeater {
                        model: root.academicRows

                        Item {
                            id: elementaryRow

                            required property int index
                            required property var modelData

                            width: parent.width
                            height: root.integerSegmentSize(
                                        parent.height,
                                        root.visibleRowCount,
                                        index)

                            Label {
                                anchors.fill: parent
                                text: elementaryRow.modelData.elementaryWeek > 0
                                      ? elementaryRow.modelData.elementaryWeek
                                      : "\u2014"
                                color: root.mutedTextColor
                                font.pixelSize: root.baseFontPixelSize
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                ToolTip.text: elementaryRow.modelData.elementaryTooltip
                                ToolTip.visible: elementaryMouse.containsMouse
                            }

                            MouseArea {
                                id: elementaryMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }
                }
            }

            Item {
                id: grid

                readonly property int month: root.shownDate.getMonth()
                readonly property int year: root.shownDate.getFullYear()
                readonly property var locale: root.calendarLocale

                Layout.column: 1
                Layout.row: 1
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1

                Rectangle {
                    anchors.fill: parent
                    color: root.calendarBackground
                }

                Grid {
                    anchors.fill: parent
                    columns: 7
                    spacing: 0

                    Repeater {
                        model: root.monthCells

                        MonthGridDelegate {
                            required property int index
                            required property var modelData

                            width: root.integerSegmentSize(
                                       grid.width,
                                       7,
                                       gridColumn)
                            height: root.integerSegmentSize(
                                        grid.height,
                                        root.visibleRowCount,
                                        gridRow)

                            today: modelData.today
                            year: modelData.year
                            month: modelData.month
                            day: modelData.day
                            visibleMonth: grid.month
                            gridRow: Math.floor(index / 7)
                            gridColumn: index % 7
                            rowCount: root.visibleRowCount
                            columnCount: 7

                            eventProvider: root.eventProvider
                            cellBackground: root.cellBackground
                            gridLineColor: root.gridLineColor
                            textColor: root.textColor
                            inactiveTextColor: root.inactiveTextColor
                            accentColor: root.accentColor
                            accentTextColor: root.accentTextColor
                            eventTypeColors: root.eventTypeColors
                            eventTypeTextColors: root.eventTypeTextColors
                            fontPixelSize: root.baseFontPixelSize

                            onDayActivated: function(year, month, day) {
                                root.dayActivated(year, month, day)
                            }

                            onEventActivated: function(eventId) {
                                root.eventActivated(eventId)
                            }
                        }
                    }
                }
            }

            Item {
                Layout.column: 2
                Layout.row: 1
                Layout.minimumWidth: root.weekColumnWidth
                Layout.preferredWidth: root.weekColumnWidth
                Layout.preferredHeight: 1
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: root.calendarBackground
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: root.sectionDividerWidth
                    color: root.gridLineColor
                }

                Column {
                    anchors.fill: parent

                    Repeater {
                        model: root.academicRows

                        Item {
                            id: middleRow

                            required property int index
                            required property var modelData

                            width: parent.width
                            height: root.integerSegmentSize(
                                        parent.height,
                                        root.visibleRowCount,
                                        index)

                            Label {
                                anchors.fill: parent
                                text: middleRow.modelData.middleWeek > 0
                                      ? middleRow.modelData.middleWeek
                                      : "\u2014"
                                color: root.mutedTextColor
                                font.pixelSize: root.baseFontPixelSize
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                ToolTip.text: middleRow.modelData.middleTooltip
                                ToolTip.visible: middleMouse.containsMouse
                            }

                            MouseArea {
                                id: middleMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }
                        }
                    }
                }
            }
        }
    }
}
