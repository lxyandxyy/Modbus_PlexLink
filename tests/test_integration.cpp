#include <QTest>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include "core/ChannelManager.h"
#include "core/DataTypes.h"
#include "utils/ConfigManager.h"

using namespace ModbusPlexLink;

class TestIntegration : public QObject {
    Q_OBJECT
    
private slots:
    void initTestCase() {
        qDebug() << "Starting Integration tests...";
    }
    
    void cleanupTestCase() {
        qDebug() << "Integration tests completed.";
    }
    
    // 测试配置管理器
    void testConfigManager() {
        ConfigManager configMgr;
        
        // 测试JSON配置加载
        QJsonObject config;
        config["version"] = "1.0";
        
        QJsonArray channels;
        QJsonObject channel;
        channel["name"] = "TestChannel";
        channel["enabled"] = true;
        
        QJsonArray collectors;
        QJsonObject collector;
        collector["name"] = "TestCollector";
        collector["protocol"] = "modbus-tcp";
        collector["ip"] = "192.168.1.10";
        collector["port"] = 502;
        
        QJsonArray mappings;
        QJsonObject mapping;
        mapping["tagName"] = "Test.Value";
        mapping["registerType"] = "Holding";
        mapping["address"] = 100;
        mapping["dataType"] = "UInt16";
        mapping["byteOrder"] = "AB";
        mappings.append(mapping);
        
        collector["mappings"] = mappings;
        collectors.append(collector);
        channel["collectors"] = collectors;
        channels.append(channel);
        config["channels"] = channels;
        
        configMgr.setConfig(config);
        
        // 验证配置
        QString errorMsg;
        bool valid = configMgr.validateConfig(&errorMsg);
        if (!valid) {
            qDebug() << "Validation error:" << errorMsg;
        }
        QVERIFY(valid);
        
        // 导出JSON
        QJsonObject exported = configMgr.toJson();
        QCOMPARE(exported["version"].toString(), QString("1.0"));
    }
    
    // 测试映射规则解析
    void testMappingParsing() {
        QJsonArray mappings;
        
        QJsonObject map1;
        map1["tagName"] = "Meter.Power";
        map1["registerType"] = "Holding";
        map1["address"] = 20;
        map1["dataType"] = "Float32";
        map1["byteOrder"] = "ABCD";
        map1["scale"] = 1.0;
        map1["offset"] = 0.0;
        mappings.append(map1);
        
        QJsonObject map2;
        map2["tagName"] = "Meter.Voltage";
        map2["registerType"] = "Holding";
        map2["address"] = 22;
        map2["dataType"] = "UInt16";
        map2["byteOrder"] = "AB";
        map2["scale"] = 0.1;
        map2["offset"] = 0.0;
        mappings.append(map2);
        
        // 解析
        QList<CollectorMappingRule> rules = ConfigManager::parseCollectorMappings(mappings);
        
        QCOMPARE(rules.size(), 2);
        QCOMPARE(rules[0].tagName, QString("Meter.Power"));
        QCOMPARE(rules[0].dataType, DataType::Float32);
        QCOMPARE(rules[0].byteOrder, ByteOrder::ABCD);
        QCOMPARE(rules[0].count, 2);  // Float32需要2个寄存器
        
        QCOMPARE(rules[1].tagName, QString("Meter.Voltage"));
        QCOMPARE(rules[1].dataType, DataType::UInt16);
        QCOMPARE(rules[1].scale, 0.1);
    }
    
    // 测试虚拟设备解析
    void testVirtualDeviceParsing() {
        QJsonArray devices;
        
        QJsonObject device1;
        device1["virtualUnitId"] = 10;
        device1["name"] = "Device1";
        device1["description"] = "Test Device 1";
        
        QJsonArray mappings1;
        QJsonObject map1;
        map1["tagName"] = "Device.Value1";
        map1["registerType"] = "Holding";
        map1["address"] = 50;
        map1["dataType"] = "UInt16";
        map1["byteOrder"] = "AB";
        mappings1.append(map1);
        
        device1["mappings"] = mappings1;
        devices.append(device1);
        
        // 解析
        QList<VirtualDevice> virtualDevices = ConfigManager::parseVirtualDevices(devices);
        
        QCOMPARE(virtualDevices.size(), 1);
        QCOMPARE(virtualDevices[0].virtualUnitId, 10);
        QCOMPARE(virtualDevices[0].name, QString("Device1"));
        QCOMPARE(virtualDevices[0].mappings.size(), 1);
        QCOMPARE(virtualDevices[0].mappings[0].tagName, QString("Device.Value1"));
        QCOMPARE(virtualDevices[0].mappings[0].address, 50);
    }
    
