/**
 * @file WaveformRecorder.h
 * @brief 指定点位录波功能 - 支持多通道、倍率、偏移量配置
 * @features 触发录波、预触发缓存、多Y轴、数据回放
 */

#ifndef WAVEFORMRECORDER_H
#define WAVEFORMRECORDER_H

#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QQueue>
#include <QTimer>
#include <QWidget>

class QCustomPlot;
class QCPGraph;
class QCPAxis;
class QCPAxisRect;
class QSplitter;
class QTableWidget;
class QToolBar;
class QStatusBar;
class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QCheckBox;
class QComboBox;
class QSlider;
class QGroupBox;

namespace ModbusPlexLink {
class UniversalDataModel;
}

// ============================================================================
// 触发条件相关
// ============================================================================

/**
 * @brief 触发条件类型
 */
enum class TriggerCondition {
  None,            ///< 无触发（手动录波）
  RisingEdge,      ///< 上升沿（值从低于阈值变为高于阈值）
  FallingEdge,     ///< 下降沿（值从高于阈值变为低于阈值）
  AboveThreshold,  ///< 高于阈值
  BelowThreshold,  ///< 低于阈值
  OutOfRange,      ///< 超出范围（高于上限或低于下限）
  InRange          ///< 进入范围（在上下限之间）
};

/**
 * @brief 触发配置结构
 */
struct TriggerConfig {
  bool enabled;                ///< 是否启用触发
  int channelIndex;            ///< 触发通道索引
  TriggerCondition condition;  ///< 触发条件
  double threshold;            ///< 阈值（用于单阈值条件）
  double upperLimit;           ///< 上限（用于范围条件）
  double lowerLimit;           ///< 下限（用于范围条件）
  double preTriggerTime;       ///< 预触发时间（秒）
  double postTriggerTime;      ///< 后触发时间（秒），0表示持续录波
  bool singleShot;             ///< 单次触发模式

  TriggerConfig()
      : enabled(false),
        channelIndex(0),
        condition(TriggerCondition::None),
        threshold(0.0),
        upperLimit(100.0),
        lowerLimit(0.0),
        preTriggerTime(5.0),
        postTriggerTime(0.0),
        singleShot(false) {}
};

/**
 * @brief 触发状态
 */
enum class TriggerState {
  Idle,         ///< 空闲，等待触发
  Armed,        ///< 已布防，正在监测
  Triggered,    ///< 已触发，正在录波
  PostTrigger,  ///< 后触发阶段
  Complete      ///< 触发完成
};

// ============================================================================
// 通道配置
// ============================================================================

/**
 * @brief Y轴配置
 */
enum class YAxisType {
  Left,        ///< 使用左侧Y轴（默认）
  Right,       ///< 使用右侧Y轴
  Independent  ///< 使用独立Y轴
};

/**
 * @brief 录波通道配置结构
 */
struct WaveformChannel {
  QString tagName;      ///< 数据点标签名（从UDM获取数据）
  QString displayName;  ///< 显示名称
  double scale;   ///< 倍率 (转换公式: displayValue = rawValue * scale + offset)
  double offset;  ///< 偏移量
  QColor color;   ///< 波形颜色
  bool enabled;   ///< 是否启用
  int graphIndex;  ///< 对应的 QCPGraph 索引 (-1 表示未创建)

  // 多Y轴支持
  YAxisType yAxisType;  ///< Y轴类型
  double yMin;          ///< Y轴最小值（用于独立Y轴）
  double yMax;          ///< Y轴最大值（用于独立Y轴）
  int yAxisIndex;       ///< Y轴索引（运行时使用）

  WaveformChannel()
      : scale(1.0),
        offset(0.0),
        color(Qt::blue),
        enabled(true),
        graphIndex(-1),
        yAxisType(YAxisType::Left),
        yMin(-100),
        yMax(100),
        yAxisIndex(0) {}

  WaveformChannel(const QString& tag, const QString& name = QString(),
                  double s = 1.0, double o = 0.0, const QColor& c = Qt::blue)
      : tagName(tag),
        displayName(name.isEmpty() ? tag : name),
        scale(s),
        offset(o),
        color(c),
        enabled(true),
        graphIndex(-1),
        yAxisType(YAxisType::Left),
        yMin(-100),
        yMax(100),
        yAxisIndex(0) {}

  /// 应用倍率和偏移量转换
  double transform(double rawValue) const { return rawValue * scale + offset; }
};

/**
 * @brief 录波数据点结构
 */
struct WaveformDataPoint {
  double timestamp;    ///< 相对时间（秒）
  double rawValue;     ///< 原始值
  double scaledValue;  ///< 转换后的值

  WaveformDataPoint(double t = 0.0, double raw = 0.0, double scaled = 0.0)
      : timestamp(t), rawValue(raw), scaledValue(scaled) {}
};

/**
 * @brief 录波器状态枚举
 */
