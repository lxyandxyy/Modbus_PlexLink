#include "ConfigManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTextStream>
#include <QDebug>

namespace ModbusPlexLink {

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_version("1.0")
{
}

ConfigManager::~ConfigManager() {
}

bool ConfigManager::loadFromFile(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QString error = QString("无法打开配置文件: %1").arg(filename);
        emit errorOccurred(error);
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QString error = QString("配置文件解析错误: %1 (位置: %2)")
            .arg(parseError.errorString())
            .arg(parseError.offset);
        emit errorOccurred(error);
        return false;
    }
    
    if (!doc.isObject()) {
        emit errorOccurred("配置文件格式错误：根节点必须是对象");
        return false;
    }
    
    return loadFromJson(doc.object());
}

bool ConfigManager::saveToFile(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        const_cast<ConfigManager*>(this)->emit errorOccurred(
            QString("无法写入配置文件: %1").arg(filename));
        return false;
    }
    
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    const_cast<ConfigManager*>(this)->emit configSaved();
    
    qInfo() << "配置已保存到:" << filename;
    return true;
}

bool ConfigManager::loadFromJson(const QJsonObject& json) {
    QString errorMsg;
    if (!validateConfig(&errorMsg)) {
        emit validationFailed(errorMsg);
        return false;
    }
    
    m_config = json;
    m_version = json["version"].toString("1.0");
    
    emit configLoaded();
    
    qInfo() << "配置加载成功，版本:" << m_version;
    return true;
}

QJsonObject ConfigManager::toJson() const {
    QJsonObject json = m_config;
    json["version"] = m_version;
    return json;
}

QJsonObject ConfigManager::getConfig() const {
    return m_config;
}

void ConfigManager::setConfig(const QJsonObject& config) {
    m_config = config;
    m_version = config["version"].toString("1.0");
}