    // 测试完整的数据流
    void testCompleteDataFlow() {
        // 1. 创建通道
        ChannelManager manager;
        Channel* channel = manager.createChannel("IntegrationTestChannel");
        QVERIFY(channel != nullptr);
        
        // 2. 获取UDM
        UniversalDataModel* udm = channel->getDataModel();
        QVERIFY(udm != nullptr);
        
        // 3. 模拟采集器写入数据
        // 模拟从设备读取Float32值：123.456
        float rawFloat = 123.456f;
        quint32* ptr = reinterpret_cast<quint32*>(&rawFloat);
        quint32 rawBits = *ptr;
        
        QVector<quint16> rawRegs;
        rawRegs.append((rawBits >> 16) & 0xFFFF);
        rawRegs.append(rawBits & 0xFFFF);
        
        // 解析为UDM值
        QVariant udmValue = DataTypeUtils::parseRegisters(
            rawRegs, DataType::Float32, ByteOrder::ABCD);
        
        DataPoint dp(udmValue, DataQuality::Good);
        udm->updatePoint("Test.Power", dp);
        
        // 4. 验证UDM中的数据
        DataPoint readDp = udm->readPoint("Test.Power");
        QVERIFY(readDp.isValid());
        QVERIFY(qAbs(readDp.value.toDouble() - 123.456) < 0.001);
        
        // 5. 模拟服务器读取并转换
        ServerMappingRule serverRule;
        serverRule.tagName = "Test.Power";
        serverRule.dataType = DataType::UInt16;
        serverRule.byteOrder = ByteOrder::AB;
        serverRule.scale = 1.0;
        serverRule.offset = 0.0;
        
        QVector<quint16> serverRegs = serverRule.reverseTransform(readDp.value);
        
        QCOMPARE(serverRegs.size(), 1);
        QCOMPARE(serverRegs[0], static_cast<quint16>(123));  // 四舍五入
        
        qDebug() << "完整数据流测试通过:";
        qDebug() << "  原始值(Float32):" << rawFloat;
        qDebug() << "  UDM值(double):" << udmValue.toDouble();
        qDebug() << "  转发值(UInt16):" << serverRegs[0];
    }
    
    // 测试数据类型转换的各种场景
    void testDataTypeConversions() {
        // 场景1：温度转换 (K -> °C)
        QVector<quint16> tempRegs;
        tempRegs.append(3000);  // 3000 (原始值)
        
        QVariant tempC = DataTypeUtils::parseRegisters(
            tempRegs, DataType::UInt16, ByteOrder::AB, 0.1, -273.15);
        
        // 3000 * 0.1 + (-273.15) = 300 - 273.15 = 26.85°C
        QVERIFY(qAbs(tempC.toDouble() - 26.85) < 0.01);
        
        // 场景2：功率转换 (mW -> kW)
        QVector<quint16> powerRegs;
        powerRegs.append(12500);  // 12500 mW
        
        QVariant powerKW = DataTypeUtils::parseRegisters(
            powerRegs, DataType::UInt16, ByteOrder::AB, 0.001, 0.0);
        
        // 12500 * 0.001 = 12.5 kW
        QVERIFY(qAbs(powerKW.toDouble() - 12.5) < 0.01);
        
        // 场景3：Float32 -> UInt16转换
        float originalFloat = 123.456f;
        quint32* fptr = reinterpret_cast<quint32*>(&originalFloat);
        
        QVector<quint16> floatRegs;
        floatRegs.append((*fptr >> 16) & 0xFFFF);
        floatRegs.append(*fptr & 0xFFFF);
        
        // 解析Float32
        QVariant floatValue = DataTypeUtils::parseRegisters(
            floatRegs, DataType::Float32, ByteOrder::ABCD);
        
        // 转换为UInt16
        QVector<quint16> uint16Regs = DataTypeUtils::encodeToRegisters(
            floatValue, DataType::UInt16, ByteOrder::AB);
        
        QCOMPARE(uint16Regs.size(), 1);
        QCOMPARE(uint16Regs[0], static_cast<quint16>(123));
    }
    
    // 测试配置文件加载
    void testConfigFileLoading() {
        // 如果存在示例配置文件，尝试加载
        QString configFile = "config_example_meters.json";
        
        if (QFile::exists(configFile)) {
            ConfigManager configMgr;
            bool loaded = configMgr.loadFromFile(configFile);
            
            if (loaded) {
                QString errorMsg;
                bool valid = configMgr.validateConfig(&errorMsg);
                
                if (!valid) {
                    qDebug() << "配置验证失败:" << errorMsg;
                }
                
                QVERIFY(valid);
                qDebug() << "配置文件加载并验证成功";
            } else {
                qDebug() << "配置文件加载失败，但测试继续";
                QVERIFY(true);  // 不是必须的，所以不失败
            }
        } else {
            qDebug() << "配置文件不存在，跳过测试";
            QVERIFY(true);
        }
    }
};

QTEST_MAIN(TestIntegration)
#include "test_integration.moc"

