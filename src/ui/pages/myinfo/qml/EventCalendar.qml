// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var eventProvider: calendarEventProvider
    property date shownDate: new Date()
    property color toolbarColor: "#3949ab"
    property color toolbarTextColor: "#f5f5f5"
    property color textColor: "#111111"
    property color mutedTextColor: "#111111"
    property color inactiveTextColor: "transparent"
    property color calendarBackground: "#d6d6d6"
    property color headerBackground: "#d6d6d6"
    property color cellBackground: calendarBackground
    property color gridLineColor: "#adadad"
    property color accentColor: toolbarColor
    property color accentTextColor: toolbarTextColor

    signal dayActivated(int year, int month, int day)
    signal eventActivated(int eventId)

    function moveMonth(delta) {
        shownDate = new Date(shownDate.getFullYear(), shownDate.getMonth() + delta, 1)
    }

    Rectangle {
        anchors.fill: parent
        color: root.headerBackground
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBar {
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
                    palette.buttonText: root.toolbarTextColor
                    onClicked: root.moveMonth(-1)

                    background: Rectangle {
                        color: "transparent"
                    }
                }

                Label {
                    text: root.shownDate.toLocaleString(Qt.locale(), "MMMM yyyy")
                    color: root.toolbarTextColor
                    font.pixelSize: Qt.application.font.pixelSize * 1.25
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

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
            }
        }

        GridLayout {
            columns: 2
            rowSpacing: 0
            columnSpacing: 0
            clip: true

            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                color: root.headerBackground
                Layout.column: 0
                Layout.row: 0
                Layout.fillWidth: true
            }

            DayOfWeekRow {
                id: dayOfWeekRow
                locale: grid.locale
                font.bold: false

                background: Rectangle {
                    color: root.headerBackground
                }

                delegate: Label {
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

            WeekNumberColumn {
                id: weekNumberColumn
                month: grid.month
                year: grid.year
                locale: grid.locale
                font.bold: false
                palette.text: root.mutedTextColor

                background: Rectangle {
                    color: root.calendarBackground
                }

                Layout.fillHeight: true
                Layout.column: 0
                Layout.row: 1

                delegate: Label {
                    required property int weekNumber

                    text: weekNumber
                    color: root.mutedTextColor
                    font: weekNumberColumn.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
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
        }
    }
}
