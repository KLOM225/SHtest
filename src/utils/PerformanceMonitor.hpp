#ifndef PERFORMANCE_MONITOR_HPP
#define PERFORMANCE_MONITOR_HPP

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <QMap>
#include "Logger.hpp"

/**
 * @brief 性能监控工具类
 * 用于测量和记录操作的执行时间
 */
class PerformanceMonitor
{
public:
    /**
     * @brief RAII风格的计时器类
     * 构造时开始计时，析构时自动记录耗时
     */
    class ScopedTimer {
    public:
        explicit ScopedTimer(const QString& operation)
            : m_operation(operation)
        {
            m_timer.start();
        }
        
        ~ScopedTimer() {
            qint64 elapsed = m_timer.elapsed();
            
            // 记录到统计信息
            PerformanceMonitor::instance().recordOperation(m_operation, elapsed);
            
            // 使用辅助函数记录性能日志
            PerformanceMonitor::instance().logPerformance(m_operation, elapsed);
        }
        
    private:
        QString m_operation;
        QElapsedTimer m_timer;
    };
    
    /**
     * @brief 获取单例实例
     */
    static PerformanceMonitor& instance() {
        static PerformanceMonitor instance;
        return instance;
    }
    
    /**
     * @brief 记录操作统计
     */
    void recordOperation(const QString& operation, qint64 elapsedMs) {
        QMutexLocker locker(&m_mutex);
        
        auto& stats = m_operationStats[operation];
        stats.count++;
        stats.totalTime += elapsedMs;
        stats.minTime = (stats.count == 1) ? elapsedMs : qMin(stats.minTime, elapsedMs);
        stats.maxTime = qMax(stats.maxTime, elapsedMs);
        stats.avgTime = stats.totalTime / stats.count;
    }
    
    /**
     * @brief 记录性能日志（辅助函数）
     * 根据耗时阈值记录不同级别的日志
     */
    void logPerformance(const QString& operation, qint64 elapsedMs) {
        if (elapsedMs > 100) {  // 100ms阈值 - 警告
            Logger::instance()->warning("Performance", 
                QString("Slow operation: %1 took %2ms").arg(operation).arg(elapsedMs), {});
        } else if (elapsedMs > 50) {  // 50ms阈值 - 调试
            Logger::instance()->debug("Performance", 
                QString("Operation: %1 took %2ms").arg(operation).arg(elapsedMs), {});
        }
    }
    
    /**
     * @brief 获取操作统计信息
     */
    struct OperationStats {
        int count = 0;
        qint64 totalTime = 0;
        qint64 minTime = 0;
        qint64 maxTime = 0;
        double avgTime = 0.0;
    };
    
    QMap<QString, OperationStats> getStats() const {
        QMutexLocker locker(&m_mutex);
        return m_operationStats;
    }
    
    /**
     * @brief 打印统计报告
     */
    void printReport() const {
        QMutexLocker locker(&m_mutex);
        
        Logger::instance()->info("Performance", "=== Performance Report ===", {});
        
        for (auto it = m_operationStats.begin(); it != m_operationStats.end(); ++it) {
            const auto& stats = it.value();
            QString report = QString(
                "Operation: %1 | Count: %2 | Avg: %3ms | Min: %4ms | Max: %5ms | Total: %6ms"
            ).arg(it.key())
             .arg(stats.count)
             .arg(stats.avgTime, 0, 'f', 2)
             .arg(stats.minTime)
             .arg(stats.maxTime)
             .arg(stats.totalTime);
            
            Logger::instance()->info("Performance", report, {});
        }
        
        Logger::instance()->info("Performance", "========================", {});
    }
    
    /**
     * @brief 重置统计信息
     */
    void reset() {
        QMutexLocker locker(&m_mutex);
        m_operationStats.clear();
    }
    
private:
    PerformanceMonitor() = default;
    
    mutable QMutex m_mutex;
    QMap<QString, OperationStats> m_operationStats;
};

// 便捷宏定义
#define PERF_MONITOR(operation) \
    PerformanceMonitor::ScopedTimer __perf_timer__(operation)

#endif // PERFORMANCE_MONITOR_HPP

