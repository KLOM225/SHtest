#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QtQml/qqmlregistration.h>

// Forward declarations
class QQmlEngine;
class QJSEngine;

/**
 * @brief 日志系统类 - 单例模式
 * 
 * 功能说明:
 * - 提供统一的日志记录接口
 * - 支持多个日志级别 (Debug, Info, Warning, Error)
 * - 可同时输出到控制台和文件
 * - 线程安全 (使用互斥锁保护)
 * - 支持在QML中使用 (通过QML_ELEMENT和QML_SINGLETON)
 * - 支持上下文信息 (通过QVariantMap传递额外参数)
 * 
 * 使用示例:
 * C++:
 *   Logger::instance()->info("MyCategory", "Something happened", {{"key", "value"}});
 * 
 * QML:
 *   Logger.info("MyCategory", "Something happened", {key: "value"})
 */
class Logger : public QObject
{
    Q_OBJECT
    QML_ELEMENT      // 使此类可在QML中作为类型使用
    QML_SINGLETON    // 将此类注册为QML单例
    
public:
    /**
     * @brief 日志级别枚举
     * Debug   - 调试信息,用于开发阶段详细跟踪
     * Info    - 一般信息,记录重要的业务流程
     * Warning - 警告信息,可能存在的问题但不影响运行
     * Error   - 错误信息,发生了错误需要关注
     */
    enum class LogLevel {
        Debug,      // 最低级别
        Info,       
        Warning,    
        Error       // 最高级别
    };
    Q_ENUM(LogLevel)  // 注册枚举到Qt元对象系统,使其可在QML中使用
    
    /**
     * @brief 获取Logger单例实例
     * @return Logger* 全局唯一的Logger实例指针
     */
    static Logger* instance();
    
    /**
     * @brief QML单例创建方法
     * @param engine QML引擎指针
     * @param scriptEngine JS引擎指针
     * @return Logger* Logger单例实例
     * 
     * 此方法是为了满足QML_SINGLETON的要求而添加的
     */
    static Logger* create(QQmlEngine* engine, QJSEngine* scriptEngine);
    
    // ========================================
    // 日志记录方法 (QML可调用)
    // ========================================
    
    /**
     * @brief 记录调试信息
     * @param category 日志分类 (如 "Application", "Network" 等)
     * @param message 日志消息内容
     * @param context 可选的上下文信息 (键值对)
     */
    Q_INVOKABLE void debug(const QString& category, const QString& message, const QVariantMap& context = QVariantMap());
    
    /**
     * @brief 记录一般信息
     * @param category 日志分类
     * @param message 日志消息内容
     * @param context 可选的上下文信息
     */
    Q_INVOKABLE void info(const QString& category, const QString& message, const QVariantMap& context = QVariantMap());
    
    /**
     * @brief 记录警告信息
     * @param category 日志分类
     * @param message 日志消息内容
     * @param context 可选的上下文信息
     */
    Q_INVOKABLE void warning(const QString& category, const QString& message, const QVariantMap& context = QVariantMap());
    
    /**
     * @brief 记录错误信息
     * @param category 日志分类
     * @param message 日志消息内容
     * @param context 可选的上下文信息
     */
    Q_INVOKABLE void error(const QString& category, const QString& message, const QVariantMap& context = QVariantMap());
    
    // ========================================
    // 配置方法 (QML可调用)
    // ========================================
    
    /**
     * @brief 设置日志过滤级别
     * 只有等于或高于此级别的日志才会被输出
     * @param level 日志级别
     */
    Q_INVOKABLE void setLogLevel(LogLevel level);
    
    /**
     * @brief 获取当前日志级别
     * @return LogLevel 当前的日志级别
     */
    Q_INVOKABLE LogLevel logLevel() const { return m_logLevel; }
    
    /**
     * @brief 启用或禁用文件日志
     * @param enabled true=启用文件日志, false=仅输出到控制台
     */
    Q_INVOKABLE void setFileLoggingEnabled(bool enabled);
    
    /**
     * @brief 检查文件日志是否已启用
     * @return bool true=已启用, false=未启用
     */
    Q_INVOKABLE bool isFileLoggingEnabled() const { return m_fileLoggingEnabled; }
    
    /**
     * @brief 设置日志文件路径
     * @param path 日志文件的完整路径
     */
    Q_INVOKABLE void setLogFilePath(const QString& path);
    
    /**
     * @brief 获取当前日志文件路径
     * @return QString 日志文件路径
     */
    Q_INVOKABLE QString logFilePath() const { return m_logFilePath; }
    
signals:
    /**
     * @brief 日志消息发出信号
     * 可在QML中连接此信号以实现自定义日志处理
     * @param level 日志级别
     * @param category 日志分类
     * @param message 日志消息
     */
    void logMessageEmitted(LogLevel level, const QString& category, const QString& message);
    
private:
    /**
     * @brief 私有构造函数 (单例模式)
     * @param parent 父对象指针
     */
    explicit Logger(QObject* parent = nullptr);
    
    /**
     * @brief 析构函数 - 确保关闭日志文件
     */
    ~Logger() override;
    
    /**
     * @brief 核心日志记录方法
     * @param level 日志级别
     * @param category 日志分类
     * @param message 日志消息
     * @param context 上下文信息
     */
    void log(LogLevel level, const QString& category, const QString& message, const QVariantMap& context);
    
    /**
     * @brief 将日志写入文件
     * @param logLine 格式化后的日志行
     */
    void writeToFile(const QString& logLine);
    
    /**
     * @brief 格式化上下文信息为字符串
     * @param context 上下文映射
     * @return QString 格式化后的字符串 (如 "key1=value1, key2=value2")
     */
    QString formatContext(const QVariantMap& context);
    
    /**
     * @brief 将日志级别转换为字符串
     * @param level 日志级别
     * @return QString 日志级别字符串 (如 "DEBUG", "INFO")
     */
    QString levelToString(LogLevel level);
    
    /**
     * @brief 输出分隔线
     * @param category 日志分类
     * @param text 分隔线中的文本（可选）
     */
    void logSeparator(const QString& category, const QString& text = QString());
    
    /**
     * @brief 批量输出多条信息
     * @param category 日志分类
     * @param messages 消息列表
     * @param level 日志级别（默认Info）
     */
    void logMultiple(const QString& category, const QStringList& messages, LogLevel level = LogLevel::Info);
    
    // ========================================
    // 成员变量
    // ========================================
    static Logger* s_instance;           ///< 单例实例指针
    
    LogLevel m_logLevel{LogLevel::Info}; ///< 当前日志级别
    bool m_fileLoggingEnabled{false};    ///< 是否启用文件日志
    QString m_logFilePath;               ///< 日志文件路径
    QMutex m_mutex;                      ///< 互斥锁,保证线程安全
    QFile m_logFile;                     ///< 日志文件对象
};

/**
 * @brief 便捷日志宏定义
 * 
 * 使用示例:
 *   LOG_DEBUG("Network", "Connection established");
 *   LOG_INFO("Application", "Starting up...");
 *   LOG_WARNING("Database", "Connection timeout, retrying...");
 *   LOG_ERROR("FileSystem", "Failed to open file");
 */
#define LOG_DEBUG(category, message)   Logger::instance()->debug(category, message)
#define LOG_INFO(category, message)    Logger::instance()->info(category, message)
#define LOG_WARNING(category, message) Logger::instance()->warning(category, message)
#define LOG_ERROR(category, message)   Logger::instance()->error(category, message)

#endif // LOGGER_HPP

