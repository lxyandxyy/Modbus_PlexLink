#include "DataTypes.h"
#include <QDebug>
#include <cstring>

namespace ModbusPlexLink {

int DataTypeUtils::getRegisterCount(DataType type) {
    switch (type) {
        case DataType::UInt16:
        case DataType::Int16:
        case DataType::Bool:
            return 1;
        case DataType::UInt32:
        case DataType::Int32:
        case DataType::Float32:
            return 2;
        case DataType::UInt64:
        case DataType::Int64:
        case DataType::Float64:
            return 4;
        case DataType::String:
            return 1; // 默认，实际需要根据字符串长度计算
        default:
            return 1;
    }
}

DataType DataTypeUtils::dataTypeFromString(const QString& str) {
    QString s = str.toLower();
    if (s == "uint16") return DataType::UInt16;
    if (s == "int16") return DataType::Int16;
    if (s == "uint32") return DataType::UInt32;
    if (s == "int32") return DataType::Int32;
    if (s == "float32" || s == "float") return DataType::Float32;
    if (s == "uint64") return DataType::UInt64;
    if (s == "int64") return DataType::Int64;
    if (s == "float64" || s == "double") return DataType::Float64;
    if (s == "bool" || s == "boolean") return DataType::Bool;
    if (s == "string") return DataType::String;
    
    qWarning() << "Unknown data type:" << str << ", using UInt16 as default";
    return DataType::UInt16;
}

QString DataTypeUtils::dataTypeToString(DataType type) {
    switch (type) {
        case DataType::UInt16: return "UInt16";
        case DataType::Int16: return "Int16";
        case DataType::UInt32: return "UInt32";
        case DataType::Int32: return "Int32";
        case DataType::Float32: return "Float32";
        case DataType::UInt64: return "UInt64";
        case DataType::Int64: return "Int64";
        case DataType::Float64: return "Float64";
        case DataType::Bool: return "Bool";
        case DataType::String: return "String";
        default: return "Unknown";
    }
}

ByteOrder DataTypeUtils::byteOrderFromString(const QString& str) {
    QString s = str.toUpper();
    if (s == "AB") return ByteOrder::AB;
    if (s == "BA") return ByteOrder::BA;
    if (s == "ABCD") return ByteOrder::ABCD;
    if (s == "DCBA") return ByteOrder::DCBA;
    if (s == "CDAB") return ByteOrder::CDAB;
    if (s == "BADC") return ByteOrder::BADC;
    if (s == "ABCDEFGH") return ByteOrder::ABCDEFGH;
    if (s == "HGFEDCBA") return ByteOrder::HGFEDCBA;
    
    qWarning() << "Unknown byte order:" << str << ", using AB as default";
    return ByteOrder::AB;
}

QString DataTypeUtils::byteOrderToString(ByteOrder order) {
    switch (order) {
        case ByteOrder::AB: return "AB";
        case ByteOrder::BA: return "BA";
        case ByteOrder::ABCD: return "ABCD";
        case ByteOrder::DCBA: return "DCBA";
        case ByteOrder::CDAB: return "CDAB";
        case ByteOrder::BADC: return "BADC";
        case ByteOrder::ABCDEFGH: return "ABCDEFGH";
        case ByteOrder::HGFEDCBA: return "HGFEDCBA";
        default: return "Unknown";
    }
}

RegisterType DataTypeUtils::registerTypeFromString(const QString& str) {
    QString s = str.toLower();
    if (s == "coil" || s == "coils") return RegisterType::Coil;
    if (s == "discrete" || s == "discreteinput") return RegisterType::DiscreteInput;
    if (s == "holding" || s == "holdingregister") return RegisterType::HoldingRegister;
    if (s == "input" || s == "inputregister") return RegisterType::InputRegister;
    
    qWarning() << "Unknown register type:" << str << ", using HoldingRegister as default";
    return RegisterType::HoldingRegister;
}

QString DataTypeUtils::registerTypeToString(RegisterType type) {
    switch (type) {
        case RegisterType::Coil: return "Coil";
        case RegisterType::DiscreteInput: return "Discrete";
        case RegisterType::HoldingRegister: return "Holding";
        case RegisterType::InputRegister: return "Input";
        default: return "Unknown";
    }
}

QVariant DataTypeUtils::parseRegisters(const QVector<quint16>& registers,
                                      DataType dataType,
                                      ByteOrder byteOrder,
                                      double scale,
                                      double offset) {
    if (registers.isEmpty()) {
        return QVariant();
    }
    
    QVariant result;
    
    switch (dataType) {
        case DataType::UInt16: {
            quint16 value = registers[0];
            if (byteOrder == ByteOrder::BA) {
                value = ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
            }
            result = value;
            break;
        }
        
        case DataType::Int16: {
            quint16 rawValue = registers[0];
            if (byteOrder == ByteOrder::BA) {
                rawValue = ((rawValue & 0xFF) << 8) | ((rawValue & 0xFF00) >> 8);
            }
            qint16 value = static_cast<qint16>(rawValue);
            result = value;
            break;
        }
        
        case DataType::UInt32: {
            if (registers.size() < 2) return QVariant();
            
            quint32 value = 0;
            
            switch (byteOrder) {
                case ByteOrder::ABCD: // Big Endian
                    value = (static_cast<quint32>(registers[0]) << 16) | registers[1];
                    break;
                case ByteOrder::DCBA: // Little Endian
                    value = (static_cast<quint32>(registers[1]) << 16) | registers[0];
                    value = ((value & 0xFF000000) >> 24) | ((value & 0x00FF0000) >> 8) |
                           ((value & 0x0000FF00) << 8) | ((value & 0x000000FF) << 24);
                    break;
                case ByteOrder::CDAB: // Mid-Big Endian
                    value = (static_cast<quint32>(registers[1]) << 16) | registers[0];
                    break;
                case ByteOrder::BADC: { // Mid-Little Endian
                    value = (static_cast<quint32>(registers[0]) << 16) | registers[1];
                    quint16 high = (value >> 16) & 0xFFFF;
                    quint16 low = value & 0xFFFF;
                    high = ((high & 0xFF) << 8) | ((high & 0xFF00) >> 8);
                    low = ((low & 0xFF) << 8) | ((low & 0xFF00) >> 8);
                    value = (static_cast<quint32>(high) << 16) | low;
                    break;
                }
                default:
                    value = (static_cast<quint32>(registers[0]) << 16) | registers[1];
                    break;
            }
            
            result = value;
            break;
        }
        
        case DataType::Int32: {
            if (registers.size() < 2) return QVariant();
            
            quint32 rawValue = 0;
            
            switch (byteOrder) {
                case ByteOrder::ABCD:
                    rawValue = (static_cast<quint32>(registers[0]) << 16) | registers[1];
                    break;
                case ByteOrder::DCBA:
                    rawValue = (static_cast<quint32>(registers[1]) << 16) | registers[0];
                    rawValue = ((rawValue & 0xFF000000) >> 24) | ((rawValue & 0x00FF0000) >> 8) |
                              ((rawValue & 0x0000FF00) << 8) | ((rawValue & 0x000000FF) << 24);
                    break;
                case ByteOrder::CDAB:
                    rawValue = (static_cast<quint32>(registers[1]) << 16) | registers[0];
                    break;
                case ByteOrder::BADC: { // 添加花括号
                    rawValue = (static_cast<quint32>(registers[0]) << 16) | registers[1];
                    quint16 high = (rawValue >> 16) & 0xFFFF;
                    quint16 low = rawValue & 0xFFFF;
                    high = ((high & 0xFF) << 8) | ((high & 0xFF00) >> 8);
                    low = ((low & 0xFF) << 8) | ((low & 0xFF00) >> 8);
                    rawValue = (static_cast<quint32>(high) << 16) | low;
                    break;
                }
                default:
                    rawValue = (static_cast<quint32>(registers[0]) << 16) | registers[1];
                    break;
            }
            
            qint32 value = static_cast<qint32>(rawValue);
            result = value;
            break;
        }
        
        case DataType::Float32: {
            if (registers.size() < 2) return QVariant();
            float value = registersToFloat32(registers[0], registers[1], byteOrder);
            result = static_cast<double>(value);
            break;
        }
        
        case DataType::Float64: {
            if (registers.size() < 4) return QVariant();
            double value = registersToFloat64(registers, byteOrder);
            result = value;
            break;
        }
        
        case DataType::Bool: {
            result = (registers[0] != 0);
            break;
        }
        
        case DataType::String: {
            QString str;
            for (quint16 reg : registers) {
                char high = (reg >> 8) & 0xFF;
                char low = reg & 0xFF;
                if (high != 0) str.append(high);
                if (low != 0) str.append(low);
            }
            result = str;
            break;
        }
        
        default:
            result = registers[0];
            break;
    }
    
    // 应用倍率和偏移
    if (result.canConvert<double>()) {
        double value = result.toDouble();
        value = value * scale + offset;
        result = value;
    }
    
    return result;
}

QVariant DataTypeUtils::parseBits(const QVector<quint8>& bits, DataType dataType) {
    if (bits.isEmpty()) {
        return QVariant();
    }
    
    if (dataType == DataType::Bool) {
        return (bits[0] != 0);
    }
    
    // 其他类型暂不支持从位数据解析
    return QVariant();
}

QVector<quint16> DataTypeUtils::encodeToRegisters(const QVariant& value,
                                                  DataType dataType,
                                                  ByteOrder byteOrder,
                                                  double scale,
                                                  double offset) {
    QVector<quint16> registers;
    
    // 先反向应用倍率和偏移
    double transmitValue = value.toDouble();
    if (scale != 0.0) {
        transmitValue = (transmitValue - offset) / scale;
    }
    
    switch (dataType) {
        case DataType::UInt16: {
            quint16 val = static_cast<quint16>(transmitValue);
            if (byteOrder == ByteOrder::BA) {
                val = ((val & 0xFF) << 8) | ((val & 0xFF00) >> 8);
            }
            registers.append(val);
            break;
        }
        
        case DataType::Int16: {
            qint16 val = static_cast<qint16>(transmitValue);
            quint16 rawVal = static_cast<quint16>(val);
            if (byteOrder == ByteOrder::BA) {
                rawVal = ((rawVal & 0xFF) << 8) | ((rawVal & 0xFF00) >> 8);
            }
            registers.append(rawVal);
            break;
        }
        
        case DataType::UInt32: {
            quint32 val = static_cast<quint32>(transmitValue);
            
            switch (byteOrder) {
                case ByteOrder::ABCD: // Big Endian
                    registers.append((val >> 16) & 0xFFFF);
                    registers.append(val & 0xFFFF);
                    break;
                case ByteOrder::DCBA: // Little Endian
                    registers.append(val & 0xFFFF);
                    registers.append((val >> 16) & 0xFFFF);
                    break;
                case ByteOrder::CDAB: // Mid-Big Endian
                    registers.append(val & 0xFFFF);
                    registers.append((val >> 16) & 0xFFFF);
                    break;
                case ByteOrder::BADC: // Mid-Little Endian
                    registers.append((val >> 16) & 0xFFFF);
                    registers.append(val & 0xFFFF);
                    break;
                default:
                    registers.append((val >> 16) & 0xFFFF);
                    registers.append(val & 0xFFFF);
                    break;
            }
            break;
        }
        
        case DataType::Float32: {
            float val = static_cast<float>(transmitValue);
            quint16 reg1, reg2;
            float32ToRegisters(val, reg1, reg2, byteOrder);
            registers.append(reg1);
            registers.append(reg2);
            break;
        }
        
        case DataType::Bool: {
            registers.append(value.toBool() ? 1 : 0);
            break;
        }
        
        default:
            registers.append(static_cast<quint16>(transmitValue));
            break;
    }
    
    return registers;
}

QVector<quint8> DataTypeUtils::encodeToBits(const QVariant& value, DataType dataType) {
    QVector<quint8> bits;
    
    if (dataType == DataType::Bool) {
        bits.append(value.toBool() ? 1 : 0);
    }
    
    return bits;
}

float DataTypeUtils::registersToFloat32(quint16 reg1, quint16 reg2, ByteOrder order) {
    quint32 combined = 0;
    
    switch (order) {
        case ByteOrder::ABCD: // Big Endian
            combined = (static_cast<quint32>(reg1) << 16) | reg2;
            break;
        case ByteOrder::DCBA: // Little Endian
            combined = (static_cast<quint32>(reg2) << 16) | reg1;
            combined = ((combined & 0xFF000000) >> 24) | ((combined & 0x00FF0000) >> 8) |
                      ((combined & 0x0000FF00) << 8) | ((combined & 0x000000FF) << 24);
            break;
        case ByteOrder::CDAB: // Mid-Big Endian
            combined = (static_cast<quint32>(reg2) << 16) | reg1;
            break;
        case ByteOrder::BADC: { // Mid-Little Endian
            combined = (static_cast<quint32>(reg1) << 16) | reg2;
            quint16 high = (combined >> 16) & 0xFFFF;
            quint16 low = combined & 0xFFFF;
            high = ((high & 0xFF) << 8) | ((high & 0xFF00) >> 8);
            low = ((low & 0xFF) << 8) | ((low & 0xFF00) >> 8);
            combined = (static_cast<quint32>(high) << 16) | low;
            break;
        }
        default:
            combined = (static_cast<quint32>(reg1) << 16) | reg2;
            break;
    }
    
    float result;
    std::memcpy(&result, &combined, sizeof(float));
    return result;
}

void DataTypeUtils::float32ToRegisters(float value, quint16& reg1, quint16& reg2, ByteOrder order) {
    quint32 combined;
    std::memcpy(&combined, &value, sizeof(float));
    
    switch (order) {
        case ByteOrder::ABCD: // Big Endian
            reg1 = (combined >> 16) & 0xFFFF;
            reg2 = combined & 0xFFFF;
            break;
        case ByteOrder::DCBA: // Little Endian
            reg1 = combined & 0xFFFF;
            reg2 = (combined >> 16) & 0xFFFF;
            break;
        case ByteOrder::CDAB: // Mid-Big Endian
            reg1 = combined & 0xFFFF;
            reg2 = (combined >> 16) & 0xFFFF;
            break;
        case ByteOrder::BADC: // Mid-Little Endian
            reg1 = (combined >> 16) & 0xFFFF;
            reg2 = combined & 0xFFFF;
            break;
        default:
            reg1 = (combined >> 16) & 0xFFFF;
            reg2 = combined & 0xFFFF;
            break;
    }
}

double DataTypeUtils::registersToFloat64(const QVector<quint16>& regs, ByteOrder order) {
    if (regs.size() < 4) {
        return 0.0;
    }
    
    quint64 combined = 0;
    
    // 根据字节序组合8个字节
    switch (order) {
        case ByteOrder::ABCDEFGH: // Big Endian
            combined = (static_cast<quint64>(regs[0]) << 48) |
                      (static_cast<quint64>(regs[1]) << 32) |
                      (static_cast<quint64>(regs[2]) << 16) |
                      static_cast<quint64>(regs[3]);
            break;
        case ByteOrder::HGFEDCBA: // Little Endian
            combined = (static_cast<quint64>(regs[3]) << 48) |
                      (static_cast<quint64>(regs[2]) << 32) |
                      (static_cast<quint64>(regs[1]) << 16) |
                      static_cast<quint64>(regs[0]);
            break;
        default:
            combined = (static_cast<quint64>(regs[0]) << 48) |
                      (static_cast<quint64>(regs[1]) << 32) |
                      (static_cast<quint64>(regs[2]) << 16) |
                      static_cast<quint64>(regs[3]);
            break;
    }
    
    double result;
    std::memcpy(&result, &combined, sizeof(double));
    return result;
}

QVector<quint16> DataTypeUtils::float64ToRegisters(double value, ByteOrder order) {
    quint64 combined;
    std::memcpy(&combined, &value, sizeof(double));
    
    QVector<quint16> regs;
    
    switch (order) {
        case ByteOrder::ABCDEFGH: // Big Endian
            regs.append((combined >> 48) & 0xFFFF);
            regs.append((combined >> 32) & 0xFFFF);
            regs.append((combined >> 16) & 0xFFFF);
            regs.append(combined & 0xFFFF);
            break;
        case ByteOrder::HGFEDCBA: // Little Endian
            regs.append(combined & 0xFFFF);
            regs.append((combined >> 16) & 0xFFFF);
            regs.append((combined >> 32) & 0xFFFF);
            regs.append((combined >> 48) & 0xFFFF);
            break;
        default:
            regs.append((combined >> 48) & 0xFFFF);
            regs.append((combined >> 32) & 0xFFFF);
            regs.append((combined >> 16) & 0xFFFF);
            regs.append(combined & 0xFFFF);
            break;
    }
    
    return regs;
}

} // namespace ModbusPlexLink