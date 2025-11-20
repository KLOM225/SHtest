import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ============================================================================
// SplitPanelContent.qml - 演示面板内容
// ============================================================================
// 
// 功能：
//   提供一个简单的演示界面，用于测试停靠系统
// 
// 说明：
//   这是面板内容的示例，展示如何访问panelNode对象
//   实际使用时，可以替换为任何自定义的QML内容
// 
// 数据流：
//   PanelView加载完成后，会自动设置panelNode属性
//   本组件可以通过panelNode访问面板的ID、标题等信息
// 
// ============================================================================

Rectangle {
    id: root
    
    // ========================================================================
    // 属性定义
    // ========================================================================
    
    property var panelNode: null  // 面板节点对象（由PanelView传入）
    
    color: "#FFFFFF"
    
    // ========================================================================
    // 辅助函数：数据获取
    // ========================================================================
    
    // 获取面板ID
    function getPanelId() {
        return root.panelNode ? root.panelNode.nodeId : "N/A"
    }
    
    // 获取面板标题
    function getPanelTitle() {
        return root.panelNode ? root.panelNode.title : "N/A"
    }
    
    // ========================================================================
    // 辅助函数：事件处理
    // ========================================================================
    
    // 处理测试按钮点击
    function handleTestButton() {
        console.log("按钮被点击")
    }
    
    // ========================================================================
    // UI布局
    // ========================================================================
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // 标题
        Text {
            Layout.fillWidth: true
            text: "演示面板"
            font.pixelSize: 18
            font.bold: true
            color: "#333333"
        }
        
        // 面板信息卡片
        PanelInfoCard {
            Layout.fillWidth: true
        }
        
        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#E0E0E0"
        }
        
        // 可滚动列表
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            Column {
                width: parent.width
                spacing: 8
                
                Repeater {
                    model: 10
                    delegate: ListItem {
                        required property int index
                        width: parent.width
                        itemIndex: index
                    }
                }
            }
        }
        
        // 测试按钮
        Button {
            Layout.alignment: Qt.AlignHCenter
            text: "测试按钮"
            onClicked: root.handleTestButton()
        }
    }
    
    // ========================================================================
    // 可复用组件定义
    // ========================================================================
    
    // 面板信息卡片组件
    component PanelInfoCard: Rectangle {
        Layout.preferredHeight: idColumn.height + 16
        color: "#F0F0F0"
        radius: 6
        border.color: "#D0D0D0"
        border.width: 1
        
        ColumnLayout {
            id: idColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            
            InfoRow {
                icon: "📌 ID:"
                value: root.getPanelId()
                valueColor: "#1976D2"
                iconColor: "#2196F3"
            }
            
            InfoRow {
                icon: "📝 标题:"
                value: root.getPanelTitle()
                valueColor: "#388E3C"
                iconColor: "#4CAF50"
            }
        }
    }
    
    // 信息行组件
    component InfoRow: RowLayout {
        property string icon: ""
        property string value: ""
        property string iconColor: "#000000"
        property string valueColor: "#000000"
        
        Layout.fillWidth: true
        spacing: 8
        
        Text {
            text: icon
            font.pixelSize: 11
            font.bold: true
            color: iconColor
        }
        
        Text {
            text: value
            font.pixelSize: 11
            font.family: "Consolas, Courier New, monospace"
            color: valueColor
            Layout.fillWidth: true
            elide: Text.ElideMiddle
        }
    }
    
    // 列表项组件
    component ListItem: Rectangle {
        property int itemIndex: 0
        
        height: 40
        color: itemIndex % 2 === 0 ? "#F5F5F5" : "#FAFAFA"
        radius: 4
        
        Text {
            anchors.centerIn: parent
            text: "项目 " + (itemIndex + 1)
            font.pixelSize: 14
            color: "#666666"
        }
    }
}
