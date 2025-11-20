#ifndef DOCKING_MANAGER_HPP
#define DOCKING_MANAGER_HPP

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QHash>
#include <QtQml/qqmlregistration.h>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <memory>
#include "DockingNode.hpp"

/**
 * ============================================================================
 * DockingManager - 停靠系统核心管理器
 * ============================================================================
 * 
 * 作用：
 *   管理整个停靠系统的生命周期和操作
 *   维护节点树，提供增删改查接口
 *   处理布局的保存和加载
 * 
 * 核心职责：
 *   1. 树管理 - 维护 DockingNode 树结构
 *   2. 面板操作 - 添加、删除、查找面板
 *   3. 布局序列化 - 保存/加载布局到 JSON 文件
 *   4. 信号通知 - 数据变化时通知 QML 更新界面
 * 
 * 设计优势（相比 QAbstractItemModel）：
 *   - 更简单：直接暴露 rootNode，QML 递归渲染
 *   - 更快：无需 ItemModel 的复杂索引系统
 *   - 更易维护：代码量减少 60%
 * 
 * QML 使用：
 *   DockingManager {
 *       id: manager
 *       onRootNodeChanged: { 界面自动更新 }
 *   }
 *   NodeRenderer { node: manager.rootNode }
 */
class DockingManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    
    // ========================================================================
    // QML 属性
    // ========================================================================
    
    // rootNode - 树的根节点
    // 只读：外部不能直接设置，只能通过 addPanel/removePanel 修改
    // QML 会监听 rootNodeChanged 信号自动更新界面
    Q_PROPERTY(DockingNode* rootNode READ rootNode NOTIFY rootNodeChanged)
    
    // panelCount - 当前面板总数
    // 只读：自动计算，不能手动设置
    Q_PROPERTY(int panelCount READ panelCount NOTIFY panelCountChanged)
    
    // minPanelSize - 最小面板尺寸（全局设置）
    // 可读写：影响所有新创建的节点
    Q_PROPERTY(double minPanelSize READ minPanelSize WRITE setMinPanelSize NOTIFY minPanelSizeChanged)
    
    // devMode - 开发模式开关
    // true: 每次启动强制从INI加载，忽略JSON（开发测试用）
    // false: 优先加载JSON，INI作为默认模板（正式使用）
    Q_PROPERTY(bool devMode READ devMode WRITE setDevMode NOTIFY devModeChanged)
    
