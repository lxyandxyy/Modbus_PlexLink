/**
 * @file WaveformRecorder.cpp
 * @brief 指定点位录波功能实现
 * @features 触发录波、预触发缓存、多Y轴、数据回放
 */

#include "WaveformRecorder.h"

#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>

#include "../core/UniversalDataModel.h"
#include "qcustomplot.h"

using namespace ModbusPlexLink;

// 默认颜色列表
const QList<QColor> WaveformRecorderWidget::s_defaultColors = {
    QColor(0x3B82F6),  // 蓝色
    QColor(0xEF4444),  // 红色
    QColor(0x10B981),  // 绿色
    QColor(0xF59E0B),  // 橙色
    QColor(0x8B5CF6),  // 紫色
    QColor(0xEC4899),  // 粉色
    QColor(0x06B6D4),  // 青色
    QColor(0x84CC16),  // 黄绿色
    QColor(0xF97316),  // 深橙色
    QColor(0x6366F1),  // 靛蓝色
};

WaveformRecorderWidget::WaveformRecorderWidget(QWidget* parent)
    : QWidget(parent),
      m_dataModel(nullptr),
      m_sampleTimer(new QTimer(this)),
      m_playbackTimer(new QTimer(this)),
      m_state(RecorderState::Idle),
      m_triggerState(TriggerState::Idle),
      m_lastTriggerValue(0.0),
      m_triggerTimestamp(0.0),
      m_playbackIndex(0),
      m_playbackSpeed(1.0),
      m_totalPlaybackPoints(0),
      m_sampleIntervalMs(100),
      m_timeWindowSec(30.0),
      m_maxDataPoints(100000),
      m_autoScroll(true),
      m_preTriggerTimeSec(5.0),
      m_preTriggerBufferSize(50),
      m_colorIndex(0) {
    setupUi();
    applyStyles();

    connect(m_sampleTimer, &QTimer::timeout, this,
            &WaveformRecorderWidget::onSampleTimer);
    connect(m_playbackTimer, &QTimer::timeout, this,
            &WaveformRecorderWidget::onPlaybackTimer);

    updateButtonStates();
}

WaveformRecorderWidget::~WaveformRecorderWidget() {
    stopRecording();
    stopPlayback();
}

void WaveformRecorderWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 工具栏
    setupToolBar();
    mainLayout->addWidget(m_toolBar);

    // 主分割器
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // 左侧面板
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(8);

    // 通道列表标题
    QLabel* channelLabel = new QLabel(tr("📊 录波通道"), leftPanel);
    QFont labelFont = channelLabel->font();
    labelFont.setBold(true);
    labelFont.setPointSize(10);
    channelLabel->setFont(labelFont);
    leftLayout->addWidget(channelLabel);

    setupChannelTable();
    leftLayout->addWidget(m_channelTable);

    // 触发配置面板
    setupTriggerPanel();
    leftLayout->addWidget(m_triggerStateLabel);

    // 回放面板
    setupPlaybackPanel();
    leftLayout->addWidget(m_playbackPanel);

    leftPanel->setMinimumWidth(300);
    leftPanel->setMaximumWidth(450);
    m_splitter->addWidget(leftPanel);

    // 右侧：波形图
    setupPlot();
    m_splitter->addWidget(m_plot);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({300, 700});

    mainLayout->addWidget(m_splitter, 1);

    // 状态栏
    setupStatusBar();
    mainLayout->addWidget(m_statusBar);
}

void WaveformRecorderWidget::setupToolBar() {
    m_toolBar = new QToolBar(tr("录波工具栏"), this);
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(20, 20));

    // 录波控制按钮
    m_startBtn = new QPushButton(tr("▶ 开始"), this);
    m_startBtn->setToolTip(tr("开始录波"));
    connect(m_startBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onStartClicked);
    m_toolBar->addWidget(m_startBtn);

    m_pauseBtn = new QPushButton(tr("⏸ 暂停"), this);
    m_pauseBtn->setToolTip(tr("暂停录波"));
    connect(m_pauseBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onPauseClicked);
    m_toolBar->addWidget(m_pauseBtn);

    m_stopBtn = new QPushButton(tr("⏹ 停止"), this);
    m_stopBtn->setToolTip(tr("停止录波"));
    connect(m_stopBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onStopClicked);
    m_toolBar->addWidget(m_stopBtn);

    m_toolBar->addSeparator();

    // 通道管理按钮
    m_addChannelBtn = new QPushButton(tr("➕ 添加通道"), this);
    m_addChannelBtn->setToolTip(tr("添加录波通道"));
    connect(m_addChannelBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onAddChannelClicked);
    m_toolBar->addWidget(m_addChannelBtn);

    m_removeChannelBtn = new QPushButton(tr("➖ 移除"), this);
    m_removeChannelBtn->setToolTip(tr("移除选中的通道"));
    connect(m_removeChannelBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onRemoveChannelClicked);
    m_toolBar->addWidget(m_removeChannelBtn);

    m_toolBar->addSeparator();

    // 触发控制
    m_triggerConfigBtn = new QPushButton(tr("⚡ 触发设置"), this);
    m_triggerConfigBtn->setToolTip(tr("配置触发条件"));
    connect(m_triggerConfigBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onTriggerConfigClicked);
    m_toolBar->addWidget(m_triggerConfigBtn);

    m_armTriggerBtn = new QPushButton(tr("🎯 布防"), this);
    m_armTriggerBtn->setToolTip(tr("启动触发监测"));
    connect(m_armTriggerBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onArmTriggerClicked);
    m_toolBar->addWidget(m_armTriggerBtn);

    m_forceTriggerBtn = new QPushButton(tr("⚡ 强制触发"), this);
    m_forceTriggerBtn->setToolTip(tr("手动强制触发"));
    connect(m_forceTriggerBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onForceTriggerClicked);
    m_toolBar->addWidget(m_forceTriggerBtn);

    m_toolBar->addSeparator();

    // 采样间隔设置
    QLabel* intervalLabel = new QLabel(tr(" 采样: "), this);
    m_toolBar->addWidget(intervalLabel);

    m_sampleIntervalSpin = new QSpinBox(this);
    m_sampleIntervalSpin->setRange(10, 10000);
    m_sampleIntervalSpin->setValue(m_sampleIntervalMs);
    m_sampleIntervalSpin->setSuffix(" ms");
    m_sampleIntervalSpin->setToolTip(tr("采样间隔（毫秒）"));
    connect(m_sampleIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &WaveformRecorderWidget::onSampleIntervalChanged);
    m_toolBar->addWidget(m_sampleIntervalSpin);

    // 时间窗口设置
    QLabel* windowLabel = new QLabel(tr(" 窗口: "), this);
    m_toolBar->addWidget(windowLabel);

    m_timeWindowSpin = new QDoubleSpinBox(this);
    m_timeWindowSpin->setRange(1.0, 3600.0);
    m_timeWindowSpin->setValue(m_timeWindowSec);
    m_timeWindowSpin->setSuffix(" s");
    m_timeWindowSpin->setDecimals(1);
    m_timeWindowSpin->setToolTip(tr("显示时间窗口（秒）"));
    connect(m_timeWindowSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &WaveformRecorderWidget::onTimeWindowChanged);
    m_toolBar->addWidget(m_timeWindowSpin);

    m_toolBar->addSeparator();

    // 自动滚动
    m_autoScrollCheck = new QCheckBox(tr("自动滚动"), this);
    m_autoScrollCheck->setChecked(m_autoScroll);
    m_autoScrollCheck->setToolTip(tr("自动滚动到最新数据"));
    connect(m_autoScrollCheck, &QCheckBox::toggled, this,
            &WaveformRecorderWidget::onAutoScrollChanged);
    m_toolBar->addWidget(m_autoScrollCheck);

    m_toolBar->addSeparator();

    // 导出按钮
    m_exportBtn = new QPushButton(tr("📥 导出"), this);
    m_exportBtn->setToolTip(tr("导出录波数据到CSV文件"));
    connect(m_exportBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onExportClicked);
    m_toolBar->addWidget(m_exportBtn);

    m_loadFileBtn = new QPushButton(tr("📂 加载"), this);
    m_loadFileBtn->setToolTip(tr("加载CSV文件进行回放"));
    connect(m_loadFileBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onLoadFileClicked);
    m_toolBar->addWidget(m_loadFileBtn);

    m_saveImageBtn = new QPushButton(tr("📸 截图"), this);
    m_saveImageBtn->setToolTip(tr("保存波形图片"));
    connect(m_saveImageBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onSaveImageClicked);
    m_toolBar->addWidget(m_saveImageBtn);

    m_clearDataBtn = new QPushButton(tr("🗑 清空"), this);
    m_clearDataBtn->setToolTip(tr("清空所有录波数据"));
    connect(m_clearDataBtn, &QPushButton::clicked, this,
            &WaveformRecorderWidget::onClearDataClicked);
    m_toolBar->addWidget(m_clearDataBtn);
}

