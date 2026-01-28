#include "ModbusRtuCollector.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

#include "core/DataTypes.h"
#include "core/UniversalDataModel.h"

namespace ModbusPlexLink {

ModbusRtuCollector::ModbusRtuCollector(const QString& name, QObject* parent)
    : ICollector(parent),
      m_name(name),
      m_running(false),
      m_connected(false),
      m_isConnecting(false),
      m_modbusContext(nullptr),
      m_baudRate(9600),
      m_parity('N'),
      m_dataBits(8),
      m_stopBits(1),
      m_unitId(1),
      m_timeout(1000),
      m_udm(nullptr),
      m_pollInterval(1000),
      m_reconnectInterval(3000),
      m_maxRetries(3),
      m_autoReconnect(true),
      m_logErrors(true),
      m_consecutiveFailures(0) {
  // 初始化统计信息
  memset(&m_stats, 0, sizeof(m_stats));

  // 创建定时器
  m_pollTimer = new QTimer(this);
  m_pollTimer->setInterval(m_pollInterval);
  connect(m_pollTimer, &QTimer::timeout, this, &ModbusRtuCollector::doPoll);

  m_reconnectTimer = new QTimer(this);
  m_reconnectTimer->setInterval(m_reconnectInterval);
  connect(m_reconnectTimer, &QTimer::timeout, this,
          &ModbusRtuCollector::doReconnect);
}

ModbusRtuCollector::~ModbusRtuCollector() {
  stop();
  disconnectFromDevice();
}

bool ModbusRtuCollector::initialize(const QJsonObject& config) {
  QMutexLocker locker(&m_mutex);

  // 解析 RTU 串口配置
  m_serialPort = config["serialPort"].toString();
  m_baudRate = config["baudRate"].toInt(9600);
  m_parity = parityFromString(config["parity"].toString("N"));
  m_dataBits = config["dataBits"].toInt(8);
  m_stopBits = config["stopBits"].toInt(1);
  m_unitId = config["unitId"].toInt(1);
  m_pollInterval = config["pollRate"].toInt(1000);
  m_timeout = config["timeout"].toInt(1000);
  m_maxRetries = config["maxRetries"].toInt(3);
  m_autoReconnect = config["autoReconnect"].toBool(true);
  m_logErrors = config["logErrors"].toBool(true);

  // 验证配置
  if (m_serialPort.isEmpty()) {
    qWarning() << "ModbusRtuCollector" << m_name
               << ": No serial port configured";
    return false;
  }

  if (m_pollInterval < 100) {
    qWarning() << "ModbusRtuCollector" << m_name
               << ": Poll interval too small, setting to 100ms";
    m_pollInterval = 100;
  }

  // 验证波特率
  QList<int> validBaudRates = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
  if (!validBaudRates.contains(m_baudRate)) {
    qWarning() << "ModbusRtuCollector" << m_name
               << ": Invalid baud rate" << m_baudRate << ", setting to 9600";
    m_baudRate = 9600;
  }

  // 验证数据位
  if (m_dataBits != 7 && m_dataBits != 8) {
    qWarning() << "ModbusRtuCollector" << m_name
               << ": Invalid data bits" << m_dataBits << ", setting to 8";
    m_dataBits = 8;
  }

  // 验证停止位
  if (m_stopBits != 1 && m_stopBits != 2) {
    qWarning() << "ModbusRtuCollector" << m_name
               << ": Invalid stop bits" << m_stopBits << ", setting to 1";
    m_stopBits = 1;
  }

  // 更新定时器间隔
  m_pollTimer->setInterval(m_pollInterval);
  m_reconnectTimer->setInterval(m_reconnectInterval);

  qInfo() << "ModbusRtuCollector" << m_name << "initialized:"
          << "Port=" << m_serialPort 
          << "Baud=" << m_baudRate
          << "Parity=" << m_parity
          << "DataBits=" << m_dataBits
          << "StopBits=" << m_stopBits
          << "UnitId=" << m_unitId
          << "PollRate=" << m_pollInterval << "ms";

  return true;
}

void ModbusRtuCollector::setDataModel(UniversalDataModel* udm) {
  QMutexLocker locker(&m_mutex);
  m_udm = udm;
}

void ModbusRtuCollector::setMappingRules(
    const QList<CollectorMappingRule>& rules) {
  QMutexLocker locker(&m_mutex);
  m_mappingRules = rules;

  // 优化映射规则
  m_optimizedReads = optimizeMappingRules();

  qDebug() << "ModbusRtuCollector" << m_name << ": Set" << rules.size()
           << "mapping rules, optimized to" << m_optimizedReads.size() << "reads";
}

bool ModbusRtuCollector::start() {
  {
    QMutexLocker locker(&m_mutex);

    if (m_running) {
      return true;
    }

    if (m_serialPort.isEmpty()) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Cannot start without serial port";
      return false;
    }

    m_running = true;
    m_consecutiveFailures = 0;
  }

