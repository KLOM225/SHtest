#include "DockingManager.hpp"
#include "../utils/Logger.hpp"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QCoreApplication>  // 用于获取应用程序目录路径
#include <QSettings>          // 用于读取INI配置文件

DockingManager::DockingManager(QObject* parent)
    : QObject(parent)
{
    LOG_INFO("DockingManager", "Manager initialized");
}

// ============================================================================
// 核心 API 实现
// ============================================================================

PanelNode* DockingManager::createPanel(const QString& title, const QString& qmlSource)
{
    QString id = generateNodeId();
    auto panel = new PanelNode(id, title, this);
    panel->setQmlSource(qmlSource);
    return panel;
}

bool DockingManager::addPanel(const QString& panelId, const QString& title, const QString& qmlSource)
{
    // 【原子操作1】创建面板节点
    auto panel = createPanelNode(panelId, title, qmlSource);
    
    // 【特殊情况】树为空，面板直接成为根节点
    if (!m_root) {
        registerPanel(panelId, panel.get());
        setAsRoot(std::move(panel));
        
        LOG_INFO("DockingManager", "Panel set as root");
        emitPanelAddedSignals(panelId);
        return true;
    }
    
    // 【原子操作2】查找最右侧面板
    DockingNode* rightmost = findRightmostPanel(m_root.get());
    if (!rightmost) {
        LOG_ERROR("DockingManager", "Failed to find rightmost panel");
        return false;
    }
    
    // 【原子操作3】注册面板到映射表
    registerPanel(panelId, panel.get());
    
    // 【原子操作4】在目标位置插入面板
    bool success = insertPanelAt(std::move(panel), rightmost, Direction::Right);
    if (success) {
        emitPanelAddedSignals(panelId);
    }
    
    return success;
}

bool DockingManager::addPanelAt(const QString& panelId, const QString& title, const QString& qmlSource,
                                  const QString& targetId, int direction)
{
    // 【原子操作1】查找目标节点
    DockingNode* target = findNode(m_root.get(), targetId);
    if (!target) {
        LOG_ERROR("DockingManager", QString("Target panel not found: %1").arg(targetId));
        return false;
    }
    
    // 【原子操作2】创建面板节点
    auto panel = createPanelNode(panelId, title, qmlSource);
    
    // 【原子操作3】注册面板到映射表
    registerPanel(panelId, panel.get());
    
    // 【原子操作4】在目标位置插入面板
    bool success = insertPanelAt(std::move(panel), target, static_cast<Direction>(direction));
    if (success) {
        emitPanelAddedSignals(panelId);
    }
    
    return success;
}

bool DockingManager::removePanel(const QString& panelId)
{
    // 【原子操作1】重入保护检查
    static QString currentlyRemoving;
    if (currentlyRemoving == panelId) {
        LOG_DEBUG("DockingManager", QString("Panel removal already in progress, ignoring: %1").arg(panelId));
        return false;
    }
    currentlyRemoving = panelId;
    
    LOG_DEBUG("DockingManager", QString("Starting panel removal: %1").arg(panelId));
    
    // 【原子操作2】查找并验证面板
    PanelNode* panel = m_panels.value(panelId, nullptr);
    if (!panel) {
        currentlyRemoving.clear();
        LOG_ERROR("DockingManager", QString("Panel not found: %1").arg(panelId));
        return false;
    }
    
    LOG_DEBUG("DockingManager", QString("Panel title: %1").arg(panel->title()));
    LOG_DEBUG("DockingManager", QString("Current panel count: %1").arg(m_panels.size()));
    
    // 【原子操作3】从面板映射中注销
    unregisterPanel(panelId);
    
    // 【特殊情况】面板直接是根节点（唯一面板）
    if (panel == m_root.get()) {
        m_root.reset();
        finalizePanelRemoval(panelId);
        currentlyRemoving.clear();
        return true;
    }
    
    // 【原子操作4】获取父容器
    auto* parentContainer = getParentContainer(panel);
    if (!parentContainer) {
        currentlyRemoving.clear();
        LOG_ERROR("DockingManager", "Panel has no valid parent container");
        return false;
    }
    
    // 【原子操作5】取出兄弟节点
    auto [sibling, isFirst] = takeSiblingNode(parentContainer, panel);
    if (!sibling && parentContainer->firstChild() != panel && parentContainer->secondChild() != panel) {
        currentlyRemoving.clear();
        LOG_ERROR("DockingManager", "Panel is not a child of its parent container");
        return false;
    }
    
    LOG_DEBUG("DockingManager", QString("Panel is %1 child, sibling %2")
        .arg(isFirst ? "first" : "second")
        .arg(sibling ? "found" : "is null"));
    
    // 【原子操作6】提升兄弟节点到父容器位置
    if (!promoteSiblingNode(parentContainer, std::move(sibling))) {
        currentlyRemoving.clear();
        LOG_ERROR("DockingManager", "Failed to promote sibling node");
        return false;
    }
    
    // 【原子操作7】完成删除：日志、清理、信号
    // 注意：必须在promoteSiblingNode之后才发送信号，因为此时树结构已重组完成
    finalizePanelRemoval(panelId);
    currentlyRemoving.clear();
    
    return true;
}