public:
    // ========================================================================
    // 方向枚举（用于 addPanelAt）
    // ========================================================================
    
    /**
     * 添加面板的方向
     * Left/Right：在目标面板左侧/右侧添加
     * Top/Bottom：在目标面板上方/下方添加
     * Center：替换目标面板（暂未使用）
     * 
     * QML 使用：DockingManager.Left, DockingManager.Right 等
     */
    enum Direction {
        Left = 1,
        Right = 2,
        Top = 3,
        Bottom = 4,
        Center = 5
    };
    Q_ENUM(Direction)  // 注册到 Qt 元对象系统
    
    /**
     * 构造函数
     * 参数：parent - Qt 父对象
     * 初始化：创建空的停靠系统（m_root = nullptr）
     */
    explicit DockingManager(QObject* parent = nullptr);
    
    // ========================================================================
    // 属性访问器
    // ========================================================================
    
    /**
     * 获取根节点
     * 返回：树的根节点指针，如果树为空则返回 nullptr
     * 用途：QML 通过此属性访问整棵树
     */
    DockingNode* rootNode() const { return m_root.get(); }
    
    /**
     * 获取面板总数
     * 返回：当前系统中的面板数量
     * 实现：直接返回 m_panels 哈希表的大小
     */
    int panelCount() const { return m_panels.size(); }
    
    /**
     * 获取最小面板尺寸
     */
    double minPanelSize() const { return m_minPanelSize; }
    
    /**
     * 设置最小面板尺寸
     * 参数：size - 新的最小尺寸
     * 限制：自动约束在 50.0 到 1000.0 之间
     */
    void setMinPanelSize(double size) {
        double validatedSize = DockingNodeHelpers::validateMinSize(size);
        if (DockingNodeHelpers::safeSetValue(m_minPanelSize, validatedSize)) {
            emit minPanelSizeChanged();
        }
    }
    
    /**
     * 获取开发模式开关
     */
    bool devMode() const { return m_devMode; }
    
    /**
     * 设置开发模式开关
     * 参数：enabled - true=开发模式，false=生产模式
     */
    void setDevMode(bool enabled) {
        if (m_devMode != enabled) {
            m_devMode = enabled;
            emit devModeChanged();
        }
    }
    
    // ========================================================================
    // 核心 API（可从 QML 调用）
    // ========================================================================
    
    /**
     * 创建面板（仅创建，不添加到树中）
     * 参数：
     *   title - 面板标题
     *   qmlSource - QML 内容文件路径
     * 返回：创建的 PanelNode 指针
     * 用途：暂未使用，保留接口
     */
    Q_INVOKABLE PanelNode* createPanel(const QString& title, const QString& qmlSource = "");
    
    /**
     * 添加面板（自动选择位置）
     * 参数：
     *   panelId - 面板唯一 ID
     *   title - 面板标题
     *   qmlSource - QML 内容文件路径
     * 返回：成功返回 true
     * 
     * 逻辑：
     *   1. 如果树为空：创建面板作为根节点
     *   2. 如果树不为空：自动添加到最右侧面板的右侧
     * 
     * QML 调用：dockingManager.addPanel("panel_1", "新面板", "qrc:/...")
     */
    Q_INVOKABLE bool addPanel(const QString& panelId, const QString& title, const QString& qmlSource = "");
    
    /**
     * 在指定位置添加面板（核心方法）
     * 参数：
     *   panelId - 新面板的唯一 ID
     *   title - 面板标题
     *   qmlSource - QML 内容文件路径
     *   targetId - 目标面板 ID（在哪个面板旁边添加）
     *   direction - 方向（Left/Right/Top/Bottom）
     * 返回：成功返回 true
     * 
     * 逻辑：
     *   1. 创建新 PanelNode
     *   2. 查找 targetId 对应的节点
     *   3. 创建新 ContainerNode 包裹目标节点和新面板
     *   4. 更新树结构
     *   5. 发送信号通知 QML
     * 
     * QML 调用：dockingManager.addPanelAt("new", "标题", "qrc:/...", "target", DockingManager.Right)
     */
    Q_INVOKABLE bool addPanelAt(const QString& panelId, const QString& title, const QString& qmlSource,
                                 const QString& targetId, int direction);
    
    
    /**
     * 移除面板（核心方法）
     * 参数：panelId - 要删除的面板 ID
     * 返回：成功返回 true
     * 
     * 逻辑：
     *   1. 查找面板节点
     *   2. 找到父 ContainerNode
     *   3. 取出兄弟节点
     *   4. 提升兄弟节点到父容器的位置
     *   5. 更新树结构
     *   6. 发送信号通知 QML
     * 
     * 重要：会重组树结构（节点提升）
     * QML 调用：dockingManager.removePanel("panel_1")
     */
    Q_INVOKABLE bool removePanel(const QString& panelId);
    
    /**
     * 查找面板
     * 参数：panelId - 面板 ID
     * 返回：找到返回 PanelNode 指针，否则返回 nullptr
     * 用途：快速查找（使用哈希表 O(1)）
     */
    Q_INVOKABLE PanelNode* findPanel(const QString& panelId);
    
    /**
     * 更新分割比例
     * 参数：
     *   containerId - 容器 ID
     *   ratio - 新比例
     * 返回：成功返回 true
     * 用途：手动调整容器的分割比例（通常由 QML 自动调用）
     */
    Q_INVOKABLE bool updateSplitRatio(const QString& containerId, double ratio);
    
    /**
     * 清空所有节点
     * 用途：重置布局时调用
     * 效果：m_root = nullptr, m_panels 清空
     */
    Q_INVOKABLE void clear();
    
    // ========================================================================
    // 布局序列化（保存和加载）
    // ========================================================================
    
    /**
     * 保存布局到内存（QVariantMap）
     * 返回：包含整棵树的 Map
     * 用途：内部使用，saveLayoutToFile() 会调用此方法
     * 
     * 格式：{
     *   version: "2.0",
     *   minPanelSize: 150,
     *   root: { 递归的树结构 }
     * }
     */
    Q_INVOKABLE QVariantMap saveLayout() const;
    
    /**
     * 从内存加载布局（QVariantMap）
     * 参数：layout - 布局数据
     * 返回：成功返回 true
     * 用途：内部使用，loadLayoutFromFile() 会调用此方法
     * 
     * 逻辑：
     *   1. 检查版本兼容性
     *   2. 清空当前树
     *   3. 递归重建树结构
     *   4. 发送 rootNodeChanged 信号
     */
    Q_INVOKABLE bool loadLayout(const QVariantMap& layout);
    
    /**
     * 保存布局到文件（JSON 格式）
     * 参数：filePath - 文件路径
     * 返回：成功返回 true
     * 
     * 流程：
     *   1. 调用 saveLayout() 获取 QVariantMap
     *   2. 转换为 QJsonDocument
     *   3. 写入文件（带缩进格式化）
     * 
     * QML 调用：dockingManager.saveLayoutToFile(path)
     */
    Q_INVOKABLE bool saveLayoutToFile(const QString& filePath) const;
    
    /**
     * 从文件加载布局（JSON 格式）
     * 参数：filePath - 文件路径
     * 返回：成功返回 true
     * 
     * 流程：
     *   1. 读取文件
     *   2. 解析 JSON
     *   3. 调用 loadLayout() 重建树
     * 
     * 错误处理：
     *   - 文件不存在：返回 false（不影响程序）
     *   - JSON 格式错误：返回 false，记录错误日志
     * 
     * QML 调用：dockingManager.loadLayoutFromFile(path)
     */
    Q_INVOKABLE bool loadLayoutFromFile(const QString& filePath);
    
    
    /**
     * 获取默认布局文件路径
     * 返回：项目根目录 + "/layout.json"
     * 
     * 查找逻辑：
     *   从可执行文件目录向上查找包含CMakeLists.txt的目录作为项目根目录
     *   如果找不到，使用可执行文件所在目录
     * 
     * 自动创建：如果目录不存在，会自动创建
     */
    Q_INVOKABLE QString getDefaultLayoutPath() const;
    
    // ========================================================================
    // 调试工具
    // ========================================================================
    
    /**
     * 打印树结构（文本格式）
     * 返回：树的文本表示，带缩进
     * 用途：调试时查看树结构
     * 
     * 示例输出：
     *   Container[node_1] (Vertical, ratio=0.5)
     *     Panel[welcome_panel] "欢迎面板"
     *     Container[node_2] (Horizontal, ratio=0.5)
     *       Panel[panel_1] "新面板"
     *       Panel[panel_2] "新面板"
     */
    Q_INVOKABLE QString dumpTree() const;
    
    /**
     * 获取所有面板的扁平列表
     * 返回：包含所有面板信息的数组
     * 用途：显示面板列表、统计信息
     */
    Q_INVOKABLE QVariantList getFlatPanelList() const;
    
