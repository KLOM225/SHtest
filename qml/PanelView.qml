import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DockingSystem 1.0

// ============================================================================
// PanelView.qml - 面板视图组件
// ============================================================================
// 
// 功能：
//   显示一个完整的面板，包含标题栏和内容区域
// 
// 组成：
//   1. 标题栏 - 显示面板标题、图标、操作按钮（添加/关闭）
//   2. 内容区 - 动态加载QML内容文件
//   3. 辅助函数 - 处理用户操作、数据获取、生命周期管理
// 
// 信号：
//   addPanel(direction) - 请求在指定方向添加新面板
//   removePanel(panelId) - 请求删除当前面板
// 
// ============================================================================

Rectangle {
    id: root
    
    // ========================================================================
    // 属性定义
    // ========================================================================
    
    required property var panel  // 面板数据对象（包含ID、标题、内容路径）
    readonly property bool showTitleBar: true  // 是否显示标题栏（开发模式=true）
    
    // ========================================================================
    // 信号定义
    // ========================================================================
    
    signal addPanel(int direction)  // 请求添加新面板
    signal removePanel(string panelId)  // 请求删除面板
    
    // ========================================================================
    // 外观样式
    // ========================================================================
    
    color: "#1e1e1e"
    border.color: "#3c3c3c"
    border.width: 1
    
    // ========================================================================
    // 辅助函数：事件处理
    // ========================================================================
    
    // 处理添加面板请求
    function handleAddPanel(direction) {
        root.addPanel(direction)
    }
    
    // 处理删除面板请求
    function handleRemovePanel() {
        if (!root.panel) return
        Logger.debug("PanelView", "Close button clicked", {
            "panelId": root.panel.nodeId,
            "title": root.panel.title
        })
        root.removePanel(root.panel.nodeId)
    }
    
    // ========================================================================
    // 辅助函数：数据获取
    // ========================================================================
    
    // 获取面板图标文本（取标题首字母大写）
    function getPanelIconText() {
        return root.panel ? root.panel.title.charAt(0).toUpperCase() : "P"
    }
    
    // 获取面板标题文本
    function getPanelTitle() {
        return root.panel ? root.panel.title : "Panel"
    }
    
    // 获取内容文件路径
    function getContentSource() {
        return root.panel ? root.panel.qmlSource : ""
    }
    
    // 计算内容区顶部位置（根据标题栏显示状态）
    function getContentAnchorTop() {
        return root.showTitleBar ? titleBar.bottom : parent.top
    }
    
    // ========================================================================
    // 辅助函数：内容加载处理
    // ========================================================================
    
    // 内容加载完成后，将面板对象传递给内容组件
    function handleContentLoaded(item) {
        if (!item || !root.panel) return
        
        try {
            item.panelNode = root.panel
        } catch (e) {
            // 内容组件不需要panelNode属性时忽略
        }
    }
    
    // ========================================================================
    // 辅助函数：生命周期日志
    // ========================================================================
    
    // 记录面板创建
    function logPanelCreated() {
        if (!root.panel) return
        
        Logger.debug("PanelView", "Panel view created", {
            "panelId": root.panel.nodeId,
            "title": root.panel.title,
            "qmlSource": root.panel.qmlSource
        })
    }
    
    // 记录面板销毁
    function logPanelDestroyed() {
        if (!root.panel) return
        
        Logger.debug("PanelView", "Panel view destroyed", {
            "panelId": root.panel.nodeId,
            "title": root.panel.title
        })
    }
    
    // ========================================================================
    // UI组件：标题栏
    // ========================================================================
    
    Rectangle {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 32
        visible: root.showTitleBar
        color: "#2b2b2b"
        border.color: "#3c3c3c"
        border.width: 1
        z: 10
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6
            
            // 面板图标（显示标题首字母）
            Rectangle {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: 3
                color: "#58a6ff"
                
                Text {
                    anchors.centerIn: parent
                    text: getPanelIconText()
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
            
            // 面板标题文本
            Text {
                Layout.fillWidth: true
                text: getPanelTitle()
                font.pixelSize: 13
                color: "#e6e6e6"
                elide: Text.ElideRight
            }
            
            // 方向按钮组（上下左右）
            RowLayout {
                spacing: 2
                
                DirectionButton {
                    text: "↑"
                    tooltipText: "在上方添加面板"
                    onClicked: handleAddPanel(DockingManager.Top)
                }
                
                DirectionButton {
                    text: "↓"
                    tooltipText: "在下方添加面板"
                    onClicked: handleAddPanel(DockingManager.Bottom)
                }
                
                DirectionButton {
                    text: "←"
                    tooltipText: "在左侧添加面板"
                    onClicked: handleAddPanel(DockingManager.Left)
                }
                
                DirectionButton {
                    text: "→"
                    tooltipText: "在右侧添加面板"
                    onClicked: handleAddPanel(DockingManager.Right)
                }
                
                // 分隔线
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    color: "#3c3c3c"
                }
            }
            
            // 关闭按钮
            CloseButton {
                onClicked: handleRemovePanel()
            }
        }
    }
    
    // ========================================================================
    // UI组件：内容加载器
    // ========================================================================
    
    Loader {
        id: contentLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: getContentAnchorTop()
        anchors.bottom: parent.bottom
        anchors.margins: 1
        
        source: getContentSource()  // 动态加载内容文件
        asynchronous: true  // 异步加载，不阻塞主线程
        
        onLoaded: handleContentLoaded(item)
        
        // 加载中提示
        LoadingIndicator {
            visible: contentLoader.status === Loader.Loading
        }
        
        // 加载失败提示
        ErrorIndicator {
            visible: contentLoader.status === Loader.Error
            errorSource: getContentSource()
        }
    }
    
    // ========================================================================
    // 生命周期回调
    // ========================================================================
    
    Component.onCompleted: logPanelCreated()
    Component.onDestruction: logPanelDestroyed()
}

