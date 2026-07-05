// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

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
    required property int gridRow
    required property int gridColumn
    required property int rowCount
    required property int columnCount
    required property color cellBackground
    required property color gridLineColor
    required property color textColor
    required property color inactiveTextColor
    required property color accentColor
    required property color accentTextColor
    required property var eventTypeColors
    required property var eventTypeTextColors
    required property int fontPixelSize

    readonly property bool activeMonth: month === visibleMonth
    readonly property bool firstColumn: gridColumn <= 0
    readonly property bool lastColumn: gridColumn >= columnCount - 1
    readonly property bool nextCellActive: {
        if (root.lastColumn)
            return false

        const next = new Date(root.year, root.month, root.day + 1)
        return next.getMonth() === root.visibleMonth
    }
    readonly property bool lowerCellActive: {
        if (root.gridRow >= root.rowCount - 1)
            return false

        const lower = new Date(root.year, root.month, root.day + 7)
        return lower.getMonth() === root.visibleMonth
    }
    readonly property int eventRevision: eventProvider ? eventProvider.revision : 0
    readonly property var dayEvents: dayEventsForRevision(root.eventRevision)
    readonly property int eventFontPixelSize: Math.max(
                                                   1,
                                                   Math.round(root.fontPixelSize * 0.8))
    readonly property int eventVerticalPadding: Math.max(
                                                     4,
                                                     Math.ceil(root.eventFontPixelSize / 3))
    readonly property int eventDelegateHeight: Math.max(
                                                   22,
                                                   root.eventFontPixelSize
                                                   + (root.eventVerticalPadding * 2)
                                                   + 8)

    function dayEventsForRevision(revision) {
        if (revision < 0 || !root.activeMonth || !root.eventProvider)
            return []

        return root.eventProvider.eventsForDate(root.year, root.month + 1, root.day)
    }
    readonly property bool redDay: {
        for (let index = 0; index < root.dayEvents.length; ++index) {
            if (root.dayEvents[index].eventType === "Holiday")
                return true
        }
        return false
    }

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

    function eventColor(eventType) {
        const color = root.eventTypeColors[eventType]
        return color ? color : root.accentColor
    }

    function eventTextColor(eventType) {
        const color = root.eventTypeTextColors[eventType]
        return color ? color : root.accentTextColor
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        z: 1

        Label {
            id: dayText
            text: root.day
            color: root.today ? root.accentTextColor
                              : (root.activeMonth ? root.textColor : root.inactiveTextColor)
            font.pixelSize: root.fontPixelSize
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

            Rectangle {
                anchors.fill: parent
                color: "#cf3f35"
                opacity: root.redDay && !root.today ? 0.16 : 0
                z: -2
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

            model: root.dayEvents

            delegate: ItemDelegate {
                id: itemDelegate

                required property var modelData
                readonly property bool marqueeActive: hovered
                                                       && eventText.implicitWidth > eventClip.width
                readonly property color eventBackgroundColor: root.eventColor(modelData.eventType)
                readonly property color eventForegroundColor: root.eventTextColor(modelData.eventType)

                width: listView.width
                implicitHeight: root.eventDelegateHeight
                height: implicitHeight
                text: modelData.title
                font.pixelSize: root.eventFontPixelSize
                leftPadding: 4
                rightPadding: 4
                topPadding: root.eventVerticalPadding
                bottomPadding: root.eventVerticalPadding

                onClicked: root.eventActivated(modelData.id)

                background: Rectangle {
                    color: itemDelegate.eventBackgroundColor
                    radius: 3
                }

                contentItem: Item {
                    Item {
                        id: eventClip
                        anchors.fill: parent
                        clip: true

                        Text {
                            id: eventText
                            text: itemDelegate.text
                            color: itemDelegate.eventForegroundColor
                            elide: itemDelegate.marqueeActive ? Text.ElideNone : Text.ElideRight
                            font: itemDelegate.font
                            horizontalAlignment: itemDelegate.marqueeActive
                                                 ? Text.AlignLeft
                                                 : Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: itemDelegate.marqueeActive
                                   ? implicitWidth
                                   : eventClip.width
                            height: Math.min(
                                        eventClip.height,
                                        Math.ceil(implicitHeight) + 4)
                            x: itemDelegate.marqueeActive ? -marqueeAnimation.offset : 0

                        }

                        SequentialAnimation {
                            id: marqueeAnimation
                            property real offset: 0
                            running: itemDelegate.marqueeActive
                            loops: Animation.Infinite

                            PauseAnimation { duration: 250 }
                            NumberAnimation {
                                target: marqueeAnimation
                                property: "offset"
                                from: 0
                                to: Math.max(0, eventText.implicitWidth - eventClip.width + 24)
                                duration: Math.max(1200, (eventText.implicitWidth - eventClip.width) * 42)
                            }
                            PauseAnimation { duration: 350 }
                            ScriptAction { script: marqueeAnimation.offset = 0 }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.redDay ? "#f5d7d2" : root.cellBackground
        opacity: root.activeMonth ? 1 : 0
        z: -1
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: root.gridLineColor
        visible: root.activeMonth && !root.firstColumn
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: root.gridLineColor
        visible: root.activeMonth
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: root.gridLineColor
        visible: root.activeMonth && !root.lastColumn && !root.nextCellActive
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: root.gridLineColor
        visible: root.activeMonth && !root.lowerCellActive
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
