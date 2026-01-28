#include "LogViewerWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QScrollBar>
#include <QDebug>

namespace ModbusPlexLink {

// 静态成员初始化
QList<LogViewerWidget*> LogViewerWidget::s_instances;
QMutex LogViewerWidget::s_instancesMutex;
QtMessageHandler LogViewerWidget::s_oldHandler = nullptr;

LogViewerWidget::LogViewerWidget(QWidget *parent)
    : QWidget(parent)
    , m_logDisplay(nullptr)
    , m_levelFilter(nullptr)
    , m_searchBox(nullptr)
    , m_findNextBtn(nullptr)
    , m_findPrevBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_exportBtn(nullptr)
    , m_autoScrollCheck(nullptr)
    , m_pauseCheck(nullptr)
    , m_minLevel(LogLevel::Debug)
    , m_autoScroll(true)
    , m_paused(false)
    , m_maxLogCount(10000)
    , m_debugCount(0)
    , m_infoCount(0)
    , m_warningCount(0)
    , m_criticalCount(0)
    , m_fatalCount(0)
{
    setupUi();

    // 注册实例以接收日志广播
    registerInstance(this);
}

LogViewerWidget::~LogViewerWidget() {
    unregisterInstance(this);
}

void LogViewerWidget::registerInstance(LogViewerWidget* instance) {
    QMutexLocker locker(&s_instancesMutex);
    if (instance && !s_instances.contains(instance)) {
        s_instances.append(instance);
    }
}

void LogViewerWidget::unregisterInstance(LogViewerWidget* instance) {
    QMutexLocker locker(&s_instancesMutex);
    s_instances.removeAll(instance);
}

void LogViewerWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 标题
    QLabel* titleLabel = new QLabel(tr("日志查看器"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // 工具栏
    QHBoxLayout* toolLayout = new QHBoxLayout();

    // 日志级别过滤
    toolLayout->addWidget(new QLabel(tr("级别:"), this));
    m_levelFilter = new QComboBox(this);
    m_levelFilter->addItems({
        tr("全部 (Debug+)"),
        tr("信息 (Info+)"),
        tr("警告 (Warning+)"),
        tr("错误 (Critical+)"),
        tr("致命 (Fatal)")
    });
    m_levelFilter->setCurrentIndex(0);
    connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogViewerWidget::onLevelFilterChanged);
    toolLayout->addWidget(m_levelFilter);

    toolLayout->addSpacing(20);

    // 搜索
    toolLayout->addWidget(new QLabel(tr("搜索:"), this));
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("输入关键字搜索..."));
    m_searchBox->setMinimumWidth(200);
    connect(m_searchBox, &QLineEdit::textChanged, this, &LogViewerWidget::onSearchTextChanged);
    toolLayout->addWidget(m_searchBox);

    m_findNextBtn = new QPushButton(tr("下一个"), this);
    connect(m_findNextBtn, &QPushButton::clicked, this, &LogViewerWidget::onFindNext);
    toolLayout->addWidget(m_findNextBtn);

    m_findPrevBtn = new QPushButton(tr("上一个"), this);
    connect(m_findPrevBtn, &QPushButton::clicked, this, &LogViewerWidget::onFindPrevious);
    toolLayout->addWidget(m_findPrevBtn);

    toolLayout->addSpacing(20);

    // 控制选项
    m_autoScrollCheck = new QCheckBox(tr("自动滚动"), this);
    m_autoScrollCheck->setChecked(true);
    connect(m_autoScrollCheck, &QCheckBox::toggled, this, &LogViewerWidget::onAutoScrollToggled);
    toolLayout->addWidget(m_autoScrollCheck);

    m_pauseCheck = new QCheckBox(tr("暂停"), this);
    m_pauseCheck->setChecked(false);
    connect(m_pauseCheck, &QCheckBox::toggled, this, &LogViewerWidget::onPauseToggled);
    toolLayout->addWidget(m_pauseCheck);

    toolLayout->addSpacing(20);

    // 操作按钮
    m_clearBtn = new QPushButton(tr("清除日志"), this);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogViewerWidget::onClear);
    toolLayout->addWidget(m_clearBtn);

    m_exportBtn = new QPushButton(tr("导出"), this);
    connect(m_exportBtn, &QPushButton::clicked, this, &LogViewerWidget::onExportToFile);
    toolLayout->addWidget(m_exportBtn);

    toolLayout->addStretch();
    mainLayout->addLayout(toolLayout);

    // 日志显示区域
    m_logDisplay = new QTextBrowser(this);
    m_logDisplay->setOpenExternalLinks(false);
    m_logDisplay->setReadOnly(true);

    // 设置等宽字体
    QFont monoFont("Courier New", 9);
    m_logDisplay->setFont(monoFont);

    mainLayout->addWidget(m_logDisplay);

    // 统计信息
    QLabel* statsLabel = new QLabel(this);
    statsLabel->setObjectName("statsLabel");
    statsLabel->setText(tr("总计: 0 | Debug: 0 | Info: 0 | Warning: 0 | Critical: 0 | Fatal: 0"));
    mainLayout->addWidget(statsLabel);
}

