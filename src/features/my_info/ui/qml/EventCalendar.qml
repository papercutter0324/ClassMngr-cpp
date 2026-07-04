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
    readonly property int termRevision: termProvider ? termProvider.revision : 0
    readonly property bool atFirstMonth: shownDate.getFullYear() < 2026
                                         || (shownDate.getFullYear() === 2026
                                             && shownDate.getMonth() === 0)
    readonly property var academicRows: {
        root.termRevision
        return termProvider
            ? termProvider.weekRows(grid.year, grid.month, grid.locale.firstDayOfWeek)
            : []
    }

    signal dayActivated(int year, int month, int day)
    signal eventActivated(int eventId)
    signal configureRequested(int year, int month)
    signal displayedMonthChanged(int year, int month)

    function moveMonth(delta) {
        const next = new Date(shownDate.getFullYear(), shownDate.getMonth() + delta, 1)
        if (next.getFullYear() < 2026)
            shownDate = new Date(2026, 0, 1)
        else
            shownDate = next
    }

    Component.onCompleted: {
        if (shownDate.getFullYear() < 2026)
            shownDate = new Date(2026, 0, 1)
        root.displayedMonthChanged(shownDate.getFullYear(), shownDate.getMonth() + 1)
    }

    onShownDateChanged: {
        root.displayedMonthChanged(shownDate.getFullYear(), shownDate.getMonth() + 1)
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
                    palette.buttonText: root.toolbarTextColor
                    onClicked: root.moveMonth(-1)

                    background: Rectangle {
                        color: "transparent"
                    }
                }

                Label {
                    text: {
                        root.termRevision
                        return root.termProvider
                            ? root.termProvider.monthTitle(
                                  root.shownDate.getFullYear(),
                                  root.shownDate.getMonth())
                            : root.shownDate.toLocaleString(Qt.locale(), "MMMM yyyy")
                    }
                    color: root.toolbarTextColor
                    font.pixelSize: toolbar.font.pixelSize * 1.12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight

                    Layout.fillWidth: true
                }

                ToolButton {
                    text: "\u203a"
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

            Rectangle {
                color: root.headerBackground
                Layout.column: 0
                Layout.row: 0

                implicitWidth: 54
                Layout.fillHeight: true

                Label {
                    anchors.fill: parent
                    text: qsTr("Elem")
                    color: root.mutedTextColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    ToolTip.text: qsTr("Elementary academic week")
                    ToolTip.visible: elementaryHeaderMouse.containsMouse
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
                locale: grid.locale
                font.bold: false

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
            }

            Rectangle {
                color: root.headerBackground
                Layout.column: 2
                Layout.row: 0
                implicitWidth: 54
                Layout.fillHeight: true

                Label {
                    anchors.fill: parent
                    text: qsTr("MS")
                    color: root.mutedTextColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    ToolTip.text: qsTr("Middle School academic week")
                    ToolTip.visible: middleHeaderMouse.containsMouse
                }

                MouseArea {
                    id: middleHeaderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            Item {
                Layout.column: 0
                Layout.row: 1
                implicitWidth: 54
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: root.calendarBackground
                }

                Column {
                    anchors.fill: parent

                    Repeater {
                        model: root.academicRows

                        Item {
                            id: elementaryRow
                            required property var modelData

                            width: parent.width
                            height: parent.height / 6

                            Label {
                                anchors.fill: parent
                                text: elementaryRow.modelData.elementaryWeek > 0
                                      ? elementaryRow.modelData.elementaryWeek
                                      : "\u2014"
                                color: root.mutedTextColor
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

            MonthGrid {
                id: grid
                month: root.shownDate.getMonth()
                year: root.shownDate.getFullYear()
                spacing: 0

                readonly property int gridLineThickness: 1

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.column: 1
                Layout.row: 1

                delegate: MonthGridDelegate {
                    visibleMonth: grid.month
                    eventProvider: root.eventProvider
                    cellBackground: root.cellBackground
                    textColor: root.textColor
                    inactiveTextColor: root.inactiveTextColor
                    accentColor: root.accentColor
                    accentTextColor: root.accentTextColor
                    onDayActivated: function(year, month, day) {
                        root.dayActivated(year, month, day)
                    }
                    onEventActivated: function(eventId) {
                        root.eventActivated(eventId)
                    }
                }

                background: Item {
                    x: grid.leftPadding
                    y: grid.topPadding
                    width: grid.availableWidth
                    height: grid.availableHeight

                    Row {
                        Repeater {
                            model: 7

                            Rectangle {
                                width: grid.availableWidth / 7
                                height: grid.availableHeight
                                color: root.cellBackground
                                border.color: root.gridLineColor
                            }
                        }
                    }

                    Column {
                        Repeater {
                            model: 6

                            Rectangle {
                                width: grid.availableWidth
                                height: grid.availableHeight / 6
                                color: "transparent"
                                border.color: root.gridLineColor
                            }
                        }
                    }
                }
            }

            Item {
                Layout.column: 2
                Layout.row: 1
                implicitWidth: 54
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    color: root.calendarBackground
                }

                Column {
                    anchors.fill: parent

                    Repeater {
                        model: root.academicRows

                        Item {
                            id: middleRow
                            required property var modelData

                            width: parent.width
                            height: parent.height / 6

                            Label {
                                anchors.fill: parent
                                text: middleRow.modelData.middleWeek > 0
                                      ? middleRow.modelData.middleWeek
                                      : "\u2014"
                                color: root.mutedTextColor
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
