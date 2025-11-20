#ifndef LAYOUT_VALIDATOR_HPP
#define LAYOUT_VALIDATOR_HPP

#include <QString>
#include <QStringList>
#include "../models/SplitNode.hpp"
#include "Logger.hpp"

/**
 * @brief 布局验证器
 * 验证布局树的完整性和合理性
 */
class LayoutValidator
{
public:
    struct ValidationResult {
        bool isValid = true;
        QStringList errors;
        QStringList warnings;
        
        void addError(const QString& error) {
            isValid = false;
            errors.append(error);
        }
        
        void addWarning(const QString& warning) {
            warnings.append(warning);
        }
    };
    
    /**
     * @brief 验证布局树
     * @param root 根节点
     * @return 验证结果
     */
    static ValidationResult validate(SplitNode* root) {
        ValidationResult result;
        
        if (!root) {
            result.addError("Root node is null");
            return result;
        }
        
        // 验证树结构
        validateNode(root, result, 0);
        
        // 检查深度
        int depth = calculateDepth(root);
        if (depth > 10) {
            result.addWarning(QString("Layout depth is very deep: %1 levels (recommended < 10)").arg(depth));
        }
        
        // 检查节点数量
        int nodeCount = countNodes(root);
        if (nodeCount > 50) {
            result.addWarning(QString("Too many nodes: %1 (recommended < 50)").arg(nodeCount));
        }
        
        return result;
    }
    
    /**
     * @brief 验证分割比例是否合理
     */
    static bool isValidSplitRatio(double ratio) {
        return ratio >= 0.1 && ratio <= 0.9;
    }
    
    /**
     * @brief 限制分割比例在合理范围内
     */
    static double clampSplitRatio(double ratio) {
        return qBound(0.1, ratio, 0.9);
    }
    
    /**
     * @brief 检查数值是否在合理范围内（通用辅助函数）
     */
    template<typename T>
    static bool isInRange(T value, T min, T max) {
        return value >= min && value <= max;
    }
    
    /**
     * @brief 限制数值在指定范围内（通用辅助函数）
     */
    template<typename T>
    static T clampValue(T value, T min, T max) {
        return qBound(min, value, max);
    }
    
    /**
     * @brief 验证并记录范围错误（辅助函数）
     */
    template<typename T>
    static void validateRange(T value, T min, T max, 
                              const QString& name, 
                              ValidationResult& result) {
        if (!isInRange(value, min, max)) {
            result.addWarning(QString("%1 out of range: %2 (expected %3-%4)")
                .arg(name).arg(value).arg(min).arg(max));
        }
    }
    
private:
    static void validateNode(SplitNode* node, ValidationResult& result, int depth) {
        if (!node) {
            result.addError("Found null node in tree");
            return;
        }
        
        // 检查节点ID
        if (node->nodeId().isEmpty()) {
            result.addError("Node has empty ID");
        }
        
        // 检查最小尺寸
        if (node->minSize() < 50.0) {
            result.addWarning(QString("Node %1 has very small minSize: %2")
                .arg(node->nodeId()).arg(node->minSize()));
        }
        
        // 如果是容器节点，验证子节点
        if (node->nodeType() == NodeType::Split) {
            auto* container = qobject_cast<SplitContainerNode*>(node);
            if (!container) {
                result.addError("Split node cast failed");
                return;
            }
            
            // 验证分割比例
            if (!isValidSplitRatio(container->splitRatio())) {
                result.addWarning(QString("Invalid split ratio in node %1: %2")
                    .arg(node->nodeId()).arg(container->splitRatio()));
            }
            
            // 验证子节点存在
            if (!container->firstChild() || !container->secondChild()) {
                result.addError(QString("Split container %1 missing child nodes")
                    .arg(node->nodeId()));
                return;
            }
            
            // 递归验证子节点
            validateNode(container->firstChild(), result, depth + 1);
            validateNode(container->secondChild(), result, depth + 1);
        }
        else if (node->nodeType() == NodeType::Panel) {
            auto* panel = qobject_cast<PanelNode*>(node);
            if (!panel) {
                result.addError("Panel node cast failed");
                return;
            }
            
            // 验证面板属性
            if (panel->title().isEmpty()) {
                result.addWarning(QString("Panel %1 has empty title")
                    .arg(panel->panelId()));
            }
            
            if (panel->qmlSource().isEmpty()) {
                result.addWarning(QString("Panel %1 has empty qmlSource")
                    .arg(panel->panelId()));
            }
        }
    }
    
    static int calculateDepth(SplitNode* node) {
        if (!node || node->nodeType() == NodeType::Panel) {
            return 1;
        }
        
        auto* container = qobject_cast<SplitContainerNode*>(node);
        if (!container) {
            return 1;
        }
        
        int leftDepth = container->firstChild() ? 
            calculateDepth(container->firstChild()) : 0;
        int rightDepth = container->secondChild() ? 
            calculateDepth(container->secondChild()) : 0;
        
        return 1 + qMax(leftDepth, rightDepth);
    }
    
    static int countNodes(SplitNode* node) {
        if (!node) {
            return 0;
        }
        
        if (node->nodeType() == NodeType::Panel) {
            return 1;
        }
        
        auto* container = qobject_cast<SplitContainerNode*>(node);
        if (!container) {
            return 1;
        }
        
        return 1 + 
            countNodes(container->firstChild()) + 
            countNodes(container->secondChild());
    }
};

#endif // LAYOUT_VALIDATOR_HPP

