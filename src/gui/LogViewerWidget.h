#ifndef LOGVIEWERWIDGET_H
#define LOGVIEWERWIDGET_H

#include <QWidget>
#include <QTextBrowser>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QDateTime>
#include <QMutex>
#include <QList>
#include <QtMessageHandler>

namespace ModbusPlexLink {

/**
 * @brief 日志等级枚举
 */
enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Critical = 3,
    Fatal = 4
};

/**
 * @brief 日志条目结构
 */
struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString category;
    QString message;
    QString file;
    int line;

    LogEntry()
        : level(LogLevel::Info), line(0) {}

    LogEntry(const QDateTime& ts, LogLevel lvl, const QString& cat,
             const QString& msg, const QString& f = QString(), int l = 0)
        : timestamp(ts), level(lvl), category(cat), message(msg), file(f), line(l) {}
};

/**
 * @brief 日志查看器
 *
 * 功能：
 * - 实时显示应用程序日志
 * - 多级别日志过滤（Debug/Info/Warning/Error/Fatal）
 * - 日志搜索与高亮
 * - 日志导出（TXT/HTML）
 * - 自动滚动控制
 * - 日志清除
 */
class LogViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit LogViewerWidget(QWidget *parent = nullptr);
    ~LogViewerWidget();

    // 添加日志条目
    void addLog(const LogEntry& entry);
    void addLog(LogLevel level, const QString& message, const QString& category = QString());

    // 清除日志
    void clearLogs();

    // 设置最大日志条数
    void setMaxLogCount(int count);

    // 获取日志统计
    int getTotalLogCount() const;
    int getLogCount(LogLevel level) const;

    // 安装Qt消息处理器
    static void installMessageHandler();
    static void uninstallMessageHandler();

public slots:
    // 日志级别过滤
    void onLevelFilterChanged(int index);

    // 搜索
    void onSearchTextChanged(const QString& text);
    void onFindNext();
    void onFindPrevious();

    // 操作
    void onClear();
    void onExportToFile();
    void onAutoScrollToggled(bool checked);
    void onPauseToggled(bool checked);

private:
    void setupUi();
    void updateDisplay();
    void appendLogToDisplay(const LogEntry& entry);
    QString logLevelToString(LogLevel level) const;
    QString logLevelToColor(LogLevel level) const;
    QString formatLogEntry(const LogEntry& entry, bool html = true) const;
    bool matchesFilter(const LogEntry& entry) const;

    // Qt消息处理
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    
    // 注册/注销实例以接收日志广播
    static void registerInstance(LogViewerWidget* instance);
    static void unregisterInstance(LogViewerWidget* instance);

private:
    // UI组件
    QTextBrowser* m_logDisplay;
    QComboBox* m_levelFilter;
    QLineEdit* m_searchBox;
    QPushButton* m_findNextBtn;
    QPushButton* m_findPrevBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_exportBtn;
    QCheckBox* m_autoScrollCheck;
    QCheckBox* m_pauseCheck;

    // 日志数据
    QList<LogEntry> m_logEntries;
    mutable QMutex m_mutex;  // 线程安全

    // 过滤与搜索
    LogLevel m_minLevel;      // 最小显示级别
    QString m_searchText;
    bool m_autoScroll;
    bool m_paused;
    int m_maxLogCount;        // 最大日志条数

    // 统计
    int m_debugCount;
    int m_infoCount;
    int m_warningCount;
    int m_criticalCount;
    int m_fatalCount;

    // 静态实例列表（支持多实例同时接收日志）
    static QList<LogViewerWidget*> s_instances;
    static QMutex s_instancesMutex;
    static QtMessageHandler s_oldHandler;
};

} // namespace ModbusPlexLink

#endif // LOGVIEWERWIDGET_H