void WaveformRecorderWidget::setupChannelTable() {
    m_channelTable = new QTableWidget(this);
    m_channelTable->setColumnCount(6);
    m_channelTable->setHorizontalHeaderLabels(
        {tr("启用"), tr("名称"), tr("倍率"), tr("偏移"), tr("Y轴"), tr("颜色")});

    m_channelTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Fixed);
    m_channelTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    m_channelTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Fixed);
    m_channelTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Fixed);
    m_channelTable->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::Fixed);
    m_channelTable->horizontalHeader()->setSectionResizeMode(
        5, QHeaderView::Fixed);

    m_channelTable->setColumnWidth(0, 40);
    m_channelTable->setColumnWidth(2, 55);
    m_channelTable->setColumnWidth(3, 55);
    m_channelTable->setColumnWidth(4, 50);
    m_channelTable->setColumnWidth(5, 45);

    m_channelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_channelTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_channelTable->setAlternatingRowColors(true);
    m_channelTable->verticalHeader()->setVisible(false);

    connect(m_channelTable, &QTableWidget::cellChanged, this,
            &WaveformRecorderWidget::onChannelTableChanged);
}

void WaveformRecorderWidget::setupPlot() {
    m_plot = new QCustomPlot(this);
    m_plot->setMinimumSize(400, 300);

    // 配置交互
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom |
                            QCP::iSelectPlottables);

    // 配置X轴（时间轴）
    m_plot->xAxis->setLabel(tr("时间 (秒)"));
    m_plot->xAxis->setRange(0, m_timeWindowSec);

    // 配置左侧Y轴
    m_plot->yAxis->setLabel(tr("数值 (左轴)"));
    m_plot->yAxis->setRange(-100, 100);

    // 配置右侧Y轴
    m_plot->yAxis2->setVisible(true);
    m_plot->yAxis2->setLabel(tr("数值 (右轴)"));
    m_plot->yAxis2->setRange(-100, 100);

    // 配置图例
    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(QFont("Microsoft YaHei", 9));
    m_plot->axisRect()->insetLayout()->setInsetAlignment(
        0, Qt::AlignTop | Qt::AlignRight);

    // 配置背景
    m_plot->setBackground(QBrush(QColor(0xFAFAFA)));
    m_plot->axisRect()->setBackground(QBrush(Qt::white));

    // 配置网格
    m_plot->xAxis->grid()->setPen(QPen(QColor(0xE5E7EB), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(0xE5E7EB), 1, Qt::DotLine));
}

void WaveformRecorderWidget::setupTriggerPanel() {
    m_triggerStateLabel = new QLabel(tr("触发状态: 未配置"), this);
    m_triggerStateLabel->setStyleSheet(
        "QLabel { padding: 8px; background: #F1F5F9; border-radius: 4px; }");
}

void WaveformRecorderWidget::setupPlaybackPanel() {
    m_playbackPanel = new QGroupBox(tr("📼 数据回放"), this);
    QVBoxLayout* layout = new QVBoxLayout(m_playbackPanel);
    layout->setSpacing(6);

    // 播放控制
    QHBoxLayout* controlLayout = new QHBoxLayout();
    m_playBtn = new QPushButton(tr("▶"), this);
    m_playBtn->setFixedWidth(40);
    m_playBtn->setToolTip(tr("播放/暂停"));
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (m_state == RecorderState::Playback) {
            pausePlayback();
        } else if (!m_playbackData.isEmpty()) {
            startPlayback();
        }
    });
    controlLayout->addWidget(m_playBtn);

    m_playbackSlider = new QSlider(Qt::Horizontal, this);
    m_playbackSlider->setRange(0, 1000);
    m_playbackSlider->setValue(0);
    connect(m_playbackSlider, &QSlider::valueChanged, this,
            &WaveformRecorderWidget::onPlaybackSliderChanged);
    controlLayout->addWidget(m_playbackSlider, 1);

    layout->addLayout(controlLayout);

    // 速度和时间
    QHBoxLayout* infoLayout = new QHBoxLayout();
    infoLayout->addWidget(new QLabel(tr("速度:"), this));

    m_playbackSpeedSpin = new QDoubleSpinBox(this);
    m_playbackSpeedSpin->setRange(0.1, 10.0);
    m_playbackSpeedSpin->setValue(1.0);
    m_playbackSpeedSpin->setSuffix("x");
    m_playbackSpeedSpin->setDecimals(1);
    connect(m_playbackSpeedSpin,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &WaveformRecorderWidget::onPlaybackSpeedChanged);
    infoLayout->addWidget(m_playbackSpeedSpin);

    infoLayout->addStretch();

    m_playbackTimeLabel = new QLabel(tr("00:00 / 00:00"), this);
    infoLayout->addWidget(m_playbackTimeLabel);

    layout->addLayout(infoLayout);

    m_playbackPanel->setVisible(false);  // 默认隐藏
}

void WaveformRecorderWidget::setupStatusBar() {
    m_statusBar = new QStatusBar(this);
    m_statusBar->setSizeGripEnabled(false);

    m_stateLabel = new QLabel(tr("状态: 空闲"), this);
    m_stateLabel->setMinimumWidth(120);
    m_statusBar->addWidget(m_stateLabel);

    m_sampleRateLabel = new QLabel(tr("采样率: 100ms"), this);
    m_sampleRateLabel->setMinimumWidth(120);
    m_statusBar->addWidget(m_sampleRateLabel);

    m_dataPointsLabel = new QLabel(tr("数据点: 0"), this);
    m_dataPointsLabel->setMinimumWidth(100);
    m_statusBar->addWidget(m_dataPointsLabel);

    m_elapsedTimeLabel = new QLabel(tr("录波时长: 00:00:00"), this);
    m_elapsedTimeLabel->setMinimumWidth(140);
    m_statusBar->addWidget(m_elapsedTimeLabel);
}

void WaveformRecorderWidget::applyStyles() {
    setStyleSheet(R"(
        QToolBar {
            background-color: #F8FAFC;
            border-bottom: 1px solid #E2E8F0;
            padding: 4px;
            spacing: 4px;
        }
        
        QPushButton {
            background-color: #3B82F6;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 10px;
            font-weight: bold;
        }
        
        QPushButton:hover {
            background-color: #2563EB;
        }
        
        QPushButton:pressed {
            background-color: #1D4ED8;
        }
        
        QPushButton:disabled {
            background-color: #94A3B8;
        }
        
        QTableWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 4px;
            gridline-color: #E2E8F0;
        }
        
        QTableWidget::item {
            padding: 4px;
        }
        
        QTableWidget::item:selected {
            background-color: #DBEAFE;
            color: #1E40AF;
        }
        
        QHeaderView::section {
            background-color: #F1F5F9;
            padding: 6px;
            border: none;
            border-bottom: 1px solid #E2E8F0;
            font-weight: bold;
        }
        
        QSpinBox, QDoubleSpinBox {
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            padding: 4px;
            background: white;
        }
        
        QCheckBox {
            spacing: 6px;
        }
        
        QStatusBar {
            background-color: #F1F5F9;
            border-top: 1px solid #E2E8F0;
        }
        
        QGroupBox {
            font-weight: bold;
            border: 1px solid #E2E8F0;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 16px;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
        }
        
        QSlider::groove:horizontal {
            height: 6px;
            background: #E2E8F0;
            border-radius: 3px;
        }
        
        QSlider::handle:horizontal {
            background: #3B82F6;
            width: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
    )");
}