bool ConfigManager::validateConfig(QString* errorMsg) const {
    // TODO: 实现完整的配置验证
    // 这里先做基本验证
    
    if (!m_config.contains("channels")) {
        if (errorMsg) *errorMsg = "配置缺少'channels'字段";
        return false;
    }
    
    QJsonArray channels = m_config["channels"].toArray();
    if (channels.isEmpty()) {
        if (errorMsg) *errorMsg = "至少需要一个通道配置";
        return false;
    }
    
    // 验证每个通道
    for (const QJsonValue& value : channels) {
        if (!value.isObject()) {
            if (errorMsg) *errorMsg = "通道配置必须是对象";
            return false;
        }
        
        QJsonObject channel = value.toObject();
        
        if (!channel.contains("name")) {
            if (errorMsg) *errorMsg = "通道配置缺少'name'字段";
            return false;
        }
        
        // 验证采集器
        if (channel.contains("collectors")) {
            QJsonArray collectors = channel["collectors"].toArray();
            for (const QJsonValue& collValue : collectors) {
                if (!validateCollector(collValue.toObject(), errorMsg)) {
                    return false;
                }
            }
        }
        
        // 验证服务器
        if (channel.contains("servers")) {
            QJsonArray servers = channel["servers"].toArray();
            for (const QJsonValue& srvValue : servers) {
                if (!validateServer(srvValue.toObject(), errorMsg)) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

QString ConfigManager::getVersion() const {
    return m_version;
}

QList<CollectorMappingRule> ConfigManager::parseCollectorMappings(const QJsonArray& mappings) {
    QList<CollectorMappingRule> rules;
    
    for (const QJsonValue& value : mappings) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject mapObj = value.toObject();
        CollectorMappingRule rule;
        
        rule.tagName = mapObj["tagName"].toString();
        rule.comment = mapObj["comment"].toString();
        rule.address = mapObj["address"].toInt();
        rule.count = mapObj["count"].toInt(0);
        rule.unit = mapObj["unit"].toString();
        rule.enabled = mapObj["enabled"].toBool(true);
        
        // 解析类型
        rule.registerType = DataTypeUtils::registerTypeFromString(
            mapObj["registerType"].toString("Holding"));
        rule.dataType = DataTypeUtils::dataTypeFromString(
            mapObj["dataType"].toString("UInt16"));
        rule.byteOrder = DataTypeUtils::byteOrderFromString(
            mapObj["byteOrder"].toString("AB"));
        
        rule.scale = mapObj["scale"].toDouble(1.0);
        rule.offset = mapObj["offset"].toDouble(0.0);
        
        // 自动计算count
        if (rule.count <= 0) {
            rule.count = DataTypeUtils::getRegisterCount(rule.dataType);
        }
        
        if (!rule.tagName.isEmpty()) {
            rules.append(rule);
        }
    }
    
    return rules;
}

QList<ServerMappingRule> ConfigManager::parseServerMappings(const QJsonArray& mappings) {
    QList<ServerMappingRule> rules;
    
    for (const QJsonValue& value : mappings) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject mapObj = value.toObject();
        ServerMappingRule rule;
        
        rule.tagName = mapObj["tagName"].toString();
        rule.comment = mapObj["comment"].toString();
        rule.address = mapObj["address"].toInt();
        rule.writable = mapObj["writable"].toBool(false);
        rule.accessLevel = mapObj["accessLevel"].toString();
        
        // 解析类型
        rule.registerType = DataTypeUtils::registerTypeFromString(
            mapObj["registerType"].toString("Holding"));
        rule.dataType = DataTypeUtils::dataTypeFromString(
            mapObj["dataType"].toString("UInt16"));
        rule.byteOrder = DataTypeUtils::byteOrderFromString(
            mapObj["byteOrder"].toString("AB"));
        
        rule.scale = mapObj["scale"].toDouble(1.0);
        rule.offset = mapObj["offset"].toDouble(0.0);
        
        if (!rule.tagName.isEmpty()) {
            rules.append(rule);
        }
    }
    
    return rules;
}

QList<VirtualDevice> ConfigManager::parseVirtualDevices(const QJsonArray& devices) {
    QList<VirtualDevice> virtualDevices;
    
    for (const QJsonValue& value : devices) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject deviceObj = value.toObject();
        VirtualDevice device;
        
        device.virtualUnitId = deviceObj["virtualUnitId"].toInt();
        device.name = deviceObj["name"].toString();
        device.description = deviceObj["description"].toString();
        device.enabled = deviceObj["enabled"].toBool(true);
        
        // 解析映射
        QJsonArray mappingsArray = deviceObj["mappings"].toArray();
        device.mappings = parseServerMappings(mappingsArray);
        
        if (device.virtualUnitId > 0 && !device.mappings.isEmpty()) {
            virtualDevices.append(device);
        }
    }
    
    return virtualDevices;
}

QJsonArray ConfigManager::encodeCollectorMappings(const QList<CollectorMappingRule>& rules) {
    QJsonArray mappings;
    
    for (const CollectorMappingRule& rule : rules) {
        QJsonObject mapObj;
        
        mapObj["tagName"] = rule.tagName;
        mapObj["comment"] = rule.comment;
        mapObj["registerType"] = DataTypeUtils::registerTypeToString(rule.registerType);
        mapObj["address"] = rule.address;
        mapObj["count"] = rule.count;
        mapObj["dataType"] = DataTypeUtils::dataTypeToString(rule.dataType);
        mapObj["byteOrder"] = DataTypeUtils::byteOrderToString(rule.byteOrder);
        mapObj["scale"] = rule.scale;
        mapObj["offset"] = rule.offset;
        mapObj["unit"] = rule.unit;
        mapObj["enabled"] = rule.enabled;
        
        mappings.append(mapObj);
    }
    
    return mappings;
}

QJsonArray ConfigManager::encodeServerMappings(const QList<ServerMappingRule>& rules) {
    QJsonArray mappings;
    
    for (const ServerMappingRule& rule : rules) {
        QJsonObject mapObj;
        
        mapObj["tagName"] = rule.tagName;
        mapObj["comment"] = rule.comment;
        mapObj["registerType"] = DataTypeUtils::registerTypeToString(rule.registerType);
        mapObj["address"] = rule.address;
        mapObj["dataType"] = DataTypeUtils::dataTypeToString(rule.dataType);
        mapObj["byteOrder"] = DataTypeUtils::byteOrderToString(rule.byteOrder);
        mapObj["scale"] = rule.scale;
        mapObj["offset"] = rule.offset;
        mapObj["writable"] = rule.writable;
        
        if (!rule.accessLevel.isEmpty()) {
            mapObj["accessLevel"] = rule.accessLevel;
        }
        
        mappings.append(mapObj);
    }
    
    return mappings;
}

QJsonArray ConfigManager::encodeVirtualDevices(const QList<VirtualDevice>& devices) {
    QJsonArray devicesArray;
    
    for (const VirtualDevice& device : devices) {
        QJsonObject deviceObj;
        
        deviceObj["virtualUnitId"] = device.virtualUnitId;
        deviceObj["name"] = device.name;
        deviceObj["description"] = device.description;
        deviceObj["enabled"] = device.enabled;
        deviceObj["mappings"] = encodeServerMappings(device.mappings);
        
        devicesArray.append(deviceObj);
    }
    
    return devicesArray;
}

bool ConfigManager::importMappingsFromCSV(const QString& filename, QList<CollectorMappingRule>& rules) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法打开CSV文件: %1").arg(filename));
        return false;
    }
    
    QTextStream in(&file);
    QString header = in.readLine(); // 跳过标题行
    
    rules.clear();
    int lineNum = 1;
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        lineNum++;
        
        if (line.trimmed().isEmpty()) {
            continue;
        }
        
        QStringList fields = line.split(',');
        if (fields.size() < 8) {
            qWarning() << "CSV行" << lineNum << "字段不足，跳过";
            continue;
        }
        
        CollectorMappingRule rule;
        
        rule.tagName = fields[0].trimmed();
        rule.registerType = DataTypeUtils::registerTypeFromString(fields[1].trimmed());
        rule.address = fields[2].toInt();
        rule.dataType = DataTypeUtils::dataTypeFromString(fields[3].trimmed());
        rule.byteOrder = DataTypeUtils::byteOrderFromString(fields[4].trimmed());
        rule.scale = fields[5].toDouble();
        rule.offset = fields[6].toDouble();
        rule.unit = fields[7].trimmed();
        
        if (fields.size() > 8) {
            rule.comment = fields[8].trimmed();
        }
        
        if (fields.size() > 9) {
            rule.enabled = (fields[9].trimmed().toLower() == "true" || fields[9].trimmed() == "1");
        }
        
        if (!rule.tagName.isEmpty()) {
            rule.count = DataTypeUtils::getRegisterCount(rule.dataType);
            rules.append(rule);
        }
    }
    
    file.close();
    
    qInfo() << "从CSV导入" << rules.size() << "条采集器映射规则";
    return true;
}

