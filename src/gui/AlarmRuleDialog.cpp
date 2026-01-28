#include "AlarmRuleDialog.h"
#include "core/ChannelManager.h"
#include "core/Channel.h"
#include "core/UniversalDataModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QUuid>
#include <QDebug>
#include <QFileDialog>
#include <QScrollArea>

namespace ModbusPlexLink {

AlarmRuleDialog::AlarmRuleDialog(ChannelManager* channelManager,
                                 const AlarmRule* rule,
                                 QWidget *parent)
    : QDialog(parent)
    , m_channelManager(channelManager)
    , m_isEdit(rule != nullptr)
    , m_originalId(rule ? rule->id : QString())
    , m_nameEdit(nullptr)
    , m_enabledCheck(nullptr)
    , m_priorityCombo(nullptr)
    , m_typeCombo(nullptr)
    , m_channelCombo(nullptr)
    , m_tagCombo(nullptr)
    , m_highLimitSpin(nullptr)
    , m_lowLimitSpin(nullptr)
    , m_highHighLimitSpin(nullptr)
    , m_lowLowLimitSpin(nullptr)
    , m_delaySpin(nullptr)
    , m_deadbandSpin(nullptr)
    , m_messageEdit(nullptr)
    , m_recordingGroup(nullptr)
    , m_recordingEnabledCheck(nullptr)
    , m_preTriggerSpin(nullptr)
    , m_postTriggerSpin(nullptr)
    , m_sampleIntervalSpin(nullptr)
    , m_recordTagsList(nullptr)
    , m_availableTagsCombo(nullptr)
    , m_addTagBtn(nullptr)
    , m_removeTagBtn(nullptr)
    , m_saveDirEdit(nullptr)
    , m_browseDirBtn(nullptr)
    , m_autoExportCheck(nullptr)
    , m_keepInMemoryCheck(nullptr)
    , m_okBtn(nullptr)
    , m_cancelBtn(nullptr)
{
    setWindowTitle(m_isEdit ? tr("编辑报警规则") : tr("新建报警规则"));
    resize(520, 620);  // 更紧凑的尺寸
    setModal(true);

    setupUi();
    loadChannelsAndTags();

    if (rule) {
        loadRule(*rule);
    } else {
        // 默认设置
        m_enabledCheck->setChecked(true);
        m_priorityCombo->setCurrentIndex(1);  // Medium
        m_typeCombo->setCurrentIndex(0);      // HighLimit
        m_delaySpin->setValue(5);
        m_deadbandSpin->setValue(0.0);
        m_messageEdit->setPlainText(tr("${tag} 触发 ${type} 报警，当前值: ${value}"));
        
        // 录波默认设置
        m_recordingGroup->setChecked(false);
        m_recordingEnabledCheck->setChecked(false);
        m_preTriggerSpin->setValue(60.0);
        m_postTriggerSpin->setValue(30.0);
        m_sampleIntervalSpin->setValue(100);
        m_autoExportCheck->setChecked(true);
        m_keepInMemoryCheck->setChecked(true);
    }

    updateLimitFields();
}

AlarmRuleDialog::~AlarmRuleDialog() {
}

void AlarmRuleDialog::setupUi() {
    // 应用现代化样式
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #E2E8F0;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 8px;
            background: #FAFBFC;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #334155;
        }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            padding: 4px 8px;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            min-height: 24px;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #3B82F6;
        }
        QPushButton {
            padding: 6px 16px;
            border-radius: 4px;
            font-weight: 500;
        }
        QPushButton#okBtn {
            background: #3B82F6;
            color: white;
            border: none;
        }
        QPushButton#okBtn:hover { background: #2563EB; }
        QPushButton#cancelBtn {
            background: #F1F5F9;
            border: 1px solid #CBD5E1;
        }
        QPushButton#cancelBtn:hover { background: #E2E8F0; }
    )");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // === 滚动区域 ===
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget* scrollContent = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(0, 0, 8, 0);
    contentLayout->setSpacing(8);

    // === 基本信息组（紧凑布局）===
    QGroupBox* basicGroup = new QGroupBox(tr("📋 基本信息"), this);
    QGridLayout* basicLayout = new QGridLayout(basicGroup);
    basicLayout->setSpacing(6);
    basicLayout->setContentsMargins(10, 16, 10, 10);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("例如：温度高限报警"));
    basicLayout->addWidget(new QLabel(tr("名称:")), 0, 0);
    basicLayout->addWidget(m_nameEdit, 0, 1);

    m_enabledCheck = new QCheckBox(tr("启用"), this);
    basicLayout->addWidget(m_enabledCheck, 0, 2);

    basicLayout->addWidget(new QLabel(tr("类型:")), 1, 0);
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItems({
        tr("高限报警"), tr("低限报警"), tr("高高限报警"), tr("低低限报警"),
        tr("值变化报警"), tr("数据质量报警"), tr("连接丢失报警")
    });
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AlarmRuleDialog::onAlarmTypeChanged);
    basicLayout->addWidget(m_typeCombo, 1, 1);

    m_priorityCombo = new QComboBox(this);
    m_priorityCombo->addItems({tr("低"), tr("中"), tr("高"), tr("紧急")});
    basicLayout->addWidget(m_priorityCombo, 1, 2);

    contentLayout->addWidget(basicGroup);

    // === 数据源组（单行布局）===
    QGroupBox* sourceGroup = new QGroupBox(tr("📡 数据源"), this);
    QHBoxLayout* sourceLayout = new QHBoxLayout(sourceGroup);
    sourceLayout->setSpacing(8);
    sourceLayout->setContentsMargins(10, 16, 10, 10);

    sourceLayout->addWidget(new QLabel(tr("通道:")));
    m_channelCombo = new QComboBox(this);
    m_channelCombo->setMinimumWidth(120);
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AlarmRuleDialog::onChannelChanged);
    sourceLayout->addWidget(m_channelCombo, 1);

    sourceLayout->addWidget(new QLabel(tr("标签:")));
    m_tagCombo = new QComboBox(this);
    m_tagCombo->setEditable(true);
    m_tagCombo->setMinimumWidth(150);
    sourceLayout->addWidget(m_tagCombo, 2);

    contentLayout->addWidget(sourceGroup);

    // === 条件配置组（网格布局）===
    QGroupBox* conditionGroup = new QGroupBox(tr("⚙️ 报警条件"), this);
    QGridLayout* conditionLayout = new QGridLayout(conditionGroup);
    conditionLayout->setSpacing(6);
    conditionLayout->setContentsMargins(10, 16, 10, 10);

    // 提示标签
    QLabel* condHintLabel = new QLabel(tr("💡 根据报警类型，只需设置对应的阈值即可"), this);
    condHintLabel->setStyleSheet("color: #64748B; font-size: 9pt; font-style: italic;");
    conditionLayout->addWidget(condHintLabel, 0, 0, 1, 4);

    // 第一行：高限和低限
    conditionLayout->addWidget(new QLabel(tr("高限:")), 1, 0);
    m_highLimitSpin = new QDoubleSpinBox(this);
    m_highLimitSpin->setRange(-999999.0, 999999.0);
    m_highLimitSpin->setDecimals(2);
    conditionLayout->addWidget(m_highLimitSpin, 1, 1);

    conditionLayout->addWidget(new QLabel(tr("低限:")), 1, 2);
    m_lowLimitSpin = new QDoubleSpinBox(this);
    m_lowLimitSpin->setRange(-999999.0, 999999.0);
    m_lowLimitSpin->setDecimals(2);
    conditionLayout->addWidget(m_lowLimitSpin, 1, 3);

    // 第二行：高高限和低低限
    conditionLayout->addWidget(new QLabel(tr("高高限:")), 2, 0);
    m_highHighLimitSpin = new QDoubleSpinBox(this);
    m_highHighLimitSpin->setRange(-999999.0, 999999.0);
    m_highHighLimitSpin->setDecimals(2);
    conditionLayout->addWidget(m_highHighLimitSpin, 2, 1);

    conditionLayout->addWidget(new QLabel(tr("低低限:")), 2, 2);
    m_lowLowLimitSpin = new QDoubleSpinBox(this);
    m_lowLowLimitSpin->setRange(-999999.0, 999999.0);
    m_lowLowLimitSpin->setDecimals(2);
    conditionLayout->addWidget(m_lowLowLimitSpin, 2, 3);

    // 第三行：延迟和死区
    conditionLayout->addWidget(new QLabel(tr("延迟:")), 3, 0);
    m_delaySpin = new QSpinBox(this);
    m_delaySpin->setRange(0, 3600);
    m_delaySpin->setSuffix(tr(" 秒"));
    m_delaySpin->setToolTip(tr("条件持续满足此时长后才触发报警"));
    conditionLayout->addWidget(m_delaySpin, 3, 1);

    conditionLayout->addWidget(new QLabel(tr("死区:")), 3, 2);
    m_deadbandSpin = new QDoubleSpinBox(this);
    m_deadbandSpin->setRange(0.0, 999999.0);
    m_deadbandSpin->setDecimals(2);
    m_deadbandSpin->setToolTip(tr("避免报警频繁触发"));
    conditionLayout->addWidget(m_deadbandSpin, 3, 3);

    contentLayout->addWidget(conditionGroup);

    // === 消息模板组（更紧凑）===
    QGroupBox* messageGroup = new QGroupBox(tr("💬 消息模板"), this);
    QVBoxLayout* messageLayout = new QVBoxLayout(messageGroup);
    messageLayout->setSpacing(4);
    messageLayout->setContentsMargins(10, 16, 10, 10);

    m_messageEdit = new QTextEdit(this);
    m_messageEdit->setMaximumHeight(45);
    m_messageEdit->setPlaceholderText(tr("可用: ${channel}, ${tag}, ${type}, ${value}"));
    messageLayout->addWidget(m_messageEdit);

    contentLayout->addWidget(messageGroup);
    
    // === 录波配置组 ===
    setupRecordingPanel();
    contentLayout->addWidget(m_recordingGroup);

    contentLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // === 按钮组 ===
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    buttonLayout->addStretch();

    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setMinimumWidth(80);
    connect(m_cancelBtn, &QPushButton::clicked, this, &AlarmRuleDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton(tr("确定"), this);
    m_okBtn->setObjectName("okBtn");
    m_okBtn->setMinimumWidth(80);
    connect(m_okBtn, &QPushButton::clicked, this, &AlarmRuleDialog::onOkClicked);
    buttonLayout->addWidget(m_okBtn);

    mainLayout->addLayout(buttonLayout);
}