PanelNode* DockingManager::findPanel(const QString& panelId)
{
    return m_panels.value(panelId, nullptr);
}

bool DockingManager::updateSplitRatio(const QString& containerId, double ratio)
{
    DockingNode* node = findNode(m_root.get(), containerId);
    if (!node || node->nodeType() != DockingNode::Container) {
        return false;
    }
    
    auto* container = qobject_cast<ContainerNode*>(node);
    if (container) {
        container->setSplitRatio(ratio);
        return true;
    }
    
    return false;
}

void DockingManager::clear()
{
    m_root.reset();
    m_panels.clear();
    
    emit rootNodeChanged();
    emit panelCountChanged();
    emit layoutChanged();
}

// ============================================================================
// 布局序列化
// ============================================================================

QVariantMap DockingManager::saveLayout() const
{
    QVariantMap layout;
    layout["version"] = "2.0";
    layout["minPanelSize"] = m_minPanelSize;
    
    if (m_root) {
        layout["root"] = m_root->toVariant();
    }
    
    return layout;
}

bool DockingManager::loadLayout(const QVariantMap& layout)
{
    if (layout.value("version").toString() != "2.0") {
        LOG_WARNING("DockingManager", "Incompatible layout version");
        return false;
    }
    
    clear();
    
    if (layout.contains("minPanelSize")) {
        setMinPanelSize(layout["minPanelSize"].toDouble());
    }
    
    if (layout.contains("root")) {
        m_root = loadNodeFromVariant(layout["root"].toMap());
        emit rootNodeChanged();
        emit panelCountChanged();
        emit layoutChanged();
        return m_root != nullptr;
    }
    
    return true;
}

bool DockingManager::saveLayoutToFile(const QString& filePath) const
{
    QVariantMap layout = saveLayout();
    QJsonDocument doc = QJsonDocument::fromVariant(layout);
    
    if (writeJsonToFile(filePath, doc)) {
        LOG_INFO("DockingManager", "Layout saved to file", {
            {"path", filePath},
            {"panelCount", QString::number(m_panels.size())}
        });
        return true;
    }
    
    LOG_ERROR("DockingManager", "Failed to write layout to file", {{"path", filePath}});
    return false;
}

bool DockingManager::loadLayoutFromFile(const QString& filePath)
{
    QJsonDocument doc;
    QString errorMsg;
    
    if (!readJsonFromFile(filePath, doc, &errorMsg)) {
        LOG_ERROR("DockingManager", "Failed to read layout file", {
            {"path", filePath},
            {"error", errorMsg}
        });
        return false;
    }
    
    QVariantMap layout = doc.object().toVariantMap();
    bool success = loadLayout(layout);
    
    if (success) {
        LOG_INFO("DockingManager", "Layout loaded from file", {
            {"path", filePath},
            {"panelCount", QString::number(m_panels.size())}
        });
    } else {
        LOG_ERROR("DockingManager", "Failed to load layout from file", {{"path", filePath}});
    }
    
    return success;
}

QString DockingManager::getDefaultLayoutPath() const
{
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString projectRoot = findProjectRoot(appDirPath);
    ensureDirectoryExists(projectRoot);
    return projectRoot + "/layout.json";
}


// ============================================================================
// 调试工具
// ============================================================================

QString DockingManager::dumpTree() const
{
    if (!m_root) {
        return "Empty tree";
    }
    return dumpNode(m_root.get(), 0);
}

QVariantList DockingManager::getFlatPanelList() const
{
    QVariantList result;
    collectPanels(m_root.get(), result);
    return result;
}

// ============================================================================
// 内部辅助方法
// ============================================================================

DockingNode* DockingManager::findNode(DockingNode* node, const QString& id)
{
    if (!node) return nullptr;
    if (node->nodeId() == id) return node;
    
    if (node->nodeType() == DockingNode::Container) {
        auto* container = qobject_cast<ContainerNode*>(node);
        if (auto* found = findNode(container->firstChild(), id)) {
            return found;
        }
        if (auto* found = findNode(container->secondChild(), id)) {
            return found;
        }
    }
    
    return nullptr;
}