// ==================== 数据源设置 ====================

void WaveformRecorderWidget::setDataModel(UniversalDataModel* model) {
    m_dataModel = model;
}

QStringList WaveformRecorderWidget::getAvailableTags() const {
    if (!m_dataModel) {
        return QStringList();
    }
    return m_dataModel->getAllTags();
}

// ==================== 通道管理 ====================

int WaveformRecorderWidget::addChannel(const QString& tagName,
                                        const QString& displayName,
                                        double scale, double offset,
                                        const QColor& color) {
    WaveformChannel channel;
    channel.tagName = tagName;
    channel.displayName = displayName.isEmpty() ? tagName : displayName;
    channel.scale = scale;
    channel.offset = offset;
    channel.color = color.isValid() ? color : getNextColor();
    channel.enabled = true;
    channel.graphIndex = -1;
    channel.yAxisType = YAxisType::Left;

    m_channels.append(channel);
    int index = m_channels.size() - 1;

    // 创建对应的数据存储
    m_channelData[index] = QVector<WaveformDataPoint>();
    m_preTriggerBuffer[index] = QQueue<WaveformDataPoint>();

    // 更新UI
    updateChannelTable();
    updatePlotGraphs();

    emit channelsChanged();

    qDebug() << "[WaveformRecorder] Added channel:" << tagName
             << "Scale:" << scale << "Offset:" << offset;

    return index;
}

void WaveformRecorderWidget::removeChannel(int index) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    // 移除图形
    if (m_channels[index].graphIndex >= 0 &&
        m_channels[index].graphIndex < m_plot->graphCount()) {
        m_plot->removeGraph(m_channels[index].graphIndex);
    }

    // 移除数据
    m_channelData.remove(index);
    m_preTriggerBuffer.remove(index);
    m_channels.removeAt(index);

    // 重新索引
    QMap<int, QVector<WaveformDataPoint>> newData;
    QMap<int, QQueue<WaveformDataPoint>> newBuffer;
    for (int i = 0; i < m_channels.size(); ++i) {
        int oldIndex = i >= index ? i + 1 : i;
        if (m_channelData.contains(oldIndex)) {
            newData[i] = m_channelData[oldIndex];
        }
        if (m_preTriggerBuffer.contains(oldIndex)) {
            newBuffer[i] = m_preTriggerBuffer[oldIndex];
        }
    }
    m_channelData = newData;
    m_preTriggerBuffer = newBuffer;

    // 更新UI
    updateChannelTable();
    updatePlotGraphs();

    emit channelsChanged();
}

void WaveformRecorderWidget::clearChannels() {
    m_plot->clearGraphs();
    m_channels.clear();
    m_channelData.clear();
    m_preTriggerBuffer.clear();
    m_colorIndex = 0;

    updateChannelTable();
    m_plot->replot();

    emit channelsChanged();
}

void WaveformRecorderWidget::updateChannelTransform(int index, double scale,
                                                     double offset) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].scale = scale;
    m_channels[index].offset = offset;

    // 重新计算已有数据的转换值
    if (m_channelData.contains(index)) {
        for (auto& point : m_channelData[index]) {
            point.scaledValue = point.rawValue * scale + offset;
        }
    }

    updatePlotGraphs();
}

void WaveformRecorderWidget::setChannelColor(int index, const QColor& color) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].color = color;

    if (m_channels[index].graphIndex >= 0 &&
        m_channels[index].graphIndex < m_plot->graphCount()) {
        QPen pen = m_plot->graph(m_channels[index].graphIndex)->pen();
        pen.setColor(color);
        m_plot->graph(m_channels[index].graphIndex)->setPen(pen);
        m_plot->replot();
    }

    updateChannelTable();
}

void WaveformRecorderWidget::setChannelEnabled(int index, bool enabled) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].enabled = enabled;

    if (m_channels[index].graphIndex >= 0 &&
        m_channels[index].graphIndex < m_plot->graphCount()) {
        m_plot->graph(m_channels[index].graphIndex)->setVisible(enabled);
        m_plot->replot();
    }
}

void WaveformRecorderWidget::setChannelYAxis(int index, YAxisType type,
                                              double yMin, double yMax) {
    if (index < 0 || index >= m_channels.size()) {
        return;
    }

    m_channels[index].yAxisType = type;
    m_channels[index].yMin = yMin;
    m_channels[index].yMax = yMax;

    updateYAxes();
    updatePlotGraphs();
}

// ==================== 录波控制 ====================

void WaveformRecorderWidget::startRecording() {
    if (m_state == RecorderState::Recording) {
        return;
    }

    if (m_channels.isEmpty()) {
        QMessageBox::warning(this, tr("警告"),
                             tr("请先添加至少一个录波通道！"));
        return;
    }

    if (!m_dataModel) {
        QMessageBox::warning(this, tr("警告"), tr("未设置数据模型！"));
        return;
    }

    m_state = RecorderState::Recording;
    m_triggerState = TriggerState::Idle;
    m_elapsedTimer.start();
    m_sampleTimer->start(m_sampleIntervalMs);

    updateButtonStates();
    updateStatusBar();

    emit recordingStarted();

    qDebug() << "[WaveformRecorder] Recording started. Channels:"
             << m_channels.size() << "Interval:" << m_sampleIntervalMs << "ms";
}

void WaveformRecorderWidget::stopRecording() {
    if (m_state == RecorderState::Idle) {
        return;
    }

    m_sampleTimer->stop();
    m_state = RecorderState::Idle;
    m_triggerState = TriggerState::Idle;

    updateButtonStates();
    updateStatusBar();

    emit recordingStopped();

    qDebug() << "[WaveformRecorder] Recording stopped.";
}

void WaveformRecorderWidget::pauseRecording() {
    if (m_state != RecorderState::Recording) {
        return;
    }

    m_sampleTimer->stop();
    m_state = RecorderState::Paused;

    updateButtonStates();
    updateStatusBar();

    emit recordingPaused();
}

void WaveformRecorderWidget::resumeRecording() {
    if (m_state != RecorderState::Paused) {
        return;
    }

    m_state = RecorderState::Recording;
    m_sampleTimer->start(m_sampleIntervalMs);

    updateButtonStates();
    updateStatusBar();

    emit recordingResumed();
}

// ==================== 触发录波 ====================

void WaveformRecorderWidget::setTriggerConfig(const TriggerConfig& config) {
    m_triggerConfig = config;

    // 更新预触发缓冲区大小
    m_preTriggerTimeSec = config.preTriggerTime;
    m_preTriggerBufferSize =
        static_cast<int>(m_preTriggerTimeSec * 1000.0 / m_sampleIntervalMs);

    // 更新触发状态显示
    if (config.enabled) {
        m_triggerStateLabel->setText(
            tr("触发通道: %1 | 条件: %2 | 阈值: %3")
                .arg(config.channelIndex < m_channels.size()
                         ? m_channels[config.channelIndex].displayName
                         : tr("未知"))
                .arg(triggerConditionToString(config.condition))
                .arg(config.threshold));
    } else {
        m_triggerStateLabel->setText(tr("触发状态: 未配置"));
    }
}

void WaveformRecorderWidget::armTrigger() {
    if (!m_triggerConfig.enabled) {
        QMessageBox::warning(this, tr("警告"), tr("请先配置触发条件！"));
        return;
    }

    if (m_channels.isEmpty()) {
        QMessageBox::warning(this, tr("警告"),
                             tr("请先添加至少一个录波通道！"));
        return;
    }

    // 清空现有数据
    clearData();

    // 初始化预触发缓冲
    for (int i = 0; i < m_channels.size(); ++i) {
        m_preTriggerBuffer[i].clear();
    }

    // 获取初始值
    if (m_dataModel && m_triggerConfig.channelIndex < m_channels.size()) {
        DataPoint point = m_dataModel->readPoint(
            m_channels[m_triggerConfig.channelIndex].tagName);
        m_lastTriggerValue =
            m_channels[m_triggerConfig.channelIndex].transform(
                point.value.toDouble());
    }

    m_state = RecorderState::WaitingTrigger;
    m_triggerState = TriggerState::Armed;
    m_elapsedTimer.start();
    m_sampleTimer->start(m_sampleIntervalMs);

    updateButtonStates();
    updateStatusBar();

    emit triggerArmed();

    qDebug() << "[WaveformRecorder] Trigger armed.";
}

