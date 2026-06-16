// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var eventProvider
    required property bool today
    required property int year
    required property int month
    required property int day
    required property int visibleMonth
    required property color cellBackground
    required property color textColor
    required property color inactiveTextColor
    required property color accentColor
    required property color accentTextColor

    readonly property bool activeMonth: month === visibleMonth
    readonly property int eventRevision: eventProvider ? eventProvider.revision : 0

    signal dayActivated(int year, int month, int day)
    signal eventActivated(int eventId)

    implicitWidth: contentLayout.implicitWidth
    implicitHeight: contentLayout.implicitHeight
    opacity: 1

    function hasEventAtCellPosition(cellX, cellY) {
        const mapped = listView.mapFromItem(root, cellX, cellY)
        if (mapped.x < 0 || mapped.y < 0
                || mapped.x >= listView.width || mapped.y >= listView.height) {
            return false
        }

        return listView.indexAt(mapped.x, mapped.y) >= 0
            || listView.indexAt(mapped.x, mapped.y + listView.contentY) >= 0
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent

        Label {
            id: dayText
            text: root.day
            color: root.today ? root.accentTextColor
                              : (root.activeMonth ? root.textColor : root.inactiveTextColor)
            topPadding: 4
            horizontalAlignment: Text.AlignHCenter
            opacity: root.activeMonth ? 1 : 0

            Layout.fillWidth: true

            Rectangle {
                width: height
                height: Math.max(dayText.implicitWidth, dayText.implicitHeight) + 8
                radius: width / 2
                color: root.accentColor
                anchors.centerIn: dayText
                z: -1
                visible: root.today
            }
        }

        ListView {
            id: listView
            spacing: 2
            clip: true
            enabled: root.activeMonth

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 4

            model: {
                root.eventRevision
                return root.activeMonth && root.eventProvider
                    ? root.eventProvider.eventsForDate(root.year, root.month + 1, root.day)
                    : []
            }

            delegate: ItemDelegate {
                id: itemDelegate

                required property var modelData

                width: listView.width
                text: modelData.title
                font.pixelSize: Qt.application.font.pixelSize * 0.8
                leftPadding: 4
                rightPadding: 4
                topPadding: 4
                bottomPadding: 4

                onClicked: root.eventActivated(modelData.id)

                background: Rectangle {
                    color: root.accentColor
                    radius: 3
                }

                contentItem: Text {
                    text: itemDelegate.text
                    color: root.accentTextColor
                    elide: Text.ElideRight
                    font: itemDelegate.font
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.activeMonth
        z: 1

        onPressed: function(mouse) {
            if (root.hasEventAtCellPosition(mouse.x, mouse.y))
                mouse.accepted = false
        }

        onClicked: root.dayActivated(root.year, root.month + 1, root.day)
    }
}
