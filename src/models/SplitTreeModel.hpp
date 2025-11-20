#ifndef SPLIT_TREE_MODEL_HPP
#define SPLIT_TREE_MODEL_HPP

#include <QAbstractItemModel>
#include <QtQml/qqmlregistration.h>
#include <QString>
#include <QVariantMap>
#include <memory>
#include <vector>

/**
 * 停靠系统树形模型 - 使用QAbstractItemModel架构
 * 
 * 优点：
 * 1. 标准Qt MVC架构
 * 2. 自动内存管理（智能指针）
 * 3. 信号自动通知视图更新
 * 4. 易于测试和调试
 */
class SplitTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(int panelCount READ panelCount NOTIFY panelCountChanged)
    Q_PROPERTY(bool hasRoot READ hasRoot NOTIFY rootChanged)
    Q_PROPERTY(double minPanelSize READ minPanelSize WRITE setMinPanelSize NOTIFY minPanelSizeChanged)
    
public:
    /**
     * 节点类型枚举
     */
    enum class NodeType {
        Panel,      // 面板节点（叶节点）
        Split       // 分割容器节点（分支节点）
    };
    Q_ENUM(NodeType)
    
    /**
     * 分割方向枚举
     */
    enum class Orientation {
        Horizontal,  // 水平分割（上下）
        Vertical     // 垂直分割（左右）
    };
    Q_ENUM(Orientation)
    
    /**
     * 停靠位置枚举
     */
    enum class DropZone {
        None,
        Left,
        Right,
        Top,
        Bottom,
        Center
    };
    Q_ENUM(DropZone)
    
    /**
     * 数据角色
     */
    enum DataRole {
        NodeTypeRole = Qt::UserRole + 1,  // 节点类型
        NodeIdRole,                        // 节点ID
        TitleRole,                         // 标题（仅面板）
        QmlSourceRole,                     // QML源文件路径（仅面板）
        CanCloseRole,                      // 是否可关闭（仅面板）
        OrientationRole,                   // 分割方向（仅容器）
        SplitRatioRole,                    // 分割比例（仅容器）
        MinSizeRole,                       // 最小尺寸
        HasChildrenRole                    // 是否有子节点
    };
    Q_ENUM(DataRole)
    
    explicit SplitTreeModel(QObject* parent = nullptr);
    ~SplitTreeModel() override = default;
    
    // ========================================
    // QAbstractItemModel 接口实现
    // ========================================
    
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // ========================================
    // 公开属性访问器
    // ========================================
    
    int panelCount() const { return m_panelCount; }
    bool hasRoot() const { return m_root != nullptr; }
    double minPanelSize() const { return m_minPanelSize; }
    void setMinPanelSize(double size);
    
    // ========================================
    // 面板管理操作
    // ========================================
    
    /**
     * 添加面板（自动选择位置）
     */
    Q_INVOKABLE bool addPanel(const QString& panelId, const QString& title, const QString& qmlSource = "");
    
    /**
     * 在指定位置添加面板
     */
    Q_INVOKABLE bool addPanelAt(const QString& panelId, const QString& title, const QString& qmlSource,
                                 const QString& targetId, int dropZone);
    
    /**
     * 移除面板
     */
    Q_INVOKABLE bool removePanel(const QString& panelId);
    
    /**
     * 查找面板节点的QModelIndex
     */
    Q_INVOKABLE QModelIndex findPanelIndex(const QString& panelId) const;
    
    /**
     * 更新分割比例
     */
    Q_INVOKABLE bool updateSplitRatio(const QString& containerId, double ratio);
    
    /**
     * 清空所有节点
     */
    Q_INVOKABLE void clear();
    
    // ========================================
    // 布局序列化
    // ========================================
    
    Q_INVOKABLE QVariantMap saveLayout() const;
    Q_INVOKABLE bool loadLayout(const QVariantMap& layout);
    
    // ========================================
    // 调试辅助
    // ========================================
    
    Q_INVOKABLE QString dumpTree() const;
    Q_INVOKABLE QVariantList getFlatPanelList() const;
    
signals:
    void panelCountChanged();
    void rootChanged();
    void minPanelSizeChanged();
    void panelAdded(const QString& panelId);
    void panelRemoved(const QString& panelId);
    void layoutChanged();
    
private:
    /**
     * 树节点结构 - 使用智能指针自动管理内存
     */
    struct TreeNode {
        NodeType type;
        QString id;
        TreeNode* parent = nullptr;  // 不拥有所有权，仅引用
        
        // 面板节点属性
        QString title;
        QString qmlSource;
        bool canClose = true;
        
        // 容器节点属性
        Orientation orientation = Orientation::Horizontal;
        double splitRatio = 0.5;
        std::unique_ptr<TreeNode> firstChild;
        std::unique_ptr<TreeNode> secondChild;
        
        // 通用属性
        double minSize = 150.0;
        
        explicit TreeNode(NodeType t, const QString& nodeId, TreeNode* p = nullptr)
            : type(t), id(nodeId), parent(p) {}
        
        ~TreeNode() = default;
        
        // 禁止拷贝
        TreeNode(const TreeNode&) = delete;
        TreeNode& operator=(const TreeNode&) = delete;
        
        // 允许移动
        TreeNode(TreeNode&&) = default;
        TreeNode& operator=(TreeNode&&) = default;
        
        /**
         * 获取子节点数量
         */
        int childCount() const {
            if (type == NodeType::Panel) return 0;
            int count = 0;
            if (firstChild) count++;
            if (secondChild) count++;
            return count;
        }
        
        /**
         * 获取第n个子节点
         */
        TreeNode* child(int n) const {
            if (type == NodeType::Panel) return nullptr;
            if (n == 0 && firstChild) return firstChild.get();
            if (n == 1 && secondChild) return secondChild.get();
            return nullptr;
        }
        
        /**
         * 获取当前节点在父节点中的行号
         */
        int row() const {
            if (!parent) return 0;
            if (parent->firstChild.get() == this) return 0;
            if (parent->secondChild.get() == this) return 1;
            return 0;
        }
        
        /**
         * 序列化为QVariantMap
         */
        QVariantMap toVariantMap() const;
    };
    
    // ========================================
    // 辅助方法
    // ========================================
    
    TreeNode* getNode(const QModelIndex& index) const;
    QModelIndex createIndexForNode(TreeNode* node) const;
    TreeNode* findNodeById(TreeNode* node, const QString& nodeId) const;
    TreeNode* findRightmostPanel(TreeNode* node) const;
    
    std::unique_ptr<TreeNode> createPanelNode(const QString& panelId, const QString& title, const QString& qmlSource);
    std::unique_ptr<TreeNode> createSplitNode(const QString& nodeId, Orientation orientation);
    
    bool insertPanelAt(std::unique_ptr<TreeNode> panel, TreeNode* targetNode, DropZone zone);
    std::unique_ptr<TreeNode> removeNode(TreeNode* node);
    
    std::unique_ptr<TreeNode> loadNodeFromVariant(const QVariantMap& data, TreeNode* parent);
    QString generateNodeId();
    
    void updatePanelCount();
    int countPanels(TreeNode* node) const;
    
    QString dumpNode(TreeNode* node, int indent = 0) const;
    void collectPanels(TreeNode* node, QVariantList& panels) const;
    
    // ========================================
    // 成员变量
    // ========================================
    
    std::unique_ptr<TreeNode> m_root;
    int m_panelCount = 0;
    double m_minPanelSize = 150.0;
    int m_nodeIdCounter = 0;
};

#endif // DOCKING_TREE_MODEL_HPP