  qInfo() << "ModbusRtuCollector" << m_name << ": Starting...";

  // 启动轮询定时器
  m_pollTimer->start();

  // 立即触发一次后台连接尝试
  QTimer::singleShot(100, this, &ModbusRtuCollector::doReconnect);

  return true;
}

void ModbusRtuCollector::stop() {
  // 1. 先停止定时器
  m_pollTimer->stop();
  m_reconnectTimer->stop();

  // 2. 快速设置停止标志
  {
    QMutexLocker locker(&m_mutex);
    if (!m_running) {
      return;
    }
    m_running = false;
    m_consecutiveFailures = 0;
    m_isConnecting = false;
  }

  qInfo() << "ModbusRtuCollector" << m_name << ": Stopping...";

  // 3. 在后台线程断开连接
  QtConcurrent::run([this]() {
    qDebug() << "ModbusRtuCollector" << m_name << ": [Background] Disconnecting...";
    disconnectFromDevice();
    qInfo() << "ModbusRtuCollector" << m_name << ": ✓ Stopped";
  });
}

bool ModbusRtuCollector::isRunning() const {
  QMutexLocker locker(&m_mutex);
  return m_running;
}

QString ModbusRtuCollector::getName() const { return m_name; }

bool ModbusRtuCollector::isConnected() const {
  QMutexLocker locker(&m_mutex);
  return m_connected;
}

QJsonObject ModbusRtuCollector::getStatistics() const {
  QMutexLocker locker(&m_mutex);

  QJsonObject stats;
  stats["name"] = m_name;
  stats["protocol"] = "modbus-rtu";
  stats["connected"] = m_connected;
  stats["serialPort"] = m_serialPort;
  stats["baudRate"] = m_baudRate;
  stats["totalPolls"] = static_cast<qint64>(m_stats.totalPolls);
  stats["successfulPolls"] = static_cast<qint64>(m_stats.successfulPolls);
  stats["failedPolls"] = static_cast<qint64>(m_stats.failedPolls);
  stats["totalPoints"] = static_cast<qint64>(m_stats.totalPoints);
  stats["bytesRead"] = static_cast<qint64>(m_stats.bytesRead);
  stats["averagePollTime"] = m_stats.averagePollTime;

  if (m_stats.lastPollTime > 0) {
    stats["lastPollTime"] = QDateTime::fromMSecsSinceEpoch(m_stats.lastPollTime)
                                .toString(Qt::ISODate);
  }

  if (m_connected && m_stats.connectionTime > 0) {
    qint64 uptime = QDateTime::currentMSecsSinceEpoch() - m_stats.connectionTime;
    stats["uptimeSeconds"] = uptime / 1000;
  }

  return stats;
}