bool ConfigManager::exportMappingsToCSV(const QString& filename, const QList<CollectorMappingRule>& rules) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法写入CSV文件: %1").arg(filename));
        return false;
    }
    
    QTextStream out(&file);
    
    // 写入标题行
    out << "TagName,RegisterType,Address,DataType,ByteOrder,Scale,Offset,Unit,Comment,Enabled\n";
    
    // 写入数据行
    for (const CollectorMappingRule& rule : rules) {
        out << rule.tagName << ","
            << DataTypeUtils::registerTypeToString(rule.registerType) << ","
            << rule.address << ","
            << DataTypeUtils::dataTypeToString(rule.dataType) << ","
            << DataTypeUtils::byteOrderToString(rule.byteOrder) << ","
            << rule.scale << ","
            << rule.offset << ","
            << rule.unit << ","
            << rule.comment << ","
            << (rule.enabled ? "true" : "false") << "\n";
    }
    
    file.close();
    
    qInfo() << "导出" << rules.size() << "条采集器映射规则到CSV";
    return true;
}

bool ConfigManager::validateCollector(const QJsonObject& collector, QString* errorMsg) const {
    if (!collector.contains("name")) {
        if (errorMsg) *errorMsg = "采集器配置缺少'name'字段";
        return false;
    }
    
    if (!collector.contains("protocol")) {
        if (errorMsg) *errorMsg = "采集器配置缺少'protocol'字段";
        return false;
    }
    
    if (!collector.contains("ip")) {
        if (errorMsg) *errorMsg = "采集器配置缺少'ip'字段";
        return false;
    }
    
    // 验证映射规则
    if (collector.contains("mappings")) {
        QJsonArray mappings = collector["mappings"].toArray();
        for (const QJsonValue& value : mappings) {
            if (!validateMapping(value.toObject(), true, errorMsg)) {
                return false;
            }
        }
    }
    
    return true;
}

