#ifndef CHANNELCARDWIDGET_H
#define CHANNELCARDWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include "core/Channel.h"

namespace ModbusPlexLink {

/**
 * @brief 通道卡片组件 - 现代化卡片式显示
 *
 * 功能：
 * - 美观的卡片式布局
 * - 显示通道名称、状态、连接信息
 * - 状态指示灯动画
 * - 悬停效果
 * - 点击选中效果
 */
class ChannelCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit ChannelCardWidget(Channel* channel, QWidget *parent = nullptr);
    ~ChannelCardWidget();

    // 获取关联的通道
    Channel* getChannel() const { return m_channel; }

    // 更新显示
    void updateDisplay();

    // 选中状态
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

signals:
    void clicked(Channel* channel);
    void startRequested(Channel* channel);
    void stopRequested(Channel* channel);
    void editRequested(Channel* channel);
    void deleteRequested(Channel* channel);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onChannelStateChanged(ChannelState state);
    void onStartStopClicked();
    void onEditClicked();
    void onDeleteClicked();
    void updateIndicatorAnimation();

private:
    void setupUi();
    void applyStyles();
    QString getChannelStateText(ChannelState state) const;
    QColor getChannelStateColor(ChannelState state) const;
    QString getConnectionInfo() const;

    Channel* m_channel;
    bool m_selected;
    bool m_hovered;

    // UI组件
    QLabel* m_nameLabel;
    QLabel* m_statusIndicator;
    QLabel* m_statusLabel;
    QLabel* m_connectionLabel;
    QLabel* m_collectorCountLabel;
    QLabel* m_serverCountLabel;
    QPushButton* m_startStopBtn;
    QPushButton* m_editBtn;
    QPushButton* m_deleteBtn;

    // 动画
    QTimer* m_animationTimer;
    int m_animationPhase;
};

} // namespace ModbusPlexLink

#endif // CHANNELCARDWIDGET_H
