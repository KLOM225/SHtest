import QtQuick
import QtQuick.Controls
import SplitPanel 1.0

// ============================================================================
// SplitSystemView.qml - 停靠系统视图
// ============================================================================
// 
// 功能：
//   停靠系统的顶层视图容器，连接数据层和渲染层
// 
// 核心机制：
//   1. 监听splitManager.rootNode的变化
//   2. 通过NodeRenderer递归渲染整棵节点树
//   3. 接收子组件的信号（添加/删除面板）并转发给DockingManager
//   4. 显示空状态提示（无面板时）
// 
// 数据流：
//   SplitManager → rootNode → NodeRenderer → 递归渲染各个Panel和Container
// 
// 信号流：
//   用户操作 → PanelView/ContainerView → NodeRenderer → 本组件 → SplitManager
// 
// ============================================================================

Item {
    id: root
    
    // ========================================================================
    // 属性定义
    // ========================================================================
    
    required property SplitManager splitManager  // 停靠管理器实例
    
    // ========================================================================
    // 信号定义
    // ========================================================================
    
    signal addPanel(string targetPanelId, int direction)
    signal removePanel(string panelId)
    
    // ========================================================================
    // 辅助函数：数据生成
    // ========================================================================
    
    // 生成唯一面板ID（使用时间戳）
    function generatePanelId() {
        return "panel_" + Date.now()
    }
    
    // ========================================================================
    // 辅助函数：事件处理
    // ========================================================================
    
    // 处理添加面板请求
    function handleAddPanel(targetId, direction) {
        var panelId = generatePanelId()
        splitManager.addPanelAt(
            panelId,
            "新面板",
            "SplitPanelContent.qml",
            targetId,
            direction
        )
    }
    
    // 处理删除面板请求
    function handleRemovePanel(panelId) {
        Logger.debug("SplitSystemView", "Remove panel signal received", {
            "panelId": panelId
        })
        splitManager.removePanel(panelId)
    }
    
    // ========================================================================
    // 辅助函数：状态判断
    // ========================================================================
    
    // 判断是否为空状态（无面板）
    function isEmptyState() {
        return !splitManager.rootNode
    }
    
    // ========================================================================
    // 辅助函数：生命周期
    // ========================================================================
    
    // 记录视图初始化
    function logViewInitialized() {
        Logger.info("SplitSystemView", "View initialized", {})
    }
    
    // ========================================================================
    // UI组件：根节点渲染器
    // ========================================================================
    
    SplitNodeRenderer {
        id: rootRenderer
        anchors.fill: parent
        node: splitManager.rootNode  // 绑定根节点，自动监听变化
        manager: splitManager
    }
    
    // 监听渲染器的信号并转发
    Connections {
        target: rootRenderer
        
        function onAddPanel(targetId, direction) {
            root.handleAddPanel(targetId, direction)
        }
        
        function onRemovePanel(panelId) {
            root.handleRemovePanel(panelId)
        }
    }
    
    // ========================================================================
    // UI组件：空状态提示
    // ========================================================================
    
    EmptyStatePrompt {
        visible: root.isEmptyState()
    }
    
    // 空状态提示组件
    component EmptyStatePrompt: Rectangle {
        anchors.centerIn: parent
        width: 300
        height: 200
        color: "#2b2b2b"
        radius: 10
        
        Column {
            anchors.centerIn: parent
            spacing: 20
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "📦"
                font.pixelSize: 48
            }
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "没有面板"
                color: "white"
                font.pixelSize: 18
            }
            
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "点击上方按钮添加面板"
                color: "#888888"
                font.pixelSize: 14
            }
        }
    }
    
    // ========================================================================
    // 生命周期回调
    // ========================================================================
    
    Component.onCompleted: logViewInitialized()
}
