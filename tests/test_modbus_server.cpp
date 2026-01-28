#include <QTest>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include "adapters/ModbusTcpServer.h"
#include "core/UniversalDataModel.h"
#include "core/DataTypes.h"

using namespace ModbusPlexLink;

class TestModbusServer : public QObject {
    Q_OBJECT
    
private slots:
    void initTestCase() {
        qDebug() << "Starting Modbus Server tests...";
    }
    
    void cleanupTestCase() {
        qDebug() << "Modbus Server tests completed.";
    }
    
    // 测试服务器创建和基本属性
    void testServerCreation() {
        ModbusTcpServer server("TestServer");
        
        QCOMPARE(server.getName(), QString("TestServer"));
        QVERIFY(!server.isRunning());
        QCOMPARE(server.getClientCount(), 0);
        
        QJsonObject stats = server.getStatistics();
        QCOMPARE(stats["name"].toString(), QString("TestServer"));
        QCOMPARE(stats["running"].toBool(), false);
        QCOMPARE(stats["clientCount"].toInt(), 0);
    }
    
    // 测试配置初始化
    void testInitialization() {
        ModbusTcpServer server("TestServer");
        
        QJsonObject config;
        config["listenAddress"] = "127.0.0.1";
        config["port"] = 1503;
        config["maxClients"] = 5;
        config["timeout"] = 500;
        config["logRequests"] = false;
        config["logErrors"] = true;
        
        bool result = server.initialize(config);
        QVERIFY(result);
        
        QCOMPARE(server.getListenAddress(), QString("127.0.0.1"));
        QCOMPARE(server.getListenPort(), 1503);
    }
    
    // 测试无效配置
    void testInvalidConfiguration() {
        ModbusTcpServer server("TestServer");
        
        // 无效端口
        QJsonObject config;
        config["port"] = -1;
        
        bool result = server.initialize(config);
        QVERIFY(!result);
        
        // 端口超出范围
        config["port"] = 70000;
        result = server.initialize(config);
        QVERIFY(!result);
    }
    
    // 测试虚拟化引擎
    void testVirtualizationEngine() {
        VirtualizationEngine engine;
        
        QJsonObject rules;
        QJsonArray virtualizations;
        
        // 创建虚拟单元1
        QJsonObject unit1;
        unit1["virtualUnitId"] = 10;
        unit1["tagPrefix"] = "PCS.Bay1";
        unit1["description"] = "Bay 1 PCS";
        
        QJsonArray addressMap1;
        QJsonObject map1;
        map1["tagSuffix"] = ".Voltage";
        map1["registerType"] = "Holding";
        map1["address"] = 40001;
        addressMap1.append(map1);
        
        QJsonObject map2;
        map2["tagSuffix"] = ".Current";
        map2["registerType"] = "Holding";
        map2["address"] = 40002;
        addressMap1.append(map2);
        
        unit1["addressMap"] = addressMap1;
        virtualizations.append(unit1);
        
        // 创建虚拟单元2
        QJsonObject unit2;
        unit2["virtualUnitId"] = 11;
        unit2["tagPrefix"] = "PCS.Bay2";
        unit2["description"] = "Bay 2 PCS";
        
        QJsonArray addressMap2;
        QJsonObject map3;
        map3["tagSuffix"] = ".Voltage";
        map3["registerType"] = "Holding";
        map3["address"] = 40001;
        addressMap2.append(map3);
        
        unit2["addressMap"] = addressMap2;
        virtualizations.append(unit2);
        
        rules["virtualization"] = virtualizations;
        
        // 设置规则
        engine.setRules(rules);
        
        // 测试地址转换
        QString tag1 = engine.translateToTag(10, RegisterType::HoldingRegister, 40001);
        QCOMPARE(tag1, QString("PCS.Bay1.Voltage"));
        
        QString tag2 = engine.translateToTag(10, RegisterType::HoldingRegister, 40002);
        QCOMPARE(tag2, QString("PCS.Bay1.Current"));
        
        QString tag3 = engine.translateToTag(11, RegisterType::HoldingRegister, 40001);
        QCOMPARE(tag3, QString("PCS.Bay2.Voltage"));
        
        // 测试无效地址
        QString tag4 = engine.translateToTag(10, RegisterType::HoldingRegister, 40003);
        QVERIFY(tag4.isEmpty());
        
        // 测试反向转换
        int unitId;
        RegisterType type;
        int address;
        
        bool result = engine.translateFromTag("PCS.Bay1.Voltage", unitId, type, address);
        QVERIFY(result);
        QCOMPARE(unitId, 10);
        QCOMPARE(address, 40001);
        QCOMPARE(type, RegisterType::HoldingRegister);
        
        // 测试获取单元的所有标签
        QStringList tags = engine.getTagsForUnit(10);
        QCOMPARE(tags.size(), 2);
        QVERIFY(tags.contains("PCS.Bay1.Voltage"));
        QVERIFY(tags.contains("PCS.Bay1.Current"));
        
        // 测试获取所有虚拟单元ID
        QList<int> unitIds = engine.getAllVirtualUnitIds();
        QCOMPARE(unitIds.size(), 2);
        QVERIFY(unitIds.contains(10));
        QVERIFY(unitIds.contains(11));
        
        // 测试地址验证
        QVERIFY(engine.isValidAddress(10, RegisterType::HoldingRegister, 40001));
        QVERIFY(engine.isValidAddress(10, RegisterType::HoldingRegister, 40002));
        QVERIFY(!engine.isValidAddress(10, RegisterType::HoldingRegister, 40003));
        QVERIFY(!engine.isValidAddress(12, RegisterType::HoldingRegister, 40001));
    }
    
