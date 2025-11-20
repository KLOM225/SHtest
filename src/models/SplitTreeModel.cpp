#include "SplitTreeModel.hpp"
#include "../utils/Logger.hpp"
#include <QDebug>
#include <algorithm>

// ============================================================================
// 构造函数和析构函数
// ============================================================================

SplitTreeModel::SplitTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    LOG_INFO("SplitTreeModel", "Tree model initialized");
}

void SplitTreeModel::setMinPanelSize(double size)
{
    double validated = qBound(50.0, size, 1000.0);
    if (!qFuzzyCompare(m_minPanelSize, validated)) {
        m_minPanelSize = validated;
        emit minPanelSizeChanged();
    }
}

// ============================================================================
// QAbstractItemModel 接口实现
// ============================================================================

QModelIndex SplitTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    
    TreeNode* parentNode = parent.isValid() ? getNode(parent) : m_root.get();
    if (!parentNode)
        return QModelIndex();
    
    TreeNode* childNode = parentNode->child(row);
    if (childNode)
        return createIndex(row, column, childNode);
    
    return QModelIndex();
}

QModelIndex SplitTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();
    
    TreeNode* childNode = getNode(child);
    if (!childNode || !childNode->parent)
        return QModelIndex();
    
    TreeNode* parentNode = childNode->parent;
    if (parentNode == m_root.get())
        return QModelIndex();
    
    return createIndex(parentNode->row(), 0, parentNode);
}

int SplitTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    
    TreeNode* parentNode = parent.isValid() ? getNode(parent) : m_root.get();
    return parentNode ? parentNode->childCount() : 0;
}

int SplitTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;  // 单列树
}

QVariant SplitTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    TreeNode* node = getNode(index);
    if (!node)
        return QVariant();
    
    switch (role) {
    case Qt::DisplayRole:
        return node->type == SplitTreeModel::NodeType::Panel ? node->title : QString("Split[%1]").arg(node->id);
        
    case NodeTypeRole:
        return static_cast<int>(node->type);
        
    case NodeIdRole:
        return node->id;
        
    case TitleRole:
        return node->type == SplitTreeModel::NodeType::Panel ? node->title : QString();
        
    case QmlSourceRole:
        return node->type == SplitTreeModel::NodeType::Panel ? node->qmlSource : QString();
        
    case CanCloseRole:
        return node->type == SplitTreeModel::NodeType::Panel ? node->canClose : false;
        
    case OrientationRole:
        return node->type == SplitTreeModel::NodeType::Split ? static_cast<int>(node->orientation) : -1;
        
    case SplitRatioRole:
        return node->type == SplitTreeModel::NodeType::Split ? node->splitRatio : 0.5;
        
    case MinSizeRole:
        return node->minSize;
        
    case HasChildrenRole:
        return node->childCount() > 0;
        
    default:
        return QVariant();
    }
}

bool SplitTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;
    
    TreeNode* node = getNode(index);
    if (!node)
        return false;
    
    bool changed = false;
    
    switch (role) {
    case TitleRole:
        if (node->type == SplitTreeModel::NodeType::Panel) {
            node->title = value.toString();
            changed = true;
        }
        break;
        
    case QmlSourceRole:
        if (node->type == SplitTreeModel::NodeType::Panel) {
            node->qmlSource = value.toString();
            changed = true;
        }
        break;
        
    case CanCloseRole:
        if (node->type == SplitTreeModel::NodeType::Panel) {
            node->canClose = value.toBool();
            changed = true;
        }
        break;
        
    case SplitRatioRole:
        if (node->type == SplitTreeModel::NodeType::Split) {
            node->splitRatio = qBound(0.1, value.toDouble(), 0.9);
            changed = true;
        }
        break;
        
    case MinSizeRole:
        node->minSize = qBound(50.0, value.toDouble(), 1000.0);
        changed = true;
        break;
        
    default:
        break;
    }
    
    if (changed) {
        emit dataChanged(index, index, {role});
        return true;
    }
    
    return false;
}