bool ConfigManager::validateServer(const QJsonObject& server, QString* errorMsg) const {
    if (!server.contains("name")) {
        if (errorMsg) *errorMsg = "服务器配置缺少'name'字段";
        return false;
    }
    
    if (!server.contains("protocol")) {
        if (errorMsg) *errorMsg = "服务器配置缺少'protocol'字段";
        return false;
    }
    
    if (!server.contains("port")) {
        if (errorMsg) *errorMsg = "服务器配置缺少'port'字段";
        return false;
    }
    
    int port = server["port"].toInt();
    if (port <= 0 || port > 65535) {
        if (errorMsg) *errorMsg = QString("服务器端口无效: %1").arg(port);
        return false;
    }
    
    // 验证虚拟设备
    if (server.contains("virtualDevices")) {
        QJsonArray devices = server["virtualDevices"].toArray();
        for (const QJsonValue& value : devices) {
            if (!validateVirtualDevice(value.toObject(), errorMsg)) {
                return false;
            }
        }
    }
    
    return true;
}

bool ConfigManager::validateMapping(const QJsonObject& mapping, bool isCollector, QString* errorMsg) const {
    if (!mapping.contains("tagName")) {
        if (errorMsg) *errorMsg = "映射规则缺少'tagName'字段";
        return false;
    }
    
    if (!mapping.contains("address")) {
        if (errorMsg) *errorMsg = "映射规则缺少'address'字段";
        return false;
    }
    
    int address = mapping["address"].toInt();
    if (address < 0 || address > 65535) {
        if (errorMsg) *errorMsg = QString("地址超出范围: %1").arg(address);
        return false;
    }
    
    return true;
}

bool ConfigManager::validateVirtualDevice(const QJsonObject& device, QString* errorMsg) const {
    if (!device.contains("virtualUnitId")) {
        if (errorMsg) *errorMsg = "虚拟设备缺少'virtualUnitId'字段";
        return false;
    }
    
    int unitId = device["virtualUnitId"].toInt();
    if (unitId <= 0 || unitId > 247) {
        if (errorMsg) *errorMsg = QString("虚拟从站ID超出范围(1-247): %1").arg(unitId);
        return false;
    }
    
    if (!device.contains("mappings")) {
        if (errorMsg) *errorMsg = "虚拟设备缺少'mappings'字段";
        return false;
    }
    
    QJsonArray mappings = device["mappings"].toArray();
    if (mappings.isEmpty()) {
        if (errorMsg) *errorMsg = "虚拟设备的映射规则不能为空";
        return false;
    }
    
    // 验证每个映射
    for (const QJsonValue& value : mappings) {
        if (!validateMapping(value.toObject(), false, errorMsg)) {
            return false;
        }
    }
    
    return true;
}

} // namespace ModbusPlexLink

