#include "CsvHelper.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

namespace ModbusPlexLink {

// ==================== 公共接口 ====================

bool CsvHelper::exportCollectorMappings(const QList<CollectorMappingRule>& mappings, 
                                       const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法创建CSV文件:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入BOM头（确保Excel正确识别UTF-8）
    out << "\xEF\xBB\xBF";
    
    // 写入表头
    QStringList header = getCollectorCsvHeader();
    out << header.join(",") << "\n";
    
    // 写入数据行
    for (const auto& mapping : mappings) {
        QStringList row = collectorMappingToRow(mapping);
        out << row.join(",") << "\n";
    }
    
    file.close();
    qInfo() << "成功导出" << mappings.size() << "条采集器映射规则到:" << filePath;
    return true;
}

bool CsvHelper::importCollectorMappings(const QString& filePath, 
                                       QList<CollectorMappingRule>& mappings,
                                       QString& errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("无法打开文件: %1").arg(filePath);
        return false;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    
    mappings.clear();
    int lineNumber = 0;
    bool headerSkipped = false;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        lineNumber++;
        
        // 跳过空行
        if (line.isEmpty()) {
            continue;
        }
        
        // 跳过BOM
        if (lineNumber == 1 && line.startsWith("\xEF\xBB\xBF")) {
            line = line.mid(3);
        }
        
        // 跳过表头
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        
        // 解析CSV行
        QStringList fields = parseCsvLine(line);
        
        // 验证字段数量（至少11个字段）
        if (fields.size() < 11) {
            errorMsg = QString("第%1行: 字段数量不足（需要至少11个字段，实际%2个）")
                          .arg(lineNumber).arg(fields.size());
            return false;
        }
        
        // 验证数据
        if (!validateCollectorRow(fields, lineNumber, errorMsg)) {
            return false;
        }
        
        // 解析为映射规则
        CollectorMappingRule rule = parseCollectorRow(fields);
        mappings.append(rule);
    }
    
    file.close();
    qInfo() << "成功导入" << mappings.size() << "条采集器映射规则";
    return true;
}

bool CsvHelper::exportServerMappings(const QList<ServerMappingRule>& mappings, 
                                    const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法创建CSV文件:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入BOM头（确保Excel正确识别UTF-8）
    out << "\xEF\xBB\xBF";
    
    // 写入表头
    QStringList header = getServerCsvHeader();
    out << header.join(",") << "\n";
    
    // 写入数据行
    for (const auto& mapping : mappings) {
        QStringList row = serverMappingToRow(mapping);
        out << row.join(",") << "\n";
    }
    
    file.close();
    qInfo() << "成功导出" << mappings.size() << "条服务器映射规则到:" << filePath;
    return true;
}

bool CsvHelper::importServerMappings(const QString& filePath, 
                                    QList<ServerMappingRule>& mappings,
                                    QString& errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("无法打开文件: %1").arg(filePath);
        return false;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    
    mappings.clear();
    int lineNumber = 0;
    bool headerSkipped = false;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        lineNumber++;
        
        // 跳过空行
        if (line.isEmpty()) {
            continue;
        }
        
        // 跳过BOM
        if (lineNumber == 1 && line.startsWith("\xEF\xBB\xBF")) {
            line = line.mid(3);
        }
        
        // 跳过表头
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        
        // 解析CSV行
        QStringList fields = parseCsvLine(line);
        
        // 验证字段数量（至少9个字段）
        if (fields.size() < 9) {
            errorMsg = QString("第%1行: 字段数量不足（需要至少9个字段，实际%2个）")
                          .arg(lineNumber).arg(fields.size());
            return false;
        }
        
        // 验证数据
        if (!validateServerRow(fields, lineNumber, errorMsg)) {
            return false;
        }
        
        // 解析为映射规则
        ServerMappingRule rule = parseServerRow(fields);
        mappings.append(rule);
    }
    
    file.close();
    qInfo() << "成功导入" << mappings.size() << "条服务器映射规则";
    return true;
}

bool CsvHelper::generateCollectorTemplate(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法创建模板文件:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入BOM头（确保Excel正确识别UTF-8）
    out << "\xEF\xBB\xBF";
    
    // 写入表头
    QStringList header = getCollectorCsvHeader();
    out << header.join(",") << "\n";
    
    // 写入示例数据
    out << "Meter1.Power,第一块表功率,03,20,2,Float32,ABCD,1.0,0.0,kW,是\n";
    out << "Meter1.Voltage,第一块表电压,03,22,1,UInt16,AB,0.1,0.0,V,是\n";
    out << "Meter1.Current,第一块表电流,03,23,1,UInt16,AB,0.01,0.0,A,是\n";
    out << "Meter2.Power,第二块表功率,03,121,2,Float32,ABCD,1.0,0.0,kW,是\n";
    
    file.close();
    qInfo() << "成功生成采集器映射模板:" << filePath;
    return true;
}