void LogViewerWidget::addLog(const LogEntry& entry) {
    QMutexLocker locker(&m_mutex);

    // 如果暂停，不添加新日志
    if (m_paused) return;

    // 更新统计
    switch (entry.level) {
        case LogLevel::Debug: m_debugCount++; break;
        case LogLevel::Info: m_infoCount++; break;
        case LogLevel::Warning: m_warningCount++; break;
        case LogLevel::Critical: m_criticalCount++; break;
        case LogLevel::Fatal: m_fatalCount++; break;
    }

    // 添加到列表
    m_logEntries.append(entry);

    // 限制日志数量
    if (m_logEntries.size() > m_maxLogCount) {
        LogEntry removed = m_logEntries.takeFirst();
        // 更新统计（减少）
        switch (removed.level) {
            case LogLevel::Debug: m_debugCount--; break;
            case LogLevel::Info: m_infoCount--; break;
            case LogLevel::Warning: m_warningCount--; break;
            case LogLevel::Critical: m_criticalCount--; break;
            case LogLevel::Fatal: m_fatalCount--; break;
        }
    }

    // 更新显示（在主线程中）
    QMetaObject::invokeMethod(this, [this, entry]() {
        if (matchesFilter(entry)) {
            appendLogToDisplay(entry);
        }

        // 更新统计标签
        QLabel* statsLabel = findChild<QLabel*>("statsLabel");
        if (statsLabel) {
            statsLabel->setText(tr("总计: %1 | Debug: %2 | Info: %3 | Warning: %4 | Critical: %5 | Fatal: %6")
                .arg(m_logEntries.size())
                .arg(m_debugCount)
                .arg(m_infoCount)
                .arg(m_warningCount)
                .arg(m_criticalCount)
                .arg(m_fatalCount));
        }
    }, Qt::QueuedConnection);
}

void LogViewerWidget::addLog(LogLevel level, const QString& message, const QString& category) {
    LogEntry entry(QDateTime::currentDateTime(), level, category, message);
    addLog(entry);
}

void LogViewerWidget::clearLogs() {
    QMutexLocker locker(&m_mutex);
    m_logEntries.clear();
    m_logDisplay->clear();

    m_debugCount = 0;
    m_infoCount = 0;
    m_warningCount = 0;
    m_criticalCount = 0;
    m_fatalCount = 0;

    QLabel* statsLabel = findChild<QLabel*>("statsLabel");
    if (statsLabel) {
        statsLabel->setText(tr("总计: 0 | Debug: 0 | Info: 0 | Warning: 0 | Critical: 0 | Fatal: 0"));
    }
}

void LogViewerWidget::setMaxLogCount(int count) {
    m_maxLogCount = count;
}

int LogViewerWidget::getTotalLogCount() const {
    QMutexLocker locker(&m_mutex);
    return m_logEntries.size();
}

int LogViewerWidget::getLogCount(LogLevel level) const {
    QMutexLocker locker(&m_mutex);
    switch (level) {
        case LogLevel::Debug: return m_debugCount;
        case LogLevel::Info: return m_infoCount;
        case LogLevel::Warning: return m_warningCount;
        case LogLevel::Critical: return m_criticalCount;
        case LogLevel::Fatal: return m_fatalCount;
        default: return 0;
    }
}

void LogViewerWidget::onLevelFilterChanged(int index) {
    m_minLevel = static_cast<LogLevel>(index);
    updateDisplay();
}

void LogViewerWidget::onSearchTextChanged(const QString& text) {
    m_searchText = text;
    // 可选：自动高亮搜索结果
}

void LogViewerWidget::onFindNext() {
    if (m_searchText.isEmpty()) return;
    m_logDisplay->find(m_searchText);
}

void LogViewerWidget::onFindPrevious() {
    if (m_searchText.isEmpty()) return;
    m_logDisplay->find(m_searchText, QTextDocument::FindBackward);
}

void LogViewerWidget::onClear() {
    auto reply = QMessageBox::question(this, tr("清除日志"),
        tr("确定要清除所有日志吗？"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        clearLogs();
    }
}

void LogViewerWidget::onExportToFile() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出日志"),
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_log.txt",
        tr("文本文件 (*.txt);;HTML文件 (*.html)"));

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法创建文件: %1").arg(fileName));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);  // Qt6 方式设置编码

    QMutexLocker locker(&m_mutex);

    bool isHtml = fileName.endsWith(".html", Qt::CaseInsensitive);

    if (isHtml) {
        out << "<!DOCTYPE html>\n<html>\n<head>\n";
        out << "<meta charset=\"UTF-8\">\n";
        out << "<title>Modbus PlexLink 日志</title>\n";
        out << "<style>\n";
        out << "body { font-family: 'Courier New', monospace; font-size: 12px; }\n";
        out << ".debug { color: gray; }\n";
        out << ".info { color: blue; }\n";
        out << ".warning { color: orange; font-weight: bold; }\n";
        out << ".critical { color: red; font-weight: bold; }\n";
        out << ".fatal { color: darkred; font-weight: bold; background: pink; }\n";
        out << "</style>\n";
        out << "</head>\n<body>\n";
        out << "<h1>Modbus PlexLink 日志</h1>\n";
        out << "<p>导出时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "</p>\n";
        out << "<hr>\n";
    }

    for (const LogEntry& entry : m_logEntries) {
        out << formatLogEntry(entry, isHtml);
        if (!isHtml) {
            out << "\n";
        }
    }

    if (isHtml) {
        out << "</body>\n</html>\n";
    }

    file.close();

    QMessageBox::information(this, tr("导出成功"),
        tr("已导出 %1 条日志到:\n%2").arg(m_logEntries.size()).arg(fileName));
}

