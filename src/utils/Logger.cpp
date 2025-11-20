/**
 * @file Logger.cpp
 * @brief 日志系统实现
 * 
 * 实现了线程安全的日志记录功能,支持:
 * - 多级别日志过滤
 * - 控制台输出
 * - 文件输出
 * - 时间戳格式化
 * - 上下文信息
 */

#include "Logger.hpp"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QJSEngine>

// 静态成员变量初始化
Logger* Logger::s_instance = nullptr;

/**
 * @brief 获取Logger单例实例
 * 
 * 使用懒汉式单例模式,第一次调用时创建实例
 * 注意: 这里没有使用线程安全的单例实现,因为通常在主线程初始化时调用
 */
Logger* Logger::instance()
{
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
}

/**
 * @brief QML单例创建方法
 * 
 * 此方法由QML引擎调用以创建Logger单例实例
 * 参数engine和scriptEngine由QML系统传入,这里未使用但必须保留
 */
Logger* Logger::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return instance();
}

/**
 * @brief Logger构造函数
 * 
 * 初始化日志系统,设置默认日志文件路径为logs文件夹下的app.log
 */
Logger::Logger(QObject* parent)
    : QObject(parent)
{
    // 查找项目根目录（包含CMakeLists.txt的目录）
    QString appDirPath = QCoreApplication::applicationDirPath();
    QDir searchDir(appDirPath);
    QString projectRoot = appDirPath;
    
    int maxLevels = 5;
    for (int i = 0; i < maxLevels; ++i) {
        if (searchDir.exists("CMakeLists.txt")) {
            projectRoot = searchDir.absolutePath();
            break;
        }
        if (!searchDir.cdUp()) {
            break;
        }
    }
    
    // 创建logs文件夹（如果不存在）
    QString logDir = projectRoot + "/logs";
    QDir dir;
    if (!dir.exists(logDir)) {
        dir.mkpath(logDir);
    }
    
    m_logFilePath = logDir + "/app.log";
}

/**
 * @brief Logger析构函数
 * 
 * 确保在程序退出时正确关闭日志文件
 */
Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

/**
 * @brief 记录Debug级别日志（原子操作）
 */
void Logger::debug(const QString& category, const QString& message, const QVariantMap& context)
{
    log(LogLevel::Debug, category, message, context);
}

/**
 * @brief 记录Info级别日志（原子操作）
 */
void Logger::info(const QString& category, const QString& message, const QVariantMap& context)
{
    log(LogLevel::Info, category, message, context);
}

/**
 * @brief 记录Warning级别日志（原子操作）
 */
void Logger::warning(const QString& category, const QString& message, const QVariantMap& context)
{
    log(LogLevel::Warning, category, message, context);
}

/**
 * @brief 记录Error级别日志（原子操作）
 */
void Logger::error(const QString& category, const QString& message, const QVariantMap& context)
{
    log(LogLevel::Error, category, message, context);
}

/**
 * @brief 设置日志级别
 * 
 * 使用互斥锁保证线程安全
 * 只有大于等于设定级别的日志才会被记录
 */
void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);  // 自动加锁/解锁
    m_logLevel = level;
}

/**
 * @brief 启用或禁用文件日志
 * 
 * @param enabled true=启用, false=禁用
 * 
 * 启用时会尝试打开日志文件,如果失败会输出警告但不影响程序运行
 * 禁用时会关闭已打开的日志文件
 */
void Logger::setFileLoggingEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_fileLoggingEnabled = enabled;
    
    if (enabled && !m_logFile.isOpen()) {
        // 启用文件日志且文件未打开,尝试打开文件
        m_logFile.setFileName(m_logFilePath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "无法打开日志文件:" << m_logFilePath;
        } else {
            qInfo() << "日志文件已启用:" << m_logFilePath;
        }
    } else if (!enabled && m_logFile.isOpen()) {
        // 禁用文件日志且文件已打开,关闭文件
        m_logFile.close();
        qInfo() << "日志文件已禁用";
    }
}

/**
 * @brief 设置日志文件路径
 * 
 * @param path 新的日志文件路径
 * 
 * 如果当前文件已打开,会先关闭旧文件,然后设置新路径
 * 如果文件日志已启用,会自动打开新文件
 */
void Logger::setLogFilePath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    
    // 关闭当前打开的文件
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    
    // 更新日志文件路径
    m_logFilePath = path;
    
    // 如果文件日志已启用,打开新文件
    if (m_fileLoggingEnabled) {
        m_logFile.setFileName(m_logFilePath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "无法打开日志文件:" << m_logFilePath;
        } else {
            qInfo() << "日志文件路径已更改为:" << m_logFilePath;
        }
    }
}

/**
 * @brief 核心日志记录方法
 * 
 * @param level 日志级别
 * @param category 日志分类
 * @param message 日志消息
 * @param context 上下文信息(可选)
 * 
 * 工作流程:
 * 1. 检查日志级别,低于设定级别的直接返回
 * 2. 格式化日志消息 (包含时间戳、级别、分类、消息、上下文)
 * 3. 根据级别输出到控制台 (使用Qt的qDebug/qInfo/qWarning/qCritical)
 * 4. 如果启用了文件日志,写入文件
 * 5. 发射信号通知其他组件
 */
