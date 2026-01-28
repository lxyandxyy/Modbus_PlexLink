#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include "core/Channel.h"
#include "core/DataTypes.h"

using namespace ModbusPlexLink;

// 模拟采集器实现（用于测试）
class MockCollector : public ICollector {
public:
    explicit MockCollector(const QString& name, QObject *parent = nullptr)
        : ICollector(parent), m_name(name), m_running(false), m_connected(false) {}
    
    bool initialize(const QJsonObject& config) override {
        m_config = config;
        return true;
    }
    
    void setDataModel(UniversalDataModel* udm) override {
        m_udm = udm;
    }
    
    void setMappingRules(const QList<CollectorMappingRule>& rules) override {
        m_rules = rules;
    }
    
    bool start() override {
        if (m_running) return true;
        m_running = true;
        m_connected = true;
        emit connectionStateChanged(true);
        
        // 模拟数据采集
        if (m_udm) {
            for (const CollectorMappingRule& rule : m_rules) {
                DataPoint dp(100.0 + rule.address, DataQuality::Good);
                m_udm->updatePoint(rule.tagName, dp);
                emit dataCollected(rule.tagName, dp.value);
            }
        }
        return true;
    }
    
    void stop() override {
        m_running = false;
        m_connected = false;
        emit connectionStateChanged(false);
    }
    
    bool isRunning() const override { return m_running; }
    QString getName() const override { return m_name; }
    bool isConnected() const override { return m_connected; }
    
    QJsonObject getStatistics() const override {
        QJsonObject stats;
        stats["totalPoints"] = m_rules.size();
        stats["connected"] = m_connected;
        return stats;
    }
    
private:
    QString m_name;
    bool m_running;
    bool m_connected;
    UniversalDataModel* m_udm = nullptr;
    QList<CollectorMappingRule> m_rules;
    QJsonObject m_config;
};

// 模拟服务器实现（用于测试）
class MockServer : public IServer {
public:
    explicit MockServer(const QString& name, QObject *parent = nullptr)
        : IServer(parent), m_name(name), m_running(false), m_clientCount(0) {}
    
    bool initialize(const QJsonObject& config) override {
        m_config = config;
        m_listenAddress = config["listenAddress"].toString("0.0.0.0");
        m_listenPort = config["port"].toInt(502);
        return true;
    }
    
    void setDataModel(UniversalDataModel* udm) override {
        m_udm = udm;
    }
    
    bool start() override {
        if (m_running) return true;
        m_running = true;
        
        // 模拟客户端连接（使用QPointer保护）
        QTimer::singleShot(100, this, [this]() {
            if (m_running) {
                m_clientCount++;
                emit clientConnected("192.168.1.100");
            }
        });
        
        return true;
    }
    
    void stop() override {
        m_running = false;
        m_clientCount = 0;
    }
    
    bool isRunning() const override { return m_running; }
    QString getName() const override { return m_name; }
    QString getListenAddress() const override { return m_listenAddress; }
    int getListenPort() const override { return m_listenPort; }
    int getClientCount() const override { return m_clientCount; }
    
    QJsonObject getStatistics() const override {
        QJsonObject stats;
        stats["clientCount"] = m_clientCount;
        stats["port"] = m_listenPort;
        return stats;
    }
    
private:
    QString m_name;
    bool m_running;
    int m_clientCount;
    QString m_listenAddress;
    int m_listenPort;
    UniversalDataModel* m_udm = nullptr;
    QJsonObject m_config;
};

class TestChannel : public QObject {
    Q_OBJECT
    
private slots:
    void initTestCase() {
        qDebug() << "Starting Channel tests...";
    }
    
    void cleanupTestCase() {
        qDebug() << "Channel tests completed.";
    }
    
    // 测试通道创建和基本属性
    void testChannelCreation() {
        Channel channel("TestChannel");
        
        QCOMPARE(channel.getName(), QString("TestChannel"));
        QCOMPARE(channel.getState(), ChannelState::Stopped);
        QVERIFY(!channel.isRunning());
        QVERIFY(channel.getDataModel() != nullptr);
        QCOMPARE(channel.getCollectors().size(), 0);
        QCOMPARE(channel.getServers().size(), 0);
    }
    
    // 测试通道配置
    void testChannelConfiguration() {
        Channel channel("TestChannel");
        
        ChannelConfig config;
        config.name = "TestChannel";
        config.enabled = true;
        
        // 测试基本配置
        bool result = channel.configure(config);
        QVERIFY(result);
        QCOMPARE(channel.getConfig().name, QString("TestChannel"));
    }
    