void ModbusRtuCollector::doPoll() {
  // 如果未连接，跳过本次轮询
  if (!m_connected) {
    if (m_autoReconnect && !m_reconnectTimer->isActive()) {
      m_reconnectTimer->start();
    }
    return;
  }

  if (!m_udm || m_optimizedReads.isEmpty()) {
    return;
  }

  QElapsedTimer timer;
  timer.start();

  bool pollSuccess = true;
  int pointsRead = 0;

  // 执行优化的读取操作
  for (const OptimizedRead& read : m_optimizedReads) {
    bool success = false;

    switch (read.type) {
      case RegisterType::Coil: {
        QVector<quint8> values;
        success = readCoils(read.startAddress, read.count, values);
        if (success) {
          processReadData(read, values);
          pointsRead += read.count;
        }
        break;
      }

      case RegisterType::DiscreteInput: {
        QVector<quint8> values;
        success = readDiscreteInputs(read.startAddress, read.count, values);
        if (success) {
          processReadData(read, values);
          pointsRead += read.count;
        }
        break;
      }

      case RegisterType::HoldingRegister: {
        QVector<quint16> values;
        success = readHoldingRegisters(read.startAddress, read.count, values);
        if (success) {
          processReadData(read, values);
          pointsRead += read.count;
        }
        break;
      }

      case RegisterType::InputRegister: {
        QVector<quint16> values;
        success = readInputRegisters(read.startAddress, read.count, values);
        if (success) {
          processReadData(read, values);
          pointsRead += read.count;
        }
        break;
      }
    }

    if (!success) {
      pollSuccess = false;
      if (m_logErrors) {
        qWarning() << "ModbusRtuCollector" << m_name << ": Failed to read"
                   << read.count << "registers starting at"
                   << read.startAddress;
      }
    }
  }

  // 更新统计信息
  qint64 pollTime = timer.elapsed();
  updateStatistics(pollSuccess);
  m_stats.lastPollTime = QDateTime::currentMSecsSinceEpoch();
  m_stats.totalPoints += pointsRead;

  // 更新平均轮询时间
  if (m_stats.totalPolls > 0) {
    m_stats.averagePollTime =
        (m_stats.averagePollTime * (m_stats.totalPolls - 1) + pollTime) /
        m_stats.totalPolls;
  }

  // 连续失败管理
  if (pollSuccess) {
    resetFailureCount();
  } else {
    incrementFailureCount();
  }
}

void ModbusRtuCollector::doReconnect() {
    if (!m_running) {
        m_reconnectTimer->stop();
        return;
    }
    
    // 防止并发连接
    {
        QMutexLocker locker(&m_mutex);
        if (m_isConnecting) {
            qDebug() << "ModbusRtuCollector" << m_name << ": Already connecting, skip";
            return;
        }
        m_isConnecting = true;
    }
    
    m_reconnectTimer->stop();
    
    qDebug() << "ModbusRtuCollector" << m_name << ": Starting connection attempt...";
    
    // 使用QtConcurrent在后台线程执行连接
    QFuture<bool> future = QtConcurrent::run([this]() -> bool {
        qDebug() << "ModbusRtuCollector" << m_name << ": [Background] Connecting to" << m_serialPort;
        bool result = connectToDevice();
        qDebug() << "ModbusRtuCollector" << m_name << ": [Background] Result:" << result;
        return result;
    });
    
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        bool success = watcher->result();
        
        {
            QMutexLocker locker(&m_mutex);
            m_isConnecting = false;
        }
        
        if (success && m_running) {
            if (!m_pollTimer->isActive()) {
                m_pollTimer->start();
            }
            qInfo() << "ModbusRtuCollector" << m_name << ": ✓ Connected to" << m_serialPort;
        } else {
            qDebug() << "ModbusRtuCollector" << m_name << ": × Connection failed, will retry in" 
                     << m_reconnectInterval << "ms";
            if (m_running && m_autoReconnect) {
                m_reconnectTimer->start();
            }
        }
        
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

bool ModbusRtuCollector::connectToDevice() {
  QString port;
  int baudRate, dataBits, stopBits, unitId, timeout;
  char parity;
  modbus_t* tempContext = nullptr;
  
  {
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) {
      return false;
    }
    
    if (m_connected) {
      return true;
    }
    
    port = m_serialPort;
    baudRate = m_baudRate;
    parity = m_parity;
    dataBits = m_dataBits;
    stopBits = m_stopBits;
    unitId = m_unitId;
    timeout = m_timeout;
  }

  // 创建 RTU 上下文
  tempContext = modbus_new_rtu(port.toStdString().c_str(), baudRate, parity, dataBits, stopBits);
  if (tempContext == nullptr) {
    qWarning() << "ModbusRtuCollector" << m_name
               << ": Failed to create modbus RTU context for" << port;
    return false;
  }

  // 设置从站ID
  if (modbus_set_slave(tempContext, unitId) == -1) {
    qWarning() << "ModbusRtuCollector" << m_name << ": Failed to set slave ID" << unitId;
    modbus_free(tempContext);
    return false;
  }

  // 设置超时
  struct timeval response_timeout;
  response_timeout.tv_sec = timeout / 1000;
  response_timeout.tv_usec = (timeout % 1000) * 1000;
  modbus_set_response_timeout(tempContext, response_timeout.tv_sec, response_timeout.tv_usec);

  struct timeval byte_timeout;
  byte_timeout.tv_sec = 0;
  byte_timeout.tv_usec = 500000;  // 500ms
  modbus_set_byte_timeout(tempContext, byte_timeout.tv_sec, byte_timeout.tv_usec);

  // 打开串口连接
  if (modbus_connect(tempContext) == -1) {
    qWarning() << "ModbusRtuCollector" << m_name << ": Failed to open serial port"
               << port << "Error:" << modbus_strerror(errno);
    modbus_free(tempContext);
    return false;
  }

  // 连接成功
  {
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) {
      modbus_close(tempContext);
      modbus_free(tempContext);
      return false;
    }
    
    m_modbusContext = tempContext;
    m_connected = true;
    m_stats.connectionTime = QDateTime::currentMSecsSinceEpoch();
    resetFailureCount();
  }

  qInfo() << "ModbusRtuCollector" << m_name << ": Connected to" << port
          << "at" << baudRate << "baud";
  emit connectionStateChanged(true);

  return true;
}