void WaveformRecorderWidget::disarmTrigger() {
    m_sampleTimer->stop();
    m_state = RecorderState::Idle;
    m_triggerState = TriggerState::Idle;

    updateButtonStates();
    updateStatusBar();

    qDebug() << "[WaveformRecorder] Trigger disarmed.";
}

void WaveformRecorderWidget::forceTrigger() {
    if (m_triggerState == TriggerState::Armed) {
        handleTrigger();
    } else if (m_state == RecorderState::Idle) {
        // 如果没有布防，直接开始录波
        startRecording();
    }
}

bool WaveformRecorderWidget::checkTriggerCondition(double currentValue,
                                                    double previousValue) {
    switch (m_triggerConfig.condition) {
        case TriggerCondition::RisingEdge:
            return previousValue < m_triggerConfig.threshold &&
                   currentValue >= m_triggerConfig.threshold;

        case TriggerCondition::FallingEdge:
            return previousValue > m_triggerConfig.threshold &&
                   currentValue <= m_triggerConfig.threshold;

        case TriggerCondition::AboveThreshold:
            return currentValue > m_triggerConfig.threshold;

        case TriggerCondition::BelowThreshold:
            return currentValue < m_triggerConfig.threshold;

        case TriggerCondition::OutOfRange:
            return currentValue > m_triggerConfig.upperLimit ||
                   currentValue < m_triggerConfig.lowerLimit;

        case TriggerCondition::InRange:
            return currentValue >= m_triggerConfig.lowerLimit &&
                   currentValue <= m_triggerConfig.upperLimit;

        default:
            return false;
    }
}

void WaveformRecorderWidget::handleTrigger() {
    m_triggerState = TriggerState::Triggered;
    m_triggerTimestamp = m_elapsedTimer.elapsed() / 1000.0;

    // 将预触发缓冲区数据刷入主数据
    flushPreTriggerBuffer();

    m_state = RecorderState::Recording;

    // 如果配置了后触发时间，启动计时器
    if (m_triggerConfig.postTriggerTime > 0) {
        m_postTriggerTimer.start();
        m_triggerState = TriggerState::PostTrigger;
    }

    updateButtonStates();
    updateStatusBar();

    emit triggerFired(m_triggerTimestamp);

    qDebug() << "[WaveformRecorder] Trigger fired at:" << m_triggerTimestamp
             << "s";
}

void WaveformRecorderWidget::flushPreTriggerBuffer() {
    // 将预触发缓冲区的数据添加到主数据中
    for (int i = 0; i < m_channels.size(); ++i) {
        while (!m_preTriggerBuffer[i].isEmpty()) {
            WaveformDataPoint point = m_preTriggerBuffer[i].dequeue();
            m_channelData[i].append(point);

            // 更新图形
            if (m_channels[i].graphIndex >= 0 &&
                m_channels[i].graphIndex < m_plot->graphCount()) {
                m_plot->graph(m_channels[i].graphIndex)
                    ->addData(point.timestamp, point.scaledValue);
            }
        }
    }
}

// ==================== 参数配置 ====================

void WaveformRecorderWidget::setSampleInterval(int ms) {
    m_sampleIntervalMs = qBound(10, ms, 10000);
    m_sampleIntervalSpin->setValue(m_sampleIntervalMs);

    // 更新预触发缓冲区大小
    m_preTriggerBufferSize =
        static_cast<int>(m_preTriggerTimeSec * 1000.0 / m_sampleIntervalMs);

    if (m_state == RecorderState::Recording) {
        m_sampleTimer->setInterval(m_sampleIntervalMs);
    }

    updateStatusBar();
}

void WaveformRecorderWidget::setTimeWindow(double seconds) {
    m_timeWindowSec = qBound(1.0, seconds, 3600.0);
    m_timeWindowSpin->setValue(m_timeWindowSec);
    m_plot->xAxis->setRange(0, m_timeWindowSec);
    m_plot->replot();
}

void WaveformRecorderWidget::setMaxDataPoints(int count) {
    m_maxDataPoints = count > 0 ? count : 0;
}

void WaveformRecorderWidget::setPreTriggerTime(double seconds) {
    m_preTriggerTimeSec = qBound(0.0, seconds, 60.0);
    m_preTriggerBufferSize =
        static_cast<int>(m_preTriggerTimeSec * 1000.0 / m_sampleIntervalMs);
}

// ==================== 数据导出 ====================

bool WaveformRecorderWidget::exportToCsv(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"),
                              tr("无法创建文件: %1").arg(filename));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    // 写入表头
    stream << "Timestamp(s)";
    for (const auto& channel : m_channels) {
        stream << "," << channel.displayName << "_Raw"
               << "," << channel.displayName << "_Scaled"
               << "(x" << channel.scale << "+" << channel.offset << ")";
    }
    stream << "\n";

    // 找到最大数据点数
    int maxPoints = 0;
    for (auto it = m_channelData.begin(); it != m_channelData.end(); ++it) {
        maxPoints = qMax(maxPoints, it.value().size());
    }

    // 写入数据
    for (int i = 0; i < maxPoints; ++i) {
        double timestamp = 0;
        if (!m_channelData.isEmpty() && i < m_channelData.first().size()) {
            timestamp = m_channelData.first()[i].timestamp;
        }

        stream << QString::number(timestamp, 'f', 3);

        for (int ch = 0; ch < m_channels.size(); ++ch) {
            if (m_channelData.contains(ch) && i < m_channelData[ch].size()) {
                const auto& point = m_channelData[ch][i];
                stream << "," << QString::number(point.rawValue, 'f', 6) << ","
                       << QString::number(point.scaledValue, 'f', 6);
            } else {
                stream << ",,";
            }
        }
        stream << "\n";
    }

    file.close();

    qDebug() << "[WaveformRecorder] Data exported to:" << filename;
    return true;
}

bool WaveformRecorderWidget::saveImage(const QString& filename, int width,
                                        int height) {
    int w = width > 0 ? width : m_plot->width();
    int h = height > 0 ? height : m_plot->height();

    bool success = false;

    if (filename.endsWith(".pdf", Qt::CaseInsensitive)) {
        success = m_plot->savePdf(filename, w, h);
    } else if (filename.endsWith(".png", Qt::CaseInsensitive)) {
        success = m_plot->savePng(filename, w, h);
    } else if (filename.endsWith(".jpg", Qt::CaseInsensitive) ||
               filename.endsWith(".jpeg", Qt::CaseInsensitive)) {
        success = m_plot->saveJpg(filename, w, h);
    } else {
        success = m_plot->savePng(filename + ".png", w, h);
    }

    if (success) {
        qDebug() << "[WaveformRecorder] Image saved to:" << filename;
    }

    return success;
}

void WaveformRecorderWidget::clearData() {
    for (auto& data : m_channelData) {
        data.clear();
    }

    for (auto& buffer : m_preTriggerBuffer) {
        buffer.clear();
    }

    for (int i = 0; i < m_plot->graphCount(); ++i) {
        m_plot->graph(i)->data()->clear();
    }

    m_plot->replot();
    updateStatusBar();

    emit dataUpdated(0);
}

// ==================== 数据回放 ====================

