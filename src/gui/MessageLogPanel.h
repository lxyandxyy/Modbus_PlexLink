#ifndef MESSAGELOGPANEL_H
#define MESSAGELOGPANEL_H

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

#include "LogViewerWidget.h"
#include "core/Channel.h"

namespace ModbusPlexLink {

/**
 * @brief Modbus报文查看器
 */
class ModbusMessageViewer : public QWidget {
  Q_OBJECT

 public:
  explicit ModbusMessageViewer(QWidget* parent = nullptr);

  // 添加报文
  void addMessage(const QString& direction, const QString& device,
                  const QString& function, const QString& address,
                  const QString& data, bool success = true);
  
  // 添加状态消息（通道上线/下线等）
  void addStatusMessage(const QString& message, const QString& color = "#3B82F6");

  // 清除报文
  void clearMessages();

  // 设置过滤通道
  void setChannel(Channel* channel);

  // 设置远程模式（切换时自动清除旧报文）
  void setRemoteMode(bool remote, const QString& channelName = QString());
  bool isRemoteMode() const { return m_isRemoteMode; }

 private slots:
  void onClearClicked();
  void onExportClicked();
  void onPauseToggled(bool paused);

 private:
  void setupUi();
  void updateModeLabel();
  QString formatMessage(const QString& direction, const QString& device,
                        const QString& function, const QString& address,
                        const QString& data, bool success);
  QString parseHexData(const QString& hexData);

  Channel* m_channel;
  QTextBrowser* m_messageDisplay;
  QPushButton* m_clearBtn;
  QPushButton* m_exportBtn;
  QCheckBox* m_pauseCheck;
  QCheckBox* m_detailCheck;  // 详细解析开关
  QLabel* m_titleLabel;      // 标题标签（显示通道名称）
  QLabel* m_modeLabel;       // 模式指示标签
  bool m_paused;
  bool m_showDetail;
  int m_messageCount;
  bool m_isRemoteMode;
  QString m_remoteChannelName;
};

/**
 * @brief 报文与日志面板 - 侧边切换式布局
 *
 * 功能：
 * - 左侧切换按钮（报文/日志）
 * - 右侧内容区域
 * - 自动筛选当前通道
 */
class MessageLogPanel : public QWidget {
  Q_OBJECT

 public:
  explicit MessageLogPanel(QWidget* parent = nullptr);
  ~MessageLogPanel();

  // 设置当前通道
  void setChannel(Channel* channel);

  // 设置远程模式
  void setRemoteMode(bool remote, const QString& channelName = QString());
  bool isRemoteMode() const { return m_isRemoteMode; }

  // 获取子组件
  ModbusMessageViewer* getMessageViewer() { return m_messageViewer; }
  LogViewerWidget* getLogViewer() { return m_logViewer; }

 private slots:
  void onMessageModeClicked();
  void onLogModeClicked();

 private:
  void setupUi();
  void setActiveButton(QPushButton* button);

  Channel* m_channel;
  bool m_isRemoteMode;
  QString m_remoteChannelName;

  // UI组件
  QPushButton* m_messageBtn;
  QPushButton* m_logBtn;
  QStackedWidget* m_stackedWidget;

  ModbusMessageViewer* m_messageViewer;
  LogViewerWidget* m_logViewer;

  QButtonGroup* m_buttonGroup;
};

}  // namespace ModbusPlexLink

#endif  // MESSAGELOGPANEL_H