void ModbusRtuCollector::disconnectFromDevice() {
  QMutexLocker locker(&m_mutex);

  if (m_modbusContext != nullptr) {
    modbus_close(m_modbusContext);
    modbus_free(m_modbusContext);
    m_modbusContext = nullptr;
  }

  if (m_connected) {
    m_connected = false;
    emit connectionStateChanged(false);
    qInfo() << "ModbusRtuCollector" << m_name << ": Disconnected";
  }
}

bool ModbusRtuCollector::readCoils(int address, int count,
                                   QVector<quint8>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  emitModbusMessage("TX", RegisterType::Coil, address, count,
                   QString("读取 %1 个线圈").arg(count), true);

  values.resize(count);
  int rc = modbus_read_bits(m_modbusContext, address, count, values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to read coils at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::Coil, address, count,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }

  emitModbusMessage("RX", RegisterType::Coil, address, count,
                   formatDataHex(values), true);

  m_stats.bytesRead += count / 8 + (count % 8 ? 1 : 0);
  return true;
}

bool ModbusRtuCollector::readDiscreteInputs(int address, int count,
                                            QVector<quint8>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  emitModbusMessage("TX", RegisterType::DiscreteInput, address, count,
                   QString("读取 %1 个离散输入").arg(count), true);

  values.resize(count);
  int rc = modbus_read_input_bits(m_modbusContext, address, count, values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to read discrete inputs at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::DiscreteInput, address, count,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }

  emitModbusMessage("RX", RegisterType::DiscreteInput, address, count,
                   formatDataHex(values), true);

  m_stats.bytesRead += count / 8 + (count % 8 ? 1 : 0);
  return true;
}

bool ModbusRtuCollector::readHoldingRegisters(int address, int count,
                                              QVector<quint16>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  emitModbusMessage("TX", RegisterType::HoldingRegister, address, count,
                   QString("读取 %1 个保持寄存器").arg(count), true);

  values.resize(count);
  int rc = modbus_read_registers(m_modbusContext, address, count, values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to read holding registers at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::HoldingRegister, address, count,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }

  emitModbusMessage("RX", RegisterType::HoldingRegister, address, count,
                   formatDataHex(values), true);

  m_stats.bytesRead += count * 2;
  return true;
}

bool ModbusRtuCollector::readInputRegisters(int address, int count,
                                            QVector<quint16>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  emitModbusMessage("TX", RegisterType::InputRegister, address, count,
                   QString("读取 %1 个输入寄存器").arg(count), true);

  values.resize(count);
  int rc = modbus_read_input_registers(m_modbusContext, address, count, values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to read input registers at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::InputRegister, address, count,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }

  emitModbusMessage("RX", RegisterType::InputRegister, address, count,
                   formatDataHex(values), true);

  m_stats.bytesRead += count * 2;
  return true;
}

