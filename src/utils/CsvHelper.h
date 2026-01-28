#ifndef CSVHELPER_H
#define CSVHELPER_H

#include <QString>
#include <QList>
#include <QStringList>
#include "core/DataTypes.h"

namespace ModbusPlexLink {

/**
 * @brief CSV导入导出辅助类
 * 
 * 功能：
 * - 导出采集器映射规则到CSV
 * - 从CSV导入采集器映射规则
 * - 导出服务器映射规则到CSV
 * - 从CSV导入服务器映射规则
 * - 生成CSV模板文件
 */
class CsvHelper {
public:
    /**
     * @brief 导出采集器映射规则到CSV文件
     * @param mappings 映射规则列表
     * @param filePath CSV文件路径
     * @return 成功返回true，失败返回false
     */
    static bool exportCollectorMappings(const QList<CollectorMappingRule>& mappings, 
                                       const QString& filePath);
    
    /**
     * @brief 从CSV文件导入采集器映射规则
     * @param filePath CSV文件路径
     * @param mappings 输出的映射规则列表
     * @param errorMsg 错误信息（如果失败）
     * @return 成功返回true，失败返回false
     */
    static bool importCollectorMappings(const QString& filePath, 
                                       QList<CollectorMappingRule>& mappings,
                                       QString& errorMsg);
    
    /**
     * @brief 导出服务器映射规则到CSV文件
     * @param mappings 映射规则列表
     * @param filePath CSV文件路径
     * @return 成功返回true，失败返回false
     */
    static bool exportServerMappings(const QList<ServerMappingRule>& mappings, 
                                    const QString& filePath);
    
    /**
     * @brief 从CSV文件导入服务器映射规则
     * @param filePath CSV文件路径
     * @param mappings 输出的映射规则列表
     * @param errorMsg 错误信息（如果失败）
     * @return 成功返回true，失败返回false
     */
    static bool importServerMappings(const QString& filePath, 
                                    QList<ServerMappingRule>& mappings,
                                    QString& errorMsg);
    
    /**
     * @brief 批量导出虚拟设备映射规则到CSV文件（包含虚拟从站ID）
     * @param devicesMap 虚拟从站ID到映射规则列表的映射
     * @param filePath CSV文件路径
     * @return 成功返回true，失败返回false
     */
    static bool exportBatchVirtualDevices(const QMap<int, QList<ServerMappingRule>>& devicesMap,
                                         const QString& filePath);
    
    /**
     * @brief 从CSV文件批量导入虚拟设备映射规则
     * @param filePath CSV文件路径
     * @param devicesMap 输出的虚拟从站ID到映射规则列表的映射
     * @param errorMsg 错误信息（如果失败）
     * @return 成功返回true，失败返回false
     */
    static bool importBatchVirtualDevices(const QString& filePath,
                                         QMap<int, QList<ServerMappingRule>>& devicesMap,
                                         QString& errorMsg);
    
    /**
     * @brief 生成采集器映射CSV模板
     * @param filePath CSV文件路径
     * @return 成功返回true，失败返回false
     */
    static bool generateCollectorTemplate(const QString& filePath);
    
    /**
     * @brief 生成服务器映射CSV模板
     * @param filePath CSV文件路径
     * @return 成功返回true，失败返回false
     */
    static bool generateServerTemplate(const QString& filePath);

private:
    // CSV解析辅助函数
    static QStringList parseCsvLine(const QString& line);
    static QString escapeCsvField(const QString& field);
    
    // 验证和转换
    static bool validateCollectorRow(const QStringList& row, int lineNumber, QString& errorMsg);
    static bool validateServerRow(const QStringList& row, int lineNumber, QString& errorMsg);
    static bool validateBatchServerRow(const QStringList& row, int lineNumber, QString& errorMsg);
    
    static CollectorMappingRule parseCollectorRow(const QStringList& row);
    static ServerMappingRule parseServerRow(const QStringList& row);
    static QPair<int, ServerMappingRule> parseBatchServerRow(const QStringList& row);
    
    static QStringList collectorMappingToRow(const CollectorMappingRule& rule);
    static QStringList serverMappingToRow(const ServerMappingRule& rule);
    static QStringList batchServerMappingToRow(int virtualUnitId, const ServerMappingRule& rule);
    
    // 功能码转换
    static QString registerTypeToFunctionCode(RegisterType type);
    static RegisterType functionCodeToRegisterType(const QString& code);
    
    // CSV表头（中文）
    static QStringList getCollectorCsvHeader();
    static QStringList getServerCsvHeader();
    static QStringList getBatchServerCsvHeader();
};

} // namespace ModbusPlexLink

#endif // CSVHELPER_H