DockingNode* DockingManager::findRightmostPanel(DockingNode* node)
{
    if (!node) return nullptr;
    
    if (node->nodeType() == DockingNode::Panel) {
        return node;
    }
    
    auto* container = qobject_cast<ContainerNode*>(node);
    if (container->secondChild()) {
        return findRightmostPanel(container->secondChild());
    }
    if (container->firstChild()) {
        return findRightmostPanel(container->firstChild());
    }
    
    return nullptr;
}

bool DockingManager::insertPanelAt(std::unique_ptr<DockingNode> panel, DockingNode* target, Direction dir)
{
    if (!target || !panel) return false;
    
    // 确定分割方向
    ContainerNode::Orientation orientation;
    bool panelIsFirst = false;
    
    switch (dir) {
    case Left:
        orientation = ContainerNode::Vertical;
        panelIsFirst = true;
        break;
    case Right:
        orientation = ContainerNode::Vertical;
        panelIsFirst = false;
        break;
    case Top:
        orientation = ContainerNode::Horizontal;
        panelIsFirst = true;
        break;
    case Bottom:
        orientation = ContainerNode::Horizontal;
        panelIsFirst = false;
        break;
    default:
        return false;
    }
    
    // 如果目标是根节点
    if (target == m_root.get()) {
        auto container = std::make_unique<ContainerNode>(generateNodeId(), orientation, this);
        
        if (panelIsFirst) {
            container->setFirstChild(std::move(panel));
            container->setSecondChild(std::move(m_root));
        } else {
            container->setFirstChild(std::move(m_root));
            container->setSecondChild(std::move(panel));
        }
        
        m_root = std::move(container);
        emit rootNodeChanged();
        return true;
    }
    
    // 如果目标有父节点
    QObject* parentObj = target->parent();
    if (!parentObj) return false;
    
    auto* parentContainer = qobject_cast<ContainerNode*>(parentObj);
    if (!parentContainer) return false;
    
    // 创建新容器
    auto newContainer = std::make_unique<ContainerNode>(generateNodeId(), orientation, this);
    
    // 从父容器取出目标节点
    std::unique_ptr<DockingNode> targetNode;
    if (parentContainer->firstChild() == target) {
        targetNode = parentContainer->takeFirstChild();
    } else if (parentContainer->secondChild() == target) {
        targetNode = parentContainer->takeSecondChild();
    } else {
        return false;
    }
    
    // 设置新容器的子节点
    if (panelIsFirst) {
        newContainer->setFirstChild(std::move(panel));
        newContainer->setSecondChild(std::move(targetNode));
    } else {
        newContainer->setFirstChild(std::move(targetNode));
        newContainer->setSecondChild(std::move(panel));
    }
    
    // 将新容器放回父容器
    if (parentContainer->firstChild() == nullptr) {
        parentContainer->setFirstChild(std::move(newContainer));
    } else {
        parentContainer->setSecondChild(std::move(newContainer));
    }
    
    return true;
}


std::unique_ptr<DockingNode> DockingManager::loadNodeFromVariant(const QVariantMap& data)
{
    QString type = data["type"].toString();
    QString id = data["id"].toString();
    
    if (type == "panel") {
        auto panel = std::make_unique<PanelNode>(id, data["title"].toString(), this);
        panel->setQmlSource(data["qmlSource"].toString());
        panel->setMinSize(data.value("minSize", m_minPanelSize).toDouble());
        
        m_panels[id] = panel.get();
        return panel;
    }
    else if (type == "container") {
        QString orientStr = data["orientation"].toString();
        auto orientation = (orientStr == "horizontal") 
            ? ContainerNode::Horizontal 
            : ContainerNode::Vertical;
        
        auto container = std::make_unique<ContainerNode>(id, orientation, this);
        container->setSplitRatio(data.value("splitRatio", 0.5).toDouble());
        container->setMinSize(data.value("minSize", m_minPanelSize).toDouble());
        
        if (data.contains("first")) {
            container->setFirstChild(loadNodeFromVariant(data["first"].toMap()));
        }
        if (data.contains("second")) {
            container->setSecondChild(loadNodeFromVariant(data["second"].toMap()));
        }
        
        return container;
    }
    
    return nullptr;
}

QString DockingManager::generateNodeId()
{
    return QString("node_%1").arg(++m_nodeIdCounter);
}

