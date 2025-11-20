#include "SplitNode.hpp"
#include <QtMath>

// ============================================================================
// SplitNode 实现
// ============================================================================

SplitNode::SplitNode(NodeType type, const QString& id, QObject* parent)
    : QObject(parent)
    , m_nodeType(type)
    , m_nodeId(id)
{
}

void SplitNode::setSizeRatio(double ratio)
{
    double clamped = qBound(0.0, ratio, 1.0);
    if (!qFuzzyCompare(m_sizeRatio, clamped)) {
        m_sizeRatio = clamped;
        emit sizeRatioChanged();
    }
}

void SplitNode::setMinSize(double size)
{
    double validated = qBound(50.0, size, 1000.0);
    if (!qFuzzyCompare(m_minSize, validated)) {
        m_minSize = validated;
        emit minSizeChanged();
    }
}

// ============================================================================
// PanelNode 实现
// ============================================================================

PanelNode::PanelNode(const QString& panelId, const QString& title, QObject* parent)
    : SplitNode(NodeType::Panel, panelId, parent)
    , m_panelId(panelId)
    , m_title(title)
{
}

void PanelNode::setTitle(const QString& title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void PanelNode::setQmlSource(const QString& source)
{
    if (m_qmlSource != source) {
        m_qmlSource = source;
        emit qmlSourceChanged();
    }
}

void PanelNode::setCanClose(bool canClose)
{
    if (m_canClose != canClose) {
        m_canClose = canClose;
        emit canCloseChanged();
    }
}

void PanelNode::setVisible(bool visible)
{
    if (m_isVisible != visible) {
        m_isVisible = visible;
        emit visibleChanged();
    }
}

QVariantMap PanelNode::toVariantMap() const
{
    QVariantMap map;
    map["type"] = "panel";
    map["nodeId"] = m_nodeId;
    map["panelId"] = m_panelId;
    map["title"] = m_title;
    map["qmlSource"] = m_qmlSource;
    map["canClose"] = m_canClose;
    map["isVisible"] = m_isVisible;
    map["sizeRatio"] = m_sizeRatio;
    map["minSize"] = m_minSize;
    return map;
}

// ============================================================================
// SplitContainerNode 实现
// ============================================================================

SplitContainerNode::SplitContainerNode(const QString& id, SplitOrientation orientation, QObject* parent)
    : SplitNode(NodeType::Split, id, parent)
    , m_orientation(orientation)
{
}

void SplitContainerNode::setOrientation(SplitOrientation orientation)
{
    if (m_orientation != orientation) {
        m_orientation = orientation;
        emit orientationChanged();
    }
}

void SplitContainerNode::setSplitRatio(double ratio)
{
    double validated = qBound(0.1, ratio, 0.9);
    if (!qFuzzyCompare(m_splitRatio, validated)) {
        m_splitRatio = validated;
        emit splitRatioChanged();
    }
}

void SplitContainerNode::setFirstChild(std::unique_ptr<SplitNode> child)
{
    if (m_firstChild.get() == child.get())
        return;
    
    // 智能指针自动删除旧的子节点
    m_firstChild = std::move(child);
    
    // 设置父对象关系（用于Qt的内存管理）
    if (m_firstChild)
        m_firstChild->setParent(this);
    
    emit firstChildChanged();
}

void SplitContainerNode::setSecondChild(std::unique_ptr<SplitNode> child)
{
    if (m_secondChild.get() == child.get())
        return;
    
    // 智能指针自动删除旧的子节点
    m_secondChild = std::move(child);
    
    // 设置父对象关系（用于Qt的内存管理）
    if (m_secondChild)
        m_secondChild->setParent(this);
    
    emit secondChildChanged();
}

std::unique_ptr<SplitNode> SplitContainerNode::takeFirstChild()
{
    auto child = std::move(m_firstChild);
    if (child)
        child->setParent(nullptr);
    
    emit firstChildChanged();
    return child;
}

std::unique_ptr<SplitNode> SplitContainerNode::takeSecondChild()
{
    auto child = std::move(m_secondChild);
    if (child)
        child->setParent(nullptr);
    
    emit secondChildChanged();
    return child;
}

QVariantMap SplitContainerNode::toVariantMap() const
{
    QVariantMap map;
    map["type"] = "split";
    map["nodeId"] = m_nodeId;
    map["orientation"] = (m_orientation == SplitOrientation::Horizontal) ? "horizontal" : "vertical";
    map["splitRatio"] = m_splitRatio;
    map["sizeRatio"] = m_sizeRatio;
    map["minSize"] = m_minSize;
    
    if (m_firstChild)
        map["firstChild"] = m_firstChild->toVariantMap();
    if (m_secondChild)
        map["secondChild"] = m_secondChild->toVariantMap();
    
    return map;
}

