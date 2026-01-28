#ifndef ALARMRULEDIALOG_H
#define ALARMRULEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QListWidget>
#include "utils/AlarmManager.h"

namespace ModbusPlexLink {

class ChannelManager;

/**
 * @brief 报警规则编辑对话框
 *
 * 功能：
 * - 创建新的报警规则
 * - 编辑现有报警规则
 * - 验证规则参数
 * - 支持所有报警类型配置
 * - 配置告警自动录波
 */
class AlarmRuleDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param channelManager 通道管理器（用于获取通道和标签列表）
     * @param rule 要编辑的规则（nullptr表示创建新规则）
     * @param parent 父窗口
     */
    explicit AlarmRuleDialog(ChannelManager* channelManager,
                            const AlarmRule* rule = nullptr,
                            QWidget *parent = nullptr);
    ~AlarmRuleDialog();

    /**
     * @brief 获取编辑后的规则
     * @return 报警规则
     */
    AlarmRule getRule() const;

private slots:
    void onAlarmTypeChanged(int index);
    void onChannelChanged(int index);
    void onOkClicked();
    void onCancelClicked();
    void onRecordingEnabledChanged(bool enabled);
    void onAddRecordTag();
    void onRemoveRecordTag();
    void onBrowseSaveDir();

private:
    void setupUi();
    void setupRecordingPanel();
    void loadChannelsAndTags();
    void updateLimitFields();
    bool validateInput();
    void loadRule(const AlarmRule& rule);

private:
    ChannelManager* m_channelManager;
    bool m_isEdit;  // 是否为编辑模式
    QString m_originalId;  // 原始规则ID（编辑模式）

    // UI组件 - 基本信息
    QLineEdit* m_nameEdit;
    QCheckBox* m_enabledCheck;
    QComboBox* m_priorityCombo;
    QComboBox* m_typeCombo;

    // UI组件 - 数据源
    QComboBox* m_channelCombo;
    QComboBox* m_tagCombo;

    // UI组件 - 条件配置
    QDoubleSpinBox* m_highLimitSpin;
    QDoubleSpinBox* m_lowLimitSpin;
    QDoubleSpinBox* m_highHighLimitSpin;
    QDoubleSpinBox* m_lowLowLimitSpin;
    QSpinBox* m_delaySpin;
    QDoubleSpinBox* m_deadbandSpin;

    // UI组件 - 消息模板
    QTextEdit* m_messageEdit;
    
    // ==================== 录波配置 ====================
    QGroupBox* m_recordingGroup;
    QCheckBox* m_recordingEnabledCheck;
    QDoubleSpinBox* m_preTriggerSpin;      // 预触发时间（秒）
    QDoubleSpinBox* m_postTriggerSpin;     // 后触发时间（秒）
    QSpinBox* m_sampleIntervalSpin;        // 采样间隔（毫秒）
    QListWidget* m_recordTagsList;         // 录波标签列表
    QComboBox* m_availableTagsCombo;       // 可选标签下拉框
    QPushButton* m_addTagBtn;
    QPushButton* m_removeTagBtn;
    QLineEdit* m_saveDirEdit;              // 保存目录
    QPushButton* m_browseDirBtn;
    QCheckBox* m_autoExportCheck;          // 自动导出CSV
    QCheckBox* m_keepInMemoryCheck;        // 保留在内存中

    // 按钮
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
};

} // namespace ModbusPlexLink

#endif // ALARMRULEDIALOG_H
