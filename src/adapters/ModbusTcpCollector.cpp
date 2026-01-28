#include "ModbusTcpCollector.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

#include "core/DataTypes.h"
#include "core/UniversalDataModel.h"

namespace ModbusPlexLink {

ModbusTcpCollector::ModbusTcpCollector(const QString& name, QObject* parent)
    : ICollector(parent),
      m_name(name),
      m_running(false),
      m_connected(false),
      m_isConnecting(false),
      m_modbusContext(nullptr),
      m_port(502),
      m_unitId(1),
      m_timeout(1000),
      m_udm(nullptr),
      m_pollInterval(1000),
      m_reconnectInterval(3000),  // 默认3秒重连
      m_maxRetries(3),
      m_autoReconnect(true),
      m_logErrors(true),
      m_consecutiveFailures(0) {
  // 初始化统计信息
  memset(&m_stats, 0, sizeof(m_stats));

  // 创建定时器
  m_pollTimer = new QTimer(this);
  m_pollTimer->setInterval(m_pollInterval);
  connect(m_pollTimer, &QTimer::timeout, this, &ModbusTcpCollector::doPoll);

  m_reconnectTimer = new QTimer(this);
  m_reconnectTimer->setInterval(m_reconnectInterval);
  connect(m_reconnectTimer, &QTimer::timeout, this,
          &ModbusTcpCollector::doReconnect);
}

ModbusTcpCollector::~ModbusTcpCollector() {
  stop();
  disconnectFromDevice();
}

bool ModbusTcpCollector::initialize(const QJsonObject& config) {
  QMutexLocker locker(&m_mutex);

  // 解析配置
  m_deviceIp = config["ip"].toString();
  m_port = config["port"].toInt(502);
  m_unitId = config["unitId"].toInt(1);
  m_pollInterval = config["pollRate"].toInt(1000);
  m_timeout = config["timeout"].toInt(1000);
  m_maxRetries = config["maxRetries"].toInt(3);
  m_autoReconnect = config["autoReconnect"].toBool(true);
  m_logErrors = config["logErrors"].toBool(true);

  // 验证配置
  if (m_deviceIp.isEmpty()) {
    qWarning() << "ModbusTcpCollector" << m_name
               << ": No IP address configured";
    return false;
  }

  if (m_pollInterval < 100) {
    qWarning() << "ModbusTcpCollector" << m_name
               << ": Poll interval too small, setting to 100ms";
    m_pollInterval = 100;
  }

  // 更新定时器间隔
  m_pollTimer->setInterval(m_pollInterval);
  m_reconnectTimer->setInterval(m_reconnectInterval);

  qInfo() << "ModbusTcpCollector" << m_name << "initialized:"
          << "IP=" << m_deviceIp << "Port=" << m_port << "UnitId=" << m_unitId
          << "PollRate=" << m_pollInterval << "ms";

  return true;
}

void ModbusTcpCollector::setDataModel(UniversalDataModel* udm) {
  QMutexLocker locker(&m_mutex);
  m_udm = udm;
  
  // 注意：不连接UDM的dataUpdated信号
  // 改为连接服务端的dataWritten信号（在Channel中配置）
}

void ModbusTcpCollector::setMappingRules(
    const QList<CollectorMappingRule>& rules) {
  QMutexLocker locker(&m_mutex);
  m_mappingRules = rules;

  // 优化映射规则
  m_optimizedReads = optimizeMappingRules();

  qDebug() << "ModbusTcpCollector" << m_name << ": Set" << rules.size()
           << "mapping rules"
           << ", optimized to" << m_optimizedReads.size() << "reads";
}

bool ModbusTcpCollector::start() {
  {
    QMutexLocker locker(&m_mutex);

    if (m_running) {
      return true;
    }

    if (m_deviceIp.isEmpty()) {
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Cannot start without IP address";
      return false;
    }

    m_running = true;
    m_consecutiveFailures = 0;
  }

  qInfo() << "ModbusTcpCollector" << m_name << ": Starting...";

  // 启动轮询定时器（即使未连接也启动，连接由重连定时器异步处理）
  m_pollTimer->start();

  // 立即触发一次后台连接尝试（非阻塞）
  QTimer::singleShot(100, this, &ModbusTcpCollector::doReconnect);

  return true;
}

void ModbusTcpCollector::stop() {
  // 1. 先停止定时器（在锁外，避免阻塞）
  m_pollTimer->stop();
  m_reconnectTimer->stop();

  // 2. 快速设置停止标志（最小化锁持有时间）
  {
    QMutexLocker locker(&m_mutex);
    if (!m_running) {
      return;
    }
    m_running = false;
    m_consecutiveFailures = 0;
    m_isConnecting = false;
  }

  qInfo() << "ModbusTcpCollector" << m_name << ": Stopping...";

  // 3. 在后台线程断开连接（完全非阻塞）
  // 忽略返回的 QFuture，因为不需要跟踪此任务
  (void)QtConcurrent::run([this]() {
    qDebug() << "ModbusTcpCollector" << m_name << ": [Background] Disconnecting...";
    disconnectFromDevice();
    qInfo() << "ModbusTcpCollector" << m_name << ": ✓ Stopped";
  });
}

bool ModbusTcpCollector::isRunning() const {
  QMutexLocker locker(&m_mutex);
  return m_running;
}

QString ModbusTcpCollector::getName() const { return m_name; }

bool ModbusTcpCollector::isConnected() const {
  QMutexLocker locker(&m_mutex);
  return m_connected;
}

QJsonObject ModbusTcpCollector::getStatistics() const {
  QMutexLocker locker(&m_mutex);

  QJsonObject stats;
  stats["name"] = m_name;
  stats["connected"] = m_connected;
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
    qint64 uptime =
        QDateTime::currentMSecsSinceEpoch() - m_stats.connectionTime;
    stats["uptimeSeconds"] = uptime / 1000;
  }