bool CsvHelper::generateServerTemplate(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法创建模板文件:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入BOM头（确保Excel正确识别UTF-8）
    out << "\xEF\xBB\xBF";
    
    // 写入表头
    QStringList header = getServerCsvHeader();
    out << header.join(",") << "\n";
    
    // 写入示例数据
    out << "Meter1.Power,有功功率（标准地址）,03,50,UInt16,AB,1.0,0.0,否\n";
    out << "Meter1.Voltage,电压（标准地址）,03,51,UInt16,AB,1.0,0.0,否\n";
    out << "Meter1.Current,电流（标准地址）,03,52,UInt16,AB,1.0,0.0,否\n";
    
    file.close();
    qInfo() << "成功生成服务器映射模板:" << filePath;
    return true;
}

bool CsvHelper::exportBatchVirtualDevices(const QMap<int, QList<ServerMappingRule>>& devicesMap,
                                         const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法创建CSV文件:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入BOM头（确保Excel正确识别UTF-8）
    out << "\xEF\xBB\xBF";
    
    // 写入表头
    QStringList header = getBatchServerCsvHeader();
    out << header.join(",") << "\n";
    
    // 写入数据行（按虚拟从站ID排序）
    QList<int> unitIds = devicesMap.keys();
    std::sort(unitIds.begin(), unitIds.end());
    
    int totalCount = 0;
    for (int unitId : unitIds) {
        const QList<ServerMappingRule>& mappings = devicesMap[unitId];
        for (const auto& mapping : mappings) {
            QStringList row = batchServerMappingToRow(unitId, mapping);
            out << row.join(",") << "\n";
            totalCount++;
        }
    }
    
    file.close();
    qInfo() << "成功导出" << devicesMap.size() << "个虚拟设备，共" << totalCount << "条映射规则到:" << filePath;
    return true;
}

bool CsvHelper::importBatchVirtualDevices(const QString& filePath,
                                         QMap<int, QList<ServerMappingRule>>& devicesMap,
                                         QString& errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("无法打开文件: %1").arg(filePath);
        return false;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    
    devicesMap.clear();
    int lineNumber = 0;
    bool headerSkipped = false;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        lineNumber++;
        
        // 跳过空行
        if (line.isEmpty()) {
            continue;
        }
        
        // 跳过BOM
        if (lineNumber == 1 && line.startsWith("\xEF\xBB\xBF")) {
            line = line.mid(3);
        }
        
        // 跳过表头
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        
        // 解析CSV行
        QStringList fields = parseCsvLine(line);
        
        // 验证字段数量（至少10个字段）
        if (fields.size() < 10) {
            errorMsg = QString("第%1行: 字段数量不足（需要至少10个字段，实际%2个）")
                          .arg(lineNumber).arg(fields.size());
            return false;
        }
        
        // 验证数据
        if (!validateBatchServerRow(fields, lineNumber, errorMsg)) {
            return false;
        }
        
        // 解析为映射规则
        QPair<int, ServerMappingRule> pair = parseBatchServerRow(fields);
        int unitId = pair.first;
        ServerMappingRule rule = pair.second;
        
        // 添加到对应的虚拟设备
        if (!devicesMap.contains(unitId)) {
            devicesMap[unitId] = QList<ServerMappingRule>();
        }
        devicesMap[unitId].append(rule);
    }
    
    file.close();
    int totalMappings = 0;
    for (const auto& mappings : devicesMap) {
        totalMappings += mappings.size();
    }
    qInfo() << "成功导入" << devicesMap.size() << "个虚拟设备，共" << totalMappings << "条映射规则";
    return true;
}

// ==================== 私有辅助函数 ====================

QStringList CsvHelper::parseCsvLine(const QString& line) {
    QStringList fields;
    QString field;
    bool inQuotes = false;
    
    for (int i = 0; i < line.length(); ++i) {
        QChar c = line[i];
        
        if (c == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                // 转义的引号
                field += '"';
                ++i;
            } else {
                // 切换引号状态
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            // 字段分隔符
            fields.append(field.trimmed());
            field.clear();
        } else {
            field += c;
        }
    }
    
    // 添加最后一个字段
    fields.append(field.trimmed());
    
    return fields;
}

