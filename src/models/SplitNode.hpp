#ifndef SPLIT_NODE_HPP
#define SPLIT_NODE_HPP

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

/**
 * 改进版SplitNode - 使用智能指针管理内存
 * 这个版本保持了与QML的兼容性，同时使用现代C++的内存管理
 */

class QQmlEngine;
class QJSEngine;

// 枚举定义
enum class SplitOrientation {
    Horizontal,  // 水平分割（上下）
    Vertical     // 垂直分割（左右）
};

enum class NodeType {
    Panel,       // 叶节点：面板
    Split        // 分支节点：分割容器
};

enum class DropZone {
    None,
    Left,
    Right,
    Top,
    Bottom,
    Center
};

/**
 * 枚举包装类 - 用于将枚举暴露给QML
 */
class DockingEnums : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    
public:
    enum SplitOrientationEnum {
        Horizontal = static_cast<int>(SplitOrientation::Horizontal),
        Vertical = static_cast<int>(SplitOrientation::Vertical)
    };
    Q_ENUM(SplitOrientationEnum)
    
    enum NodeTypeEnum {
        Panel = static_cast<int>(NodeType::Panel),
        Split = static_cast<int>(NodeType::Split)
    };
    Q_ENUM(NodeTypeEnum)
    
    enum DropZoneEnum {
        None = static_cast<int>(DropZone::None),
        Left = static_cast<int>(DropZone::Left),
        Right = static_cast<int>(DropZone::Right),
        Top = static_cast<int>(DropZone::Top),
        Bottom = static_cast<int>(DropZone::Bottom),
        Center = static_cast<int>(DropZone::Center)
    };
    Q_ENUM(DropZoneEnum)
    
    explicit DockingEnums(QObject* parent = nullptr) : QObject(parent) {}
    
    static DockingEnums* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
        Q_UNUSED(qmlEngine)
        Q_UNUSED(jsEngine)
        return new DockingEnums();
    }
};

/**
 * 分割节点基类 - 使用智能指针管理内存
 */
class SplitNode : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(NodeType nodeType READ nodeType CONSTANT)
    Q_PROPERTY(QString nodeId READ nodeId CONSTANT)
    Q_PROPERTY(double sizeRatio READ sizeRatio WRITE setSizeRatio NOTIFY sizeRatioChanged)
    Q_PROPERTY(double minSize READ minSize WRITE setMinSize NOTIFY minSizeChanged)
    
public:
    explicit SplitNode(NodeType type, const QString& id, QObject* parent = nullptr);
    virtual ~SplitNode() = default;
    
    NodeType nodeType() const { return m_nodeType; }
    QString nodeId() const { return m_nodeId; }
    double sizeRatio() const { return m_sizeRatio; }
    void setSizeRatio(double ratio);
    double minSize() const { return m_minSize; }
    void setMinSize(double size);
    
    virtual QVariantMap toVariantMap() const = 0;
    
signals:
    void sizeRatioChanged();
    void minSizeChanged();
    
protected:
    NodeType m_nodeType;
    QString m_nodeId;
    double m_sizeRatio{0.5};
    double m_minSize{150.0};
};

/**
 * 面板节点
 */
class PanelNode : public SplitNode
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString panelId READ panelId CONSTANT)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString qmlSource READ qmlSource WRITE setQmlSource NOTIFY qmlSourceChanged)
    Q_PROPERTY(bool canClose READ canClose WRITE setCanClose NOTIFY canCloseChanged)
    Q_PROPERTY(bool isVisible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    
public:
    explicit PanelNode(const QString& panelId, const QString& title, QObject* parent = nullptr);
    
    QString panelId() const { return m_panelId; }
    QString title() const { return m_title; }
    void setTitle(const QString& title);
    QString qmlSource() const { return m_qmlSource; }
    void setQmlSource(const QString& source);
    bool canClose() const { return m_canClose; }
    void setCanClose(bool canClose);
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible);
    
    QVariantMap toVariantMap() const override;
    
signals:
    void titleChanged();
    void qmlSourceChanged();
    void canCloseChanged();
    void visibleChanged();
    
private:
    QString m_panelId;
    QString m_title;
    QString m_qmlSource;
    bool m_canClose{true};
    bool m_isVisible{true};
};

/**
 * 分割容器节点 - 使用智能指针管理子节点
 * 
 * 关键改进：
 * 1. 使用 std::unique_ptr 自动管理子节点内存
 * 2. 提供原始指针接口给QML（Qt的属性系统需要）
 * 3. 确保内存安全，防止内存泄漏
 */
class SplitContainerNode : public SplitNode
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(SplitOrientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(double splitRatio READ splitRatio WRITE setSplitRatio NOTIFY splitRatioChanged)
    Q_PROPERTY(SplitNode* firstChild READ firstChild NOTIFY firstChildChanged)
    Q_PROPERTY(SplitNode* secondChild READ secondChild NOTIFY secondChildChanged)
    
public:
    explicit SplitContainerNode(const QString& id, SplitOrientation orientation, QObject* parent = nullptr);
    
    SplitOrientation orientation() const { return m_orientation; }
    void setOrientation(SplitOrientation orientation);
    
    double splitRatio() const { return m_splitRatio; }
    void setSplitRatio(double ratio);
    
    // QML使用的接口（返回原始指针）
    SplitNode* firstChild() const { return m_firstChild.get(); }
    SplitNode* secondChild() const { return m_secondChild.get(); }
    
    // C++使用的接口（使用智能指针）
    void setFirstChild(std::unique_ptr<SplitNode> child);
    void setSecondChild(std::unique_ptr<SplitNode> child);
    
    // 释放子节点所有权（用于重组树结构）
    std::unique_ptr<SplitNode> takeFirstChild();
    std::unique_ptr<SplitNode> takeSecondChild();
    
    QVariantMap toVariantMap() const override;
    
signals:
    void orientationChanged();
    void splitRatioChanged();
    void firstChildChanged();
    void secondChildChanged();
    
private:
    SplitOrientation m_orientation;
    double m_splitRatio{0.5};
    std::unique_ptr<SplitNode> m_firstChild;
    std::unique_ptr<SplitNode> m_secondChild;
};

// 注册元类型
Q_DECLARE_METATYPE(SplitOrientation)
Q_DECLARE_METATYPE(NodeType)
Q_DECLARE_METATYPE(DropZone)

#endif // SPLIT_NODE_HPP