void AlarmRuleDialog::setupRecordingPanel() {
    m_recordingGroup = new QGroupBox(tr("📹 告警自动录波"), this);
    m_recordingGroup->setCheckable(true);
    m_recordingGroup->setChecked(false);
    
    QVBoxLayout* recordingLayout = new QVBoxLayout(m_recordingGroup);
    recordingLayout->setSpacing(6);
    recordingLayout->setContentsMargins(10, 16, 10, 10);
    
    // 隐藏的复选框用于保持兼容性
    m_recordingEnabledCheck = new QCheckBox(this);
    m_recordingEnabledCheck->setVisible(false);
    
    // 连接GroupBox的checked信号到录波启用
    connect(m_recordingGroup, &QGroupBox::toggled, this, [this](bool checked) {
        m_recordingEnabledCheck->setChecked(checked);
        onRecordingEnabledChanged(checked);
    });
    
    // 时间配置（紧凑网格）
    QGridLayout* timeLayout = new QGridLayout();
    timeLayout->setSpacing(6);
    
    timeLayout->addWidget(new QLabel(tr("预触发:")), 0, 0);
    m_preTriggerSpin = new QDoubleSpinBox(this);
    m_preTriggerSpin->setRange(0, 300);
    m_preTriggerSpin->setValue(60);
    m_preTriggerSpin->setSuffix(tr("s"));
    m_preTriggerSpin->setToolTip(tr("保留故障发生前的数据"));
    timeLayout->addWidget(m_preTriggerSpin, 0, 1);
    
    timeLayout->addWidget(new QLabel(tr("后触发:")), 0, 2);
    m_postTriggerSpin = new QDoubleSpinBox(this);
    m_postTriggerSpin->setRange(0, 3600);
    m_postTriggerSpin->setValue(30);
    m_postTriggerSpin->setSuffix(tr("s"));
    m_postTriggerSpin->setToolTip(tr("故障后继续录波时长"));
    timeLayout->addWidget(m_postTriggerSpin, 0, 3);
    
    timeLayout->addWidget(new QLabel(tr("采样:")), 0, 4);
    m_sampleIntervalSpin = new QSpinBox(this);
    m_sampleIntervalSpin->setRange(10, 10000);
    m_sampleIntervalSpin->setValue(100);
    m_sampleIntervalSpin->setSuffix(tr("ms"));
    timeLayout->addWidget(m_sampleIntervalSpin, 0, 5);
    
    recordingLayout->addLayout(timeLayout);
    
    // 录波标签列表（改进的多选体验）
    QLabel* tagsLabel = new QLabel(tr("录波标签 <span style='color:#64748B;'>(留空则使用报警标签)</span>:"), this);
    recordingLayout->addWidget(tagsLabel);
    
    QHBoxLayout* tagsLayout = new QHBoxLayout();
    tagsLayout->setSpacing(4);
    
    // 左侧：可用标签列表
    QVBoxLayout* availableLayout = new QVBoxLayout();
    QLabel* availLabel = new QLabel(tr("可用标签:"), this);
    availLabel->setStyleSheet("color: #64748B; font-size: 9pt;");
    availableLayout->addWidget(availLabel);
    
    m_availableTagsCombo = new QComboBox(this);
    m_availableTagsCombo->setEditable(true);
    m_availableTagsCombo->setMinimumWidth(150);
    m_availableTagsCombo->setMaxVisibleItems(10);
    availableLayout->addWidget(m_availableTagsCombo);
    availableLayout->addStretch();
    tagsLayout->addLayout(availableLayout);
    
    // 中间：添加/移除按钮
    QVBoxLayout* btnLayout = new QVBoxLayout();
    btnLayout->addStretch();
    m_addTagBtn = new QPushButton(tr("→"), this);
    m_addTagBtn->setFixedSize(30, 26);
    m_addTagBtn->setToolTip(tr("添加到录波列表"));
    connect(m_addTagBtn, &QPushButton::clicked, this, &AlarmRuleDialog::onAddRecordTag);
    btnLayout->addWidget(m_addTagBtn);
    
    m_removeTagBtn = new QPushButton(tr("←"), this);
    m_removeTagBtn->setFixedSize(30, 26);
    m_removeTagBtn->setToolTip(tr("从录波列表移除"));
    connect(m_removeTagBtn, &QPushButton::clicked, this, &AlarmRuleDialog::onRemoveRecordTag);
    btnLayout->addWidget(m_removeTagBtn);
    btnLayout->addStretch();
    tagsLayout->addLayout(btnLayout);
    
    // 右侧：已选标签列表
    QVBoxLayout* selectedLayout = new QVBoxLayout();
    QLabel* selLabel = new QLabel(tr("已选标签:"), this);
    selLabel->setStyleSheet("color: #64748B; font-size: 9pt;");
    selectedLayout->addWidget(selLabel);
    
    m_recordTagsList = new QListWidget(this);
    m_recordTagsList->setMaximumHeight(60);
    m_recordTagsList->setMinimumWidth(150);
    m_recordTagsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_recordTagsList->setStyleSheet("QListWidget { border: 1px solid #CBD5E1; border-radius: 4px; }");
    selectedLayout->addWidget(m_recordTagsList);
    tagsLayout->addLayout(selectedLayout, 1);
    
    recordingLayout->addLayout(tagsLayout);
    
    // 保存目录和选项（单行）
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    optionsLayout->setSpacing(8);
    
    m_saveDirEdit = new QLineEdit(this);
    m_saveDirEdit->setPlaceholderText(tr("保存目录（留空使用默认）"));
    m_saveDirEdit->setMaximumWidth(180);
    optionsLayout->addWidget(m_saveDirEdit);
    
    m_browseDirBtn = new QPushButton(tr("..."), this);
    m_browseDirBtn->setFixedWidth(30);
    connect(m_browseDirBtn, &QPushButton::clicked, this, &AlarmRuleDialog::onBrowseSaveDir);
    optionsLayout->addWidget(m_browseDirBtn);
    
    optionsLayout->addSpacing(10);
    
    m_autoExportCheck = new QCheckBox(tr("自动导出CSV"), this);
    m_autoExportCheck->setChecked(true);
    optionsLayout->addWidget(m_autoExportCheck);
    
    m_keepInMemoryCheck = new QCheckBox(tr("内存回放"), this);
    m_keepInMemoryCheck->setChecked(true);
    optionsLayout->addWidget(m_keepInMemoryCheck);
    
    optionsLayout->addStretch();
    recordingLayout->addLayout(optionsLayout);
}