QList<ModbusRtuCollector::OptimizedRead>
ModbusRtuCollector::optimizeMappingRules() {
  QList<OptimizedRead> optimized;

  if (m_mappingRules.isEmpty()) {
    return optimized;
  }

  // 按寄存器类型分组
  QMap<RegisterType, QList<int>> rulesByType;

  for (int i = 0; i < m_mappingRules.size(); ++i) {
    const CollectorMappingRule& rule = m_mappingRules[i];
    if (rule.enabled) {
      rulesByType[rule.registerType].append(i);
    }
  }

  // 为每种类型生成优化的读取操作
  for (auto typeIt = rulesByType.begin(); typeIt != rulesByType.end(); ++typeIt) {
    RegisterType type = typeIt.key();
    QList<int>& ruleIndexes = typeIt.value();

    if (ruleIndexes.isEmpty()) {
      continue;
    }

    // 按地址排序
    std::sort(ruleIndexes.begin(), ruleIndexes.end(), [this](int a, int b) {
      return m_mappingRules[a].address < m_mappingRules[b].address;
    });

    // 合并连续地址
    for (int ruleIndex : ruleIndexes) {
      const CollectorMappingRule& rule = m_mappingRules[ruleIndex];

      int registerCount = rule.count;
      if (registerCount <= 0) {
        registerCount = DataTypeUtils::getRegisterCount(rule.dataType);
      }

      bool merged = false;

      for (OptimizedRead& existing : optimized) {
        if (existing.type != type) {
          continue;
        }

        int existingEnd = existing.startAddress + existing.count;
        int ruleStart = rule.address;
        int ruleEnd = rule.address + registerCount;

        const int MAX_GAP = 5;
        if (ruleStart >= existingEnd && ruleStart <= existingEnd + MAX_GAP) {
          int newCount = ruleEnd - existing.startAddress;

          const int MAX_READ_COUNT = (type == RegisterType::Coil ||
                                      type == RegisterType::DiscreteInput)
                                         ? 2000
                                         : 125;
          if (newCount <= MAX_READ_COUNT) {
            existing.count = newCount;
            existing.ruleIndexes.append(ruleIndex);
            merged = true;
            break;
          }
        }
      }

      if (!merged) {
        OptimizedRead read;
        read.type = type;
        read.startAddress = rule.address;
        read.count = registerCount;
        read.ruleIndexes.append(ruleIndex);
        optimized.append(read);
      }
    }
  }

  return optimized;
}

void ModbusRtuCollector::processReadData(const OptimizedRead& read,
                                         const QVector<quint16>& data) {
  if (!m_udm) {
    return;
  }

  int dataOffset = 0;
  for (int ruleIndex : read.ruleIndexes) {
    const CollectorMappingRule& rule = m_mappingRules[ruleIndex];

    int registerCount = rule.count;
    if (registerCount <= 0) {
      registerCount = DataTypeUtils::getRegisterCount(rule.dataType);
    }

    if (dataOffset + registerCount > data.size()) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Not enough data for tag" << rule.tagName;
      break;
    }

    QVector<quint16> registers;
    for (int i = 0; i < registerCount; ++i) {
      registers.append(data[dataOffset + i]);
    }

    QVariant value = DataTypeUtils::parseRegisters(
        registers, rule.dataType, rule.byteOrder, rule.scale, rule.offset);

    DataPoint dp(value, DataQuality::Good);
    m_udm->updatePoint(rule.tagName, dp);
    emit dataCollected(rule.tagName, value);

    dataOffset += registerCount;
  }
}

void ModbusRtuCollector::processReadData(const OptimizedRead& read,
                                         const QVector<quint8>& data) {
  if (!m_udm) {
    return;
  }

  for (int i = 0; i < read.ruleIndexes.size() && i < data.size(); ++i) {
    const CollectorMappingRule& rule = m_mappingRules[read.ruleIndexes[i]];

    QVector<quint8> bits;
    bits.append(data[i]);

    QVariant value = DataTypeUtils::parseBits(bits, rule.dataType);

    if (!value.isValid()) {
      value = (data[i] != 0);
    }

    DataPoint dp(value, DataQuality::Good);
    m_udm->updatePoint(rule.tagName, dp);
    emit dataCollected(rule.tagName, value);
  }
}

