#ifndef MODBUSRTUCOLLECTOR_H
#define MODBUSRTUCOLLECTOR_H

#include "interfaces.h"
#include "core/DataTypes.h"
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <modbus.h>

namespace ModbusPlexLink {

/**
 * @brief Modbus RTU 采集器实现
 * 
 * 支持通过串口（COM端口）连接 Modbus RTU 设备进行数据采集。
 * 
 * 配置参数：
 * - serialPort: 串口名称（如 "COM1", "/dev/ttyUSB0"）
 * - baudRate: 波特率（9600, 19200, 38400, 57600, 115200）
 * - dataBits: 数据位（7, 8）
 * - parity: 校验位（'N'=无, 'E'=偶, 'O'=奇）
 * - stopBits: 停止位（1, 2）
 * - unitId: 从站ID
 * - pollRate: 轮询间隔（毫秒）
 */
class ModbusRtuCollector : public ICollector {
    Q_OBJECT
    
public:
    explicit ModbusRtuCollector(const QString& name, QObject *parent = nullptr);
    ~ModbusRtuCollector() override;
    
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
    // 当服务端写入数据时（只处理上层写入的数据）
    void onDataUpdated(const QString& tagName, const QVariant& value);
    
private slots:
    // 执行轮询
    void doPoll();
    
    // 重连定时器
    void doReconnect();
    
private:
    // 连接到Modbus设备（打开串口）
    bool connectToDevice();
    
    // 断开连接（关闭串口）
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
        QList<int> ruleIndexes;
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
    bool m_isConnecting;
    
    // Modbus RTU 连接
    modbus_t* m_modbusContext;
    
    // RTU 串口参数
    QString m_serialPort;     // 串口名称 (COM1, /dev/ttyUSB0)
    int m_baudRate;           // 波特率 (9600, 19200, 115200等)
    char m_parity;            // 校验位 ('N', 'E', 'O')
    int m_dataBits;           // 数据位 (7, 8)
    int m_stopBits;           // 停止位 (1, 2)
    int m_unitId;             // 从站ID
    int m_timeout;            // 超时时间（毫秒）
    
    // 数据模型
    UniversalDataModel* m_udm;
    
    // 映射规则
    QList<CollectorMappingRule> m_mappingRules;
    QList<OptimizedRead> m_optimizedReads;
    
    // 定时器
    QTimer* m_pollTimer;
    QTimer* m_reconnectTimer;
    int m_pollInterval;
    int m_reconnectInterval;
    
    // 统计信息
    struct Statistics {
        quint64 totalPolls;
        quint64 successfulPolls;
        quint64 failedPolls;
        quint64 totalPoints;
        quint64 bytesRead;
        qint64 lastPollTime;
        qint64 connectionTime;
        double averagePollTime;
    } m_stats;
    
    // 线程安全
    mutable QMutex m_mutex;
    
    // 配置参数
    int m_maxRetries;
    bool m_autoReconnect;
    bool m_logErrors;
    
    // 连接状态管理
    int m_consecutiveFailures;
    const int MAX_CONSECUTIVE_FAILURES = 5;
    QString m_lastError;
    
    // 辅助方法
    void resetFailureCount();
    void incrementFailureCount();
    void setConnectionStatus(const QString& status);
    
    // 串口参数转换
    static char parityFromString(const QString& str);
    static QString parityToString(char parity);
    
    // 报文日志辅助方法
    QString formatFunctionCode(RegisterType type, bool isWrite = false) const;
    QString formatDataHex(const QVector<quint16>& data) const;
    QString formatDataHex(const QVector<quint8>& data) const;
    void emitModbusMessage(const QString& direction, RegisterType type, 
                          int address, int count, const QString& data, bool success);
};

} // namespace ModbusPlexLink

#endif // MODBUSRTUCOLLECTOR_H
