#ifndef MODBUSTCPCOLLECTOR_H
#define MODBUSTCPCOLLECTOR_H

#include "interfaces.h"
#include "core/DataTypes.h"
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <modbus.h>

namespace ModbusPlexLink {

// Modbus TCP采集器实现
class ModbusTcpCollector : public ICollector {
    Q_OBJECT
    
public:
    explicit ModbusTcpCollector(const QString& name, QObject *parent = nullptr);
    ~ModbusTcpCollector() override;
    
    // ICollector interface
    bool initialize(const QJsonObject& config) override;
    void setDataModel(UniversalDataModel* udm) override;
    void setMappingRules(const QList<CollectorMappingRule>& rules) override;
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    QString getName() const override;
    bool isConnected() const override;
    QJsonObject getStatistics() const override;
    
public slots:
    // 当服务端写入数据时（只处理上层写入的数据，不处理采集器自己读取的数据）
    void onDataUpdated(const QString& tagName, const QVariant& value);
    
private slots:
    // 执行轮询
    void doPoll();
    
    // 重连定时器
    void doReconnect();
    
private:
    // 连接到Modbus设备
    bool connectToDevice();
    
    // 断开连接
    void disconnectFromDevice();
    
    // 读取线圈（Coils）
    bool readCoils(int address, int count, QVector<quint8>& values);
    
    // 读取离散输入（Discrete Inputs）
    bool readDiscreteInputs(int address, int count, QVector<quint8>& values);
    
    // 读取保持寄存器（Holding Registers）
    bool readHoldingRegisters(int address, int count, QVector<quint16>& values);
    
    // 读取输入寄存器（Input Registers）
    bool readInputRegisters(int address, int count, QVector<quint16>& values);
    
    // 写入单个寄存器到下行设备
    bool writeSingleRegister(int address, quint16 value);
    
    // 写入多个寄存器到下行设备
    bool writeMultipleRegisters(int address, const QVector<quint16>& values);
    
    // 写入单个线圈到下行设备
    bool writeSingleCoil(int address, bool value);
    
    // 写入多个线圈到下行设备
    bool writeMultipleCoils(int address, const QVector<quint8>& values);
    
    // 根据标签名查找映射规则
    const CollectorMappingRule* findMappingByTag(const QString& tagName) const;
    
    // 优化映射规则（合并连续地址）
    struct OptimizedRead {
        RegisterType type;
        int startAddress;
        int count;
        QList<int> ruleIndexes;  // 对应的规则索引
    };
    QList<OptimizedRead> optimizeMappingRules();
    
    // 处理读取的数据
    void processReadData(const OptimizedRead& read, const QVector<quint16>& data);
    void processReadData(const OptimizedRead& read, const QVector<quint8>& data);
    
    // 更新统计信息
    void updateStatistics(bool success);
    
private:
    // 基本属性
    QString m_name;
    bool m_running;
    bool m_connected;
    bool m_isConnecting;  // 防止并发连接
    
    // Modbus连接
    modbus_t* m_modbusContext;
    QString m_deviceIp;
    int m_port;
    int m_unitId;
    int m_timeout;  // 超时时间（毫秒）
    
    // 数据模型
    UniversalDataModel* m_udm;
    
    // 映射规则（使用新的CollectorMappingRule）
    QList<CollectorMappingRule> m_mappingRules;
    QList<OptimizedRead> m_optimizedReads;
    
    // 定时器
    QTimer* m_pollTimer;
    QTimer* m_reconnectTimer;
    int m_pollInterval;      // 轮询间隔（毫秒）
    int m_reconnectInterval; // 重连间隔（毫秒）
    
    // 统计信息
    struct Statistics {
        quint64 totalPolls;       // 总轮询次数
        quint64 successfulPolls;  // 成功轮询次数
        quint64 failedPolls;      // 失败轮询次数
        quint64 totalPoints;      // 总数据点数
        quint64 bytesRead;        // 读取字节数
        qint64 lastPollTime;      // 最后轮询时间
        qint64 connectionTime;    // 连接建立时间
        double averagePollTime;   // 平均轮询时间（毫秒）
    } m_stats;
    
    // 线程安全
    mutable QMutex m_mutex;  // mutable允许在const函数中加锁
    
    // 配置参数
    int m_maxRetries;      // 最大重试次数
    bool m_autoReconnect;  // 自动重连
    bool m_logErrors;      // 记录错误
    
    // 连接状态管理
    int m_consecutiveFailures;      // 连续失败次数
    const int MAX_CONSECUTIVE_FAILURES = 5;  // 最大连续失败次数
    QString m_lastError;            // 最后错误信息
    
    // 辅助方法
    void resetFailureCount();
    void incrementFailureCount();
    void setConnectionStatus(const QString& status);
    
    // 报文日志辅助方法
    QString formatFunctionCode(RegisterType type, bool isWrite = false) const;
    QString formatDataHex(const QVector<quint16>& data) const;
    QString formatDataHex(const QVector<quint8>& data) const;
    void emitModbusMessage(const QString& direction, RegisterType type, 
                          int address, int count, const QString& data, bool success);
};

// Modbus TCP写入器（可选，用于写入数据到设备）
class ModbusTcpWriter : public QObject {
    Q_OBJECT
    
public:
    explicit ModbusTcpWriter(modbus_t* context, QObject *parent = nullptr);
    
    // 写入单个线圈
    bool writeCoil(int address, bool value);
    
    // 写入多个线圈
    bool writeCoils(int address, const QVector<quint8>& values);
    
    // 写入单个寄存器
    bool writeRegister(int address, quint16 value);
    
    // 写入多个寄存器
    bool writeRegisters(int address, const QVector<quint16>& values);
    
signals:
    void writeCompleted(bool success, int address, int count);
    void errorOccurred(const QString& error);
    
private:
    modbus_t* m_context;
};

} // namespace ModbusPlexLink

#endif // MODBUSTCPCOLLECTOR_H
