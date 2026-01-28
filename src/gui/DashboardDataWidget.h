#ifndef DASHBOARDDATAWIDGET_H
#define DASHBOARDDATAWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QMap>
#include <QJsonObject>
#include "core/Channel.h"
#include "core/DataPoint.h"

namespace ModbusPlexLink {

/**
 * @brief 数据点卡片 - 单个数据点的可视化展示
 */
class DataPointCard : public QFrame {
    Q_OBJECT

public:
    explicit DataPointCard(const QString& tagName, const DataPoint& point, QWidget *parent = nullptr);

    void updateData(const DataPoint& point);
    void updateData(const QJsonObject& pointJson);  // 远程数据更新
    QString getTagName() const { return m_tagName; }
    
    // 设置远程标记
    void setRemoteMode(bool remote);
    
    // 设置离线状态
    void setOffline(bool offline);

private:
    void setupUi();
    QString formatValue(const QVariant& value) const;
    QColor getQualityColor(DataQuality quality) const;

    QString m_tagName;
    DataPoint m_dataPoint;
    bool m_isRemote;
    bool m_isOffline;

    QLabel* m_tagLabel;
    QLabel* m_valueLabel;
    QLabel* m_unitLabel;
    QLabel* m_qualityIndicator;
    QLabel* m_timestampLabel;
    QLabel* m_remoteIndicator;
    QLabel* m_offlineOverlay;  // 离线遮罩层
};

/**
 * @brief 仪表盘风格数据展示组件
 *
 * 功能：
 * - 卡片网格形式展示数据点
 * - 实时更新数据值
 * - 质量状态可视化
 * - 自动布局和滚动
 * - 支持本地和远程两种数据源
 */
class DashboardDataWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardDataWidget(QWidget *parent = nullptr);
    ~DashboardDataWidget();

    // 设置显示的通道（本地模式）
    void setChannel(Channel* channel);

    // 设置远程模式显示
    void setRemoteMode(bool remote, const QString& channelName = QString());
    
    // 获取当前远程通道名称
    QString getRemoteChannelName() const { return m_remoteChannelName; }
    
    // 检查是否处于远程模式
    bool isRemoteMode() const { return m_isRemoteMode; }
    
    // 更新远程数据
    void updateRemoteData(const QString& channelName, const QJsonObject& data);

    // 刷新显示
    void refreshDisplay();

    // 清空显示
    void clear();
    
    // 显示远程模式提示
    void showRemoteModeHint(const QString& hint);

private slots:
    void onDataUpdated(const QString& tagName, const DataPoint& point);
    void onAutoRefreshTimeout();

private:
    void setupUi();
    void rebuildCards();
    void addOrUpdateCard(const QString& tagName, const DataPoint& point);
    void addOrUpdateRemoteCard(const QString& tagName, const QJsonObject& pointJson);

    Channel* m_channel;
    bool m_isRemoteMode;
    QString m_remoteChannelName;

    // UI组件
    QWidget* m_headerWidget;       // 头部：显示通道名称和状态
    QLabel* m_channelNameLabel;    // 通道名称标签
    QLabel* m_channelStatusLabel;  // 通道状态标签（运行/离线）
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QWidget* m_gridContainer;      // 网格布局容器（用于居中）
    QGridLayout* m_gridLayout;
    QLabel* m_emptyLabel;

    // 数据卡片
    QMap<QString, DataPointCard*> m_cards;

    // 自动刷新
    QTimer* m_refreshTimer;
    
    // 通道状态
    bool m_channelOnline;          // 通道是否在线
    
    // 更新头部显示
    void updateHeader();
};

} // namespace ModbusPlexLink

#endif // DASHBOARDDATAWIDGET_H