void AlarmRuleDialog::onRecordingEnabledChanged(bool enabled) {
    // GroupBox的内容会自动根据checkable状态禁用
    // 这里只需要同步隐藏的复选框状态
    if (m_recordingEnabledCheck->isChecked() != enabled) {
        m_recordingEnabledCheck->setChecked(enabled);
    }
    if (m_recordingGroup->isChecked() != enabled) {
        m_recordingGroup->setChecked(enabled);
    }
}

void AlarmRuleDialog::onAddRecordTag() {
    QString tag = m_availableTagsCombo->currentText().trimmed();
    if (tag.isEmpty()) return;
    
    // 检查是否已存在
    for (int i = 0; i < m_recordTagsList->count(); ++i) {
        if (m_recordTagsList->item(i)->text() == tag) {
            return;
        }
    }
    
    m_recordTagsList->addItem(tag);
}

void AlarmRuleDialog::onRemoveRecordTag() {
    QListWidgetItem* item = m_recordTagsList->currentItem();
    if (item) {
        delete m_recordTagsList->takeItem(m_recordTagsList->row(item));
    }
}

void AlarmRuleDialog::onBrowseSaveDir() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择保存目录"),
                                                     m_saveDirEdit->text());
    if (!dir.isEmpty()) {
        m_saveDirEdit->setText(dir);
    }
}