QString CsvHelper::escapeCsvField(const QString& field) {
    // 如果字段包含逗号、引号或换行符，需要用引号包围
    if (field.contains(',') || field.contains('"') || field.contains('\n')) {
        QString escaped = field;
        escaped.replace("\"", "\"\""); // 转义引号
        return "\"" + escaped + "\"";
    }
    return field;
}

bool CsvHelper::validateCollectorRow(const QStringList& row, int lineNumber, QString& errorMsg) {
    // 验证功能码
    QString funcCode = row[2].trimmed();
    if (funcCode != "01" && funcCode != "1" && funcCode != "02" && funcCode != "2" &&
        funcCode != "03" && funcCode != "3" && funcCode != "04" && funcCode != "4") {
        // 尝试兼容旧格式
        QString regType = funcCode;
        if (regType != "Holding" && regType != "Input" && regType != "Coil" && regType != "Discrete") {
            errorMsg = QString("第%1行: 无效的功能码 '%2'（应为: 01/02/03/04）")
                          .arg(lineNumber).arg(funcCode);
            return false;
        }
    }
    
    // 验证地址
    bool ok;
    int address = row[3].trimmed().toInt(&ok);
    if (!ok || address < 0 || address > 65535) {
        errorMsg = QString("第%1行: 无效的地址 '%2'（应为: 0-65535）")
                      .arg(lineNumber).arg(row[3]);
        return false;
    }
    
    // 验证数据类型
    QString dataType = row[5].trimmed();
    QStringList validDataTypes = {"UInt16", "Int16", "UInt32", "Int32", "Float32", 
                                  "UInt64", "Int64", "Float64", "Bool", "String"};
    if (!validDataTypes.contains(dataType)) {
        errorMsg = QString("第%1行: 无效的数据类型 '%2'").arg(lineNumber).arg(dataType);
        return false;
    }
    
    // 验证字节序
    QString byteOrder = row[6].trimmed();
    QStringList validByteOrders = {"AB", "BA", "ABCD", "DCBA", "CDAB", "BADC", 
                                   "ABCDEFGH", "HGFEDCBA"};
    if (!validByteOrders.contains(byteOrder)) {
        errorMsg = QString("第%1行: 无效的字节序 '%2'").arg(lineNumber).arg(byteOrder);
        return false;
    }
    
    return true;
}

bool CsvHelper::validateServerRow(const QStringList& row, int lineNumber, QString& errorMsg) {
    // 验证功能码
    QString funcCode = row[2].trimmed();
    if (funcCode != "01" && funcCode != "1" && funcCode != "02" && funcCode != "2" &&
        funcCode != "03" && funcCode != "3" && funcCode != "04" && funcCode != "4") {
        // 尝试兼容旧格式
        QString regType = funcCode;
        if (regType != "Holding" && regType != "Input" && regType != "Coil" && regType != "Discrete") {
            errorMsg = QString("第%1行: 无效的功能码 '%2'（应为: 01/02/03/04）")
                          .arg(lineNumber).arg(funcCode);
            return false;
        }
    }
    
    // 验证地址
    bool ok;
    int address = row[3].trimmed().toInt(&ok);
    if (!ok || address < 0 || address > 65535) {
        errorMsg = QString("第%1行: 无效的地址 '%2'（应为: 0-65535）")
                      .arg(lineNumber).arg(row[3]);
        return false;
    }
    
    // 验证数据类型
    QString dataType = row[4].trimmed();
    QStringList validDataTypes = {"UInt16", "Int16", "UInt32", "Int32", "Float32", 
                                  "UInt64", "Int64", "Float64", "Bool", "String"};
    if (!validDataTypes.contains(dataType)) {
        errorMsg = QString("第%1行: 无效的数据类型 '%2'").arg(lineNumber).arg(dataType);
        return false;
    }
    
    // 验证字节序
    QString byteOrder = row[5].trimmed();
    QStringList validByteOrders = {"AB", "BA", "ABCD", "DCBA", "CDAB", "BADC", 
                                   "ABCDEFGH", "HGFEDCBA"};
    if (!validByteOrders.contains(byteOrder)) {
        errorMsg = QString("第%1行: 无效的字节序 '%2'").arg(lineNumber).arg(byteOrder);
        return false;
    }
    
    return true;
}