Qt::ItemFlags SplitTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> SplitTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";
    roles[NodeTypeRole] = "nodeType";
    roles[NodeIdRole] = "nodeId";
    roles[TitleRole] = "title";
    roles[QmlSourceRole] = "qmlSource";
    roles[CanCloseRole] = "canClose";
    roles[OrientationRole] = "orientation";
    roles[SplitRatioRole] = "splitRatio";
    roles[MinSizeRole] = "minSize";
    roles[HasChildrenRole] = "hasChildren";
    return roles;
}

// ============================================================================
// 面板管理操作
// ============================================================================

bool SplitTreeModel::addPanel(const QString& panelId, const QString& title, const QString& qmlSource)
{
    if (panelId.isEmpty()) {
        LOG_ERROR("SplitTreeModel", "Panel ID cannot be empty");
        return false;
    }
    
    // 检查面板是否已存在
    if (findNodeById(m_root.get(), panelId)) {
        LOG_ERROR("SplitTreeModel", "Panel already exists");
        return false;
    }
    
    auto panel = createPanelNode(panelId, title, qmlSource);
    
    // 如果没有根节点，直接设置为根
    if (!m_root) {
        beginResetModel();
        m_root = std::move(panel);
        endResetModel();
        
        updatePanelCount();
        emit rootChanged();
        emit panelAdded(panelId);
        emit layoutChanged();
        
        LOG_INFO("SplitTreeModel", "Panel set as root");
        return true;
    }
    
    // 查找最右侧的面板作为目标
    TreeNode* target = findRightmostPanel(m_root.get());
    if (!target)
        target = m_root.get();
    
    // 默认停靠到底部
    return insertPanelAt(std::move(panel), target, DropZone::Bottom);
}

bool SplitTreeModel::addPanelAt(const QString& panelId, const QString& title, const QString& qmlSource,
                                   const QString& targetId, int dropZone)
{
    if (panelId.isEmpty()) {
        LOG_ERROR("SplitTreeModel", "Panel ID cannot be empty");
        return false;
    }
    
    // 检查面板是否已存在
    if (findNodeById(m_root.get(), panelId)) {
        LOG_ERROR("SplitTreeModel", "Panel already exists");
        return false;
    }
    
    auto panel = createPanelNode(panelId, title, qmlSource);
    
    // 如果没有根节点，直接设置为根
    if (!m_root) {
        beginResetModel();
        m_root = std::move(panel);
        endResetModel();
        
        updatePanelCount();
        emit rootChanged();
        emit panelAdded(panelId);
        emit layoutChanged();
        
        return true;
    }
    
    // 查找目标节点
    TreeNode* target = findNodeById(m_root.get(), targetId);
    if (!target)
        target = m_root.get();
    
    return insertPanelAt(std::move(panel), target, static_cast<DropZone>(dropZone));
}

bool SplitTreeModel::removePanel(const QString& panelId)
{
    TreeNode* node = findNodeById(m_root.get(), panelId);
    if (!node || node->type != SplitTreeModel::NodeType::Panel) {
        LOG_ERROR("SplitTreeModel", "Panel not found");
        return false;
    }
    
    // 如果是根节点
    if (node == m_root.get()) {
        beginResetModel();
        m_root.reset();
        endResetModel();
        
        updatePanelCount();
        emit rootChanged();
        emit panelRemoved(panelId);
        emit layoutChanged();
        
        return true;
    }
    
    // 获取父节点
    TreeNode* parent = node->parent;
    if (!parent || parent->type != SplitTreeModel::NodeType::Split) {
        LOG_ERROR("SplitTreeModel", "Invalid parent node");
        return false;
    }
    
    // 获取兄弟节点
    std::unique_ptr<TreeNode> sibling;
    if (parent->firstChild.get() == node) {
        sibling = std::move(parent->secondChild);
    } else {
        sibling = std::move(parent->firstChild);
    }
    
    // 移除当前节点
    if (parent->firstChild.get() == node) {
        parent->firstChild.reset();
    } else {
        parent->secondChild.reset();
    }
    
    // 如果父节点是根节点
    if (parent == m_root.get()) {
        beginResetModel();
        if (sibling) {
            sibling->parent = nullptr;
            m_root = std::move(sibling);
        } else {
            m_root.reset();
        }
        endResetModel();
    } else {
        // 父节点不是根节点，需要用兄弟节点替换父节点
        TreeNode* grandParent = parent->parent;
        if (grandParent && sibling) {
            beginResetModel();
            sibling->parent = grandParent;
            
            if (grandParent->firstChild.get() == parent) {
                grandParent->firstChild = std::move(sibling);
            } else {
                grandParent->secondChild = std::move(sibling);
            }
            endResetModel();
        }
    }
    
    updatePanelCount();
    emit panelRemoved(panelId);
    emit layoutChanged();
    
    return true;
}