void AlarmRuleDialog::loadChannelsAndTags() {
    if (!m_channelManager) {
        return;
    }

    // 加载通道列表
    m_channelCombo->clear();
    QStringList channelNames = m_channelManager->getChannelNames();
    for (const QString& channelName : channelNames) {
        m_channelCombo->addItem(channelName, channelName);
    }

    // 如果有通道，加载第一个通道的标签
    if (m_channelCombo->count() > 0) {
        onChannelChanged(0);
    }
}

void AlarmRuleDialog::onChannelChanged(int index) {
    m_tagCombo->clear();
    if (m_availableTagsCombo) {
        m_availableTagsCombo->clear();
    }

    if (index < 0 || !m_channelManager) {
        return;
    }

    QString channelName = m_channelCombo->itemData(index).toString();
    Channel* channel = m_channelManager->getChannel(channelName);

    if (!channel) {
        return;
    }

    // 获取该通道的 UniversalDataModel
    UniversalDataModel* udm = channel->getDataModel();
    if (!udm) {
        return;
    }

    // 加载该通道的所有标签
    QStringList tagNames = udm->getAllTags();
    for (const QString& tagName : tagNames) {
        m_tagCombo->addItem(tagName);
        if (m_availableTagsCombo) {
            m_availableTagsCombo->addItem(tagName);
        }
    }
}