bool CsvHelper::validateBatchServerRow(const QStringList& row, int lineNumber, QString& errorMsg) {
    // 验证虚拟从站ID
    bool ok;
    int unitId = row[0].trimmed().toInt(&ok);
    if (!ok || unitId < 1 || unitId > 247) {
        errorMsg = QString("第%1行: 无效的虚拟从站ID '%2'（应为: 1-247）")
                      .arg(lineNumber).arg(row[0]);
        return false;
    }
    
    // 验证功能码
    QString funcCode = row[3].trimmed();
    if (funcCode != "01" && funcCode != "1" && funcCode != "02" && funcCode != "2" &&
        funcCode != "03" && funcCode != "3" && funcCode != "04" && funcCode != "4") {
        // 尝试兼容旧格式
        QString regType = funcCode;
        if (regType != "Holding" && regType != "Input" && regType != "Coil" && regType != "Discrete") {
            errorMsg = QString("第%1行: 无效的功能码 '%2'（应为: 01/02/03/04）")
                          .arg(lineNumber).arg(funcCode);
            return false;
        }
    }
    
    // 验证地址
    int address = row[4].trimmed().toInt(&ok);
    if (!ok || address < 0 || address > 65535) {
        errorMsg = QString("第%1行: 无效的地址 '%2'（应为: 0-65535）")
                      .arg(lineNumber).arg(row[4]);
        return false;
    }
    
    // 验证数据类型
    QString dataType = row[5].trimmed();
    QStringList validDataTypes = {"UInt16", "Int16", "UInt32", "Int32", "Float32", 
                                  "UInt64", "Int64", "Float64", "Bool", "String"};
    if (!validDataTypes.contains(dataType)) {
        errorMsg = QString("第%1行: 无效的数据类型 '%2'").arg(lineNumber).arg(dataType);
        return false;
    }
    
    // 验证字节序
    QString byteOrder = row[6].trimmed();
    QStringList validByteOrders = {"AB", "BA", "ABCD", "DCBA", "CDAB", "BADC", 
                                   "ABCDEFGH", "HGFEDCBA"};
    if (!validByteOrders.contains(byteOrder)) {
        errorMsg = QString("第%1行: 无效的字节序 '%2'").arg(lineNumber).arg(byteOrder);
        return false;
    }
    
    return true;
}

CollectorMappingRule CsvHelper::parseCollectorRow(const QStringList& row) {
    CollectorMappingRule rule;
    
    rule.tagName = row[0].trimmed();
    rule.comment = row[1].trimmed();
    rule.registerType = functionCodeToRegisterType(row[2].trimmed());
    rule.address = row[3].trimmed().toInt();
    rule.count = row[4].trimmed().toInt();
    rule.dataType = DataTypeUtils::dataTypeFromString(row[5].trimmed());
    rule.byteOrder = DataTypeUtils::byteOrderFromString(row[6].trimmed());
    rule.scale = row[7].trimmed().toDouble();
    rule.offset = row[8].trimmed().toDouble();
    rule.unit = row[9].trimmed();
    // 解析启用状态（支持多种格式，只有明确表示"否/false/0/禁用"时才为false）
    QString enabledStr = row[10].trimmed().toLower();
    rule.enabled = !(enabledStr == "false" || enabledStr == "0" || 
                     enabledStr == "否" || enabledStr == "禁用");
    
    // 如果count为0，自动计算
    if (rule.count == 0) {
        rule.count = DataTypeUtils::getRegisterCount(rule.dataType);
    }
    
    return rule;
}

ServerMappingRule CsvHelper::parseServerRow(const QStringList& row) {
    ServerMappingRule rule;
    
    rule.tagName = row[0].trimmed();
    rule.comment = row[1].trimmed();
    rule.registerType = functionCodeToRegisterType(row[2].trimmed());
    rule.address = row[3].trimmed().toInt();
    rule.dataType = DataTypeUtils::dataTypeFromString(row[4].trimmed());
    rule.byteOrder = DataTypeUtils::byteOrderFromString(row[5].trimmed());
    rule.scale = row[6].trimmed().toDouble();
    rule.offset = row[7].trimmed().toDouble();
    rule.writable = (row[8].trimmed().toLower() == "true" || row[8].trimmed() == "1" ||
                     row[8].trimmed() == "是" || row[8].trimmed() == "可写");
    
    return rule;
}

QPair<int, ServerMappingRule> CsvHelper::parseBatchServerRow(const QStringList& row) {
    int virtualUnitId = row[0].trimmed().toInt();
    
    ServerMappingRule rule;
    rule.tagName = row[1].trimmed();
    rule.comment = row[2].trimmed();
    rule.registerType = functionCodeToRegisterType(row[3].trimmed());
    rule.address = row[4].trimmed().toInt();
    rule.dataType = DataTypeUtils::dataTypeFromString(row[5].trimmed());
    rule.byteOrder = DataTypeUtils::byteOrderFromString(row[6].trimmed());
    rule.scale = row[7].trimmed().toDouble();
    rule.offset = row[8].trimmed().toDouble();
    rule.writable = (row[9].trimmed().toLower() == "true" || row[9].trimmed() == "1" ||
                     row[9].trimmed() == "是" || row[9].trimmed() == "可写");
    
    return qMakePair(virtualUnitId, rule);
}

