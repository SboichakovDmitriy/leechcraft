import QtQuick 1.1
import "."

Rectangle {
    id: rootRect

    width: parent.width
    property real launcherItemHeight: parent.width
    property real currentGapSize: launcherItemHeight / 4
    height: launcherColumn.height

    color: "transparent"

    Column {
        id: launcherColumn
        Repeater {
            model: SB2_launcherModel
            Item {
                id: tcItem

                height: rootRect.launcherItemHeight + pregap.height + postgap.height
                width: rootRect.width

                Item {
                    id: pregap
                    height: isCurrentTab ? rootRect.currentGapSize : 0
                    Behavior on height { PropertyAnimation { duration: 200 } }
                    anchors.top: parent.top
                }

                ActionButton {
                    id: tcButton
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: pregap.bottom
                    anchors.bottom: postgap.top

                    actionIconURL: tabClassIcon
                    isHighlight: openedTabsCount
                    isStrongHighlight: openedTabsCount
                    isCurrent: isCurrentTab

                    onTriggered: SB2_launcherProxy.tabOpenRequested(tabClassID)
                }

                Item {
                    id: postgap
                    height: isCurrentTab ? rootRect.currentGapSize : 0
                    Behavior on height { PropertyAnimation { duration: 200 } }
                    anchors.bottom: parent.bottom
                }
            }
        }
    }
}