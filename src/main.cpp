#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include "core/ChannelManager.h"
#include "core/UniversalDataModel.h"
#include "core/DataTypes.h"
#include "gui/MainWindow.h"

using namespace ModbusPlexLink;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    QCoreApplication::setOrganizationName("ModbusPlexLink");
    QCoreApplication::setOrganizationDomain("modbusplexlink.com");
    QCoreApplication::setApplicationName("Modbus PlexLink");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    qDebug() << "========================================";
    qDebug() << "Modbus PlexLink v1.0.0";
    qDebug() << "数据采集与虚拟化服务系统";
    qDebug() << "========================================";
    
    // 测试数据类型转换
    qDebug() << "\n[测试] 数据类型转换功能";
    
    // 测试Float32转换
    QVector<quint16> testRegs;
    float testFloat = 123.456f;
    quint32* ptr = reinterpret_cast<quint32*>(&testFloat);
    testRegs.append((*ptr >> 16) & 0xFFFF);
    testRegs.append(*ptr & 0xFFFF);
    
    QVariant parsed = DataTypeUtils::parseRegisters(testRegs, DataType::Float32, ByteOrder::ABCD);
    qDebug() << "Float32 解析测试: 原始值=" << testFloat << ", 解析值=" << parsed.toDouble();
    
    // 测试倍率和偏移
    QVector<quint16> tempRegs;
    tempRegs.append(3000);  // 原始值3000
    QVariant tempValue = DataTypeUtils::parseRegisters(tempRegs, DataType::UInt16, ByteOrder::AB, 0.1, -273.15);
    qDebug() << "温度转换测试: 原始值=3000, 倍率=0.1, 偏移=-273.15, 结果=" << tempValue.toDouble() << "°C";
    
    // 创建通道管理器
    qDebug() << "\n[启动] 创建通道管理器";
    ChannelManager manager;
    
    // 尝试加载配置文件
    QString configFile = "config_example_meters.json";
    if (QFile::exists(configFile)) {
        qDebug() << "[配置] 加载配置文件:" << configFile;
        if (manager.loadConfig(configFile)) {
            qDebug() << "[成功] 配置加载成功";
            qDebug() << "[信息] 通道数:" << manager.getChannelCount();
            qDebug() << "[信息] 运行中通道数:" << manager.getRunningChannelCount();
            
            // 显示通道信息
            for (const QString& channelName : manager.getChannelNames()) {
                Channel* channel = manager.getChannel(channelName);
                if (channel) {
                    qDebug() << "  - 通道:" << channelName 
                             << ", 状态:" << (channel->isRunning() ? "运行中" : "已停止")
                             << ", 采集器数:" << channel->getCollectors().size()
                             << ", 服务器数:" << channel->getServers().size();
                }
            }
        } else {
            qDebug() << "[警告] 配置加载失败，将使用默认配置";
        }
    } else {
        qDebug() << "[信息] 配置文件不存在，创建测试通道";
        
        // 创建测试通道
        Channel* testChannel = manager.createChannel("TestChannel");
        if (testChannel) {
            qDebug() << "[成功] 测试通道创建成功:" << testChannel->getName();
            
            // 测试UDM
            UniversalDataModel* udm = testChannel->getDataModel();
            
            // 写入测试数据
            DataPoint dp1(100, DataQuality::Good);
            udm->updatePoint("Test.Value1", dp1);
            
            DataPoint dp2(200.5, DataQuality::Good);
            udm->updatePoint("Test.Value2", dp2);
            
            // 读取测试数据
            DataPoint readDp1 = udm->readPoint("Test.Value1");
            DataPoint readDp2 = udm->readPoint("Test.Value2");
            
            qDebug() << "  Test.Value1:" << readDp1.value.toInt() 
                     << ", Quality:" << static_cast<int>(readDp1.quality);
            qDebug() << "  Test.Value2:" << readDp2.value.toDouble() 
                     << ", Quality:" << static_cast<int>(readDp2.quality);
            
            qDebug() << "  UDM中的总标签数:" << udm->size();
        }
    }
    
    qDebug() << "\n========================================";
    qDebug() << "初始化完成，系统就绪";
    qDebug() << "========================================";
    
    // 显示GUI主窗口
    qDebug() << "\n[启动] 显示主窗口...";
    
    ModbusPlexLink::MainWindow window;
    window.show();
    
    qDebug() << "[成功] 主窗口已显示";
    qDebug() << "[提示] 系统正在运行，请在GUI中操作";
    
    return app.exec();
}
