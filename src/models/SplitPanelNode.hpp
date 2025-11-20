#ifndef SPLIT_PANEL_NODE_HPP
#define SPLIT_PANEL_NODE_HPP

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <QtMath>
#include <memory>

// ============================================================================
// 内联辅助函数（原CommonHelpers中的函数）
// ============================================================================

namespace SplitPanelNodeHelpers {

/**
 * @brief 安全的浮点数比较
 */
inline bool fuzzyEqual(double a, double b) {
    return qFuzzyCompare(a, b);
}

/**
 * @brief 限制数值在指定范围内
 */
template<typename T>
inline T clampValue(T value, T min, T max) {
    return qBound(min, value, max);
}

/**
 * @brief 安全的属性设置（浮点数）
 */
inline bool safeSetValue(double& current, double newValue) {
    if (qFuzzyCompare(current, newValue)) {
        return false;
    }
    current = newValue;
    return true;
}

/**
 * @brief 安全的属性设置（通用类型）
 */
template<typename T>
inline bool safeSetValue(T& current, const T& newValue) {
    if (current == newValue) {
        return false;
    }
    current = newValue;
    return true;
}

/**
 * @brief 验证分割比例
 */
inline double validateSplitRatio(double ratio) {
    return clampValue(ratio, 0.1, 0.9);
}

/**
 * @brief 验证最小尺寸
 */
inline double validateMinSize(double size) {
    return clampValue(size, 50.0, 1000.0);
}

} // namespace DockingNodeHelpers

/**
 * 统一的停靠节点类 - 消除双节点系统冗余
 * 
 * 改进说明：
 * 1. 合并了原来的 TreeNode 和 SplitNode/PanelNode 两套系统
 * 2. 直接暴露给 QML，无需通过 QAbstractItemModel
 * 3. 使用智能指针管理内存，自动清理
 * 4. 简化了代码，提高了性能
 */
class SplitPanelNode : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SplitPanelNod is abstract - use PanelNode or ContainerNode")
    
    Q_PROPERTY(NodeType nodeType READ nodeType CONSTANT)
    Q_PROPERTY(QString nodeId READ nodeId CONSTANT)
    Q_PROPERTY(double minSize READ minSize WRITE setMinSize NOTIFY minSizeChanged)
    
public:
    enum NodeType { Panel, Container };
    Q_ENUM(NodeType)
    
    NodeType nodeType() const { return m_type; }
    QString nodeId() const { return m_id; }
    
    double minSize() const { return m_minSize; }
    void setMinSize(double size) {
        double validatedSize = SplitPanelNodeHelpers::validateMinSize(size);
        if (SplitPanelNodeHelpers::safeSetValue(m_minSize, validatedSize)) {
            emit minSizeChanged();
        }
    }
    
    virtual QVariantMap toVariant() const = 0;
    
    // 优化: 虚析构函数确保正确清理
    virtual ~SplitPanelNode() = default;
    
signals:
    void minSizeChanged();
    
protected:
    explicit SplitPanelNode(NodeType type, const QString& id, QObject* parent = nullptr)
        : QObject(parent), m_type(type), m_id(id) {}
    
private:
    NodeType m_type;
    QString m_id;
    double m_minSize = 150.0;
};

// ============================================================================
// PanelNode - 面板节点（叶子节点）
// ============================================================================
/**
 * 作用：
 *   表示一个实际的面板（如"欢迎面板"、"编辑器面板"）
 *   树的叶子节点，不包含子节点
 * 
 * 属性：
 *   - title: 面板标题（显示在标题栏）
 *   - qmlSource: QML 内容文件路径（面板内部显示的内容）
 * 
 * 用途：
 *   - 在 PanelView.qml 中渲染为带标题栏的面板
 *   - 通过 Loader 加载 qmlSource 指定的内容
 * 
 * 示例：
 *   PanelNode("welcome_panel", "欢迎面板")
 *     ├─ title: "欢迎面板"
 *     └─ qmlSource: "qrc:/qml/DemoPanelContent.qml"
 */
class PanelNode : public SplitPanelNode {
    Q_OBJECT
    QML_ELEMENT
    
    // ========================================================================
    // QML 属性
    // ========================================================================
    