int DockingManager::countPanels(DockingNode* node) const
{
    if (!node) return 0;
    
    if (node->nodeType() == DockingNode::Panel) {
        return 1;
    }
    
    auto* container = qobject_cast<ContainerNode*>(node);
    return countPanels(container->firstChild()) + countPanels(container->secondChild());
}

QString DockingManager::dumpNode(DockingNode* node, int indent) const
{
    if (!node) return "";
    
    QString indentStr = QString(indent * 2, ' ');
    QString result;
    
    if (node->nodeType() == DockingNode::Panel) {
        auto* panel = qobject_cast<PanelNode*>(node);
        result = QString("%1Panel[%2]: %3\n")
            .arg(indentStr)
            .arg(panel->nodeId())
            .arg(panel->title());
    } else {
        auto* container = qobject_cast<ContainerNode*>(node);
        QString orientStr = container->orientation() == ContainerNode::Horizontal ? "H" : "V";
        result = QString("%1Container[%2]: %3 (ratio: %4)\n")
            .arg(indentStr)
            .arg(container->nodeId())
            .arg(orientStr)
            .arg(container->splitRatio());
        
        result += dumpNode(container->firstChild(), indent + 1);
        result += dumpNode(container->secondChild(), indent + 1);
    }
    
    return result;
}

void DockingManager::collectPanels(DockingNode* node, QVariantList& panels) const
{
    if (!node) return;
    
    if (node->nodeType() == DockingNode::Panel) {
        panels.append(QVariant::fromValue(qobject_cast<PanelNode*>(node)));
    } else {
        auto* container = qobject_cast<ContainerNode*>(node);
        collectPanels(container->firstChild(), panels);
        collectPanels(container->secondChild(), panels);
    }
}

void DockingManager::processDelayedDeletion()
{
    // 延迟删除机制已移除，此方法保留以兼容现有代码
    LOG_DEBUG("DockingManager", "processDelayedDeletion called but no longer used");
}

// ============================================================================
// 通用辅助函数实现
// ============================================================================

std::unique_ptr<PanelNode> DockingManager::createPanelNode(
    const QString& id,
    const QString& title,
    const QString& qmlSource)
{
    auto panel = std::make_unique<PanelNode>(id, title, this);
    panel->setQmlSource(qmlSource);
    panel->setMinSize(m_minPanelSize);
    return panel;
}

void DockingManager::registerPanel(const QString& panelId, PanelNode* panel)
{
    // 将面板指针添加到哈希表，用于快速查找 O(1)
    m_panels[panelId] = panel;
}

void DockingManager::unregisterPanel(const QString& panelId)
{
    // 从哈希表中移除面板指针（不影响实际节点的生命周期）
    m_panels.remove(panelId);
}

void DockingManager::setAsRoot(std::unique_ptr<DockingNode> node)
{
    // 设置新的根节点，旧根节点自动释放（智能指针）
    m_root = std::move(node);
    // 立即通知 QML 根节点已改变
    emit rootNodeChanged();
}

void DockingManager::emitPanelAddedSignals(const QString& panelId)
{
    // 统一发送面板添加相关的三个信号
    emit panelCountChanged();  // 更新面板计数
    emit panelAdded(panelId);  // 通知具体哪个面板被添加
    emit layoutChanged();      // 通知布局已改变
}

void DockingManager::emitPanelRemovedSignals(const QString& panelId)
{
    // 【重要】立即发送信号，不使用延迟机制
    // 原因：QTimer::singleShot 会延迟发送信号到事件循环的下一轮
    //       此时父容器可能已被智能指针删除，导致 QML 访问空指针崩溃
    // 修复：改为立即发送，确保信号在节点树重组完成后立即通知 QML 更新
    emit rootNodeChanged();
    emit panelCountChanged();
    emit panelRemoved(panelId);
    emit layoutChanged();
}

std::pair<std::unique_ptr<DockingNode>, bool> DockingManager::takeSiblingNode(
    ContainerNode* parent,
    DockingNode* targetChild)
{
    if (!parent || !targetChild) {
        return {nullptr, false};
    }
    
    // 判断目标子节点是第一个还是第二个
    bool isFirst = (parent->firstChild() == targetChild);
    
    if (isFirst) {
        // 目标是第一个子节点，取出第二个子节点（兄弟）
        return {parent->takeSecondChild(), true};
    } else if (parent->secondChild() == targetChild) {
        // 目标是第二个子节点，取出第一个子节点（兄弟）
        return {parent->takeFirstChild(), false};
    }
    
    // 目标子节点不在父容器中（理论上不应该发生）
    return {nullptr, false};
}