bool WaveformRecorderWidget::loadFromCsv(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"),
                              tr("无法打开文件: %1").arg(filename));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    // 读取表头
    QString header = stream.readLine();
    QStringList headers = header.split(',');

    if (headers.isEmpty()) {
        QMessageBox::critical(this, tr("错误"), tr("CSV文件格式错误！"));
        return false;
    }

    // 清空现有通道
    clearChannels();
    m_playbackData.clear();

    // 解析表头，每个通道有两列（Raw 和 Scaled）
    int numChannels = (headers.size() - 1) / 2;

    for (int i = 0; i < numChannels; ++i) {
        QString rawHeader = headers[1 + i * 2];
        QString channelName = rawHeader;
        if (channelName.endsWith("_Raw")) {
            channelName.chop(4);
        }
        addChannel(channelName, channelName);
    }

    // 读取数据
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        QStringList values = line.split(',');

        if (values.size() < 1 + numChannels * 2) {
            continue;
        }

        double timestamp = values[0].toDouble();

        for (int ch = 0; ch < numChannels; ++ch) {
            double rawValue = values[1 + ch * 2].toDouble();
            double scaledValue = values[2 + ch * 2].toDouble();

            WaveformDataPoint point(timestamp, rawValue, scaledValue);
            m_playbackData[ch].append(point);
        }
    }

    file.close();

    // 计算总点数
    m_totalPlaybackPoints = 0;
    for (auto it = m_playbackData.begin(); it != m_playbackData.end(); ++it) {
        m_totalPlaybackPoints = qMax(m_totalPlaybackPoints, it.value().size());
    }

    // 显示回放面板
    m_playbackPanel->setVisible(true);
    m_playbackSlider->setValue(0);
    m_playbackIndex = 0;

    // 更新时间标签
    if (m_totalPlaybackPoints > 0 && !m_playbackData.isEmpty()) {
        double totalTime = m_playbackData.first().last().timestamp;
        int totalSec = static_cast<int>(totalTime);
        m_playbackTimeLabel->setText(
            tr("00:00 / %1:%2")
                .arg(totalSec / 60, 2, 10, QChar('0'))
                .arg(totalSec % 60, 2, 10, QChar('0')));
    }

    updatePlotGraphs();

    QMessageBox::information(
        this, tr("成功"),
        tr("已加载 %1 个通道，%2 个数据点")
            .arg(numChannels)
            .arg(m_totalPlaybackPoints));

    qDebug() << "[WaveformRecorder] Loaded" << numChannels << "channels,"
             << m_totalPlaybackPoints << "points from:" << filename;

    return true;
}

bool WaveformRecorderWidget::loadFromAlarmRecording(
    const QStringList& recordedTags,
    const QVector<QPair<qint64, QMap<QString, double>>>& data,
    const QString& title) {
    
    if (recordedTags.isEmpty() || data.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("录波数据为空！"));
        return false;
    }

    // 停止当前录波
    if (m_state == RecorderState::Recording) {
        stopRecording();
    }

    // 清空现有通道和数据
    clearChannels();
    m_playbackData.clear();

    // 为每个标签创建通道
    for (int i = 0; i < recordedTags.size(); ++i) {
        const QString& tag = recordedTags[i];
        QColor color = getNextColor();
        addChannel(tag, tag, 1.0, 0.0, color);
    }

    // 计算基准时间
    qint64 baseTime = data.isEmpty() ? 0 : data.first().first;

    // 填充回放数据
    for (int ch = 0; ch < recordedTags.size(); ++ch) {
        const QString& tag = recordedTags[ch];
        QVector<WaveformDataPoint>& channelData = m_playbackData[ch];

        for (const auto& point : data) {
            double timestamp = (point.first - baseTime) / 1000.0;  // 转换为秒
            double value = point.second.value(tag, 0.0);
            channelData.append(WaveformDataPoint(timestamp, value, value));
        }
    }

    // 计算总点数
    m_totalPlaybackPoints = data.size();

    // 切换到回放模式
    m_state = RecorderState::Playback;
    
    // 显示回放面板
    if (m_playbackPanel) {
        m_playbackPanel->setVisible(true);
    }
    if (m_playbackSlider) {
        m_playbackSlider->setMaximum(m_totalPlaybackPoints > 0 ? m_totalPlaybackPoints - 1 : 0);
        m_playbackSlider->setValue(0);
    }
    m_playbackIndex = 0;

    // 更新时间标签
    if (m_totalPlaybackPoints > 0 && !m_playbackData.isEmpty()) {
        double totalTime = m_playbackData.first().last().timestamp;
        int totalSec = static_cast<int>(totalTime);
        if (m_playbackTimeLabel) {
            m_playbackTimeLabel->setText(
                tr("00:00 / %1:%2")
                    .arg(totalSec / 60, 2, 10, QChar('0'))
                    .arg(totalSec % 60, 2, 10, QChar('0')));
        }
    }

    // 更新图表
    updatePlotGraphs();
    updatePlotWithPlaybackData();
    updateButtonStates();

    QString infoTitle = title.isEmpty() ? tr("告警录波数据") : title;
    qDebug() << "[WaveformRecorder] Loaded alarm recording:" << infoTitle
             << "channels:" << recordedTags.size()
             << "points:" << m_totalPlaybackPoints;

    return true;
}

void WaveformRecorderWidget::startPlayback() {
    if (m_playbackData.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先加载回放数据！"));
        return;
    }

    m_state = RecorderState::Playback;
    m_playbackTimer->start(m_sampleIntervalMs / m_playbackSpeed);

    m_playBtn->setText(tr("⏸"));
    updateButtonStates();

    emit playbackStarted();
}

void WaveformRecorderWidget::stopPlayback() {
    m_playbackTimer->stop();
    m_state = RecorderState::Idle;
    m_playbackIndex = 0;
    m_playbackSlider->setValue(0);

    m_playBtn->setText(tr("▶"));
    updateButtonStates();

    emit playbackStopped();
}

void WaveformRecorderWidget::pausePlayback() {
    m_playbackTimer->stop();
    m_state = RecorderState::Paused;

    m_playBtn->setText(tr("▶"));
    updateButtonStates();
}

void WaveformRecorderWidget::setPlaybackPosition(double position) {
    position = qBound(0.0, position, 1.0);
    m_playbackIndex = static_cast<int>(position * (m_totalPlaybackPoints - 1));
    m_playbackSlider->setValue(static_cast<int>(position * 1000));

    // 更新显示
    updatePlotWithPlaybackData();
}

void WaveformRecorderWidget::setPlaybackSpeed(double speed) {
    m_playbackSpeed = qBound(0.1, speed, 10.0);
    m_playbackSpeedSpin->setValue(m_playbackSpeed);

    if (m_state == RecorderState::Playback) {
        m_playbackTimer->setInterval(
            static_cast<int>(m_sampleIntervalMs / m_playbackSpeed));
    }
}

// ==================== 私有槽函数 ====================

void WaveformRecorderWidget::onSampleTimer() {
    if (m_state == RecorderState::WaitingTrigger) {
        samplePreTriggerData();
    } else if (m_state == RecorderState::Recording) {
        sampleData();

        // 检查后触发时间
        if (m_triggerState == TriggerState::PostTrigger) {
            double elapsed = m_postTriggerTimer.elapsed() / 1000.0;
            if (elapsed >= m_triggerConfig.postTriggerTime) {
                m_triggerState = TriggerState::Complete;

                if (m_triggerConfig.singleShot) {
                    stopRecording();
                } else {
                    // 重新布防
                    armTrigger();
                }

                emit triggerComplete();
            }
        }
    }
}

void WaveformRecorderWidget::samplePreTriggerData() {
    if (!m_dataModel) {
        return;
    }

    double timestamp = m_elapsedTimer.elapsed() / 1000.0;

    // 采样所有通道
    for (int i = 0; i < m_channels.size(); ++i) {
        const auto& channel = m_channels[i];

        if (!channel.enabled) {
            continue;
        }

        DataPoint point = m_dataModel->readPoint(channel.tagName);

        if (point.quality == DataQuality::Good ||
            point.quality == DataQuality::Uncertain) {
            double rawValue = point.value.toDouble();
            double scaledValue = channel.transform(rawValue);

            // 添加到预触发缓冲
            WaveformDataPoint wfPoint(timestamp, rawValue, scaledValue);
            m_preTriggerBuffer[i].enqueue(wfPoint);

            // 限制缓冲区大小
            while (m_preTriggerBuffer[i].size() > m_preTriggerBufferSize) {
                m_preTriggerBuffer[i].dequeue();
            }

            // 检查触发条件
            if (i == m_triggerConfig.channelIndex &&
                m_triggerState == TriggerState::Armed) {
                if (checkTriggerCondition(scaledValue, m_lastTriggerValue)) {
                    handleTrigger();
                }
                m_lastTriggerValue = scaledValue;
            }
        }
    }

    updateStatusBar();
}

