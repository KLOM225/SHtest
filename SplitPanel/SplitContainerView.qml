import QtQuick
import QtQuick.Controls
import SplitPanel 1.0

// ============================================================================
// SplitContainerView.qml - 容器视图组件
// ============================================================================
// 
// 功能：
//   渲染分割容器，将两个子节点左右或上下分割显示
//   处理分割手柄的拖动，实时更新分割比例
//   递归渲染子容器，形成树状结构
// 
// 核心机制：
//   1. 根据orientation确定分割方向（横向/纵向）
//   2. 根据splitRatio分配两个子节点的空间比例
//   3. 监听用户拖动分割手柄，实时更新splitRatio
//   4. 使用Loader动态加载子节点，避免递归实例化问题
// 
// 注意：
//   orientation映射关系（反直觉但正确）：
//   - ContainerNode.Horizontal → Qt.Vertical （子节点左右排列，分割线竖着）
//   - ContainerNode.Vertical → Qt.Horizontal （子节点上下排列，分割线横着）
// 
// 信号：
//   addPanel(targetId, direction) - 请求添加面板
//   removePanel(panelId) - 请求删除面板
// 
// ============================================================================

SplitView {
    id: root
    
    // ========================================================================
    // 属性定义
    // ========================================================================
    
    property var container: null  // 容器节点对象
    property var manager: null    // DockingManager实例
    
    // ========================================================================
    // 信号定义
    // ========================================================================
    
    signal addPanel(string targetId, int direction)
    signal removePanel(string panelId)
    
    // ========================================================================
    // 分割方向设置
    // ========================================================================
    
    orientation: getOrientation()
    
    // ========================================================================
    // 辅助函数：计算和数据处理
    // ========================================================================
    
    // 计算SplitView方向（需要反转映射）
    function getOrientation() {
        if (!root.container) return Qt.Horizontal
        return root.container.orientation === ContainerNode.Horizontal 
            ? Qt.Vertical 
            : Qt.Horizontal
    }
    
    // 获取第一个子节点的组件类型
    function getFirstChildComponent() {
        if (!root.container || !root.container.firstChild) return null
        
        switch (root.container.firstChild.nodeType) {
            case SplitPanelNode.Panel:
                return panelComponent
            case SplitPanelNode.Container:
                return null  // 使用source加载文件
            default:
                return null
        }
    }
    
    // 获取第一个子节点的文件路径
    function getFirstChildSource() {
        if (!root.container || !root.container.firstChild) return ""
        if (root.container.firstChild.nodeType === SplitPanelNode.Container) {
            return "SplitContainerView.qml"
        }
        return ""
    }
    
    // 获取第二个子节点的组件类型
    function getSecondChildComponent() {
        if (!root.container || !root.container.secondChild) return null
        
        switch (root.container.secondChild.nodeType) {
            case SplitPanelNode.Panel:
                return panelComponent2
            case SplitPanelNode.Container:
                return null
            default:
                return null
        }
    }
    
    // 获取第二个子节点的文件路径
    function getSecondChildSource() {
        if (!root.container || !root.container.secondChild) return ""
        if (root.container.secondChild.nodeType === SplitPanelNode.Container) {
            return "SplitContainerView.qml"
        }
        return ""
    }
    
    // 计算首选宽度
    function calculatePreferredWidth(loader) {
        if (!root.container) return 100
        return getOrientation() === Qt.Horizontal 
            ? root.width * root.container.splitRatio 
            : root.width
    }
    
    // 计算首选高度
    function calculatePreferredHeight(loader) {
        if (!root.container) return 100
        return getOrientation() === Qt.Vertical 
            ? root.height * root.container.splitRatio 
            : root.height
    }
    
    // ========================================================================
    // 辅助函数：分割比例更新
    // ========================================================================
    
    // 更新分割比例（带5%-95%限制，防止极端值）
    function updateSplitRatio(newRatio) {
        if (!root.container) return
        if (newRatio > 0.05 && newRatio < 0.95) {
            root.container.splitRatio = newRatio
        }
    }
    
    // 处理宽度变化（横向分割时）
    function handleWidthChanged(loader) {
        if (!root.container) return
        if (getOrientation() !== Qt.Horizontal) return
        if (root.width <= 0 || loader.width <= 0) return
        
        var newRatio = loader.width / root.width
        updateSplitRatio(newRatio)
    }
    
    // 处理高度变化（纵向分割时）
    function handleHeightChanged(loader) {
        if (!root.container) return
        if (getOrientation() !== Qt.Vertical) return
        if (root.height <= 0 || loader.height <= 0) return
        
        var newRatio = loader.height / root.height
        updateSplitRatio(newRatio)
    }
    
    // ========================================================================
    // 辅助函数：加载完成处理
    // ========================================================================
    
    // 第一个子节点加载完成
    function handleFirstLoaded(item) {
        if (!item || !root.container || !root.container.firstChild) return
        if (root.container.firstChild.nodeType !== SplitPanelNode.Container) return
        
        item.container = Qt.binding(function() { 
            return root.container ? root.container.firstChild : null 
        })
        item.manager = Qt.binding(function() { return root.manager })
    }
    
    // 第二个子节点加载完成
    function handleSecondLoaded(item) {
        if (!item || !root.container || !root.container.secondChild) return
        if (root.container.secondChild.nodeType !== SplitPanelNode.Container) return
        
        item.container = Qt.binding(function() { 
            return root.container ? root.container.secondChild : null 
        })
        item.manager = Qt.binding(function() { return root.manager })
    }
    
    // ========================================================================
    // 辅助函数：信号处理
    // ========================================================================
    
    // 处理第一个子节点的添加面板请求
    function handleAddPanelFromFirst(arg1, arg2) {
        if (arguments.length === 1) {
            // 来自PanelView（只有direction参数）
            if (firstLoader.item && firstLoader.item.panel) {
                root.addPanel(firstLoader.item.panel.nodeId, arg1)
            }
        } else {
            // 来自嵌套的ContainerView（有targetId和direction）
            root.addPanel(arg1, arg2)
        }
    }
    
    // 处理第二个子节点的添加面板请求
    function handleAddPanelFromSecond(arg1, arg2) {
        if (arguments.length === 1) {
            // 来自PanelView
            if (secondLoader.item && secondLoader.item.panel) {
                root.addPanel(secondLoader.item.panel.nodeId, arg1)
            }
        } else {
            // 来自嵌套的ContainerView
            root.addPanel(arg1, arg2)
        }
    }
    
    // 处理删除面板请求
    function handleRemovePanel(panelId, source) {
        Logger.debug("SplitContainerView", "Close requested for " + source + " panel", {
            "panelId": panelId
        })
        root.removePanel(panelId)
    }
    
    // ========================================================================
    // UI组件：第一个子节点加载器
    // ========================================================================
    
    Loader {
        id: firstLoader
        
        sourceComponent: getFirstChildComponent()
        source: getFirstChildSource()
        
        SplitView.preferredWidth: calculatePreferredWidth(firstLoader)
        SplitView.preferredHeight: calculatePreferredHeight(firstLoader)
        SplitView.minimumWidth: root.container ? root.container.minSize : 150
        SplitView.minimumHeight: root.container ? root.container.minSize : 150
        
        onWidthChanged: handleWidthChanged(firstLoader)
        onHeightChanged: handleHeightChanged(firstLoader)
        onLoaded: handleFirstLoaded(item)
    }
    
    // Panel组件模板（用于第一个子节点）
    Component {
        id: panelComponent
        SplitPanelView {
            panel: root.container ? root.container.firstChild : null
        }
    }
    
    // 监听第一个子节点的信号
    Connections {
        target: firstLoader.item
        enabled: firstLoader.item
        
        function onAddPanel(arg1, arg2) {
            handleAddPanelFromFirst(arg1, arg2)
        }
        
        function onRemovePanel(panelId) {
            handleRemovePanel(panelId, "first")
        }
    }
    
    // ========================================================================
    // UI组件：分割手柄
    // ========================================================================
    
    handle: SplitHandle {}
    
    // ========================================================================
    // UI组件：第二个子节点加载器
    // ========================================================================
    
    Loader {
        id: secondLoader
        
        sourceComponent: getSecondChildComponent()
        source: getSecondChildSource()
        
        SplitView.fillWidth: getOrientation() === Qt.Horizontal
        SplitView.fillHeight: getOrientation() === Qt.Vertical
        SplitView.minimumWidth: root.container ? root.container.minSize : 150
        SplitView.minimumHeight: root.container ? root.container.minSize : 150
        
        onLoaded: handleSecondLoaded(item)
    }
    
    // Panel组件模板（用于第二个子节点）
    Component {
        id: panelComponent2
        SplitPanelView {
            panel: root.container ? root.container.secondChild : null
        }
    }
    
    // 监听第二个子节点的信号
    Connections {
        target: secondLoader.item
        enabled: secondLoader.item
        
        function onAddPanel(arg1, arg2) {
            handleAddPanelFromSecond(arg1, arg2)
        }
        
        function onRemovePanel(panelId) {
            handleRemovePanel(panelId, "second")
        }
    }

    // ========================================================================
    // 可复用组件定义
    // ========================================================================

    // 分割手柄组件（可拖动，鼠标悬停时高亮）
    component SplitHandle: Rectangle {
        implicitWidth: root.orientation === Qt.Horizontal ? 4 : parent.width
        implicitHeight: root.orientation === Qt.Vertical ? 4 : parent.height
        color: hoverHandler.hovered ? "#58a6ff" : "#3c3c3c"
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
        
        HoverHandler {
            id: hoverHandler
        }
    }
}