void Logger::log(LogLevel level, const QString& category, const QString& message, const QVariantMap& context)
{
    QMutexLocker locker(&m_mutex);  // 线程安全保护
    
    // 级别过滤: 如果当前消息级别低于设定的最低级别,直接返回
    if (level < m_logLevel) {
        return;
    }
    
    // ========================================
    // 格式化日志消息
    // ========================================
    // 时间戳格式: "2025-10-23 14:18:59.880"
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    
    // 日志级别字符串: "DEBUG", "INFO ", "WARN ", "ERROR"
    QString levelStr = levelToString(level);
    
    // 上下文信息格式化: "key1=value1, key2=value2"
    QString contextStr = formatContext(context);
    
    // 最终日志格式: [时间戳] [级别] [分类] 消息 | 上下文
    // 示例: [2025-10-23 14:18:59.880] [INFO ] [Application] Starting app | version=1.0
    QString logLine = QString("[%1] [%2] [%3] %4%5")
                          .arg(timestamp)
                          .arg(levelStr)
                          .arg(category)
                          .arg(message)
                          .arg(contextStr.isEmpty() ? "" : " | " + contextStr);
    
    // ========================================
    // 输出到控制台
    // ========================================
    // 根据不同级别使用不同的Qt日志函数
    switch (level) {
        case LogLevel::Debug:
            qDebug().noquote() << logLine;    // 调试信息 (可能在Release版本被禁用)
            break;
        case LogLevel::Info:
            qInfo().noquote() << logLine;     // 一般信息
            break;
        case LogLevel::Warning:
            qWarning().noquote() << logLine;  // 警告信息
            break;
        case LogLevel::Error:
            qCritical().noquote() << logLine; // 错误信息
            break;
    }
    
    // ========================================
    // 写入文件 (如果已启用)
    // ========================================
    if (m_fileLoggingEnabled) {
        writeToFile(logLine);
    }
    
    // ========================================
    // 发射信号 (供QML或其他C++组件监听)
    // ========================================
    emit logMessageEmitted(level, category, message);
}

/**
 * @brief 将日志写入文件
 * 
 * @param logLine 格式化好的日志行
 * 
 * 使用QTextStream进行文本写入,并立即刷新缓冲区
 * 确保日志及时写入磁盘,即使程序崩溃也能保留日志
 */
void Logger::writeToFile(const QString& logLine)
{
    if (m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << logLine << "\n";  // 写入日志行并添加换行符
        stream.flush();              // 立即刷新到文件,不等待缓冲区满
    }
}

/**
 * @brief 格式化上下文信息为字符串
 * 
 * @param context 上下文映射表
 * @return QString 格式化后的字符串
 * 
 * 将QVariantMap转换为 "key1=value1, key2=value2" 格式
 * 如果上下文为空,返回空字符串
 * 
 * 示例:
 *   输入: {{"userId", 123}, {"action", "login"}}
 *   输出: "userId=123, action=login"
 */
QString Logger::formatContext(const QVariantMap& context)
{
    if (context.isEmpty()) {
        return QString();
    }
    
    QStringList parts;
    for (auto it = context.constBegin(); it != context.constEnd(); ++it) {
        // 将每个键值对格式化为 "key=value"
        parts << QString("%1=%2").arg(it.key()).arg(it.value().toString());
    }
    
    // 用逗号和空格连接所有部分
    return parts.join(", ");
}

/**
 * @brief 将日志级别枚举转换为字符串
 * 
 * @param level 日志级别枚举
 * @return QString 日志级别字符串
 * 
 * 所有字符串长度固定为5个字符,用于对齐日志输出
 * 格式: "DEBUG", "INFO ", "WARN ", "ERROR"
 */
QString Logger::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";   // 调试
        case LogLevel::Info:    return "INFO ";   // 信息 (后面加空格对齐)
        case LogLevel::Warning: return "WARN ";   // 警告 (后面加空格对齐)
        case LogLevel::Error:   return "ERROR";   // 错误
        default:                return "UNKNOWN"; // 未知级别 (不应该出现)
    }
}

void Logger::logSeparator(const QString& category, const QString& text)
{
    const QString separator = "========================================";
    if (text.isEmpty()) {
        info(category, separator, {});
    } else {
        info(category, separator, {});
        info(category, text, {});
        info(category, separator, {});
    }
}

void Logger::logMultiple(const QString& category, const QStringList& messages, LogLevel level)
{
    for (const QString& msg : messages) {
        switch (level) {
            case LogLevel::Debug:   debug(category, msg, {}); break;
            case LogLevel::Info:    info(category, msg, {}); break;
            case LogLevel::Warning: warning(category, msg, {}); break;
            case LogLevel::Error:   error(category, msg, {}); break;
        }
    }
}