void WaveformRecorderWidget::sampleData() {
    if (!m_dataModel || m_state != RecorderState::Recording) {
        return;
    }

    double timestamp = m_elapsedTimer.elapsed() / 1000.0;

    bool dataUpdated = false;

    for (int i = 0; i < m_channels.size(); ++i) {
        const auto& channel = m_channels[i];

        if (!channel.enabled) {
            continue;
        }

        // 从数据模型获取数据
        DataPoint point = m_dataModel->readPoint(channel.tagName);

        if (point.quality == DataQuality::Good ||
            point.quality == DataQuality::Uncertain) {
            double rawValue = point.value.toDouble();
            double scaledValue = channel.transform(rawValue);

            // 存储数据
            m_channelData[i].append(
                WaveformDataPoint(timestamp, rawValue, scaledValue));

            // 限制数据点数
            if (m_maxDataPoints > 0 &&
                m_channelData[i].size() > m_maxDataPoints) {
                m_channelData[i].removeFirst();
            }

            // 更新图形
            if (channel.graphIndex >= 0 &&
                channel.graphIndex < m_plot->graphCount()) {
                m_plot->graph(channel.graphIndex)
                    ->addData(timestamp, scaledValue);

                // 限制图形数据点
                if (m_maxDataPoints > 0) {
                    m_plot->graph(channel.graphIndex)
                        ->data()
                        ->removeBefore(timestamp - m_timeWindowSec * 10);
                }
            }

            dataUpdated = true;
        }
    }

    if (dataUpdated) {
        // 自动滚动
        if (m_autoScroll) {
            double rangeEnd = timestamp;
            double rangeStart = qMax(0.0, rangeEnd - m_timeWindowSec);
            m_plot->xAxis->setRange(rangeStart, rangeEnd);
        }

        // 自动调整Y轴范围
        m_plot->yAxis->rescale(true);
        m_plot->yAxis2->rescale(true);

        m_plot->replot(QCustomPlot::rpQueuedReplot);

        updateStatusBar();

        int totalPoints = 0;
        for (auto it = m_channelData.begin(); it != m_channelData.end();
             ++it) {
            totalPoints += it.value().size();
        }
        emit this->dataUpdated(totalPoints);
    }
}

void WaveformRecorderWidget::onPlaybackTimer() {
    if (m_playbackIndex >= m_totalPlaybackPoints) {
        stopPlayback();
        return;
    }

    updatePlotWithPlaybackData();

    m_playbackIndex++;

    // 更新滑块
    double position =
        static_cast<double>(m_playbackIndex) / (m_totalPlaybackPoints - 1);
    m_playbackSlider->blockSignals(true);
    m_playbackSlider->setValue(static_cast<int>(position * 1000));
    m_playbackSlider->blockSignals(false);

    // 更新时间标签
    if (!m_playbackData.isEmpty() &&
        m_playbackIndex < m_playbackData.first().size()) {
        double currentTime = m_playbackData.first()[m_playbackIndex].timestamp;
        double totalTime = m_playbackData.first().last().timestamp;

        int currentSec = static_cast<int>(currentTime);
        int totalSec = static_cast<int>(totalTime);

        m_playbackTimeLabel->setText(
            tr("%1:%2 / %3:%4")
                .arg(currentSec / 60, 2, 10, QChar('0'))
                .arg(currentSec % 60, 2, 10, QChar('0'))
                .arg(totalSec / 60, 2, 10, QChar('0'))
                .arg(totalSec % 60, 2, 10, QChar('0')));
    }

    emit playbackPositionChanged(position);
}

void WaveformRecorderWidget::updatePlotWithPlaybackData() {
    // 清空现有数据
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        m_plot->graph(i)->data()->clear();
    }

    // 计算显示范围
    double currentTime = 0;
    if (!m_playbackData.isEmpty() &&
        m_playbackIndex < m_playbackData.first().size()) {
        currentTime = m_playbackData.first()[m_playbackIndex].timestamp;
    }

    double rangeStart = qMax(0.0, currentTime - m_timeWindowSec);
    double rangeEnd = currentTime;

    // 加载数据到图形
    for (int ch = 0; ch < m_channels.size(); ++ch) {
        if (!m_playbackData.contains(ch)) continue;
        if (m_channels[ch].graphIndex < 0) continue;

        QCPGraph* graph = m_plot->graph(m_channels[ch].graphIndex);
        if (!graph) continue;

        for (int i = 0; i <= m_playbackIndex && i < m_playbackData[ch].size();
             ++i) {
            const auto& point = m_playbackData[ch][i];
            if (point.timestamp >= rangeStart) {
                graph->addData(point.timestamp, point.scaledValue);
            }
        }
    }

    m_plot->xAxis->setRange(rangeStart, rangeEnd);
    m_plot->yAxis->rescale(true);
    m_plot->yAxis2->rescale(true);
    m_plot->replot();
}

void WaveformRecorderWidget::onAddChannelClicked() {
    QStringList availableTags = getAvailableTags();

    // 创建选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("添加录波通道"));
    dialog.setMinimumWidth(450);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // 标签选择
    QFormLayout* formLayout = new QFormLayout();

    QComboBox* tagCombo = new QComboBox(&dialog);
    tagCombo->addItems(availableTags);
    tagCombo->setEditable(true);
    formLayout->addRow(tr("数据点:"), tagCombo);

    QLineEdit* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(tr("（可选）"));
    formLayout->addRow(tr("显示名称:"), nameEdit);

    QDoubleSpinBox* scaleSpin = new QDoubleSpinBox(&dialog);
    scaleSpin->setRange(-1e10, 1e10);
    scaleSpin->setDecimals(6);
    scaleSpin->setValue(1.0);
    formLayout->addRow(tr("倍率:"), scaleSpin);

    QDoubleSpinBox* offsetSpin = new QDoubleSpinBox(&dialog);
    offsetSpin->setRange(-1e10, 1e10);
    offsetSpin->setDecimals(6);
    offsetSpin->setValue(0.0);
    formLayout->addRow(tr("偏移量:"), offsetSpin);

    QComboBox* yAxisCombo = new QComboBox(&dialog);
    yAxisCombo->addItem(tr("左轴"), static_cast<int>(YAxisType::Left));
    yAxisCombo->addItem(tr("右轴"), static_cast<int>(YAxisType::Right));
    formLayout->addRow(tr("Y轴:"), yAxisCombo);

    layout->addLayout(formLayout);

    // 说明
    QLabel* formulaLabel =
        new QLabel(tr("转换公式: 显示值 = 原始值 × 倍率 + 偏移量"), &dialog);
    formulaLabel->setStyleSheet("color: #64748B; font-style: italic;");
    layout->addWidget(formulaLabel);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        QString tagName = tagCombo->currentText();
        QString displayName = nameEdit->text();
        double scale = scaleSpin->value();
        double offset = offsetSpin->value();

        if (!tagName.isEmpty()) {
            int idx = addChannel(tagName, displayName, scale, offset);
            if (idx >= 0) {
                YAxisType yAxisType = static_cast<YAxisType>(
                    yAxisCombo->currentData().toInt());
                setChannelYAxis(idx, yAxisType);
            }
        }
    }
}

void WaveformRecorderWidget::onRemoveChannelClicked() {
    int row = m_channelTable->currentRow();
    if (row >= 0) {
        removeChannel(row);
    }
}

void WaveformRecorderWidget::onStartClicked() {
    if (m_state == RecorderState::Paused) {
        resumeRecording();
    } else {
        startRecording();
    }
}

void WaveformRecorderWidget::onPauseClicked() {
    if (m_state == RecorderState::Recording) {
        pauseRecording();
    } else if (m_state == RecorderState::Paused) {
        resumeRecording();
    }
}

void WaveformRecorderWidget::onStopClicked() {
    stopRecording();
    disarmTrigger();
}

void WaveformRecorderWidget::onExportClicked() {
    QString filename = QFileDialog::getSaveFileName(this, tr("导出CSV"),
                                                     QString(),
                                                     tr("CSV文件 (*.csv)"));
    if (!filename.isEmpty()) {
        if (exportToCsv(filename)) {
            QMessageBox::information(this, tr("成功"),
                                     tr("数据已导出到: %1").arg(filename));
        }
    }
}

