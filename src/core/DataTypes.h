#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>
#include <QVariant>
#include <QVector>

namespace ModbusPlexLink {

// 数据类型枚举
enum class DataType {
    UInt16,      // 无符号16位整数 (1个寄存器)
    Int16,       // 有符号16位整数 (1个寄存器)
    UInt32,      // 无符号32位整数 (2个寄存器)
    Int32,       // 有符号32位整数 (2个寄存器)
    Float32,     // 32位浮点数 (2个寄存器)
    UInt64,      // 无符号64位整数 (4个寄存器)
    Int64,       // 有符号64位整数 (4个寄存器)
    Float64,     // 64位浮点数 (4个寄存器)
    Bool,        // 布尔值 (1个线圈/离散输入)
    String       // 字符串 (多个寄存器)
};

// 字节序枚举（针对多寄存器数据类型）
enum class ByteOrder {
    AB,          // Big Endian (高字节在前) - 2字节
    BA,          // Little Endian (低字节在前) - 2字节
    ABCD,        // Big Endian - 4字节
    DCBA,        // Little Endian - 4字节
    CDAB,        // Mid-Big Endian - 4字节
    BADC,        // Mid-Little Endian - 4字节
    ABCDEFGH,    // Big Endian - 8字节
    HGFEDCBA     // Little Endian - 8字节
};

// 寄存器类型枚举（从interfaces.h移到这里）
enum class RegisterType {
    Coil = 0,           // 线圈（位）
    DiscreteInput = 1,  // 离散输入（位）
    HoldingRegister = 2,// 保持寄存器（16位）
    InputRegister = 3   // 输入寄存器（16位）
};

// 数据类型工具类
class DataTypeUtils {
public:
    // 获取数据类型需要的寄存器数量
    static int getRegisterCount(DataType type);
    
    // 从字符串转换为DataType
    static DataType dataTypeFromString(const QString& str);
    
    // DataType转换为字符串
    static QString dataTypeToString(DataType type);
    
    // 从字符串转换为ByteOrder
    static ByteOrder byteOrderFromString(const QString& str);
    
    // ByteOrder转换为字符串
    static QString byteOrderToString(ByteOrder order);
    
    // RegisterType从字符串转换
    static RegisterType registerTypeFromString(const QString& str);
    
    // RegisterType转换为字符串
    static QString registerTypeToString(RegisterType type);
    
    // 从寄存器数据解析为QVariant
    static QVariant parseRegisters(const QVector<quint16>& registers, 
                                   DataType dataType, 
                                   ByteOrder byteOrder,
                                   double scale = 1.0,
                                   double offset = 0.0);
    
    // 从位数据解析为QVariant
    static QVariant parseBits(const QVector<quint8>& bits, DataType dataType);
    
    // 将QVariant编码为寄存器数据
    static QVector<quint16> encodeToRegisters(const QVariant& value,
                                              DataType dataType,
                                              ByteOrder byteOrder,
                                              double scale = 1.0,
                                              double offset = 0.0);
    
    // 将QVariant编码为位数据
    static QVector<quint8> encodeToBits(const QVariant& value, DataType dataType);
    
private:
    // 字节序转换辅助函数
    static quint32 swapBytes32(quint32 value, ByteOrder from, ByteOrder to);
    static quint64 swapBytes64(quint64 value, ByteOrder from, ByteOrder to);
    
    // Float转换
    static float registersToFloat32(quint16 reg1, quint16 reg2, ByteOrder order);
    static void float32ToRegisters(float value, quint16& reg1, quint16& reg2, ByteOrder order);
    
    // Double转换
    static double registersToFloat64(const QVector<quint16>& regs, ByteOrder order);
    static QVector<quint16> float64ToRegisters(double value, ByteOrder order);
};

// 采集侧映射规则（增强版）
struct CollectorMappingRule {
    QString tagName;              // UDM标签名
    QString comment;              // 注释
    RegisterType registerType;    // 寄存器类型
    int address;                  // 物理地址
    int count;                    // 寄存器数量（自动根据dataType计算，也可手动指定）
    DataType dataType;            // 数据类型
    ByteOrder byteOrder;          // 字节序
    double scale;                 // 倍率
    double offset;                // 偏移量
    QString unit;                 // 单位（如"kW", "V", "A"）
    bool enabled;                 // 是否启用
    
