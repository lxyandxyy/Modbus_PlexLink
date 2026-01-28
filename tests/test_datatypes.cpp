#include <QDebug>
#include <QTest>

#include "core/DataTypes.h"

using namespace ModbusPlexLink;

class TestDataTypes : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase() { qDebug() << "Starting DataTypes tests..."; }

  void cleanupTestCase() { qDebug() << "DataTypes tests completed."; }

  // 测试UInt16解析
  void testParseUInt16() {
    QVector<quint16> regs;
    regs.append(12345);

    QVariant result =
        DataTypeUtils::parseRegisters(regs, DataType::UInt16, ByteOrder::AB);
    QCOMPARE(result.toUInt(), static_cast<uint>(12345));

    // 测试字节序转换（BA）
    regs.clear();
    regs.append(0x1234);  // Big Endian
    QVariant resultBA =
        DataTypeUtils::parseRegisters(regs, DataType::UInt16, ByteOrder::BA);
    QCOMPARE(resultBA.toUInt(), static_cast<uint>(0x3412));  // 字节交换后
  }

  // 测试Int16解析
  void testParseInt16() {
    QVector<quint16> regs;
    regs.append(static_cast<quint16>(-100));  // -100的补码表示

    QVariant result =
        DataTypeUtils::parseRegisters(regs, DataType::Int16, ByteOrder::AB);
    QCOMPARE(result.toInt(), -100);
  }

  // 测试UInt32解析
  void testParseUInt32() {
    QVector<quint16> regs;

    // ABCD: 高字在前
    regs.append(0x1234);
    regs.append(0x5678);

    QVariant resultABCD =
        DataTypeUtils::parseRegisters(regs, DataType::UInt32, ByteOrder::ABCD);
    QCOMPARE(resultABCD.toUInt(), static_cast<uint>(0x12345678));

    // CDAB: 字交换
    QVariant resultCDAB =
        DataTypeUtils::parseRegisters(regs, DataType::UInt32, ByteOrder::CDAB);
    QCOMPARE(resultCDAB.toUInt(), static_cast<uint>(0x56781234));
  }

  // 测试Float32解析
  void testParseFloat32() {
    // IEEE 754: 3.14159 的表示
    float testValue = 3.14159f;
    quint32* ptr = reinterpret_cast<quint32*>(&testValue);
    quint32 rawBits = *ptr;

    QVector<quint16> regs;
    regs.append((rawBits >> 16) & 0xFFFF);
    regs.append(rawBits & 0xFFFF);

    QVariant result =
        DataTypeUtils::parseRegisters(regs, DataType::Float32, ByteOrder::ABCD);
    QVERIFY(qAbs(result.toDouble() - 3.14159) < 0.0001);
  }

  // 测试倍率和偏移量
  void testScaleAndOffset() {
    QVector<quint16> regs;
    regs.append(2731);  // 原始值

    // scale = 0.1, offset = -273.1
    // 结果应该是: 2731 * 0.1 + (-273.1) = 273.1 - 273.1 = 0
    QVariant result = DataTypeUtils::parseRegisters(regs, DataType::UInt16,
                                                    ByteOrder::AB, 0.1, -273.1);
    QVERIFY(qAbs(result.toDouble() - 0.0) < 0.1);

    // 另一个例子：温度转换
    // 原始值 3000 -> 3000 * 0.1 = 300K -> 300 - 273.15 = 26.85°C
    regs.clear();
    regs.append(3000);
    QVariant temp = DataTypeUtils::parseRegisters(regs, DataType::UInt16,
                                                  ByteOrder::AB, 0.1, -273.15);
    QVERIFY(qAbs(temp.toDouble() - 26.85) < 0.01);
  }

  // 测试编码为寄存器
  void testEncodeUInt16() {
    QVariant value = 12345;
    QVector<quint16> regs = DataTypeUtils::encodeToRegisters(
        value, DataType::UInt16, ByteOrder::AB);

    QCOMPARE(regs.size(), 1);
    QCOMPARE(regs[0], static_cast<quint16>(12345));
  }

  // 测试编码UInt32
  void testEncodeUInt32() {
    QVariant value = static_cast<uint>(0x12345678);

    QVector<quint16> regsABCD = DataTypeUtils::encodeToRegisters(
        value, DataType::UInt32, ByteOrder::ABCD);
    QCOMPARE(regsABCD.size(), 2);
    QCOMPARE(regsABCD[0], static_cast<quint16>(0x1234));
    QCOMPARE(regsABCD[1], static_cast<quint16>(0x5678));

    QVector<quint16> regsCDAB = DataTypeUtils::encodeToRegisters(
        value, DataType::UInt32, ByteOrder::CDAB);
    QCOMPARE(regsCDAB.size(), 2);
    QCOMPARE(regsCDAB[0], static_cast<quint16>(0x5678));
    QCOMPARE(regsCDAB[1], static_cast<quint16>(0x1234));
  }

  // 测试编码Float32
  void testEncodeFloat32() {
    QVariant value = 3.14159;
    QVector<quint16> regs = DataTypeUtils::encodeToRegisters(
        value, DataType::Float32, ByteOrder::ABCD);

    QCOMPARE(regs.size(), 2);

    // 反向解析应该得到相同的值
    QVariant parsed =
        DataTypeUtils::parseRegisters(regs, DataType::Float32, ByteOrder::ABCD);
    QVERIFY(qAbs(parsed.toDouble() - 3.14159) < 0.0001);
  }

  // 测试反向倍率和偏移
  void testReverseScaleOffset() {
    // 正向：原始值100 -> 100 * 0.1 + 0 = 10
    // 反向：实际值10 -> (10 - 0) / 0.1 = 100

    QVariant actualValue = 10.0;
    QVector<quint16> regs = DataTypeUtils::encodeToRegisters(
        actualValue, DataType::UInt16, ByteOrder::AB, 0.1, 0.0);

    QCOMPARE(regs.size(), 1);
    QCOMPARE(regs[0], static_cast<quint16>(100));

    // 验证往返转换
    QVariant parsed = DataTypeUtils::parseRegisters(regs, DataType::UInt16,
                                                    ByteOrder::AB, 0.1, 0.0);
    QVERIFY(qAbs(parsed.toDouble() - 10.0) < 0.01);
  }

  // 测试字符串转换工具
  void testStringConversions() {
    // 测试DataType转换
    QCOMPARE(DataTypeUtils::dataTypeFromString("UInt16"), DataType::UInt16);
    QCOMPARE(DataTypeUtils::dataTypeFromString("Float32"), DataType::Float32);
    QCOMPARE(DataTypeUtils::dataTypeFromString("float"), DataType::Float32);
    QCOMPARE(DataTypeUtils::dataTypeToString(DataType::UInt16),
             QString("UInt16"));

    // 测试ByteOrder转换
    QCOMPARE(DataTypeUtils::byteOrderFromString("AB"), ByteOrder::AB);
    QCOMPARE(DataTypeUtils::byteOrderFromString("ABCD"), ByteOrder::ABCD);
    QCOMPARE(DataTypeUtils::byteOrderToString(ByteOrder::ABCD),
             QString("ABCD"));

    // 测试RegisterType转换
    QCOMPARE(DataTypeUtils::registerTypeFromString("Holding"),
             RegisterType::HoldingRegister);
    QCOMPARE(DataTypeUtils::registerTypeFromString("Coil"), RegisterType::Coil);
    QCOMPARE(DataTypeUtils::registerTypeToString(RegisterType::HoldingRegister),
             QString("Holding"));
  }

  // 测试获取寄存器数量
  void testGetRegisterCount() {
    QCOMPARE(DataTypeUtils::getRegisterCount(DataType::UInt16), 1);
    QCOMPARE(DataTypeUtils::getRegisterCount(DataType::Int16), 1);
    QCOMPARE(DataTypeUtils::getRegisterCount(DataType::UInt32), 2);
    QCOMPARE(DataTypeUtils::getRegisterCount(DataType::Int32), 2);
    QCOMPARE(DataTypeUtils::getRegisterCount(DataType::Float32), 2);
    QCOMPARE(DataTypeUtils::getRegisterCount(DataType::Float64), 4);
  }

  // 测试CollectorMappingRule的转换方法
  void testCollectorMappingRule() {
    CollectorMappingRule rule;
    rule.dataType = DataType::Float32;
    rule.byteOrder = ByteOrder::ABCD;
    rule.scale = 0.1;
    rule.offset = -273.15;

    // 模拟从设备读取的Float32值：3000.0K
    float rawValue = 3000.0f;
    quint32* ptr = reinterpret_cast<quint32*>(&rawValue);
    quint32 rawBits = *ptr;

    QVector<quint16> rawRegs;
    rawRegs.append((rawBits >> 16) & 0xFFFF);
    rawRegs.append(rawBits & 0xFFFF);

    // 应用转换
    QVariant result = rule.applyTransform(rawRegs);

    // 期望值：3000 * 0.1 + (-273.15) = 300 - 273.15 = 26.85°C
    QVERIFY(qAbs(result.toDouble() - 26.85) < 0.1);
  }

  // 测试ServerMappingRule的反向转换
  void testServerMappingRule() {
    ServerMappingRule rule;
    rule.dataType = DataType::UInt16;
    rule.byteOrder = ByteOrder::AB;
    rule.scale = 1.0;
    rule.offset = 0.0;

    QVariant actualValue = 123.45;
    QVector<quint16> regs = rule.reverseTransform(actualValue);

    QCOMPARE(regs.size(), 1);
    QCOMPARE(regs[0], static_cast<quint16>(123));  // 四舍五入
  }

  // 测试Bool类型
  void testBoolType() {
    QVector<quint8> bits;
    bits.append(1);

    QVariant result = DataTypeUtils::parseBits(bits, DataType::Bool);
    QCOMPARE(result.toBool(), true);

    bits[0] = 0;
    result = DataTypeUtils::parseBits(bits, DataType::Bool);
    QCOMPARE(result.toBool(), false);

    // 编码
    QVector<quint8> encoded =
        DataTypeUtils::encodeToBits(QVariant(true), DataType::Bool);
    QCOMPARE(encoded.size(), 1);
    QCOMPARE(encoded[0], static_cast<quint8>(1));
  }
};

QTEST_MAIN(TestDataTypes)
#include "test_datatypes.moc"