void AlarmRuleDialog::onAlarmTypeChanged(int index) {
    Q_UNUSED(index);
    updateLimitFields();
}

void AlarmRuleDialog::updateLimitFields() {
    int typeIndex = m_typeCombo->currentIndex();

    // 禁用所有限值字段
    m_highLimitSpin->setEnabled(false);
    m_lowLimitSpin->setEnabled(false);
    m_highHighLimitSpin->setEnabled(false);
    m_lowLowLimitSpin->setEnabled(false);

    // 根据类型启用相应字段
    switch (typeIndex) {
        case 0:  // HighLimit
            m_highLimitSpin->setEnabled(true);
            break;
        case 1:  // LowLimit
            m_lowLimitSpin->setEnabled(true);
            break;
        case 2:  // HighHighLimit
            m_highHighLimitSpin->setEnabled(true);
            break;
        case 3:  // LowLowLimit
            m_lowLowLimitSpin->setEnabled(true);
            break;
        default:
            // ValueChange, DataQuality, ConnectionLost 不需要限值
            break;
    }
}

void AlarmRuleDialog::loadRule(const AlarmRule& rule) {
    m_nameEdit->setText(rule.name);
    m_enabledCheck->setChecked(rule.enabled);
    m_priorityCombo->setCurrentIndex(static_cast<int>(rule.priority));
    m_typeCombo->setCurrentIndex(static_cast<int>(rule.type));

    // 设置通道
    for (int i = 0; i < m_channelCombo->count(); ++i) {
        if (m_channelCombo->itemText(i) == rule.channelName) {
            m_channelCombo->setCurrentIndex(i);
            break;
        }
    }

    // 设置标签
    m_tagCombo->setCurrentText(rule.tagName);

    // 设置限值
    m_highLimitSpin->setValue(rule.highLimit);
    m_lowLimitSpin->setValue(rule.lowLimit);
    m_highHighLimitSpin->setValue(rule.highHighLimit);
    m_lowLowLimitSpin->setValue(rule.lowLowLimit);

    m_delaySpin->setValue(rule.delaySeconds);
    m_deadbandSpin->setValue(rule.deadband);
    m_messageEdit->setPlainText(rule.message);
    
    // 加载录波配置
    const AlarmRecordingConfig& recConfig = rule.recordingConfig;
    m_recordingGroup->setChecked(recConfig.enabled);
    m_recordingEnabledCheck->setChecked(recConfig.enabled);
    m_preTriggerSpin->setValue(recConfig.preTriggerSeconds);
    m_postTriggerSpin->setValue(recConfig.postTriggerSeconds);
    m_sampleIntervalSpin->setValue(recConfig.sampleIntervalMs);
    
    m_recordTagsList->clear();
    for (const QString& tag : recConfig.recordTags) {
        m_recordTagsList->addItem(tag);
    }
    
    m_saveDirEdit->setText(recConfig.saveDirectory);
    m_autoExportCheck->setChecked(recConfig.autoExportCsv);
    m_keepInMemoryCheck->setChecked(recConfig.keepInMemory);
}