enum class RecorderState {
  Idle,            ///< 空闲
  Recording,       ///< 录波中
  Paused,          ///< 暂停
  WaitingTrigger,  ///< 等待触发
  Playback         ///< 回放模式
};

// ============================================================================
// 录波器控件
// ============================================================================

/**
 * @brief 指定点位录波控件
 *
 * 功能特点：
 * - 支持多通道同时录波
 * - 每个通道可配置倍率(Scale)和偏移量(Offset)
 * - 实时滚动波形显示
 * - 支持暂停/继续
 * - 数据导出CSV
 * - 波形图片保存
 * - 触发录波（条件触发）
 * - 预触发缓存
 * - 多Y轴支持
 * - 数据回放
 */
class WaveformRecorderWidget : public QWidget {
  Q_OBJECT

 public:
  explicit WaveformRecorderWidget(QWidget* parent = nullptr);
  ~WaveformRecorderWidget();

  // ==================== 数据源设置 ====================

  void setDataModel(ModbusPlexLink::UniversalDataModel* model);
  QStringList getAvailableTags() const;

  // ==================== 通道管理 ====================

  int addChannel(const QString& tagName, const QString& displayName = QString(),
                 double scale = 1.0, double offset = 0.0,
                 const QColor& color = QColor());

  void removeChannel(int index);
  void clearChannels();
  int channelCount() const { return m_channels.size(); }

  WaveformChannel& channel(int index) { return m_channels[index]; }
  const WaveformChannel& channel(int index) const { return m_channels[index]; }

  void updateChannelTransform(int index, double scale, double offset);
  void setChannelColor(int index, const QColor& color);
  void setChannelEnabled(int index, bool enabled);

  // ==================== 多Y轴支持 ====================

  /**
   * @brief 设置通道的Y轴类型
   * @param index 通道索引
   * @param type Y轴类型
   * @param yMin Y轴最小值（仅用于独立Y轴）
   * @param yMax Y轴最大值（仅用于独立Y轴）
   */
  void setChannelYAxis(int index, YAxisType type, double yMin = -100,
                       double yMax = 100);

  // ==================== 录波控制 ====================

  void startRecording();
  void stopRecording();
  void pauseRecording();
  void resumeRecording();

  RecorderState state() const { return m_state; }
  bool isRecording() const { return m_state == RecorderState::Recording; }

  // ==================== 触发录波 ====================

  /**
   * @brief 设置触发配置
   */
  void setTriggerConfig(const TriggerConfig& config);

  /**
   * @brief 获取触发配置
   */
  TriggerConfig triggerConfig() const { return m_triggerConfig; }

  /**
   * @brief 启动触发模式（等待触发）
   */
  void armTrigger();

  /**
   * @brief 取消触发模式
   */
  void disarmTrigger();

  /**
   * @brief 强制触发（手动触发）
   */
  void forceTrigger();

  /**
   * @brief 获取触发状态
   */
  TriggerState triggerState() const { return m_triggerState; }

  // ==================== 参数配置 ====================

  void setSampleInterval(int ms);
  int sampleInterval() const { return m_sampleIntervalMs; }

  void setTimeWindow(double seconds);
  double timeWindow() const { return m_timeWindowSec; }

  void setMaxDataPoints(int count);

  /**
   * @brief 设置预触发缓存时间
   * @param seconds 预触发时间（秒）
   */
  void setPreTriggerTime(double seconds);
  double preTriggerTime() const { return m_preTriggerTimeSec; }

  // ==================== 数据导出 ====================

  bool exportToCsv(const QString& filename);
  bool saveImage(const QString& filename, int width = 0, int height = 0);
  void clearData();

  // ==================== 数据回放 ====================

  /**
   * @brief 从CSV文件加载数据进行回放
   * @param filename CSV文件路径
   * @return 是否成功
   */
  bool loadFromCsv(const QString& filename);

  /**
   * @brief 从告警录波数据加载进行回放
   * @param recordedTags 录波的标签列表
   * @param data 录波数据点列表 (timestamp, values map)
   * @param title 可选的标题
   * @return 是否成功
   */
  bool loadFromAlarmRecording(const QStringList& recordedTags,
                              const QVector<QPair<qint64, QMap<QString, double>>>& data,
                              const QString& title = QString());

  /**
   * @brief 开始回放
   */
  void startPlayback();

  /**
   * @brief 停止回放
   */
  void stopPlayback();

  /**
   * @brief 暂停回放
   */
  void pausePlayback();

  /**
   * @brief 设置回放位置
   * @param position 位置比例（0.0-1.0）
   */
  void setPlaybackPosition(double position);

  /**
   * @brief 设置回放速度
   * @param speed 速度倍率（0.1x - 10x）
   */
  void setPlaybackSpeed(double speed);

  /**
   * @brief 是否处于回放模式
   */
  bool isPlaybackMode() const { return m_state == RecorderState::Playback; }