QModelIndex SplitTreeModel::findPanelIndex(const QString& panelId) const
{
    TreeNode* node = findNodeById(m_root.get(), panelId);
    if (!node)
        return QModelIndex();
    
    return createIndexForNode(node);
}

bool SplitTreeModel::updateSplitRatio(const QString& containerId, double ratio)
{
    TreeNode* node = findNodeById(m_root.get(), containerId);
    if (!node || node->type != SplitTreeModel::NodeType::Split) {
        return false;
    }
    
    QModelIndex idx = createIndexForNode(node);
    return setData(idx, ratio, SplitRatioRole);
}

void SplitTreeModel::clear()
{
    if (!m_root)
        return;
    
    beginResetModel();
    m_root.reset();
    endResetModel();
    
    updatePanelCount();
    emit rootChanged();
    emit layoutChanged();
}

// ============================================================================
// 布局序列化
// ============================================================================

QVariantMap SplitTreeModel::saveLayout() const
{
    QVariantMap layout;
    layout["version"] = "2.0";  // 新版本标识
    layout["model"] = "DockingTreeModel";
    layout["minPanelSize"] = m_minPanelSize;
    
    if (m_root) {
        layout["root"] = m_root->toVariantMap();
    }
    
    return layout;
}

bool SplitTreeModel::loadLayout(const QVariantMap& layout)
{
    if (layout.isEmpty()) {
        LOG_ERROR("SplitTreeModel", "Layout data is empty");
        return false;
    }
    
    QString version = layout.value("version").toString();
    if (version != "2.0") {
        LOG_ERROR("SplitTreeModel", "Unsupported layout version");
        return false;
    }
    
    beginResetModel();
    m_root.reset();
    
    if (layout.contains("minPanelSize")) {
        m_minPanelSize = layout.value("minPanelSize").toDouble();
    }
    
    if (layout.contains("root")) {
        QVariantMap rootData = layout.value("root").toMap();
        m_root = loadNodeFromVariant(rootData, nullptr);
    }
    
    endResetModel();
    
    updatePanelCount();
    emit rootChanged();
    emit minPanelSizeChanged();
    emit layoutChanged();
    
    LOG_INFO("SplitTreeModel", "Layout loaded successfully");
    return true;
}

// ============================================================================
// 调试辅助
// ============================================================================

QString SplitTreeModel::dumpTree() const
{
    if (!m_root)
        return "Empty tree";
    
    return dumpNode(m_root.get(), 0);
}

QVariantList SplitTreeModel::getFlatPanelList() const
{
    QVariantList panels;
    collectPanels(m_root.get(), panels);
    return panels;
}

// ============================================================================
// TreeNode 序列化
// ============================================================================

QVariantMap SplitTreeModel::TreeNode::toVariantMap() const
{
    QVariantMap map;
    
    if (type == SplitTreeModel::NodeType::Panel) {
        map["type"] = "panel";
        map["id"] = id;
        map["title"] = title;
        map["qmlSource"] = qmlSource;
        map["canClose"] = canClose;
        map["minSize"] = minSize;
    } else {
        map["type"] = "split";
        map["id"] = id;
        map["orientation"] = (orientation == Orientation::Horizontal) ? "horizontal" : "vertical";
        map["splitRatio"] = splitRatio;
        map["minSize"] = minSize;
        
        if (firstChild)
            map["firstChild"] = firstChild->toVariantMap();
        if (secondChild)
            map["secondChild"] = secondChild->toVariantMap();
    }
    
    return map;
}

// ============================================================================
// 辅助方法实现
// ============================================================================

SplitTreeModel::TreeNode* SplitTreeModel::getNode(const QModelIndex& index) const
{
    if (index.isValid()) {
        return static_cast<TreeNode*>(index.internalPointer());
    }
    return nullptr;
}