signals:
    /**
     * 根节点改变信号
     * 触发时机：树结构发生任何变化时（添加、删除、重组）
     * 重要性：★★★★★ QML 的核心更新机制依赖此信号
     * QML 绑定：node: dockingManager.rootNode 会监听此信号
     */
    void rootNodeChanged();
    
    /**
     * 面板数量改变信号
     * 触发时机：添加或删除面板时
     * 用途：更新工具栏的面板计数显示
     */
    void panelCountChanged();
    
    /**
     * 最小面板尺寸改变信号
     */
    void minPanelSizeChanged();
    
    /**
     * 开发模式开关改变信号
     */
    void devModeChanged();
    
    /**
     * 面板添加信号
     * 参数：panelId - 新添加的面板 ID
     * 用途：日志记录、统计
     */
    void panelAdded(const QString& panelId);
    
    /**
     * 面板移除信号
     * 参数：panelId - 被移除的面板 ID
     * 用途：日志记录、清理资源
     */
    void panelRemoved(const QString& panelId);
    
    /**
     * 布局改变信号
     * 触发时机：布局加载完成时
     * 用途：通知其他组件布局已更新
     */
    void layoutChanged();
    
private:
    // ========================================================================
    // 内部辅助方法（不暴露给 QML）
    // ========================================================================
    
    /**
     * 递归查找节点
     * 参数：
     *   node - 起始节点
     *   id - 要查找的节点 ID
     * 返回：找到返回指针，否则返回 nullptr
     * 
     * 算法：深度优先搜索（DFS）
     *   1. 如果当前节点 ID 匹配，返回
     *   2. 如果是容器，递归查找 firstChild
     *   3. 递归查找 secondChild
     */
    DockingNode* findNode(DockingNode* node, const QString& id);
    
    /**
     * 查找最右侧的面板
     * 参数：node - 起始节点
     * 返回：最右侧的 PanelNode
     * 用途：addPanel() 时自动添加到最右侧
     * 
     * 算法：
     *   1. 如果是 Panel，返回自己
     *   2. 如果是 Container，优先查找 secondChild
     *   3. 如果 secondChild 不存在，查找 firstChild
     */
    DockingNode* findRightmostPanel(DockingNode* node);
    
    /**
     * 在目标节点旁插入面板（核心算法）
     * 参数：
     *   panel - 要插入的面板（智能指针，所有权转移）
     *   target - 目标节点
     *   dir - 插入方向
     * 返回：成功返回 true
     * 
     * 算法步骤：
     *   1. 确定分割方向（Left/Right→Vertical, Top/Bottom→Horizontal）
     *   2. 创建新 ContainerNode
     *   3. 从父容器取出 target
     *   4. 设置新容器的子节点（根据方向确定顺序）
     *   5. 将新容器放回父容器
     * 
     * 特殊情况：
     *   - 如果 target 是 root，直接替换 root
     */
    bool insertPanelAt(std::unique_ptr<DockingNode> panel, DockingNode* target, Direction dir);
    
    /**
     * 从 QVariantMap 加载节点（递归）
     * 参数：data - 节点数据
     * 返回：创建的节点（智能指针）
     * 
     * 递归逻辑：
     *   1. 读取 type 字段
     *   2. 如果是 "panel"：创建 PanelNode，设置属性
     *   3. 如果是 "container"：
     *      - 创建 ContainerNode
     *      - 递归加载 first 和 second 子节点
     *   4. 返回创建的节点
     */
    std::unique_ptr<DockingNode> loadNodeFromVariant(const QVariantMap& data);
    
    /**
     * 生成唯一节点 ID
     * 返回：如 "node_1", "node_2", ...
     * 用途：创建容器时自动生成 ID
     */
    QString generateNodeId();
    
    /**
     * 递归统计面板数量
     * 参数：node - 起始节点
     * 返回：以该节点为根的子树中的面板数量
     */
    int countPanels(DockingNode* node) const;
    
    /**
     * 递归生成树的文本表示（用于调试）
     * 参数：
     *   node - 当前节点
     *   indent - 缩进层级
     * 返回：带缩进的树结构字符串
     */
    QString dumpNode(DockingNode* node, int indent = 0) const;
    
    /**
     * 递归收集所有面板（扁平化）
     * 参数：
     *   node - 起始节点
     *   panels - 输出列表（引用传递）
     * 用途：getFlatPanelList() 调用
     */
    void collectPanels(DockingNode* node, QVariantList& panels) const;
    
    // ========================================================================
    // 通用辅助函数（代码复用）
    // ========================================================================
    
    /**
     * 创建面板节点（工厂方法 - 原子操作）
     * 作用：统一创建逻辑，避免重复代码
     * 参数：
     *   id - 面板ID
     *   title - 标题
     *   qmlSource - QML文件路径
     * 返回：面板节点智能指针
     */
    std::unique_ptr<PanelNode> createPanelNode(
        const QString& id,
        const QString& title,
        const QString& qmlSource);
    
    /**
     * 注册面板到映射表（原子操作）
     * 作用：将面板指针添加到 m_panels 哈希表
     * 参数：
     *   panelId - 面板ID
     *   panel - 面板指针
     */
    void registerPanel(const QString& panelId, PanelNode* panel);
    
    /**
     * 从映射表注销面板（原子操作）
     * 作用：从 m_panels 哈希表移除面板
     * 参数：panelId - 面板ID
     */
    void unregisterPanel(const QString& panelId);
    
    /**
     * 设置节点为根节点（原子操作）
     * 作用：将节点设为树的根节点
     * 参数：node - 节点智能指针
     */
    void setAsRoot(std::unique_ptr<DockingNode> node);
    
    /**
     * 发送面板添加相关信号组（原子操作）
     * 作用：统一发送 panelCountChanged、panelAdded、layoutChanged
     * 参数：panelId - 被添加的面板ID
     */
    void emitPanelAddedSignals(const QString& panelId);
    
    /**
     * 发送面板删除相关信号组
     * 作用：统一发送 rootNodeChanged、panelCountChanged、panelRemoved、layoutChanged
     * 参数：
     *   panelId - 被删除的面板ID（用于 panelRemoved 信号参数）
     * 注意：立即发送信号，不延迟（避免 QML 访问已删除的节点指针）
     */
    void emitPanelRemovedSignals(const QString& panelId);
    
    /**
     * 获取兄弟节点
     * 作用：删除面板时取出兄弟节点用于提升
     * 参数：
     *   parent - 父容器
     *   targetChild - 目标子节点
     * 返回：{兄弟节点, 是否为第一个子节点}
     */
    std::pair<std::unique_ptr<DockingNode>, bool> takeSiblingNode(
        ContainerNode* parent,
        DockingNode* targetChild);
    
    /**
     * 替换容器中的子节点
     * 作用：用新节点替换旧节点
     * 参数：
     *   container - 目标容器
     *   oldChild - 旧节点
     *   newChild - 新节点
     * 返回：是否成功
     */
    bool replaceChildInContainer(
        ContainerNode* container,
        DockingNode* oldChild,
        std::unique_ptr<DockingNode> newChild);
    
    /**
     * 获取父容器
     * 作用：安全获取节点的父容器
     * 参数：node - 目标节点
     * 返回：父容器指针
     */
    ContainerNode* getParentContainer(DockingNode* node);
    
    /**
     * 提升兄弟节点到父容器位置（原子操作）
     * 作用：删除面板时，将兄弟节点提升以替换父容器
     * 参数：
     *   parentContainer - 父容器（将被替换）
     *   sibling - 兄弟节点（提升到父容器位置）
     * 返回：是否成功
     * 说明：
     *   - 如果父容器是根节点，直接替换 m_root
     *   - 否则在祖父容器中替换父容器为兄弟节点
     */
    bool promoteSiblingNode(
        ContainerNode* parentContainer,
        std::unique_ptr<DockingNode> sibling);
    
    /**
     * 完成面板删除后的清理工作（原子操作）
     * 作用：记录日志、清除保护标志、发送信号
     * 参数：panelId - 被删除的面板ID
     */
    void finalizePanelRemoval(const QString& panelId);
    
    /**
     * 写入JSON到文件（静态辅助方法）
     */
    static bool writeJsonToFile(const QString& filePath, const QJsonDocument& jsonDoc);
    
    /**
     * 从文件读取JSON（静态辅助方法）
     */
    static bool readJsonFromFile(const QString& filePath, QJsonDocument& outDoc, QString* errorMsg = nullptr);
    
    /**
     * 确保目录存在（静态辅助方法）
     */
    static bool ensureDirectoryExists(const QString& dirPath);
    
    /**
     * 查找项目根目录（静态辅助方法）
     */
    static QString findProjectRoot(const QString& startPath, const QString& marker = "CMakeLists.txt", int maxLevels = 5);
    
    // ========================================================================
    // 成员变量
    // ========================================================================
    
    std::unique_ptr<DockingNode> m_root;  // 树的根节点（所有权）
    QHash<QString, PanelNode*> m_panels;  // 面板快速查找表（ID → 指针）
    double m_minPanelSize = 150.0;        // 全局最小面板尺寸
    int m_nodeIdCounter = 0;              // 节点 ID 计数器（用于生成唯一 ID）
    bool m_devMode = false;               // 【开发模式开关】false=生产模式（默认），true=开发模式
    
private slots:
    /**
     * 处理延迟删除（已废弃）
     * 注：使用智能指针后，不再需要延迟删除机制
     */
    void processDelayedDeletion();
};

#endif // DOCKING_MANAGER_HPP