bool AlarmRuleDialog::validateInput() {
    // 检查规则名称
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请输入规则名称"));
        m_nameEdit->setFocus();
        return false;
    }

    // 检查通道
    if (m_channelCombo->currentIndex() < 0) {
        QMessageBox::warning(this, tr("验证失败"), tr("请选择通道"));
        m_channelCombo->setFocus();
        return false;
    }

    // 检查标签
    if (m_tagCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请选择或输入标签名称"));
        m_tagCombo->setFocus();
        return false;
    }

    // 检查限值设置（根据类型）
    int typeIndex = m_typeCombo->currentIndex();
    if (typeIndex == 0 && !m_highLimitSpin->isEnabled()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请设置高限值"));
        return false;
    }
    if (typeIndex == 1 && !m_lowLimitSpin->isEnabled()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请设置低限值"));
        return false;
    }

    // 检查消息模板
    if (m_messageEdit->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请输入消息模板"));
        m_messageEdit->setFocus();
        return false;
    }

    return true;
}

AlarmRule AlarmRuleDialog::getRule() const {
    AlarmRule rule;

    // 如果是编辑模式，保留原始ID；否则生成新ID
    rule.id = m_isEdit ? m_originalId : QUuid::createUuid().toString(QUuid::WithoutBraces);

    rule.name = m_nameEdit->text().trimmed();
    rule.enabled = m_enabledCheck->isChecked();
    rule.priority = static_cast<AlarmPriority>(m_priorityCombo->currentIndex());
    rule.type = static_cast<AlarmType>(m_typeCombo->currentIndex());

    // 获取通道名称（直接使用显示的文本）
    if (m_channelCombo->currentIndex() >= 0) {
        rule.channelName = m_channelCombo->currentText();
    }

    rule.tagName = m_tagCombo->currentText().trimmed();

    rule.highLimit = m_highLimitSpin->value();
    rule.lowLimit = m_lowLimitSpin->value();
    rule.highHighLimit = m_highHighLimitSpin->value();
    rule.lowLowLimit = m_lowLowLimitSpin->value();

    rule.delaySeconds = m_delaySpin->value();
    rule.deadband = m_deadbandSpin->value();
    rule.message = m_messageEdit->toPlainText();
    
    // 获取录波配置
    rule.recordingConfig.enabled = m_recordingGroup->isChecked();
    rule.recordingConfig.preTriggerSeconds = m_preTriggerSpin->value();
    rule.recordingConfig.postTriggerSeconds = m_postTriggerSpin->value();
    rule.recordingConfig.sampleIntervalMs = m_sampleIntervalSpin->value();
    
    rule.recordingConfig.recordTags.clear();
    for (int i = 0; i < m_recordTagsList->count(); ++i) {
        rule.recordingConfig.recordTags.append(m_recordTagsList->item(i)->text());
    }
    
    rule.recordingConfig.saveDirectory = m_saveDirEdit->text();
    rule.recordingConfig.autoExportCsv = m_autoExportCheck->isChecked();
    rule.recordingConfig.keepInMemory = m_keepInMemoryCheck->isChecked();

    return rule;
}

void AlarmRuleDialog::onOkClicked() {
    if (validateInput()) {
        accept();
    }
}

void AlarmRuleDialog::onCancelClicked() {
    reject();
}

} // namespace ModbusPlexLink