QModelIndex SplitTreeModel::createIndexForNode(TreeNode* node) const
{
    if (!node || node == m_root.get())
        return QModelIndex();
    
    return createIndex(node->row(), 0, node);
}

SplitTreeModel::TreeNode* SplitTreeModel::findNodeById(TreeNode* node, const QString& nodeId) const
{
    if (!node || nodeId.isEmpty())
        return nullptr;
    
    if (node->id == nodeId)
        return node;
    
    if (node->type == SplitTreeModel::NodeType::Split) {
        if (auto* found = findNodeById(node->firstChild.get(), nodeId))
            return found;
        if (auto* found = findNodeById(node->secondChild.get(), nodeId))
            return found;
    }
    
    return nullptr;
}

SplitTreeModel::TreeNode* SplitTreeModel::findRightmostPanel(TreeNode* node) const
{
    if (!node)
        return nullptr;
    
    if (node->type == SplitTreeModel::NodeType::Panel)
        return node;
    
    // 优先查找右侧/底部
    if (node->secondChild) {
        if (auto* panel = findRightmostPanel(node->secondChild.get()))
            return panel;
    }
    
    if (node->firstChild) {
        return findRightmostPanel(node->firstChild.get());
    }
    
    return nullptr;
}

std::unique_ptr<SplitTreeModel::TreeNode> SplitTreeModel::createPanelNode(
    const QString& panelId, const QString& title, const QString& qmlSource)
{
    auto node = std::make_unique<TreeNode>(SplitTreeModel::NodeType::Panel, panelId);
    node->title = title;
    node->qmlSource = qmlSource;
    node->canClose = true;
    node->minSize = m_minPanelSize;
    return node;
}

std::unique_ptr<SplitTreeModel::TreeNode> SplitTreeModel::createSplitNode(
    const QString& nodeId, Orientation orientation)
{
    auto node = std::make_unique<TreeNode>(SplitTreeModel::NodeType::Split, nodeId);
    node->orientation = orientation;
    node->splitRatio = 0.5;
    node->minSize = m_minPanelSize;
    return node;
}

bool SplitTreeModel::insertPanelAt(std::unique_ptr<TreeNode> panel, TreeNode* targetNode, DropZone zone)
{
    if (!panel || !targetNode) {
        LOG_ERROR("SplitTreeModel", "Invalid panel or target node");
        return false;
    }
    
    // 确定分割方向
    Orientation orientation;
    bool reverseOrder = false;
    
    switch (zone) {
    case DropZone::Left:
        orientation = Orientation::Vertical;
        reverseOrder = true;
        break;
    case DropZone::Right:
        orientation = Orientation::Vertical;
        reverseOrder = false;
        break;
    case DropZone::Top:
        orientation = Orientation::Horizontal;
        reverseOrder = true;
        break;
    case DropZone::Bottom:
        orientation = Orientation::Horizontal;
        reverseOrder = false;
        break;
    default:
        LOG_ERROR("SplitTreeModel", "Invalid drop zone");
        return false;
    }
    
    // 创建新的分割容器
    QString splitId = generateNodeId();
    auto container = createSplitNode(splitId, orientation);
    
    beginResetModel();
    
    // 如果目标是根节点
    if (targetNode == m_root.get()) {
        auto oldRoot = std::move(m_root);
        oldRoot->parent = container.get();
        panel->parent = container.get();
        
        if (reverseOrder) {
            container->firstChild = std::move(panel);
            container->secondChild = std::move(oldRoot);
        } else {
            container->firstChild = std::move(oldRoot);
            container->secondChild = std::move(panel);
        }
        
        m_root = std::move(container);
    } else {
        // 目标不是根节点
        TreeNode* parent = targetNode->parent;
        if (!parent) {
            endResetModel();
            LOG_ERROR("SplitTreeModel", "Target node has no parent");
            return false;
        }
        
        container->parent = parent;
        panel->parent = container.get();
        
        std::unique_ptr<TreeNode> targetOwnership;
        if (parent->firstChild.get() == targetNode) {
            targetOwnership = std::move(parent->firstChild);
        } else {
            targetOwnership = std::move(parent->secondChild);
        }
        
        targetOwnership->parent = container.get();
        
        if (reverseOrder) {
            container->firstChild = std::move(panel);
            container->secondChild = std::move(targetOwnership);
        } else {
            container->firstChild = std::move(targetOwnership);
            container->secondChild = std::move(panel);
        }
        
        if (parent->firstChild.get() == nullptr) {
            parent->firstChild = std::move(container);
        } else {
            parent->secondChild = std::move(container);
        }
    }
    
    endResetModel();
    
    updatePanelCount();
    emit panelAdded(panel->id);
    emit layoutChanged();
    
    return true;
}