bool DockingManager::replaceChildInContainer(
    ContainerNode* container,
    DockingNode* oldChild,
    std::unique_ptr<DockingNode> newChild)
{
    if (!container || !oldChild) {
        return false;
    }
    
    // 设置新子节点的 Qt 父对象（用于内存管理和对象树）
    if (newChild) {
        newChild->setParent(container);
    }
    
    // 在容器中查找旧子节点，并用新子节点替换
    if (container->firstChild() == oldChild) {
        // 旧子节点是第一个子节点
        container->setFirstChild(std::move(newChild));
        return true;
    } else if (container->secondChild() == oldChild) {
        // 旧子节点是第二个子节点
        container->setSecondChild(std::move(newChild));
        return true;
    }
    
    // 旧子节点不在容器中
    return false;
}

ContainerNode* DockingManager::getParentContainer(DockingNode* node)
{
    if (!node) return nullptr;
    // 获取 Qt 父对象，并尝试转换为 ContainerNode
    // 如果父对象不是 ContainerNode，返回 nullptr
    return qobject_cast<ContainerNode*>(node->parent());
}

bool DockingManager::promoteSiblingNode(
    ContainerNode* parentContainer,
    std::unique_ptr<DockingNode> sibling)
{
    if (!parentContainer) return false;
    
    // 【情况1】父容器是根节点，直接替换根节点
    // 删除前树结构：root(container) -> [panel, sibling]
    // 删除后树结构：root(sibling)
    if (parentContainer == m_root.get()) {
        if (sibling) {
            // 【关键修复】先设置兄弟节点的Qt父对象为nullptr（根节点没有父对象）
            // 这样当m_root替换时，QML绑定访问sibling的parent()会返回nullptr而不是旧的container
            sibling->setParent(nullptr);
            
            // 兄弟节点提升为新根节点
            m_root = std::move(sibling);
            LOG_DEBUG("DockingManager", "Parent is root, replacing root with sibling");
        } else {
            // 没有兄弟节点，树变为空
            m_root.reset();
            LOG_DEBUG("DockingManager", "Parent is root, no sibling, clearing root");
        }
        return true;
    }
    
    // 【情况2】父容器不是根节点，需要在祖父容器中替换
    // 删除前树结构：grandParent -> parentContainer -> [panel, sibling]
    // 删除后树结构：grandParent -> sibling
    auto* grandParent = getParentContainer(parentContainer);
    if (!grandParent) {
        LOG_ERROR("DockingManager", "Parent container has no grandparent");
        return false;
    }
    
    // 在祖父容器中用兄弟节点替换父容器
    if (replaceChildInContainer(grandParent, parentContainer, std::move(sibling))) {
        LOG_DEBUG("DockingManager", "Replaced parent container with sibling in grandparent");
        return true;
    }
    
    return false;
}

void DockingManager::finalizePanelRemoval(const QString& panelId)
{
    // 【重要】先发送信号通知QML更新，确保树结构变更立即同步到QML层
    // 这样QML可以在日志输出前就看到最新的树结构，避免延迟导致的不一致
    emitPanelRemovedSignals(panelId);
    
    // 记录删除成功日志（在信号发送后记录，此时QML已完成更新）
    LOG_INFO("DockingManager", QString("Panel removed successfully: %1").arg(panelId));
    LOG_DEBUG("DockingManager", QString("New panel count: %1").arg(m_panels.size()));
}

// ============================================================================
// 静态辅助方法实现
// ============================================================================

bool DockingManager::writeJsonToFile(const QString& filePath, const QJsonDocument& jsonDoc)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    qint64 bytesWritten = file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();
    return bytesWritten > 0;
}

bool DockingManager::readJsonFromFile(const QString& filePath, QJsonDocument& outDoc, QString* errorMsg)
{
    QFile file(filePath);
    if (!file.exists()) {
        if (errorMsg) *errorMsg = "File does not exist";
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = "Failed to open file";
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    outDoc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMsg) *errorMsg = parseError.errorString();
        return false;
    }
    
    if (!outDoc.isObject()) {
        if (errorMsg) *errorMsg = "JSON root is not an object";
        return false;
    }
    
    return true;
}

bool DockingManager::ensureDirectoryExists(const QString& dirPath)
{
    QDir dir;
    if (!dir.exists(dirPath)) {
        return dir.mkpath(dirPath);
    }
    return true;
}

QString DockingManager::findProjectRoot(const QString& startPath, const QString& marker, int maxLevels)
{
    QDir searchDir(startPath);
    for (int i = 0; i < maxLevels; ++i) {
        if (searchDir.exists(marker)) {
            return searchDir.absolutePath();
        }
        if (!searchDir.cdUp()) {
            break;
        }
    }
    return startPath;
}