  return stats;
}

void ModbusTcpCollector::doPoll() {
  // 如果未连接，跳过本次轮询（连接由doReconnect异步处理）
  if (!m_connected) {
    // 确保重连定时器在运行
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
        qWarning() << "ModbusTcpCollector" << m_name << ": Failed to read"
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
    // 成功，重置失败计数
    resetFailureCount();
  } else {
    // 失败，递增失败计数（达到阈值会自动进入重连模式）
    incrementFailureCount();
  }

  // 旧的检查逻辑（保持兼容）
  if (!pollSuccess && m_autoReconnect) {
    // 多次失败后尝试重连
    static int failCount = 0;
    failCount++;
    if (failCount >= m_maxRetries) {
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Too many failures, attempting reconnection";
      disconnectFromDevice();
      m_pollTimer->stop();
      m_reconnectTimer->start();
      failCount = 0;
    }
  }
}

void ModbusTcpCollector::doReconnect() {
    if (!m_running) {
        m_reconnectTimer->stop();
        return;
    }
    
    // 防止并发连接：如果已经在连接中，直接返回
    {
        QMutexLocker locker(&m_mutex);
        if (m_isConnecting) {
            qDebug() << "ModbusTcpCollector" << m_name << ": Already connecting, skip this attempt";
            return;
        }
        m_isConnecting = true;  // 标记为正在连接
    }
    
    // 临时停止重连定时器，防止重复触发
    m_reconnectTimer->stop();
    
    qDebug() << "ModbusTcpCollector" << m_name << ": Starting connection attempt in background...";
    
    // 使用QtConcurrent在后台线程执行连接，完全避免阻塞UI
    QFuture<bool> future = QtConcurrent::run([this]() -> bool {
        qDebug() << "ModbusTcpCollector" << m_name << ": [Background Thread] Connecting...";
        bool result = connectToDevice();
        qDebug() << "ModbusTcpCollector" << m_name << ": [Background Thread] Result:" << result;
        return result;
    });
    
    // 使用QFutureWatcher监听连接结果（非阻塞）
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        bool success = watcher->result();
        
        // 清除连接中标志
        {
            QMutexLocker locker(&m_mutex);
            m_isConnecting = false;
        }
        
        if (success && m_running) {
            // 连接成功，启动轮询定时器
            if (!m_pollTimer->isActive()) {
                m_pollTimer->start();
            }
            qInfo() << "ModbusTcpCollector" << m_name << ": ✓ Connected successfully!";
        } else {
            // 连接失败，重新启动重连定时器（继续尝试）
            qDebug() << "ModbusTcpCollector" << m_name << ": × Connection failed, will retry in" 
                     << m_reconnectInterval << "ms";
            if (m_running && m_autoReconnect) {
                m_reconnectTimer->start();
            }
        }
        
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

bool ModbusTcpCollector::connectToDevice() {
  // 1. 快速检查状态（持有锁时间最短）
  QString ip;
  int port, unitId, timeout;
  modbus_t* tempContext = nullptr;
  
  {
    QMutexLocker locker(&m_mutex);
    
    // 如果已经停止，直接返回失败
    if (!m_running) {
      return false;
    }
    
    if (m_connected) {
      return true;
    }
    
    // 复制连接参数（避免在锁外访问成员变量）
    ip = m_deviceIp;
    port = m_port;
    unitId = m_unitId;
    timeout = m_timeout;
  }
  // ⬆️ 锁已释放，stop() 可以快速获取锁并设置 m_running = false

  // 2. 在锁外执行阻塞操作（不影响 stop()）
  tempContext = modbus_new_tcp(ip.toStdString().c_str(), port);
  if (tempContext == nullptr) {
    qWarning() << "ModbusTcpCollector" << m_name
               << ": Failed to create modbus context for" << ip << ":" << port;
    return false;
  }

  // 设置从站ID
  if (modbus_set_slave(tempContext, unitId) == -1) {
    qWarning() << "ModbusTcpCollector" << m_name << ": Failed to set slave ID" << unitId;
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

  // 执行阻塞连接（在锁外，不影响 stop()）
  if (modbus_connect(tempContext) == -1) {
    qWarning() << "ModbusTcpCollector" << m_name << ": Failed to connect to"
               << ip << ":" << port << "Error:" << modbus_strerror(errno);
    modbus_free(tempContext);
    return false;
  }

  // 3. 连接成功，快速获取锁并更新状态
  {
    QMutexLocker locker(&m_mutex);
    
    // 再次检查是否已停止（可能在连接期间被停止）
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
  // ⬆️ 锁已释放

  qInfo() << "ModbusTcpCollector" << m_name << ": Connected to" << ip << ":" << port;
  emit connectionStateChanged(true);

  return true;
}

void ModbusTcpCollector::disconnectFromDevice() {
  QMutexLocker locker(&m_mutex);

  if (m_modbusContext != nullptr) {
    modbus_close(m_modbusContext);
    modbus_free(m_modbusContext);
    m_modbusContext = nullptr;
  }

  if (m_connected) {
    m_connected = false;
    emit connectionStateChanged(false);
    qInfo() << "ModbusTcpCollector" << m_name << ": Disconnected";
  }
}

bool ModbusTcpCollector::readCoils(int address, int count,
                                   QVector<quint8>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  // 发送请求报文日志
  emitModbusMessage("TX", RegisterType::Coil, address, count, 
                   QString("读取 %1 个线圈").arg(count), true);

  values.resize(count);
  int rc = modbus_read_bits(m_modbusContext, address, count, values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Failed to read coils at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::Coil, address, count,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }

  // 发送响应报文日志
  emitModbusMessage("RX", RegisterType::Coil, address, count, 
                   formatDataHex(values), true);

  m_stats.bytesRead += count / 8 + (count % 8 ? 1 : 0);
  return true;
}

bool ModbusTcpCollector::readDiscreteInputs(int address, int count,
                                            QVector<quint8>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  emitModbusMessage("TX", RegisterType::DiscreteInput, address, count,
                   QString("读取 %1 个离散输入").arg(count), true);

  values.resize(count);
  int rc =
      modbus_read_input_bits(m_modbusContext, address, count, values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusTcpCollector" << m_name
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

bool ModbusTcpCollector::readHoldingRegisters(int address, int count,
                                              QVector<quint16>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  emitModbusMessage("TX", RegisterType::HoldingRegister, address, count,
                   QString("读取 %1 个保持寄存器").arg(count), true);

  values.resize(count);
  int rc =
      modbus_read_registers(m_modbusContext, address, count, values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusTcpCollector" << m_name
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

bool ModbusTcpCollector::readInputRegisters(int address, int count,
                                            QVector<quint16>& values) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }

  emitModbusMessage("TX", RegisterType::InputRegister, address, count,
                   QString("读取 %1 个输入寄存器").arg(count), true);

  values.resize(count);
  int rc = modbus_read_input_registers(m_modbusContext, address, count,
                                       values.data());

  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusTcpCollector" << m_name
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

QList<ModbusTcpCollector::OptimizedRead>
ModbusTcpCollector::optimizeMappingRules() {
  QList<OptimizedRead> optimized;

  if (m_mappingRules.isEmpty()) {
    return optimized;
  }

  // 按寄存器类型分组
  QMap<RegisterType, QList<int>> rulesByType;  // type -> list of rule indexes

  for (int i = 0; i < m_mappingRules.size(); ++i) {
    const CollectorMappingRule& rule = m_mappingRules[i];
    if (rule.enabled) {
      rulesByType[rule.registerType].append(i);
    }
  }

  // 为每种类型生成优化的读取操作
  for (auto typeIt = rulesByType.begin(); typeIt != rulesByType.end();
       ++typeIt) {
    RegisterType type = typeIt.key();
    QList<int>& ruleIndexes = typeIt.value();

    if (ruleIndexes.isEmpty()) {
      continue;
    }

    // 按地址排序规则
    std::sort(ruleIndexes.begin(), ruleIndexes.end(), [this](int a, int b) {
      return m_mappingRules[a].address < m_mappingRules[b].address;
    });

    // 尝试合并连续或接近的地址
    for (int ruleIndex : ruleIndexes) {
      const CollectorMappingRule& rule = m_mappingRules[ruleIndex];

      // 计算该规则需要的寄存器数量
      int registerCount = rule.count;
      if (registerCount <= 0) {
        registerCount = DataTypeUtils::getRegisterCount(rule.dataType);
      }

      bool merged = false;

      // 尝试将此规则合并到现有的读取操作中
      for (OptimizedRead& existing : optimized) {
        if (existing.type != type) {
          continue;
        }

        int existingEnd = existing.startAddress + existing.count;
        int ruleStart = rule.address;
        int ruleEnd = rule.address + registerCount;

        // 检查是否连续或有小间隙（间隙小于5个寄存器时也合并）
        const int MAX_GAP = 5;
        if (ruleStart >= existingEnd && ruleStart <= existingEnd + MAX_GAP) {
          // 可以合并
          int newCount = ruleEnd - existing.startAddress;

          // 检查是否超过Modbus限制
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

      // 如果无法合并，创建新的读取操作
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

void ModbusTcpCollector::processReadData(const OptimizedRead& read,
                                         const QVector<quint16>& data) {
  if (!m_udm) {
    return;
  }

  int dataOffset = 0;
  for (int ruleIndex : read.ruleIndexes) {
    const CollectorMappingRule& rule = m_mappingRules[ruleIndex];

    // 获取该规则需要的寄存器数量
    int registerCount = rule.count;
    if (registerCount <= 0) {
      registerCount = DataTypeUtils::getRegisterCount(rule.dataType);
    }

    // 检查是否有足够的数据
    if (dataOffset + registerCount > data.size()) {
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Not enough data for tag" << rule.tagName;
      break;
    }

    // 提取该规则对应的寄存器数据
    QVector<quint16> registers;
    for (int i = 0; i < registerCount; ++i) {
      registers.append(data[dataOffset + i]);
    }

    // 使用DataTypeUtils解析数据（包含数据类型转换、字节序转换、倍率和偏移）
    QVariant value = DataTypeUtils::parseRegisters(
        registers, rule.dataType, rule.byteOrder, rule.scale, rule.offset);

    // 创建数据点
    DataPoint dp(value, DataQuality::Good);

    // 写入UDM
    m_udm->updatePoint(rule.tagName, dp);

    // 发送信号
    emit dataCollected(rule.tagName, value);

    // 移动偏移量
    dataOffset += registerCount;
  }
}

void ModbusTcpCollector::processReadData(const OptimizedRead& read,
                                         const QVector<quint8>& data) {
  if (!m_udm) {
    return;
  }

  for (int i = 0; i < read.ruleIndexes.size() && i < data.size(); ++i) {
    const CollectorMappingRule& rule = m_mappingRules[read.ruleIndexes[i]];

    // 提取单个位数据
    QVector<quint8> bits;
    bits.append(data[i]);

    // 使用DataTypeUtils解析位数据
    QVariant value = DataTypeUtils::parseBits(bits, rule.dataType);

    // 如果解析失败，使用默认的bool转换
    if (!value.isValid()) {
      value = (data[i] != 0);
    }

    // 创建数据点
    DataPoint dp(value, DataQuality::Good);

    // 写入UDM
    m_udm->updatePoint(rule.tagName, dp);

    // 发送信号
    emit dataCollected(rule.tagName, value);
  }
}

void ModbusTcpCollector::updateStatistics(bool success) {
  m_stats.totalPolls++;
  if (success) {
    m_stats.successfulPolls++;
  } else {
    m_stats.failedPolls++;
  }
}

// ModbusTcpWriter implementation
ModbusTcpWriter::ModbusTcpWriter(modbus_t* context, QObject* parent)
    : QObject(parent), m_context(context) {}

bool ModbusTcpWriter::writeCoil(int address, bool value) {
  if (!m_context) {
    emit errorOccurred("Modbus context is null");
    return false;
  }

  int rc = modbus_write_bit(m_context, address, value ? 1 : 0);
  bool success = (rc != -1);

  if (!success) {
    emit errorOccurred(
        QString("Failed to write coil: %1").arg(modbus_strerror(errno)));
  } else {
    emit writeCompleted(success, address, 1);
  }

  return success;
}

bool ModbusTcpWriter::writeCoils(int address, const QVector<quint8>& values) {
  if (!m_context) {
    emit errorOccurred("Modbus context is null");
    return false;
  }

  int rc = modbus_write_bits(m_context, address, values.size(), values.data());
  bool success = (rc != -1);

  if (!success) {
    emit errorOccurred(
        QString("Failed to write coils: %1").arg(modbus_strerror(errno)));
  } else {
    emit writeCompleted(success, address, values.size());
  }

  return success;
}

bool ModbusTcpWriter::writeRegister(int address, quint16 value) {
  if (!m_context) {
    emit errorOccurred("Modbus context is null");
    return false;
  }

  int rc = modbus_write_register(m_context, address, value);
  bool success = (rc != -1);

  if (!success) {
    emit errorOccurred(
        QString("Failed to write register: %1").arg(modbus_strerror(errno)));
  } else {
    emit writeCompleted(success, address, 1);
  }

  return success;
}

bool ModbusTcpWriter::writeRegisters(int address,
                                     const QVector<quint16>& values) {
  if (!m_context) {
    emit errorOccurred("Modbus context is null");
    return false;
  }

  int rc =
      modbus_write_registers(m_context, address, values.size(), values.data());
  bool success = (rc != -1);

  if (!success) {
    emit errorOccurred(
        QString("Failed to write registers: %1").arg(modbus_strerror(errno)));
  } else {
    emit writeCompleted(success, address, values.size());
  }

  return success;
}

// ========== 连接状态管理辅助方法 ==========

void ModbusTcpCollector::resetFailureCount() { m_consecutiveFailures = 0; }

void ModbusTcpCollector::incrementFailureCount() {
  m_consecutiveFailures++;

  if (m_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
    qWarning() << "ModbusTcpCollector" << m_name
               << ": Consecutive failures reached" << m_consecutiveFailures
               << ", entering reconnect mode";

    // 断开当前连接
    disconnectFromDevice();

    // 启动重连定时器
    if (m_autoReconnect && !m_reconnectTimer->isActive()) {
      m_reconnectTimer->start();
    }

    // 停止轮询定时器，等待重连成功
    m_pollTimer->stop();
  }
}

void ModbusTcpCollector::setConnectionStatus(const QString& status) {
  m_lastError = status;
}

// ========== 写操作方法 ==========

bool ModbusTcpCollector::writeSingleRegister(int address, quint16 value) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }
  
  QVector<quint16> data{value};
  emitModbusMessage("TX", RegisterType::HoldingRegister, address, 1,
                   QString("写入: %1").arg(formatDataHex(data)), true);
  
  int rc = modbus_write_register(m_modbusContext, address, value);
  
  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Failed to write register at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::HoldingRegister, address, 1,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::HoldingRegister, address, 1, "OK", true);
  qDebug() << "ModbusTcpCollector" << m_name 
          << ": Written register" << address << "=" << value;
  return true;
}

bool ModbusTcpCollector::writeMultipleRegisters(int address, const QVector<quint16>& values) {
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
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Failed to write" << values.size() << "registers at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::HoldingRegister, address, values.size(),
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::HoldingRegister, address, values.size(), "OK", true);
  qDebug() << "ModbusTcpCollector" << m_name 
          << ": Written" << values.size() << "registers starting at" << address;
  return true;
}

bool ModbusTcpCollector::writeSingleCoil(int address, bool value) {
  if (!m_connected || !m_modbusContext) {
    return false;
  }
  
  emitModbusMessage("TX", RegisterType::Coil, address, 1,
                   QString("写入: %1").arg(value ? "ON" : "OFF"), true);
  
  int rc = modbus_write_bit(m_modbusContext, address, value ? 1 : 0);
  
  if (rc == -1) {
    if (m_logErrors) {
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Failed to write coil at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::Coil, address, 1,
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::Coil, address, 1, "OK", true);
  qDebug() << "ModbusTcpCollector" << m_name 
          << ": Written coil" << address << "=" << value;
  return true;
}

bool ModbusTcpCollector::writeMultipleCoils(int address, const QVector<quint8>& values) {
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
      qWarning() << "ModbusTcpCollector" << m_name
                 << ": Failed to write" << values.size() << "coils at" << address
                 << "Error:" << modbus_strerror(errno);
    }
    emitModbusMessage("RX", RegisterType::Coil, address, values.size(),
                     QString("错误: %1").arg(modbus_strerror(errno)), false);
    return false;
  }
  
  emitModbusMessage("RX", RegisterType::Coil, address, values.size(), "OK", true);
  qDebug() << "ModbusTcpCollector" << m_name 
          << ": Written" << values.size() << "coils starting at" << address;
  return true;
}

const CollectorMappingRule* ModbusTcpCollector::findMappingByTag(const QString& tagName) const {
  for (const CollectorMappingRule& rule : m_mappingRules) {
    if (rule.tagName == tagName) {
      return &rule;
    }
  }
  return nullptr;
}

void ModbusTcpCollector::onDataUpdated(const QString& tagName, const QVariant& value) {
  // 当服务端写入数据时（只处理上层写入的数据），将数据转发到下行设备
  
  // 检查是否连接
  if (!m_connected) {
    qDebug() << "ModbusTcpCollector" << m_name 
            << ": Not connected, skip writing tag" << tagName;
    return;
  }
  
  // 查找该标签对应的映射规则
  const CollectorMappingRule* rule = findMappingByTag(tagName);
  
  if (!rule) {
    // 该标签不属于本采集器，忽略
    return;
  }
  
  // 检查寄存器类型是否可写（只有线圈和保持寄存器可写）
  if (rule->registerType != RegisterType::Coil && 
      rule->registerType != RegisterType::HoldingRegister) {
    qDebug() << "ModbusTcpCollector" << m_name 
            << ": Tag" << tagName << "register type is read-only";
    return;
  }
  
  qDebug() << "ModbusTcpCollector" << m_name 
          << ": [Write-Through] Writing tag" << tagName << "value" << value 
          << "to downstream device";
  
  // 根据寄存器类型写入数据
  if (rule->registerType == RegisterType::Coil) {
    // 写入线圈
    bool boolValue = value.toBool();
    writeSingleCoil(rule->address, boolValue);
  } 
  else if (rule->registerType == RegisterType::HoldingRegister) {
    // 写入保持寄存器
    // 使用DataTypeUtils将值转换为寄存器数据（反向转换）
    QVector<quint16> registers = DataTypeUtils::encodeToRegisters(
      value, rule->dataType, rule->byteOrder, rule->scale, rule->offset);
    
    if (registers.isEmpty()) {
      qWarning() << "ModbusTcpCollector" << m_name 
                << ": Failed to convert value" << value << "to registers for tag" << tagName;
      return;
    }
    
    // 根据寄存器数量选择写单个或多个
    if (registers.size() == 1) {
      writeSingleRegister(rule->address, registers[0]);
    } else {
      writeMultipleRegisters(rule->address, registers);
    }
  }
}

// ========== 报文日志辅助方法 ==========

QString ModbusTcpCollector::formatFunctionCode(RegisterType type, bool isWrite) const {
  switch (type) {
    case RegisterType::Coil:
      return isWrite ? "05/0F" : "01";
    case RegisterType::DiscreteInput:
      return "02";
    case RegisterType::HoldingRegister:
      return isWrite ? "06/10" : "03";
    case RegisterType::InputRegister:
      return "04";
    default:
      return "??";
  }
}

QString ModbusTcpCollector::formatDataHex(const QVector<quint16>& data) const {
  QStringList hexValues;
  
  // 显示所有数据，不截断
  for (int i = 0; i < data.size(); ++i) {
    hexValues << QString("%1").arg(data[i], 4, 16, QChar('0')).toUpper();
  }
  
  return hexValues.join(" ");
}

QString ModbusTcpCollector::formatDataHex(const QVector<quint8>& data) const {
  QStringList values;
  
  // 显示所有数据，不截断
  for (int i = 0; i < data.size(); ++i) {
    values << (data[i] ? "1" : "0");
  }
  
  return values.join("");
}

void ModbusTcpCollector::emitModbusMessage(const QString& direction, RegisterType type,
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
  QString device = QString("%1:%2").arg(m_deviceIp).arg(m_port);
  
  emit modbusMessage(direction, device, funcDesc, addrStr, data, success);
}

}  // namespace ModbusPlexLink