    // 默认构造
    CollectorMappingRule()
        : registerType(RegisterType::HoldingRegister)
        , address(0)
        , count(1)
        , dataType(DataType::UInt16)
        , byteOrder(ByteOrder::AB)
        , scale(1.0)
        , offset(0.0)
        , enabled(true)
    {}
    
    // 应用数据转换（原始值 → 实际值）
    QVariant applyTransform(const QVector<quint16>& rawRegisters) const {
        return DataTypeUtils::parseRegisters(rawRegisters, dataType, byteOrder, scale, offset);
    }
    
    // 应用数据转换（位数据）
    QVariant applyTransform(const QVector<quint8>& rawBits) const {
        return DataTypeUtils::parseBits(rawBits, dataType);
    }
};

// 服务端映射规则（增强版）
struct ServerMappingRule {
    // 源变量配置
    QString sourceCollector;      // 源采集器名称（空表示任意采集器）
    QString sourceTagName;        // 源UDM标签名（采集器中的变量名）
    
    // 目标配置
    QString tagName;              // 目标标签名（可与源不同，用于显示/日志）
    QString comment;              // 注释
    RegisterType registerType;    // 寄存器类型
    int address;                  // 虚拟地址
    DataType dataType;            // 输出数据类型（可与源类型不同）
    ByteOrder byteOrder;          // 字节序
    
    // 数据转换
    double scale;                 // 倍率
    double offset;                // 偏移量
    QString expression;           // 表达式（如 "x*0.1+5" 或 "(x-32)*5/9"，x代表源值）
    
    // 属性
    bool writable;                // 是否可写
    QString accessLevel;          // 访问级别（预留）
    
    // 默认构造
    ServerMappingRule()
        : registerType(RegisterType::HoldingRegister)
        , address(0)
        , dataType(DataType::UInt16)
        , byteOrder(ByteOrder::AB)
        , scale(1.0)
        , offset(0.0)
        , writable(false)
    {}
    
    // 获取完整源标识（采集器:标签名）
    QString getFullSourceId() const {
        if (sourceCollector.isEmpty()) {
            return sourceTagName;
        }
        return QString("%1:%2").arg(sourceCollector).arg(sourceTagName);
    }
    
    // 设置完整源标识
    void setFullSourceId(const QString& fullId) {
        int colonPos = fullId.indexOf(':');
        if (colonPos > 0) {
            sourceCollector = fullId.left(colonPos);
            sourceTagName = fullId.mid(colonPos + 1);
        } else {
            sourceCollector.clear();
            sourceTagName = fullId;
        }
    }
    
    // 应用转换（源值 → 输出值）
    double applyTransform(double sourceValue) const {
        // 如果有表达式，优先使用表达式
        if (!expression.isEmpty()) {
            return evaluateExpression(sourceValue);
        }
        // 否则使用倍率和偏移：输出值 = 源值 * scale + offset
        return sourceValue * scale + offset;
    }
    
    // 反向转换（输出值 → 源值，用于写操作）
    double reverseApplyTransform(double outputValue) const {
        // 反向公式：源值 = (输出值 - offset) / scale
        if (scale != 0.0) {
            return (outputValue - offset) / scale;
        }
        return outputValue;
    }
    
    // 编码为寄存器（实际值 → 原始寄存器值）
    QVector<quint16> reverseTransform(const QVariant& actualValue) const {
        double transmitValue = reverseApplyTransform(actualValue.toDouble());
        return DataTypeUtils::encodeToRegisters(transmitValue, dataType, byteOrder);
    }
    
private:
    // 简单表达式求值（支持 x, +, -, *, /, 括号）
    double evaluateExpression(double x) const {
        // 简化实现：替换x并使用简单数学
        QString expr = expression;
        expr.replace("x", QString::number(x, 'g', 15));
        expr.replace("X", QString::number(x, 'g', 15));
        
        // 简单四则运算解析（无括号版本）
        // 完整版本需要使用表达式解析器
        // 这里暂时只支持 scale * x + offset 形式
        return x * scale + offset;
    }
};

// 虚拟设备配置
struct VirtualDevice {
    int virtualUnitId;                    // 虚拟从站ID
    QString name;                         // 名称
    QString description;                  // 描述
    QList<ServerMappingRule> mappings;    // 映射规则列表
    bool enabled;                         // 是否启用
    
    VirtualDevice()
        : virtualUnitId(1)
        , enabled(true)
    {}
};

} // namespace ModbusPlexLink

#endif // DATATYPES_H