void WaveformRecorderWidget::onSaveImageClicked() {
    QString filename = QFileDialog::getSaveFileName(
        this, tr("保存图片"), QString(),
        tr("PNG图片 (*.png);;JPEG图片 (*.jpg);;PDF文档 (*.pdf)"));
    if (!filename.isEmpty()) {
        if (saveImage(filename)) {
            QMessageBox::information(this, tr("成功"),
                                     tr("图片已保存到: %1").arg(filename));
        }
    }
}

void WaveformRecorderWidget::onClearDataClicked() {
    if (QMessageBox::question(this, tr("确认"),
                               tr("确定要清空所有录波数据吗？"),
                               QMessageBox::Yes | QMessageBox::No) ==
        QMessageBox::Yes) {
        clearData();
    }
}

void WaveformRecorderWidget::onTriggerConfigClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("触发配置"));
    dialog.setMinimumWidth(400);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QFormLayout* formLayout = new QFormLayout();

    // 启用触发
    QCheckBox* enableCheck = new QCheckBox(&dialog);
    enableCheck->setChecked(m_triggerConfig.enabled);
    formLayout->addRow(tr("启用触发:"), enableCheck);

    // 触发通道
    QComboBox* channelCombo = new QComboBox(&dialog);
    for (int i = 0; i < m_channels.size(); ++i) {
        channelCombo->addItem(m_channels[i].displayName, i);
    }
    if (m_triggerConfig.channelIndex < m_channels.size()) {
        channelCombo->setCurrentIndex(m_triggerConfig.channelIndex);
    }
    formLayout->addRow(tr("触发通道:"), channelCombo);

    // 触发条件
    QComboBox* conditionCombo = new QComboBox(&dialog);
    conditionCombo->addItem(tr("上升沿"),
                            static_cast<int>(TriggerCondition::RisingEdge));
    conditionCombo->addItem(tr("下降沿"),
                            static_cast<int>(TriggerCondition::FallingEdge));
    conditionCombo->addItem(tr("高于阈值"),
                            static_cast<int>(TriggerCondition::AboveThreshold));
    conditionCombo->addItem(tr("低于阈值"),
                            static_cast<int>(TriggerCondition::BelowThreshold));
    conditionCombo->addItem(tr("超出范围"),
                            static_cast<int>(TriggerCondition::OutOfRange));
    conditionCombo->addItem(tr("进入范围"),
                            static_cast<int>(TriggerCondition::InRange));
    conditionCombo->setCurrentIndex(
        static_cast<int>(m_triggerConfig.condition) - 1);
    formLayout->addRow(tr("触发条件:"), conditionCombo);

    // 阈值
    QDoubleSpinBox* thresholdSpin = new QDoubleSpinBox(&dialog);
    thresholdSpin->setRange(-1e10, 1e10);
    thresholdSpin->setDecimals(4);
    thresholdSpin->setValue(m_triggerConfig.threshold);
    formLayout->addRow(tr("阈值:"), thresholdSpin);

    // 上限/下限
    QDoubleSpinBox* upperSpin = new QDoubleSpinBox(&dialog);
    upperSpin->setRange(-1e10, 1e10);
    upperSpin->setValue(m_triggerConfig.upperLimit);
    formLayout->addRow(tr("上限:"), upperSpin);

    QDoubleSpinBox* lowerSpin = new QDoubleSpinBox(&dialog);
    lowerSpin->setRange(-1e10, 1e10);
    lowerSpin->setValue(m_triggerConfig.lowerLimit);
    formLayout->addRow(tr("下限:"), lowerSpin);

    // 预触发时间
    QDoubleSpinBox* preTriggerSpin = new QDoubleSpinBox(&dialog);
    preTriggerSpin->setRange(0, 60);
    preTriggerSpin->setSuffix(" s");
    preTriggerSpin->setValue(m_triggerConfig.preTriggerTime);
    formLayout->addRow(tr("预触发时间:"), preTriggerSpin);

    // 后触发时间
    QDoubleSpinBox* postTriggerSpin = new QDoubleSpinBox(&dialog);
    postTriggerSpin->setRange(0, 3600);
    postTriggerSpin->setSuffix(" s");
    postTriggerSpin->setValue(m_triggerConfig.postTriggerTime);
    postTriggerSpin->setSpecialValueText(tr("持续录波"));
    formLayout->addRow(tr("后触发时间:"), postTriggerSpin);

    // 单次触发
    QCheckBox* singleShotCheck = new QCheckBox(&dialog);
    singleShotCheck->setChecked(m_triggerConfig.singleShot);
    formLayout->addRow(tr("单次触发:"), singleShotCheck);

    layout->addLayout(formLayout);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        TriggerConfig config;
        config.enabled = enableCheck->isChecked();
        config.channelIndex = channelCombo->currentData().toInt();
        config.condition = static_cast<TriggerCondition>(
            conditionCombo->currentData().toInt());
        config.threshold = thresholdSpin->value();
        config.upperLimit = upperSpin->value();
        config.lowerLimit = lowerSpin->value();
        config.preTriggerTime = preTriggerSpin->value();
        config.postTriggerTime = postTriggerSpin->value();
        config.singleShot = singleShotCheck->isChecked();

        setTriggerConfig(config);
    }
}

void WaveformRecorderWidget::onArmTriggerClicked() {
    if (m_triggerState == TriggerState::Armed ||
        m_state == RecorderState::WaitingTrigger) {
        disarmTrigger();
    } else {
        armTrigger();
    }
}

void WaveformRecorderWidget::onForceTriggerClicked() { forceTrigger(); }

void WaveformRecorderWidget::onLoadFileClicked() {
    QString filename = QFileDialog::getOpenFileName(this, tr("加载CSV文件"),
                                                     QString(),
                                                     tr("CSV文件 (*.csv)"));
    if (!filename.isEmpty()) {
        loadFromCsv(filename);
    }
}

void WaveformRecorderWidget::onPlaybackSliderChanged(int value) {
    if (m_state != RecorderState::Playback) {
        setPlaybackPosition(value / 1000.0);
    }
}

void WaveformRecorderWidget::onPlaybackSpeedChanged(double value) {
    setPlaybackSpeed(value);
}

void WaveformRecorderWidget::onChannelTableChanged(int row, int column) {
    if (row < 0 || row >= m_channels.size()) {
        return;
    }

    QTableWidgetItem* item = m_channelTable->item(row, column);
    if (!item) {
        return;
    }

    switch (column) {
        case 0:  // 启用
            setChannelEnabled(row, item->checkState() == Qt::Checked);
            break;
        case 2:  // 倍率
        {
            bool ok;
            double scale = item->text().toDouble(&ok);
            if (ok) {
                updateChannelTransform(row, scale, m_channels[row].offset);
            }
        } break;
        case 3:  // 偏移
        {
            bool ok;
            double offset = item->text().toDouble(&ok);
            if (ok) {
                updateChannelTransform(row, m_channels[row].scale, offset);
            }
        } break;
        case 5:  // 颜色
        {
            QColor color =
                QColorDialog::getColor(m_channels[row].color, this, tr("选择颜色"));
            if (color.isValid()) {
                setChannelColor(row, color);
            }
        } break;
    }
}

void WaveformRecorderWidget::onAutoScrollChanged(bool checked) {
    m_autoScroll = checked;
}

void WaveformRecorderWidget::onSampleIntervalChanged(int value) {
    setSampleInterval(value);
}

void WaveformRecorderWidget::onTimeWindowChanged(double value) {
    setTimeWindow(value);
}

// ==================== 私有方法 ====================