void ModbusRtuCollector::updateStatistics(bool success) {
  m_stats.totalPolls++;
  if (success) {
    m_stats.successfulPolls++;
  } else {
    m_stats.failedPolls++;
  }
}

void ModbusRtuCollector::resetFailureCount() { 
  m_consecutiveFailures = 0; 
}

void ModbusRtuCollector::incrementFailureCount() {
  m_consecutiveFailures++;

  if (m_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
    qWarning() << "ModbusRtuCollector" << m_name
               << ": Consecutive failures reached" << m_consecutiveFailures
               << ", entering reconnect mode";

    disconnectFromDevice();

    if (m_autoReconnect && !m_reconnectTimer->isActive()) {
      m_reconnectTimer->start();
    }

    m_pollTimer->stop();
  }
}

void ModbusRtuCollector::setConnectionStatus(const QString& status) {
  m_lastError = status;
}

// ========== 写操作方法 ==========

bool ModbusRtuCollector::writeSingleRegister(int address, quint16 value) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }
  
  QVector<quint16> data{value};
  emitModbusMessage("TX", RegisterType::HoldingRegister, address, 1,
                   QString("写入: %1").arg(formatDataHex(data)), true);
  
  int rc = modbus_write_register(m_modbusContext, address, value);
  
  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to write register at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::HoldingRegister, address, 1,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::HoldingRegister, address, 1, "OK", true);
  qDebug() << "ModbusRtuCollector" << m_name 
          << ": Written register" << address << "=" << value;
  return true;
}

bool ModbusRtuCollector::writeMultipleRegisters(int address, const QVector<quint16>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }
  
  if (values.isEmpty()) {
    return false;
  }
  
  emitModbusMessage("TX", RegisterType::HoldingRegister, address, values.size(),
                   QString("写入: %1").arg(formatDataHex(values)), true);
  
  int rc = modbus_write_registers(m_modbusContext, address, values.size(), values.data());
  
  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to write" << values.size() << "registers at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::HoldingRegister, address, values.size(),
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::HoldingRegister, address, values.size(), "OK", true);
  qDebug() << "ModbusRtuCollector" << m_name 
          << ": Written" << values.size() << "registers at" << address;
  return true;
}

bool ModbusRtuCollector::writeSingleCoil(int address, bool value) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }
  
  emitModbusMessage("TX", RegisterType::Coil, address, 1,
                   QString("写入: %1").arg(value ? "ON" : "OFF"), true);
  
  int rc = modbus_write_bit(m_modbusContext, address, value ? 1 : 0);
  
  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to write coil at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::Coil, address, 1,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::Coil, address, 1, "OK", true);
  qDebug() << "ModbusRtuCollector" << m_name 
          << ": Written coil" << address << "=" << value;
  return true;
}

bool ModbusRtuCollector::writeMultipleCoils(int address, const QVector<quint8>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }
  
  if (values.isEmpty()) {
    return false;
  }
  
  emitModbusMessage("TX", RegisterType::Coil, address, values.size(),
                   QString("写入: %1").arg(formatDataHex(values)), true);
  
  int rc = modbus_write_bits(m_modbusContext, address, values.size(), values.data());
  
  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusRtuCollector" << m_name
                 << ": Failed to write" << values.size() << "coils at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::Coil, address, values.size(),
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::Coil, address, values.size(), "OK", true);
  qDebug() << "ModbusRtuCollector" << m_name 
          << ": Written" << values.size() << "coils at" << address;
  return true;
}

const CollectorMappingRule* ModbusRtuCollector::findMappingByTag(const QString& tagName) const {
  for (const CollectorMappingRule& rule : m_mappingRules) {
    if (rule.tagName == tagName) {
      return &rule;
    }
  }
  return nullptr;
}