    // 测试JSON配置
    void testJsonConfiguration() {
        Channel channel("TestChannel");
        
        QJsonObject config;
        config["name"] = "TestChannel";
        config["enabled"] = true;
        
        // 配置采集器（包含映射规则）
        QJsonArray collectors;
        QJsonObject collector;
        collector["name"] = "TestCollector";
        collector["protocol"] = "modbus-tcp";
        collector["ip"] = "127.0.0.1";
        collector["port"] = 502;
        
        QJsonArray mappings;
        QJsonObject mapping1;
        mapping1["tagName"] = "Test.Value1";
        mapping1["registerType"] = "Holding";
        mapping1["address"] = 100;
        mapping1["dataType"] = "UInt16";
        mapping1["byteOrder"] = "AB";
        mappings.append(mapping1);
        
        collector["mappings"] = mappings;
        collectors.append(collector);
        config["collectors"] = collectors;
        
        bool result = channel.configure(config);
        QVERIFY(result);
        QCOMPARE(channel.getConfig().name, QString("TestChannel"));
    }
    
    // 测试添加和移除采集器
    void testAddRemoveCollector() {
        Channel channel("TestChannel");
        
        MockCollector* collector = new MockCollector("TestCollector");
        
        // 添加采集器
        bool added = channel.addCollector(collector);
        QVERIFY(added);
        QCOMPARE(channel.getCollectors().size(), 1);
        QCOMPARE(channel.getCollector("TestCollector"), collector);
        
        // 尝试添加重复的采集器
        MockCollector* duplicate = new MockCollector("TestCollector");
        bool addedDup = channel.addCollector(duplicate);
        QVERIFY(!addedDup);
        QCOMPARE(channel.getCollectors().size(), 1);
        delete duplicate;
        
        // 移除采集器
        bool removed = channel.removeCollector("TestCollector");
        QVERIFY(removed);
        QCOMPARE(channel.getCollectors().size(), 0);
        
        // 尝试移除不存在的采集器
        bool removedNonExist = channel.removeCollector("NonExistent");
        QVERIFY(!removedNonExist);
    }
    
    // 测试添加和移除服务器
    void testAddRemoveServer() {
        Channel channel("TestChannel");
        
        MockServer* server = new MockServer("TestServer");
        
        // 添加服务器
        bool added = channel.addServer(server);
        QVERIFY(added);
        QCOMPARE(channel.getServers().size(), 1);
        QCOMPARE(channel.getServer("TestServer"), server);
        
        // 移除服务器
        bool removed = channel.removeServer("TestServer");
        QVERIFY(removed);
        QCOMPARE(channel.getServers().size(), 0);
    }
    
    // 测试通道启动和停止
    void testChannelStartStop() {
        Channel channel("TestChannel");
        
        // 添加模拟采集器和服务器
        MockCollector* collector = new MockCollector("TestCollector");
        MockServer* server = new MockServer("TestServer");
        
        channel.addCollector(collector);
        channel.addServer(server);
        
        // 监听状态变化信号
        QSignalSpy stateSpy(&channel, &Channel::stateChanged);
        
        // 启动通道
        bool started = channel.start();
        QVERIFY(started);
        QCOMPARE(channel.getState(), ChannelState::Running);
        QVERIFY(channel.isRunning());
        QVERIFY(collector->isRunning());
        QVERIFY(server->isRunning());
        
        // 验证状态变化信号
        QVERIFY(stateSpy.count() >= 1);
        
        // 停止通道
        channel.stop();
        QCOMPARE(channel.getState(), ChannelState::Stopped);
        QVERIFY(!channel.isRunning());
        QVERIFY(!collector->isRunning());
        QVERIFY(!server->isRunning());
    }
    
    // 测试通道重启
    void testChannelRestart() {
        Channel channel("TestChannel");
        
        MockCollector* collector = new MockCollector("TestCollector");
        channel.addCollector(collector);
        
        // 启动通道
        channel.start();
        QVERIFY(channel.isRunning());
        
        // 重启通道
        bool restarted = channel.restart();
        QVERIFY(restarted);
        QVERIFY(channel.isRunning());
    }
    