    // title - 面板标题
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    
    // qmlSource - QML 内容文件路径
    Q_PROPERTY(QString qmlSource READ qmlSource WRITE setQmlSource NOTIFY qmlSourceChanged)
    
public:
    /**
     * 构造函数
     * 参数：
     *   id - 唯一标识符（如 "welcome_panel"）
     *   title - 面板标题（如 "欢迎面板"）
     *   parent - Qt 父对象
     */
    explicit PanelNode(const QString& id, const QString& title = "Panel", QObject* parent = nullptr)
        : SplitPanelNode(NodeType::Panel, id, parent), m_title(title) {}
    
    // ========================================================================
    // 属性访问器
    // ========================================================================
    
    QString title() const { return m_title; }
    void setTitle(const QString& title) {
        if (SplitPanelNodeHelpers::safeSetValue(m_title, title)) {
            emit titleChanged();
        }
    }
    
    QString qmlSource() const { return m_qmlSource; }
    void setQmlSource(const QString& source) {
        if (SplitPanelNodeHelpers::safeSetValue(m_qmlSource, source)) {
            emit qmlSourceChanged();
        }
    }
    
    // ========================================================================
    // 序列化
    // ========================================================================
    
    /**
     * 序列化为 QVariantMap（保存布局时调用）
     * 返回：包含所有面板信息的 Map
     * 格式：{
     *   type: "panel",
     *   id: "welcome_panel",
     *   title: "欢迎面板",
     *   qmlSource: "qrc:/qml/...",
     *   minSize: 150
     * }
     */
    QVariantMap toVariant() const override {
        return {
            {"type", "panel"},
            {"id", nodeId()},
            {"title", m_title},
            {"qmlSource", m_qmlSource},
            {"minSize", minSize()}
        };
    }
    
signals:
    void titleChanged();      // 标题改变信号
    void qmlSourceChanged();  // QML 源文件改变信号
    
private:
    QString m_title;          // 面板标题
    QString m_qmlSource;      // QML 内容文件路径
};

// ============================================================================
// 容器节点 - 替代原来的 SplitContainerNode
// ============================================================================
class ContainerNode : public SplitPanelNode {
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(qreal splitRatio READ splitRatio WRITE setSplitRatio NOTIFY splitRatioChanged)
    Q_PROPERTY(SplitPanelNode* firstChild READ firstChild NOTIFY childrenChanged)
    Q_PROPERTY(SplitPanelNode* secondChild READ secondChild NOTIFY childrenChanged)
    
public:
    enum Orientation { Horizontal, Vertical };
    Q_ENUM(Orientation)
    
    explicit ContainerNode(const QString& id, Orientation orient = Horizontal, QObject* parent = nullptr)
        : SplitPanelNode(NodeType::Container, id, parent), m_orientation(orient) {}
    
    // 优化: 析构时清理子节点
    ~ContainerNode() override {
        // 智能指针会自动清理，但显式重置确保顺序
        m_firstChild.reset();
        m_secondChild.reset();
    }
    
    Orientation orientation() const { return m_orientation; }
    void setOrientation(Orientation orient) {
        if (SplitPanelNodeHelpers::safeSetValue(m_orientation, orient)) {
            emit orientationChanged();
        }
    }
    
    qreal splitRatio() const { return m_splitRatio; }
    void setSplitRatio(qreal ratio) {
        double validatedRatio = SplitPanelNodeHelpers::validateSplitRatio(ratio);
        if (SplitPanelNodeHelpers::safeSetValue(m_splitRatio, validatedRatio)) {
            emit splitRatioChanged();
        }
    }
    
    // ========================================================================
    // 子节点访问（QML 接口 - 返回原始指针）
    // ========================================================================
    
    /**
     * 获取第一个子节点
     * 返回：原始指针（QML 可以直接使用）
     * 用途：在 QML 中访问子节点（如 container.firstChild）
     */
    SplitPanelNode* firstChild() const { return m_firstChild.get(); }
    
    /**
     * 获取第二个子节点
     * 返回：原始指针（QML 可以直接使用）
     */
    SplitPanelNode* secondChild() const { return m_secondChild.get(); }
    
    // ========================================================================
    // 子节点管理（C++ 接口 - 使用智能指针）
    // ========================================================================
    
    /**
     * 设置第一个子节点
     * 参数：child - 智能指针（所有权转移）
     * 用途：添加面板、重组树时调用
     * 
     * 重要：使用 std::move() 转移所有权
     * 示例：container->setFirstChild(std::move(panelNode))
     */
    void setFirstChild(std::unique_ptr<SplitPanelNode> child) {
        m_firstChild = std::move(child);  // 所有权转移
        if (m_firstChild) {
            m_firstChild->setParent(this);  // 设置 Qt 父对象（内存管理）
        }
        // 立即发送信号，与 SplitManager::emitPanelRemovedSignals 保持同步
        // 避免延迟导致 QML 绑定访问到不一致状态
        emit childrenChanged();
    }
    