 signals:
  void recordingStarted();
  void recordingStopped();
  void recordingPaused();
  void recordingResumed();
  void dataUpdated(int pointCount);
  void channelsChanged();

  // 触发相关信号
  void triggerArmed();
  void triggerFired(double timestamp);
  void triggerComplete();

  // 回放相关信号
  void playbackStarted();
  void playbackStopped();
  void playbackPositionChanged(double position);

 private slots:
  void onSampleTimer();
  void onAddChannelClicked();
  void onRemoveChannelClicked();
  void onStartClicked();
  void onPauseClicked();
  void onStopClicked();
  void onExportClicked();
  void onSaveImageClicked();
  void onClearDataClicked();
  void onChannelTableChanged(int row, int column);
  void onAutoScrollChanged(bool checked);
  void onSampleIntervalChanged(int value);
  void onTimeWindowChanged(double value);

  // 触发相关槽
  void onTriggerConfigClicked();
  void onArmTriggerClicked();
  void onForceTriggerClicked();

  // 回放相关槽
  void onLoadFileClicked();
  void onPlaybackTimer();
  void onPlaybackSliderChanged(int value);
  void onPlaybackSpeedChanged(double value);
  void updatePlotWithPlaybackData();

 private:
  void setupUi();
  void setupToolBar();
  void setupChannelTable();
  void setupPlot();
  void setupStatusBar();
  void setupTriggerPanel();
  void setupPlaybackPanel();
  void applyStyles();

  void updateChannelTable();
  void updatePlotGraphs();
  void updateStatusBar();
  void updateButtonStates();
  void updateYAxes();

  void sampleData();
  void samplePreTriggerData();
  bool checkTriggerCondition(double currentValue, double previousValue);
  void handleTrigger();
  void flushPreTriggerBuffer();

  QColor getNextColor();
  QString triggerConditionToString(TriggerCondition cond) const;
  QString triggerStateToString(TriggerState state) const;

 private:
  // UI 组件
  QToolBar* m_toolBar;
  QSplitter* m_splitter;
  QTableWidget* m_channelTable;
  QCustomPlot* m_plot;
  QStatusBar* m_statusBar;

  // 工具栏按钮
  QPushButton* m_startBtn;
  QPushButton* m_pauseBtn;
  QPushButton* m_stopBtn;
  QPushButton* m_addChannelBtn;
  QPushButton* m_removeChannelBtn;
  QPushButton* m_exportBtn;
  QPushButton* m_saveImageBtn;
  QPushButton* m_clearDataBtn;
  QCheckBox* m_autoScrollCheck;
  QSpinBox* m_sampleIntervalSpin;
  QDoubleSpinBox* m_timeWindowSpin;

  // 触发相关UI
  QPushButton* m_triggerConfigBtn;
  QPushButton* m_armTriggerBtn;
  QPushButton* m_forceTriggerBtn;
  QLabel* m_triggerStateLabel;

  // 回放相关UI
  QPushButton* m_loadFileBtn;
  QPushButton* m_playBtn;
  QSlider* m_playbackSlider;
  QDoubleSpinBox* m_playbackSpeedSpin;
  QLabel* m_playbackTimeLabel;
  QGroupBox* m_playbackPanel;

  // 状态栏标签
  QLabel* m_stateLabel;
  QLabel* m_sampleRateLabel;
  QLabel* m_dataPointsLabel;
  QLabel* m_elapsedTimeLabel;

  // 数据
  ModbusPlexLink::UniversalDataModel* m_dataModel;
  QList<WaveformChannel> m_channels;
  QMap<int, QVector<WaveformDataPoint>> m_channelData;

  // 预触发缓存（环形缓冲区）
  QMap<int, QQueue<WaveformDataPoint>> m_preTriggerBuffer;
  int m_preTriggerBufferSize;

  // 录波控制
  QTimer* m_sampleTimer;
  QElapsedTimer m_elapsedTimer;
  RecorderState m_state;

  // 触发控制
  TriggerConfig m_triggerConfig;
  TriggerState m_triggerState;
  double m_lastTriggerValue;
  double m_triggerTimestamp;
  QElapsedTimer m_postTriggerTimer;

  // 回放控制
  QTimer* m_playbackTimer;
  int m_playbackIndex;
  double m_playbackSpeed;
  int m_totalPlaybackPoints;
  QMap<int, QVector<WaveformDataPoint>> m_playbackData;

  // 配置
  int m_sampleIntervalMs;
  double m_timeWindowSec;
  int m_maxDataPoints;
  bool m_autoScroll;
  double m_preTriggerTimeSec;

  // 多Y轴
  QList<QCPAxis*> m_rightAxes;

  // 颜色管理
  int m_colorIndex;
  static const QList<QColor> s_defaultColors;
};

#endif  // WAVEFORMRECORDER_H