    // 测试数据流
    void testDataFlow() {
        Channel channel("TestChannel");
        
        // 配置映射规则
        CollectorMappingRule rule;
        rule.tagName = "Test.Temperature";
        rule.registerType = RegisterType::HoldingRegister;
        rule.address = 100;
        rule.dataType = DataType::UInt16;
        rule.byteOrder = ByteOrder::AB;
        rule.scale = 0.1;
        rule.offset = -273.15;
        
        MockCollector* collector = new MockCollector("TestCollector");
        collector->setMappingRules({rule});
        
        channel.addCollector(collector);
        
        // 监听数据更新信号
        QSignalSpy dataSpy(&channel, &Channel::dataUpdated);
        
        // 启动通道（这会触发模拟数据采集）
        channel.start();
        
        // 等待数据更新
        QTest::qWait(100);
        
        // 验证数据已写入UDM
        UniversalDataModel* udm = channel.getDataModel();
        QVERIFY(udm->hasTag("Test.Temperature"));
        
        DataPoint dp = udm->readPoint("Test.Temperature");
        QVERIFY(dp.isValid());
        QCOMPARE(dp.quality, DataQuality::Good);
        
        // 验证数据更新信号
        QVERIFY(dataSpy.count() > 0);
    }
    
    // 测试错误处理
    void testErrorHandling() {
        Channel channel("TestChannel");
        
        QSignalSpy errorSpy(&channel, &Channel::errorOccurred);
        
        MockCollector* collector = new MockCollector("TestCollector");
        channel.addCollector(collector);
        
        // 手动触发错误
        emit collector->errorOccurred("Test error from collector");
        
        // 验证错误信号
        QCOMPARE(errorSpy.count(), 1);
        QList<QVariant> arguments = errorSpy.takeFirst();
        QString error = arguments.at(0).toString();
        QVERIFY(error.contains("Test error"));
    }
    
    // 测试统计信息
    void testStatistics() {
        Channel channel("TestChannel");
        
        MockCollector* collector = new MockCollector("TestCollector");
        MockServer* server = new MockServer("TestServer");
        
        channel.addCollector(collector);
        channel.addServer(server);
        channel.start();
        
        // 等待模拟客户端连接，使用事件循环
        QEventLoop loop;
        QTimer::singleShot(200, &loop, &QEventLoop::quit);
        loop.exec();
        
        QJsonObject stats = channel.getStatistics();
        
        QVERIFY(stats.contains("name"));
        QVERIFY(stats.contains("state"));
        QVERIFY(stats.contains("dataPointCount"));
        QVERIFY(stats.contains("collectors"));
        QVERIFY(stats.contains("servers"));
        
        QCOMPARE(stats["name"].toString(), QString("TestChannel"));
        QCOMPARE(stats["state"].toInt(), static_cast<int>(ChannelState::Running));
        
        QJsonArray collectorStats = stats["collectors"].toArray();
        QCOMPARE(collectorStats.size(), 1);
        
        QJsonArray serverStats = stats["servers"].toArray();
        QCOMPARE(serverStats.size(), 1);
        
        // 停止通道，确保清理
        channel.stop();
    }
    
    // 测试并发访问UDM
    void testConcurrentUDMAccess() {
        Channel channel("TestChannel");
        UniversalDataModel* udm = channel.getDataModel();
        
        const int NUM_THREADS = 5;
        const int WRITES_PER_THREAD = 100;
        
        QList<QThread*> threads;
        
        // 创建多个线程同时写入UDM
        for (int i = 0; i < NUM_THREADS; ++i) {
            QThread* thread = QThread::create([udm, i, WRITES_PER_THREAD]() {
                for (int j = 0; j < WRITES_PER_THREAD; ++j) {
                    QString tag = QString("Thread%1.Value%2").arg(i).arg(j);
                    DataPoint dp(i * 1000 + j, DataQuality::Good);
                    udm->updatePoint(tag, dp);
                }
            });
            threads.append(thread);
            thread->start();
        }
        
        // 等待所有线程完成
        for (QThread* thread : threads) {
            thread->wait();
            delete thread;
        }
        
        // 验证所有数据都已写入
        QCOMPARE(udm->size(), NUM_THREADS * WRITES_PER_THREAD);
        
        // 验证数据正确性
        for (int i = 0; i < NUM_THREADS; ++i) {
            for (int j = 0; j < WRITES_PER_THREAD; ++j) {
                QString tag = QString("Thread%1.Value%2").arg(i).arg(j);
                DataPoint dp = udm->readPoint(tag);
                QCOMPARE(dp.value.toInt(), i * 1000 + j);
            }
        }
    }
};

QTEST_MAIN(TestChannel)
#include "test_channel.moc"