QStringList CsvHelper::collectorMappingToRow(const CollectorMappingRule& rule) {
    QStringList row;
    
    row << escapeCsvField(rule.tagName);
    row << escapeCsvField(rule.comment);
    row << registerTypeToFunctionCode(rule.registerType);  // 使用功能码
    row << QString::number(rule.address);
    row << QString::number(rule.count);
    row << DataTypeUtils::dataTypeToString(rule.dataType);
    row << DataTypeUtils::byteOrderToString(rule.byteOrder);
    row << QString::number(rule.scale, 'f', 6);
    row << QString::number(rule.offset, 'f', 6);
    row << escapeCsvField(rule.unit);
    row << (rule.enabled ? "是" : "否");  // 中文
    
    return row;
}

QStringList CsvHelper::serverMappingToRow(const ServerMappingRule& rule) {
    QStringList row;
    
    row << escapeCsvField(rule.tagName);
    row << escapeCsvField(rule.comment);
    row << registerTypeToFunctionCode(rule.registerType);  // 使用功能码
    row << QString::number(rule.address);
    row << DataTypeUtils::dataTypeToString(rule.dataType);
    row << DataTypeUtils::byteOrderToString(rule.byteOrder);
    row << QString::number(rule.scale, 'f', 6);
    row << QString::number(rule.offset, 'f', 6);
    row << (rule.writable ? "是" : "否");  // 中文
    
    return row;
}

QStringList CsvHelper::batchServerMappingToRow(int virtualUnitId, const ServerMappingRule& rule) {
    QStringList row;
    
    row << QString::number(virtualUnitId);
    row << escapeCsvField(rule.tagName);
    row << escapeCsvField(rule.comment);
    row << registerTypeToFunctionCode(rule.registerType);  // 使用功能码
    row << QString::number(rule.address);
    row << DataTypeUtils::dataTypeToString(rule.dataType);
    row << DataTypeUtils::byteOrderToString(rule.byteOrder);
    row << QString::number(rule.scale, 'f', 6);
    row << QString::number(rule.offset, 'f', 6);
    row << (rule.writable ? "是" : "否");  // 中文
    
    return row;
}

// 功能码转换
QString CsvHelper::registerTypeToFunctionCode(RegisterType type) {
    switch (type) {
        case RegisterType::Coil: return "01";
        case RegisterType::DiscreteInput: return "02";
        case RegisterType::InputRegister: return "04";
        case RegisterType::HoldingRegister: return "03";
        default: return "03";
    }
}

RegisterType CsvHelper::functionCodeToRegisterType(const QString& code) {
    QString c = code.trimmed();
    if (c == "01" || c == "1") return RegisterType::Coil;
    if (c == "02" || c == "2") return RegisterType::DiscreteInput;
    if (c == "03" || c == "3") return RegisterType::HoldingRegister;
    if (c == "04" || c == "4") return RegisterType::InputRegister;
    
    // 兼容旧格式
    return DataTypeUtils::registerTypeFromString(code);
}

QStringList CsvHelper::getCollectorCsvHeader() {
    return QStringList{
        "标签名",        // TagName
        "注释",          // Comment
        "功能码",        // RegisterType (Function Code)
        "地址",          // Address
        "数量",          // Count
        "数据类型",      // DataType
        "字节序",        // ByteOrder
        "倍率",          // Scale
        "偏移",          // Offset
        "单位",          // Unit
        "启用"           // Enabled
    };
}

QStringList CsvHelper::getServerCsvHeader() {
    return QStringList{
        "标签名",        // TagName
        "注释",          // Comment
        "功能码",        // RegisterType (Function Code)
        "地址",          // Address
        "数据类型",      // DataType
        "字节序",        // ByteOrder
        "倍率",          // Scale
        "偏移",          // Offset
        "可写"           // Writable
    };
}

QStringList CsvHelper::getBatchServerCsvHeader() {
    return QStringList{
        "虚拟从站ID",    // Virtual Unit ID
        "标签名",        // TagName
        "注释",          // Comment
        "功能码",        // RegisterType (Function Code)
        "地址",          // Address
        "数据类型",      // DataType
        "字节序",        // ByteOrder
        "倍率",          // Scale
        "偏移",          // Offset
        "可写"           // Writable
    };
}

} // namespace ModbusPlexLink

