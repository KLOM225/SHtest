import QtQuick
import QtQuick.Controls
import SplitPanel 1.0

/**
 * ============================================================================
 * SplitNodeRenderer.qml - 节点渲染器（路由器）
 * ============================================================================
 * 
 * 作用：
 *   根据节点类型动态选择和加载对应的视图组件
 *   相当于一个"路由器"，将数据节点映射到视图组件
 * 
 * 核心功能：
 *   1. 类型判断：检查 node.nodeType
 *   2. 动态加载：Panel → PanelView, Container → ContainerView
 *   3. 信号转发：将子组件的信号向上传递
 * 
 * 为什么需要这个组件？
 *   因为节点类型在运行时才能确定，不能硬编码
 *   使用 Loader + switch 实现动态加载
 * 
 * 数据流：
 *   SplitPanelNod (C++) → NodeRenderer → PanelView/ContainerView (QML)
 */
Loader {
    id: root
    
    // ========================================================================
    // 属性定义
    // ========================================================================
    
    /**
     * node - 要渲染的节点
     * 类型：SplitPanelNod (可以是 PanelNode 或 ContainerNode)
     * 绑定：通常绑定到 SplitManager.rootNode
     * 
     * 变化时：
     *   node 改变 → sourceComponent 重新求值 → 加载新组件
     */
    property SplitPanelNode node: null
    
    /**
     * manager - SplitManager 实例
     * 用途：传递给子组件，用于操作树（添加/删除面板）
     */
    property SplitManager manager: null
    
    // ========================================================================
    // 信号定义（向上传递）
    // ========================================================================
    
    /**
     * addPanel 信号
     * 参数：
     *   targetId - 目标面板 ID
     *   direction - 添加方向
     * 
     * 触发时机：
     *   子组件（PanelView/ContainerView）发出 addPanel 信号
     * 
     * 传递链：
     *   PanelView/ContainerView → NodeRenderer → SplitSystemView → SplitManager
     */
    signal addPanel(string targetId, int direction)
    
    /**
     * removePanel 信号
     * 参数：panelId - 要删除的面板 ID
     * 
     * 触发时机：
     *   用户点击面板的关闭按钮
     * 
     * 传递链：
     *   PanelView → ContainerView → NodeRenderer → SplitSystemView → SplitManager
     */
    signal removePanel(string panelId)
    
    // ========================================================================
    // 组件选择（核心逻辑）
    // ========================================================================
    
    /**
     * sourceComponent - 动态选择要加载的组件
     * 
     * 逻辑：
     *   1. 如果 node 为 null：不加载任何组件
     *   2. 如果 node.nodeType == Panel：加载 PanelView
     *   3. 如果 node.nodeType == Container：加载 ContainerView
     * 
     * 自动更新：
     *   node 变化 → sourceComponent 重新求值 → 销毁旧组件 → 创建新组件
     */
    sourceComponent: {
        if (!node) {
            return null
        }
        
        // 根据节点类型选择组件
        switch (node.nodeType) {
            case SplitPanelNode.Panel:
                return panelComponent      // 面板 → PanelView
            case SplitPanelNode.Container:
                return containerComponent  // 容器 → ContainerView
            default:
                return null
        }
    }
    
    // ========================================================================
    // 性能优化
    // ========================================================================
    
    /**
     * 异步加载
     * 
     * 优点：
     *   不会阻塞主线程
     *   加载大型内容时界面不会卡顿
     */
    asynchronous: true
    
    /**
     * 组件销毁回调
     * 
     * 注意：
     *   Loader 会自动清理加载的 item
     *   不需要手动调用 destroy()
     */
    Component.onDestruction: {
        // 自动清理，无需手动操作
    }
    
    // ========================================================================
    // 组件定义
    // ========================================================================
    
    /**
     * panelComponent - 面板组件模板
     * 
     * 用途：
     *   当 node.nodeType == Panel 时，由 Loader 实例化
     * 
     * 绑定：
     *   panel: root.node - 将节点对象传给 PanelView
     */
    Component {
        id: panelComponent
        
        SplitPanelView {
            panel: root.node  // 绑定面板节点
        }
    }
    
    /**
     * containerComponent - 容器组件模板
     * 
     * 用途：
     *   当 node.nodeType == Container 时，由 Loader 实例化
     * 
     * 绑定：
     *   container: root.node - 将容器节点传给 ContainerView
     *   manager: root.manager - 传递管理器（用于添加/删除操作）
     */
    Component {
        id: containerComponent
        
        SplitContainerView {
            container: root.node  // 绑定容器节点
            manager: root.manager // 传递管理器
        }
    }
    
    // ========================================================================
    // 信号转发（子组件 → 父组件）
    // ========================================================================
    
    /**
     * Connections - 监听加载的组件（root.item）的信号
     * 
     * 作用：
     *   将子组件（PanelView/ContainerView）的信号向上传递
     * 
     * 信号冒泡链：
     *   PanelView/ContainerView → NodeRenderer → SplitSystemView → Main
     */
    Connections {
        target: root.item  // 监听 Loader 加载的组件实例
        enabled: root.item  // 只在组件存在时启用
        
        /**
         * addPanel 信号处理（多态）
         * 
         * 参数处理：
         *   - PanelView 发出：addPanel(direction)          [1个参数]
         *   - ContainerView 发出：addPanel(targetId, direction) [2个参数]
         * 
         * 统一转换为 2 个参数：addPanel(targetId, direction)
         */
        function onAddPanel(arg1, arg2) {
            if (arguments.length === 1) {
                // 来自 PanelView：只有 direction
                // 需要补充 targetId（使用当前节点的 ID）
                if (root.node && root.node.nodeId) {
                    root.addPanel(root.node.nodeId, arg1)
                }
            } else {
                // 来自嵌套的 ContainerView：已有 targetId 和 direction
                root.addPanel(arg1, arg2)
            }
        }
        
        /**
         * removePanel 信号处理
         * 
         * 直接向上转发
         */
        function onRemovePanel(panelId) {
            root.removePanel(panelId)
        }
    }
}