std::unique_ptr<SplitTreeModel::TreeNode> SplitTreeModel::loadNodeFromVariant(
    const QVariantMap& data, TreeNode* parent)
{
    QString type = data.value("type").toString();
    QString id = data.value("id").toString();
    
    if (type == "panel") {
        auto node = std::make_unique<TreeNode>(SplitTreeModel::NodeType::Panel, id, parent);
        node->title = data.value("title").toString();
        node->qmlSource = data.value("qmlSource").toString();
        node->canClose = data.value("canClose", true).toBool();
        node->minSize = data.value("minSize", m_minPanelSize).toDouble();
        return node;
    }
    
    if (type == "split") {
        QString orientationStr = data.value("orientation").toString();
        Orientation orientation = (orientationStr == "horizontal") 
            ? Orientation::Horizontal 
            : Orientation::Vertical;
        
        auto node = std::make_unique<TreeNode>(SplitTreeModel::NodeType::Split, id, parent);
        node->orientation = orientation;
        node->splitRatio = data.value("splitRatio", 0.5).toDouble();
        node->minSize = data.value("minSize", m_minPanelSize).toDouble();
        
        if (data.contains("firstChild")) {
            node->firstChild = loadNodeFromVariant(data.value("firstChild").toMap(), node.get());
        }
        
        if (data.contains("secondChild")) {
            node->secondChild = loadNodeFromVariant(data.value("secondChild").toMap(), node.get());
        }
        
        return node;
    }
    
    return nullptr;
}

QString SplitTreeModel::generateNodeId()
{
    return QString("node_%1").arg(++m_nodeIdCounter);
}

void SplitTreeModel::updatePanelCount()
{
    int oldCount = m_panelCount;
    m_panelCount = countPanels(m_root.get());
    
    if (oldCount != m_panelCount) {
        emit panelCountChanged();
    }
}

int SplitTreeModel::countPanels(TreeNode* node) const
{
    if (!node)
        return 0;
    
    if (node->type == SplitTreeModel::NodeType::Panel)
        return 1;
    
    return countPanels(node->firstChild.get()) + countPanels(node->secondChild.get());
}

QString SplitTreeModel::dumpNode(TreeNode* node, int indent) const
{
    if (!node)
        return "";
    
    QString result;
    QString indentStr = QString("  ").repeated(indent);
    
    if (node->type == SplitTreeModel::NodeType::Panel) {
        result = QString("%1Panel[%2]: %3\n").arg(indentStr).arg(node->id).arg(node->title);
    } else {
        QString orientStr = (node->orientation == Orientation::Horizontal) ? "H" : "V";
        result = QString("%1Split[%2] %3 (%.2f):\n")
            .arg(indentStr)
            .arg(node->id)
            .arg(orientStr)
            .arg(node->splitRatio);
        
        if (node->firstChild)
            result += dumpNode(node->firstChild.get(), indent + 1);
        if (node->secondChild)
            result += dumpNode(node->secondChild.get(), indent + 1);
    }
    
    return result;
}

void SplitTreeModel::collectPanels(TreeNode* node, QVariantList& panels) const
{
    if (!node)
        return;
    
    if (node->type == SplitTreeModel::NodeType::Panel) {
        QVariantMap panelData;
        panelData["id"] = node->id;
        panelData["title"] = node->title;
        panelData["qmlSource"] = node->qmlSource;
        panels.append(panelData);
    } else {
        collectPanels(node->firstChild.get(), panels);
        collectPanels(node->secondChild.get(), panels);
    }
}