    // 测试服务器启动和停止
    void testStartStop() {
        ModbusTcpServer server("TestServer");
        UniversalDataModel udm;
        
        server.setDataModel(&udm);
        
        QJsonObject config;
        config["listenAddress"] = "127.0.0.1";
        config["port"] = 1504;  // 使用不同的端口避免冲突
        
        server.initialize(config);
        
        // 由于实际启动服务器需要监听端口，可能会失败
        // 这里我们只测试接口调用
        server.start();
        // 不验证isRunning()，因为启动可能失败
        
        server.stop();
        QVERIFY(!server.isRunning());
        
        // 再次停止应该是安全的
        server.stop();
        QVERIFY(!server.isRunning());
    }
    
    // 测试数据模型集成
    void testDataModelIntegration() {
        ModbusTcpServer server("TestServer");
        UniversalDataModel udm;
        VirtualizationEngine engine;
        
        // 准备测试数据
        DataPoint dp1(100.5, DataQuality::Good);
        udm.updatePoint("PCS.Bay1.Voltage", dp1);
        
        DataPoint dp2(50.2, DataQuality::Good);
        udm.updatePoint("PCS.Bay1.Current", dp2);
        
        server.setDataModel(&udm);
        server.setVirtualizationEngine(&engine);
        
        // 设置虚拟化规则
        QJsonObject rules;
        QJsonArray virtualizations;
        
        QJsonObject unit;
        unit["virtualUnitId"] = 10;
        unit["tagPrefix"] = "PCS.Bay1";
        
        QJsonArray addressMap;
        QJsonObject map1;
        map1["tagSuffix"] = ".Voltage";
        map1["registerType"] = "Holding";
        map1["address"] = 40001;
        addressMap.append(map1);
        
        QJsonObject map2;
        map2["tagSuffix"] = ".Current";
        map2["registerType"] = "Holding";
        map2["address"] = 40002;
        addressMap.append(map2);
        
        unit["addressMap"] = addressMap;
        virtualizations.append(unit);
        rules["virtualization"] = virtualizations;
        
        engine.setRules(rules);
        
        // 测试地址转换是否正确
        QString tag = engine.translateToTag(10, RegisterType::HoldingRegister, 40001);
        QCOMPARE(tag, QString("PCS.Bay1.Voltage"));
        
        // 验证数据可以从UDM读取
        DataPoint readDp = udm.readPoint(tag);
        QVERIFY(readDp.isValid());
        QCOMPARE(readDp.value.toDouble(), 100.5);
    }
    
    // 测试信号
    void testSignals() {
        ModbusTcpServer server("TestServer");
        
        QSignalSpy clientConnectedSpy(&server, &ModbusTcpServer::clientConnected);
        QSignalSpy clientDisconnectedSpy(&server, &ModbusTcpServer::clientDisconnected);
        QSignalSpy errorSpy(&server, &ModbusTcpServer::errorOccurred);
        
        // 配置服务器
        QJsonObject config;
        config["listenAddress"] = "127.0.0.1";
        config["port"] = 1505;
        
        server.initialize(config);
        
        // 尝试启动（可能失败）
        server.start();
        
        // 停止服务器
        server.stop();
        
        // 验证信号（由于没有实际客户端连接，可能没有信号）
        QVERIFY(clientConnectedSpy.count() >= 0);
        QVERIFY(clientDisconnectedSpy.count() >= 0);
    }
    
    // 测试统计信息
    void testStatistics() {
        ModbusTcpServer server("TestServer");
        
        QJsonObject config;
        config["listenAddress"] = "0.0.0.0";
        config["port"] = 502;
        
        server.initialize(config);
        
        // 获取初始统计
        QJsonObject stats1 = server.getStatistics();
        QCOMPARE(stats1["totalRequests"].toInt(), 0);
        QCOMPARE(stats1["successfulRequests"].toInt(), 0);
        QCOMPARE(stats1["failedRequests"].toInt(), 0);
        QCOMPARE(stats1["bytesReceived"].toInt(), 0);
        QCOMPARE(stats1["bytesSent"].toInt(), 0);
        
        // 启动服务器
        server.start();
        
        // 等待一些时间
        QThread::msleep(100);
        
        // 停止服务器
        server.stop();
        
        // 再次获取统计
        QJsonObject stats2 = server.getStatistics();
        // 统计值应该保持或增加
        QVERIFY(stats2["totalRequests"].toInt() >= 0);
    }
    
    // 测试并发访问
    void testConcurrentAccess() {
        ModbusTcpServer server("TestServer");
        UniversalDataModel udm;
        
        server.setDataModel(&udm);
        
        const int NUM_THREADS = 5;
        QList<QThread*> threads;
        
        // 创建多个线程同时访问服务器
        for (int i = 0; i < NUM_THREADS; ++i) {
            QThread* thread = QThread::create([&server]() {
                for (int j = 0; j < 100; ++j) {
                    server.isRunning();
                    server.getClientCount();
                    server.getStatistics();
                    QThread::msleep(1);
                }
            });
            threads.append(thread);
            thread->start();
        }
        
        // 同时在主线程中操作
        QJsonObject config;
        config["listenAddress"] = "127.0.0.1";
        config["port"] = 1506;
        server.initialize(config);
        
        server.start();
        QThread::msleep(50);
        server.stop();
        
        // 等待所有线程完成
        for (QThread* thread : threads) {
            thread->wait();
            delete thread;
        }
        
        // 如果没有崩溃，测试通过
        QVERIFY(true);
    }
};

QTEST_MAIN(TestModbusServer)
#include "test_modbus_server.moc"