void WaveformRecorderWidget::updateChannelTable() {
    m_channelTable->blockSignals(true);
    m_channelTable->setRowCount(m_channels.size());

    for (int i = 0; i < m_channels.size(); ++i) {
        const auto& channel = m_channels[i];

        // 启用复选框
        QTableWidgetItem* enableItem = new QTableWidgetItem();
        enableItem->setCheckState(channel.enabled ? Qt::Checked
                                                  : Qt::Unchecked);
        enableItem->setFlags(enableItem->flags() | Qt::ItemIsUserCheckable);
        m_channelTable->setItem(i, 0, enableItem);

        // 名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(channel.displayName);
        nameItem->setToolTip(tr("标签: %1").arg(channel.tagName));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_channelTable->setItem(i, 1, nameItem);

        // 倍率
        QTableWidgetItem* scaleItem =
            new QTableWidgetItem(QString::number(channel.scale));
        m_channelTable->setItem(i, 2, scaleItem);

        // 偏移
        QTableWidgetItem* offsetItem =
            new QTableWidgetItem(QString::number(channel.offset));
        m_channelTable->setItem(i, 3, offsetItem);

        // Y轴
        QString yAxisStr;
        switch (channel.yAxisType) {
            case YAxisType::Left:
                yAxisStr = tr("左");
                break;
            case YAxisType::Right:
                yAxisStr = tr("右");
                break;
            case YAxisType::Independent:
                yAxisStr = tr("独立");
                break;
        }
        QTableWidgetItem* yAxisItem = new QTableWidgetItem(yAxisStr);
        yAxisItem->setFlags(yAxisItem->flags() & ~Qt::ItemIsEditable);
        m_channelTable->setItem(i, 4, yAxisItem);

        // 颜色
        QTableWidgetItem* colorItem = new QTableWidgetItem();
        colorItem->setBackground(channel.color);
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
        m_channelTable->setItem(i, 5, colorItem);
    }

    m_channelTable->blockSignals(false);
}

void WaveformRecorderWidget::updatePlotGraphs() {
    // 清除所有图形
    m_plot->clearGraphs();

    // 为每个通道创建图形
    for (int i = 0; i < m_channels.size(); ++i) {
        auto& channel = m_channels[i];

        QCPGraph* graph = m_plot->addGraph();

        // 设置Y轴
        switch (channel.yAxisType) {
            case YAxisType::Left:
                graph->setValueAxis(m_plot->yAxis);
                break;
            case YAxisType::Right:
                graph->setValueAxis(m_plot->yAxis2);
                break;
            case YAxisType::Independent:
                // 使用右轴作为独立轴的基础
                graph->setValueAxis(m_plot->yAxis2);
                break;
        }

        graph->setName(channel.displayName);
        graph->setPen(QPen(channel.color, 2));
        graph->setVisible(channel.enabled);
        graph->setAntialiased(true);

        channel.graphIndex = m_plot->graphCount() - 1;

        // 如果有数据，加载数据
        if (m_channelData.contains(i)) {
            QVector<double> keys, values;
            for (const auto& point : m_channelData[i]) {
                keys.append(point.timestamp);
                values.append(point.scaledValue);
            }
            graph->setData(keys, values);
        }
    }

    m_plot->replot();
}

void WaveformRecorderWidget::updateYAxes() {
    // 检查是否需要右轴
    bool needRightAxis = false;
    for (const auto& channel : m_channels) {
        if (channel.yAxisType == YAxisType::Right ||
            channel.yAxisType == YAxisType::Independent) {
            needRightAxis = true;
            break;
        }
    }

    m_plot->yAxis2->setVisible(needRightAxis);
}

void WaveformRecorderWidget::updateStatusBar() {
    // 状态
    QString stateText;
    switch (m_state) {
        case RecorderState::Idle:
            stateText = tr("状态: 空闲");
            break;
        case RecorderState::Recording:
            stateText = tr("状态: 🔴 录波中");
            break;
        case RecorderState::Paused:
            stateText = tr("状态: ⏸ 暂停");
            break;
        case RecorderState::WaitingTrigger:
            stateText = tr("状态: 🎯 等待触发");
            break;
        case RecorderState::Playback:
            stateText = tr("状态: 📼 回放");
            break;
    }

    // 添加触发状态
    if (m_triggerState == TriggerState::Armed) {
        stateText += tr(" [已布防]");
    } else if (m_triggerState == TriggerState::Triggered) {
        stateText += tr(" [已触发]");
    }

    m_stateLabel->setText(stateText);

    // 采样率
    m_sampleRateLabel->setText(tr("采样率: %1ms").arg(m_sampleIntervalMs));

    // 数据点数
    int totalPoints = 0;
    for (auto it = m_channelData.begin(); it != m_channelData.end(); ++it) {
        totalPoints += it.value().size();
    }
    m_dataPointsLabel->setText(tr("数据点: %1").arg(totalPoints));

    // 录波时长
    if (m_state == RecorderState::Recording ||
        m_state == RecorderState::WaitingTrigger ||
        m_state == RecorderState::Paused) {
        qint64 elapsed = m_elapsedTimer.elapsed();
        int hours = elapsed / 3600000;
        int minutes = (elapsed % 3600000) / 60000;
        int seconds = (elapsed % 60000) / 1000;
        m_elapsedTimeLabel->setText(tr("录波时长: %1:%2:%3")
                                        .arg(hours, 2, 10, QChar('0'))
                                        .arg(minutes, 2, 10, QChar('0'))
                                        .arg(seconds, 2, 10, QChar('0')));
    } else {
        m_elapsedTimeLabel->setText(tr("录波时长: 00:00:00"));
    }

    // 更新触发状态
    if (m_triggerConfig.enabled) {
        m_triggerStateLabel->setText(
            tr("触发: %1 | %2")
                .arg(triggerConditionToString(m_triggerConfig.condition))
                .arg(triggerStateToString(m_triggerState)));
        m_triggerStateLabel->setStyleSheet(
            m_triggerState == TriggerState::Armed
                ? "QLabel { padding: 8px; background: #FEF3C7; border-radius: "
                  "4px; color: #92400E; }"
                : m_triggerState == TriggerState::Triggered
                      ? "QLabel { padding: 8px; background: #DCFCE7; "
                        "border-radius: 4px; color: #166534; }"
                      : "QLabel { padding: 8px; background: #F1F5F9; "
                        "border-radius: 4px; }");
    }
}

void WaveformRecorderWidget::updateButtonStates() {
    bool isIdle = (m_state == RecorderState::Idle);
    bool isRecording = (m_state == RecorderState::Recording);
    bool isPaused = (m_state == RecorderState::Paused);
    bool isWaiting = (m_state == RecorderState::WaitingTrigger);
    bool isPlayback = (m_state == RecorderState::Playback);

    m_startBtn->setEnabled(isIdle || isPaused);
    m_startBtn->setText(isPaused ? tr("▶ 继续") : tr("▶ 开始"));

    m_pauseBtn->setEnabled(isRecording || isPaused);
    m_pauseBtn->setText(isPaused ? tr("▶ 继续") : tr("⏸ 暂停"));

    m_stopBtn->setEnabled(!isIdle && !isPlayback);

    m_addChannelBtn->setEnabled(isIdle);
    m_removeChannelBtn->setEnabled(isIdle &&
                                   m_channelTable->currentRow() >= 0);

    m_sampleIntervalSpin->setEnabled(isIdle);

    // 触发按钮
    m_armTriggerBtn->setEnabled(isIdle || isWaiting);
    m_armTriggerBtn->setText(isWaiting ? tr("🔓 取消布防") : tr("🎯 布防"));
    m_forceTriggerBtn->setEnabled(isIdle || isWaiting);

    // 回放按钮
    m_loadFileBtn->setEnabled(isIdle);
}

QColor WaveformRecorderWidget::getNextColor() {
    QColor color = s_defaultColors[m_colorIndex % s_defaultColors.size()];
    m_colorIndex++;
    return color;
}

QString WaveformRecorderWidget::triggerConditionToString(
    TriggerCondition cond) const {
    switch (cond) {
        case TriggerCondition::None:
            return tr("无");
        case TriggerCondition::RisingEdge:
            return tr("上升沿");
        case TriggerCondition::FallingEdge:
            return tr("下降沿");
        case TriggerCondition::AboveThreshold:
            return tr("高于阈值");
        case TriggerCondition::BelowThreshold:
            return tr("低于阈值");
        case TriggerCondition::OutOfRange:
            return tr("超出范围");
        case TriggerCondition::InRange:
            return tr("进入范围");
        default:
            return tr("未知");
    }
}

QString WaveformRecorderWidget::triggerStateToString(TriggerState state) const {
    switch (state) {
        case TriggerState::Idle:
            return tr("空闲");
        case TriggerState::Armed:
            return tr("已布防");
        case TriggerState::Triggered:
            return tr("已触发");
        case TriggerState::PostTrigger:
            return tr("后触发");
        case TriggerState::Complete:
            return tr("完成");
        default:
            return tr("未知");
    }
}
