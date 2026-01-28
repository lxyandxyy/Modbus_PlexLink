#include "TemplateManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace ModbusPlexLink {

TemplateManager& TemplateManager::instance() {
    static TemplateManager instance;
    return instance;
}

TemplateManager::TemplateManager(QObject* parent)
    : QObject(parent)
{
}

int TemplateManager::loadTemplates(const QString& templatesDir) {
    m_templates.clear();
    m_categories.clear();
    
    QDir dir(templatesDir);
    if (!dir.exists()) {
        qWarning() << "Templates directory does not exist:" << templatesDir;
        return 0;
    }
    
    int loadedCount = 0;
    
    // 遍历子目录
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subDir : subDirs) {
        QString categoryPath = dir.filePath(subDir);
        QDir categoryDir(categoryPath);
        
        // 遍历 JSON 文件
        QStringList jsonFiles = categoryDir.entryList({"*.json"}, QDir::Files);
        for (const QString& jsonFile : jsonFiles) {
            // 跳过 schema 文件
            if (jsonFile.contains("schema", Qt::CaseInsensitive)) {
                continue;
            }
            
            QString filePath = categoryDir.filePath(jsonFile);
            DeviceTemplate tmpl;
            
            if (parseTemplateFile(filePath, tmpl)) {
                m_templates[tmpl.id] = tmpl;
                
                // 收集类别
                if (!m_categories.contains(tmpl.category)) {
                    m_categories.append(tmpl.category);
                }
                
                loadedCount++;
                qDebug() << "Loaded template:" << tmpl.name << "(" << tmpl.id << ")";
            }
        }
    }
    
    // 排序类别
    m_categories.sort();
    
    emit templatesLoaded(loadedCount);
    qInfo() << "Loaded" << loadedCount << "device templates from" << templatesDir;
    
    return loadedCount;
}

bool TemplateManager::parseTemplateFile(const QString& filePath, DeviceTemplate& tmpl) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit templateLoadError(filePath, "Cannot open file");
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        emit templateLoadError(filePath, parseError.errorString());
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // 解析 templateInfo
    QJsonObject info = root["templateInfo"].toObject();
    tmpl.id = info["id"].toString();
    tmpl.name = info["name"].toString();
    tmpl.manufacturer = info["manufacturer"].toString();
    tmpl.model = info["model"].toString();
    tmpl.category = info["category"].toString();
    tmpl.version = info["version"].toString();
    tmpl.author = info["author"].toString();
    tmpl.description = info["description"].toString();
    tmpl.icon = info["icon"].toString();
    
    // 验证必填字段
    if (tmpl.id.isEmpty() || tmpl.name.isEmpty()) {
        emit templateLoadError(filePath, "Missing required fields: id or name");
        return false;
    }
    
    // 解析 defaultConfig
    QJsonObject config = root["defaultConfig"].toObject();
    tmpl.protocol = config["protocol"].toString("modbus-tcp");
    tmpl.port = config["port"].toInt(502);
    tmpl.unitId = config["unitId"].toInt(1);
    tmpl.pollRate = config["pollRate"].toInt(1000);
    tmpl.timeout = config["timeout"].toInt(3000);
    tmpl.defaultByteOrder = config["byteOrder"].toString("AB");
    
    // 解析 mappings
    QJsonArray mappingsArray = root["mappings"].toArray();
    for (const QJsonValue& val : mappingsArray) {
        CollectorMappingRule rule = parseMappingRule(val.toObject());
        tmpl.mappings.append(rule);
    }
    
    // 解析 documentation
    QJsonObject docs = root["documentation"].toObject();
    tmpl.manualUrl = docs["manualUrl"].toString();
    tmpl.notes = docs["notes"].toString();
    
    tmpl.filePath = filePath;
    
    return true;
}