void LogViewerWidget::onAutoScrollToggled(bool checked) {
    m_autoScroll = checked;
}

void LogViewerWidget::onPauseToggled(bool checked) {
    m_paused = checked;
    if (checked) {
        m_logDisplay->append(tr("<span style='color:blue;font-weight:bold;'>[日志已暂停]</span>"));
    } else {
        m_logDisplay->append(tr("<span style='color:green;font-weight:bold;'>[日志已恢复]</span>"));
    }
}

void LogViewerWidget::updateDisplay() {
    m_logDisplay->clear();

    QMutexLocker locker(&m_mutex);
    for (const LogEntry& entry : m_logEntries) {
        if (matchesFilter(entry)) {
            appendLogToDisplay(entry);
        }
    }
}

void LogViewerWidget::appendLogToDisplay(const LogEntry& entry) {
    QString html = formatLogEntry(entry, true);
    m_logDisplay->append(html);

    // 自动滚动到底部
    if (m_autoScroll) {
        QScrollBar* scrollBar = m_logDisplay->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

QString LogViewerWidget::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug: return tr("DEBUG");
        case LogLevel::Info: return tr("INFO");
        case LogLevel::Warning: return tr("WARN");
        case LogLevel::Critical: return tr("ERROR");
        case LogLevel::Fatal: return tr("FATAL");
        default: return tr("UNKNOWN");
    }
}

QString LogViewerWidget::logLevelToColor(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug: return "gray";
        case LogLevel::Info: return "blue";
        case LogLevel::Warning: return "orange";
        case LogLevel::Critical: return "red";
        case LogLevel::Fatal: return "darkred";
        default: return "black";
    }
}

QString LogViewerWidget::formatLogEntry(const LogEntry& entry, bool html) const {
    QString timeStr = entry.timestamp.toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString levelStr = logLevelToString(entry.level);
    QString category = entry.category.isEmpty() ? "General" : entry.category;

    if (html) {
        QString color = logLevelToColor(entry.level);
        QString style = QString("color:%1;").arg(color);
        if (entry.level >= LogLevel::Warning) {
            style += "font-weight:bold;";
        }
        if (entry.level == LogLevel::Fatal) {
            style += "background:pink;";
        }

        return QString("<span style='%1'>[%2] [%3] [%4] %5</span>")
            .arg(style)
            .arg(timeStr)
            .arg(levelStr.leftJustified(5))
            .arg(category)
            .arg(entry.message);
    } else {
        return QString("[%1] [%2] [%3] %4")
            .arg(timeStr)
            .arg(levelStr.leftJustified(5))
            .arg(category)
            .arg(entry.message);
    }
}

bool LogViewerWidget::matchesFilter(const LogEntry& entry) const {
    // 级别过滤
    if (entry.level < m_minLevel) {
        return false;
    }

    // 文本搜索（可选）
    // if (!m_searchText.isEmpty()) {
    //     if (!entry.message.contains(m_searchText, Qt::CaseInsensitive)) {
    //         return false;
    //     }
    // }

    return true;
}

// Qt消息处理器
void LogViewerWidget::installMessageHandler() {
    s_oldHandler = qInstallMessageHandler(qtMessageHandler);
}

void LogViewerWidget::uninstallMessageHandler() {
    if (s_oldHandler) {
        qInstallMessageHandler(s_oldHandler);
        s_oldHandler = nullptr;
    }
}

void LogViewerWidget::qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // 调用旧的处理器（保持控制台输出）
    if (s_oldHandler) {
        s_oldHandler(type, context, msg);
    }

    // 广播日志到所有注册的实例
    QMutexLocker locker(&s_instancesMutex);
    if (!s_instances.isEmpty()) {
        LogLevel level;
        switch (type) {
            case QtDebugMsg: level = LogLevel::Debug; break;
            case QtInfoMsg: level = LogLevel::Info; break;
            case QtWarningMsg: level = LogLevel::Warning; break;
            case QtCriticalMsg: level = LogLevel::Critical; break;
            case QtFatalMsg: level = LogLevel::Fatal; break;
            default: level = LogLevel::Info; break;
        }

        QString category = context.category ? QString::fromLatin1(context.category) : QString();
        QString file = context.file ? QString::fromLatin1(context.file) : QString();

        LogEntry entry(
            QDateTime::currentDateTime(),
            level,
            category,
            msg,
            file,
            context.line
        );

        // 广播到所有实例
        for (LogViewerWidget* instance : s_instances) {
            if (instance) {
                instance->addLog(entry);
            }
        }
    }
}

} // namespace ModbusPlexLink