    /**
     * 设置第二个子节点
     * 参数：child - 智能指针（所有权转移）
     */
    void setSecondChild(std::unique_ptr<SplitPanelNode> child) {
        m_secondChild = std::move(child);
        if (m_secondChild) {
            m_secondChild->setParent(this);
        }
        // 立即发送信号，与 SplitManager::emitPanelRemovedSignals 保持同步
        emit childrenChanged();
    }
    
    /**
     * 取出第一个子节点（移除并返回所有权）
     * 返回：智能指针（所有权转移给调用者）
     * 用途：移除面板、重组树时调用
     * 
     * 重要：调用后 m_firstChild 变为 nullptr
     */
    std::unique_ptr<SplitPanelNode> takeFirstChild() {
        auto child = std::move(m_firstChild);  // 所有权转出
        // 立即发送信号，确保在 removePanel 流程中子节点状态实时更新
        // 避免 QML 绑定访问到已被 take 的悬空指针
        emit childrenChanged();
        return child;
    }
    
    /**
     * 取出第二个子节点（移除并返回所有权）
     */
    std::unique_ptr<SplitPanelNode> takeSecondChild() {
        auto child = std::move(m_secondChild);
        // 立即发送信号，确保在 removePanel 流程中子节点状态实时更新
        emit childrenChanged();
        return child;
    }
    
    /**
     * 获取子节点数量
     * 返回：0, 1, 或 2
     */
    int childCount() const {
        int count = 0;
        if (m_firstChild) count++;
        if (m_secondChild) count++;
        return count;
    }
    
    /**
     * 按索引获取子节点
     * 参数：index - 0 或 1
     * 返回：对应的子节点指针，无效索引返回 nullptr
     */
    SplitPanelNode* child(int index) const {
        if (index == 0) return m_firstChild.get();
        if (index == 1) return m_secondChild.get();
        return nullptr;
    }
    
    // ========================================================================
    // 序列化（递归）
    // ========================================================================
    
    /**
     * 序列化为 QVariantMap（保存布局时调用）
     * 返回：包含容器和所有子节点信息的嵌套 Map
     * 
     * 格式：{
     *   type: "container",
     *   id: "node_1",
     *   orientation: "horizontal",
     *   splitRatio: 0.3,
     *   minSize: 150,
     *   first: {子节点的 toVariant()},    ← 递归！
     *   second: {子节点的 toVariant()}   ← 递归！
     * }
     * 
     * 递归示例：
     *   Container
     *   ├─ Panel A
     *   └─ Container
     *      ├─ Panel B
     *      └─ Panel C
     * 
     * 生成：{
     *   type: "container",
     *   first: {type: "panel", id: "A", ...},
     *   second: {
     *     type: "container",
     *     first: {type: "panel", id: "B", ...},
     *     second: {type: "panel", id: "C", ...}
     *   }
     * }
     */
    QVariantMap toVariant() const override {
        QVariantMap result{
            {"type", "container"},
            {"id", nodeId()},
            {"orientation", m_orientation == Horizontal ? "horizontal" : "vertical"},
            {"splitRatio", m_splitRatio},  // 保存分割比例（0.1-0.9）
            {"minSize", minSize()}
        };
        
        // 递归序列化子节点
        if (m_firstChild) result["first"] = m_firstChild->toVariant();
        if (m_secondChild) result["second"] = m_secondChild->toVariant();
        
        return result;
    }
    
signals:
    void orientationChanged();  // 方向改变信号
    void splitRatioChanged();   // 比例改变信号（重要：触发界面重新布局）
    void childrenChanged();     // 子节点改变信号
    
private:
    Orientation m_orientation;   // 排列方向（Horizontal 或 Vertical）
    qreal m_splitRatio = 0.5;    // 分割比例（默认 50%）
    
    // 智能指针管理子节点（自动释放内存）
    std::unique_ptr<SplitPanelNode> m_firstChild;   // 第一个子节点
    std::unique_ptr<SplitPanelNode> m_secondChild;  // 第二个子节点
};

#endif // DOCKING_NODE_HPP

