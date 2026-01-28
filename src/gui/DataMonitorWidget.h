#ifndef DATAMONITORWIDGET_H
#define DATAMONITORWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QMap>
#include <QLabel>
#include "core/ChannelManager.h"
#include "core/DataPoint.h"

namespace ModbusPlexLink {

/**
 * @brief 实时数据监控面板
 *
 * 功能：
 * - 显示所有数据点的实时值
 * - 数据点搜索与过滤
 * - 数据质量显示
 * - 时间戳显示
 * - 支持单通道或全部通道监控
 */
class DataMonitorWidget : public QWidget {
    Q_OBJECT

public:
    explicit DataMonitorWidget(ChannelManager* channelManager, QWidget *parent = nullptr);
    ~DataMonitorWidget();

    // 设置监控的通道（nullptr表示监控所有通道）
    void setChannel(Channel* channel);

    // 刷新显示
    void refreshDisplay();

private slots:
    // 搜索相关
    void onSearchTextChanged(const QString& text);
    void onChannelFilterChanged(int index);
    void onClearSearch();

    // 数据更新
    void onDataUpdated(const QString& tagName, const DataPoint& point);
    void onAutoRefreshToggled(bool checked);
    void onRefreshIntervalChanged(int index);
    void onManualRefresh();

    // 表格操作
    void onTableContextMenu(const QPoint& pos);
    void onCopySelectedValue();
    void onExportToCSV();

private:
    void setupUi();
    void updateDataTable();
    void addOrUpdateRow(const QString& channelName, const QString& tagName, const DataPoint& point);
    QString qualityToString(DataQuality quality) const;
    QColor qualityColor(DataQuality quality) const;
    QString formatTimestamp(qint64 timestamp) const;
    bool matchesFilter(const QString& channelName, const QString& tagName) const;

private:
    ChannelManager* m_channelManager;
    Channel* m_currentChannel;  // 当前监控的通道（nullptr = 全部）

    // UI组件
    QComboBox* m_channelFilter;
    QLineEdit* m_searchBox;
    QPushButton* m_clearSearchBtn;
    QTableWidget* m_dataTable;
    QPushButton* m_refreshBtn;
    QPushButton* m_exportBtn;
    QComboBox* m_refreshIntervalCombo;
    QLabel* m_statusLabel;

    // 自动刷新
    QTimer* m_refreshTimer;
    bool m_autoRefresh;
    bool m_channelFilterLocked;

    // 数据缓存（用于搜索和过滤）
    struct DataEntry {
        QString channelName;
        QString tagName;
        DataPoint point;
    };
    QMap<QString, DataEntry> m_dataCache;  // key: channelName:tagName

    // 搜索过滤
    QString m_searchText;
    QString m_selectedChannelFilter;  // "全部" 或具体通道名
};

} // namespace ModbusPlexLink

#endif // DATAMONITORWIDGET_H