// ============================================================================
// 可复用组件定义（必须在文件末尾）
// ============================================================================

// 方向按钮组件
component DirectionButton: ToolButton {
    property string tooltipText: ""
    
    Layout.preferredWidth: 24
    Layout.preferredHeight: 24
    font.pixelSize: 14
    
    ToolTip.visible: hovered
    ToolTip.text: tooltipText
    
    background: Rectangle {
        color: parent.hovered ? "#3c3c3c" : "transparent"
        radius: 3
    }
    
    contentItem: Text {
        text: parent.text
        font: parent.font
        color: "#e6e6e6"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

// 关闭按钮组件
component CloseButton: ToolButton {
    Layout.preferredWidth: 24
    Layout.preferredHeight: 24
    text: "✕"
    font.pixelSize: 14
    
    ToolTip.visible: hovered
    ToolTip.text: "关闭面板"
    
    background: Rectangle {
        color: parent.hovered ? "#e74856" : "transparent"
        radius: 3
    }
    
    contentItem: Text {
        text: parent.text
        font: parent.font
        color: "#e6e6e6"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

// 加载指示器组件
component LoadingIndicator: Rectangle {
    anchors.fill: parent
    color: "#1e1e1e"
    
    Column {
        anchors.centerIn: parent
        spacing: 10
        
        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: true
        }
        
        Text {
            text: "加载中..."
            color: "#888888"
            font.pixelSize: 14
        }
    }
}

// 错误指示器组件
component ErrorIndicator: Rectangle {
    property string errorSource: ""
    
    anchors.fill: parent
    color: "#1e1e1e"
    
    Column {
        anchors.centerIn: parent
        spacing: 10
        
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "⚠"
            color: "#e74856"
            font.pixelSize: 48
        }
        
        Text {
            text: "加载失败"
            color: "#e74856"
            font.pixelSize: 14
        }
        
        Text {
            text: errorSource
            color: "#666666"
            font.pixelSize: 12
            font.family: "Consolas"
        }
    }
}
