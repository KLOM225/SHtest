import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import SplitPanel 1.0

// ============================================================================
// Main.qml - 应用程序主窗口
// ============================================================================
// 
// 功能：
//   应用程序的顶层容器，管理应用生命周期和布局持久化
// 
// 核心机制：
//   1. 启动时：自动从layout.json加载上次保存的布局
//   2. 运行中：提供工具栏按钮操作布局（添加/保存/加载/重置）
//   3. 退出时：自动保存当前布局到layout.json
// 
// 组成：
//   - SplitManager：数据层，管理所有面板和布局
//   - 工具栏：提供用户操作按钮
//   - SplitSystemView：视图层，渲染停靠系统
// 
// ============================================================================

Window {
    id: root
    width: 1280
    height: 800
    visible: true
    title: "停靠系统演示 - Simplified Architecture"
    
    // 状态文本引用
    property var statusTextRef: null
    property var statusTimerRef: null
    
    // ========================================================================
    // 辅助函数：布局管理
    // ========================================================================
    
    // 初始化默认布局（创建一个欢迎面板）
    function initializeLayout() {
        splitManager.addPanel(
            "welcome_panel",
            "欢迎面板",
            "SplitPanelContent.qml"
        )
        
        Logger.info("Main", "Initial layout created", {
            "panelCount": splitManager.panelCount
        })
    }
    
    // ========================================================================
    // 辅助函数：用户操作处理
    // ========================================================================
    
    // 处理添加面板请求
    function handleAddPanel() {
        var panelId = "panel_" + Date.now()  // 生成唯一ID
        
        var success = splitManager.addPanel(
            panelId,
            "新面板",
            "SplitPanelContent.qml"
        )
        
        if (success) {
            Logger.info("Main", "Panel added successfully", {
                "panelId": panelId
            })
        } else {
            Logger.error("Main", "Failed to add panel", {
                "panelId": panelId
            })
        }
    }
    
    // 处理保存布局请求
    function handleSaveLayout() {
        var defaultPath = splitManager.getDefaultLayoutPath()
        if (splitManager.saveLayoutToFile(defaultPath)) {
            Logger.info("Main", "Layout saved successfully", {
                "path": defaultPath,
                "panelCount": splitManager.panelCount
            })
            showStatus("布局已保存: " + splitManager.panelCount + " 个面板", true)
        } else {
            Logger.error("Main", "Failed to save layout", {})
            showStatus("保存失败", false)
        }
    }
    
    // 处理加载布局请求
    function handleLoadLayout() {
        var defaultPath = splitManager.getDefaultLayoutPath()
        if (splitManager.loadLayoutFromFile(defaultPath)) {
            Logger.info("Main", "Layout loaded successfully", {
                "path": defaultPath,
                "panelCount": splitManager.panelCount
            })
            showStatus("布局已加载: " + splitManager.panelCount + " 个面板", true)
        } else {
            Logger.error("Main", "Failed to load layout", {})
            showStatus("加载失败或文件不存在", false)
        }
    }
    
    // 处理重置布局请求
    function handleResetLayout() {
        splitManager.clear()
        initializeLayout()
        Logger.info("Main", "Layout reset", {})
    }
    
    // ========================================================================
    // 辅助函数：UI辅助
    // ========================================================================
    
    // 显示状态提示（3秒后自动隐藏）
    function showStatus(message, isSuccess) {
        if (statusTextRef) {
            statusTextRef.text = message
            statusTextRef.color = isSuccess ? "#4ec9b0" : "#e74856"
            statusTextRef.visible = true
            statusTimerRef.restart()
        }
    }
    
    // ========================================================================
    // 辅助函数：生命周期
    // ========================================================================
    
    // 记录窗口创建完成
    function logWindowCompleted() {
        Logger.info("Main", "Application window loaded", {
            "width": width,
            "height": height
        })
    }
    
    // 处理窗口关闭（自动保存布局）
    function handleClosing() {
        var defaultPath = splitManager.getDefaultLayoutPath()
        if (splitManager.saveLayoutToFile(defaultPath)) {
            Logger.info("Main", "Layout auto-saved on exit", {
                "path": defaultPath
            })
        }
    }
    
    // ========================================================================
    // 数据层：停靠管理器
    // ========================================================================
    
    SplitManager {
        id: splitManager
        minPanelSize: 150  // 全局最小面板尺寸
        
        Component.onCompleted: {
            Logger.info("Main", "SplitManager created", {
                "minPanelSize": minPanelSize
            })
            
            var jsonPath = getDefaultLayoutPath()
            Logger.info("Main", "Layout path: " + jsonPath, {})
            
            // 尝试加载保存的布局，失败则创建默认布局
            if (loadLayoutFromFile(jsonPath)) {
                Logger.info("Main", "Layout loaded from JSON file", {
                    "panelCount": panelCount
                })
            } else {
                Logger.info("Main", "No saved layout found, creating default layout", {})
                root.initializeLayout()
            }
        }
    }
    
    // ========================================================================
    // 信号监听：SplitManager事件
    // ========================================================================
    
    Connections {
        target: splitManager
        
        function onPanelAdded(panelId) {
            Logger.info("Main", "Panel added: " + panelId, {
                "totalPanels": splitManager.panelCount
            })
        }
        
        function onPanelRemoved(panelId) {
            Logger.info("Main", "Panel removed: " + panelId, {
                "totalPanels": splitManager.panelCount
            })
        }
    }
    
    // ========================================================================
    // UI层：主界面布局
    // ========================================================================
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // 工具栏
        Toolbar {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
        }
        
        // 停靠系统视图
        SplitSystemView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            splitManager: splitManager
        }
    }
    
    // ========================================================================
    // 生命周期回调
    // ========================================================================
    
    Component.onCompleted: logWindowCompleted()
    onClosing: handleClosing()
    
    // 工具栏组件
    component Toolbar: Rectangle {
        color: "#2b2b2b"
        
        Component.onCompleted: {
            root.statusTextRef = statusText
            root.statusTimerRef = statusTimer
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            spacing: 10
            
            // 标题
            Text {
                text: "停靠系统演示"
                color: "white"
                font.pixelSize: 16
                font.bold: true
            }
            
            // 分隔线
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: "#444444"
            }
            
            // 操作按钮
            Button {
                text: "添加面板"
                onClicked: root.handleAddPanel()
            }
            
            Button {
                text: "保存布局"
                onClicked: root.handleSaveLayout()
            }
            
            Button {
                text: "加载布局"
                onClicked: root.handleLoadLayout()
            }
            
            Button {
                text: "重置布局"
                onClicked: root.handleResetLayout()
            }
            
            // 弹性空间
            Item {
                Layout.fillWidth: true
            }
            
            // 状态提示文本（3秒后自动隐藏）
            Text {
                id: statusText
                visible: false
                color: "#4ec9b0"
                font.pixelSize: 13
                
                Timer {
                    id: statusTimer
                    interval: 3000
                    onTriggered: statusText.visible = false
                }
            }
            
            // 面板计数显示
            Text {
                text: "面板数量: " + splitManager.panelCount
                color: "#aaaaaa"
                font.pixelSize: 14
            }
        }
    }
}