CollectorMappingRule TemplateManager::parseMappingRule(const QJsonObject& json) const {
    CollectorMappingRule rule;
    
    rule.tagName = json["tagName"].toString();
    rule.comment = json["description"].toString();
    rule.unit = json["unit"].toString();
    rule.address = json["address"].toInt();
    rule.count = json["count"].toInt(1);
    rule.scale = json["scale"].toDouble(1.0);
    rule.offset = json["offset"].toDouble(0.0);
    rule.enabled = json["enabled"].toBool(true);
    
    // 解析寄存器类型
    QString regTypeStr = json["registerType"].toString();
    rule.registerType = DataTypeUtils::registerTypeFromString(regTypeStr);
    
    // 解析数据类型
    QString dataTypeStr = json["dataType"].toString();
    rule.dataType = DataTypeUtils::dataTypeFromString(dataTypeStr);
    
    // 解析字节序
    QString byteOrderStr = json["byteOrder"].toString("AB");
    rule.byteOrder = DataTypeUtils::byteOrderFromString(byteOrderStr);
    
    return rule;
}

QList<DeviceTemplate> TemplateManager::getAllTemplates() const {
    return m_templates.values();
}

QList<DeviceTemplate> TemplateManager::getTemplatesByCategory(const QString& category) const {
    QList<DeviceTemplate> result;
    for (const DeviceTemplate& tmpl : m_templates) {
        if (tmpl.category == category) {
            result.append(tmpl);
        }
    }
    return result;
}

QStringList TemplateManager::getCategories() const {
    return m_categories;
}

DeviceTemplate TemplateManager::getTemplate(const QString& id) const {
    return m_templates.value(id);
}

bool TemplateManager::hasTemplate(const QString& id) const {
    return m_templates.contains(id);
}

QJsonObject TemplateManager::templateToCollectorConfig(const DeviceTemplate& tmpl,
                                                       const QString& name,
                                                       const QString& ip) const {
    QJsonObject config;
    
    config["name"] = name;
    config["protocol"] = tmpl.protocol;
    config["ip"] = ip;
    config["port"] = tmpl.port;
    config["unitId"] = tmpl.unitId;
    config["pollRate"] = tmpl.pollRate;
    config["timeout"] = tmpl.timeout;
    config["autoReconnect"] = true;
    config["maxRetries"] = 3;
    config["logErrors"] = true;
    
    // 转换映射规则
    QJsonArray mappingsArray;
    for (const CollectorMappingRule& rule : tmpl.mappings) {
        QJsonObject mapping;
        mapping["tagName"] = rule.tagName;
        mapping["registerType"] = DataTypeUtils::registerTypeToString(rule.registerType);
        mapping["address"] = rule.address;
        mapping["count"] = rule.count;
        mapping["dataType"] = DataTypeUtils::dataTypeToString(rule.dataType);
        mapping["byteOrder"] = DataTypeUtils::byteOrderToString(rule.byteOrder);
        mapping["scale"] = rule.scale;
        mapping["offset"] = rule.offset;
        mapping["unit"] = rule.unit;
        mapping["comment"] = rule.comment;
        mapping["enabled"] = rule.enabled;
        mappingsArray.append(mapping);
    }
    config["mappings"] = mappingsArray;
    
    return config;
}

QString TemplateManager::categoryDisplayName(const QString& category) {
    static QMap<QString, QString> names = {
        {"power_meter", "电力仪表"},
        {"plc", "PLC"},
        {"sensor", "传感器"},
        {"inverter", "变频器"},
        {"controller", "控制器"},
        {"other", "其他设备"}
    };
    return names.value(category, category);
}

QString TemplateManager::categoryIcon(const QString& category) {
    static QMap<QString, QString> icons = {
        {"power_meter", "⚡"},
        {"plc", "🔲"},
        {"sensor", "📡"},
        {"inverter", "🔄"},
        {"controller", "🎛️"},
        {"other", "📦"}
    };
    return icons.value(category, "📦");
}

} // namespace ModbusPlexLink
