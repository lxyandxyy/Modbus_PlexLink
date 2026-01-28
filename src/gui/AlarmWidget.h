#ifndef ALARMWIDGET_H
#define ALARMWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include "utils/AlarmManager.h"

namespace ModbusPlexLink {

class ChannelManager;

/**
 * @brief 报警管理界面
 *
 * 功能：
 * - 活动报警列表
 * - 报警历史查询
 * - 报警规则管理
 * - 报警确认与清除
 * - 报警统计显示
 * - 报警录波管理（回放跳转到系统录波界面）
 */
class AlarmWidget : public QWidget {
    Q_OBJECT

public:
    explicit AlarmWidget(AlarmManager* alarmManager, ChannelManager* channelManager, QWidget *parent = nullptr);
    ~AlarmWidget();

    // 刷新显示
    void refreshDisplay();

signals:
    /**
     * @brief 请求在系统录波界面中回放数据
     * @param csvFilePath CSV文件路径（如果有的话）
     * @param recordingData 录波数据（如果内存中有的话）
     */
    void requestPlaybackInRecorder(const QString& csvFilePath, const AlarmRecordingData& recordingData);

private slots:
    // 报警信号
    void onAlarmTriggered(const AlarmEvent& event);
    void onAlarmAcknowledged(const QString& eventId);
    void onAlarmCleared(const QString& eventId);

    // 操作
    void onAcknowledgeAlarm();
    void onClearAlarm();
    void onAcknowledgeAll();
    void onClearAll();

    // 规则管理
    void onAddRule();
    void onEditRule();
    void onDeleteRule();
    void onToggleRule();

    // 过滤
    void onPriorityFilterChanged(int index);
    void onRefreshHistory();
    
    // 录波相关
    void onRecordingStarted(const QString& alarmEventId);
    void onRecordingCompleted(const QString& alarmEventId, const QString& csvFilePath);
    void onViewRecording();
    void onExportRecording();
    void onPlaybackRecording();
    void onDeleteRecording();

private:
    void setupUi();
    void setupRecordingTab();
    void updateActiveAlarmsTable();
    void updateHistoryTable();
    void updateRulesTable();
    void updateStatistics();
    void updateRecordingsTable();

    QString alarmTypeToString(AlarmType type) const;
    QString alarmPriorityToString(AlarmPriority priority) const;
    QString alarmStateToString(AlarmState state) const;
    QColor alarmPriorityColor(AlarmPriority priority) const;

private:
    AlarmManager* m_alarmManager;
    ChannelManager* m_channelManager;

    // UI组件
    QTabWidget* m_tabWidget;

    // 活动报警
    QTableWidget* m_activeAlarmsTable;
    QPushButton* m_acknowledgeBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_acknowledgeAllBtn;
    QPushButton* m_clearAllBtn;
    QLabel* m_statsLabel;

    // 报警历史
    QTableWidget* m_historyTable;
    QPushButton* m_refreshHistoryBtn;
    QComboBox* m_historyDaysCombo;

    // 规则管理
    QTableWidget* m_rulesTable;
    QPushButton* m_addRuleBtn;
    QPushButton* m_editRuleBtn;
    QPushButton* m_deleteRuleBtn;
    QPushButton* m_toggleRuleBtn;
    QComboBox* m_priorityFilter;
    
    // ==================== 录波相关 ====================
    
    // 录波数据列表
    QTableWidget* m_recordingsTable;
    QPushButton* m_viewRecordingBtn;
    QPushButton* m_exportRecordingBtn;
    QPushButton* m_playbackRecordingBtn;
    QPushButton* m_deleteRecordingBtn;
    QLabel* m_recordingInfoLabel;
};

} // namespace ModbusPlexLink

#endif // ALARMWIDGET_H