void ModbusRtuCollector::onDataUpdated(const QString& tagName, const QVariant& value) {
  if (!m_connected) {
    return;
  }
  
  const CollectorMappingRule* rule = findMappingByTag(tagName);
  
  if (!rule) {
    return;
  }
  
  if (rule->registerType != RegisterType::Coil && 
      rule->registerType != RegisterType::HoldingRegister) {
    return;
  }
  
  qDebug() << "ModbusRtuCollector" << m_name 
          << ": [Write-Through] Writing tag" << tagName << "value" << value;
  
  if (rule->registerType == RegisterType::Coil) {
    bool boolValue = value.toBool();
    writeSingleCoil(rule->address, boolValue);
  } 
  else if (rule->registerType == RegisterType::HoldingRegister) {
    QVector<quint16> registers = DataTypeUtils::encodeToRegisters(
      value, rule->dataType, rule->byteOrder, rule->scale, rule->offset);
    
    if (registers.isEmpty()) {
      qWarning() << "ModbusRtuCollector" << m_name 
                << ": Failed to convert value for tag" << tagName;
      return;
    }
    
    if (registers.size() == 1) {
      writeSingleRegister(rule->address, registers[0]);
    } else {
      writeMultipleRegisters(rule->address, registers);
    }
  }
}

// ========== 辅助方法 ==========

char ModbusRtuCollector::parityFromString(const QString& str) {
  if (str.isEmpty() || str.toUpper() == "N" || str.toUpper() == "NONE") {
    return 'N';
  } else if (str.toUpper() == "E" || str.toUpper() == "EVEN") {
    return 'E';
  } else if (str.toUpper() == "O" || str.toUpper() == "ODD") {
    return 'O';
  }
  return 'N';  // 默认无校验
}

QString ModbusRtuCollector::parityToString(char parity) {
  switch (parity) {
    case 'N': return "None";
    case 'E': return "Even";
    case 'O': return "Odd";
    default: return "None";
  }
}

// ========== 报文日志辅助方法 ==========

QString ModbusRtuCollector::formatFunctionCode(RegisterType type, bool isWrite) const {
  switch (type) {
    case RegisterType::Coil:
      return isWrite ? "05/0F" : "01";
    case RegisterType::DiscreteInput:
      return "02";
    case RegisterType::HoldingRegister:
      return isWrite ? "06/10" : "03";
    case RegisterType::InputRegister:
      return "04";
    default: return "??";
  }
}

QString ModbusRtuCollector::formatDataHex(const QVector<quint16>& data) const {
  QStringList hexValues;
  
  // 显示所有数据，不截断
  for (int i = 0; i < data.size(); ++i) {
    hexValues << QString("%1").arg(data[i], 4, 16, QChar('0')).toUpper();
  }
  
  return hexValues.join(" ");
}

QString ModbusRtuCollector::formatDataHex(const QVector<quint8>& data) const {
  QStringList values;
  
  // 显示所有数据，不截断
  for (int i = 0; i < data.size(); ++i) {
    values << (data[i] ? "1" : "0");
  }
  
  return values.join("");
}

void ModbusRtuCollector::emitModbusMessage(const QString& direction, RegisterType type,
                                           int address, int count, const QString& data, bool success) {
  QString funcCode = formatFunctionCode(type, direction == "TX" && data.contains("写入"));
  QString funcDesc;
  
  switch (type) {
    case RegisterType::Coil:
      funcDesc = QString("FC%1 线圈").arg(funcCode);
      break;
    case RegisterType::DiscreteInput:
      funcDesc = QString("FC%1 离散输入").arg(funcCode);
      break;
    case RegisterType::HoldingRegister:
      funcDesc = QString("FC%1 保持寄存器").arg(funcCode);
      break;
    case RegisterType::InputRegister:
      funcDesc = QString("FC%1 输入寄存器").arg(funcCode);
      break;
  }
  
  QString addrStr = QString("%1~%2").arg(address).arg(address + count - 1);
  QString device = QString("%1 [%2-%3-%4-%5]").arg(m_serialPort)
                     .arg(m_baudRate).arg(m_dataBits).arg(m_parity).arg(m_stopBits);
  
  emit modbusMessage(direction, device, funcDesc, addrStr, data, success);
}

}  // namespace ModbusPlexLink
