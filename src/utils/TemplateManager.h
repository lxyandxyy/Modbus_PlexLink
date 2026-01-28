#ifndef TEMPLATEMANAGER_H
#define TEMPLATEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include "core/DataTypes.h"

namespace ModbusPlexLink {

/**
 * @brief 设备模板信息
 */
struct DeviceTemplate {
    // 基本信息
    QString id;              // 模板ID
    QString name;            // 设备名称
    QString manufacturer;    // 制造商
    QString model;           // 型号
    QString category;        // 类别: power_meter, plc, sensor, inverter, other
    QString version;         // 模板版本
    QString author;          // 作者
    QString description;     // 描述
    QString icon;            // 图标名称
    
    // 默认配置
    QString protocol;        // 协议: modbus-tcp, modbus-rtu
    int port;                // 端口
    int unitId;              // 从站地址
    int pollRate;            // 采集周期
    int timeout;             // 超时时间
    QString defaultByteOrder;// 默认字节序
    
    // 映射规则
    QList<CollectorMappingRule> mappings;
    
    // 文档
    QString manualUrl;       // 手册链接
    QString notes;           // 注意事项
    
    // 源文件路径
    QString filePath;
    
    DeviceTemplate()
        : port(502)
        , unitId(1)
        , pollRate(1000)
        , timeout(3000)
    {}
};

/**
 * @brief 设备模板管理器
 * 
 * 负责加载、管理和应用设备模板
 */
class TemplateManager : public QObject {
    Q_OBJECT
    
public:
    static TemplateManager& instance();
    
    /**
     * @brief 从目录加载所有模板
     * @param templatesDir 模板目录路径
     * @return 加载的模板数量
     */
    int loadTemplates(const QString& templatesDir);
    
    /**
     * @brief 获取所有模板
     */
    QList<DeviceTemplate> getAllTemplates() const;
    
    /**
     * @brief 按类别获取模板
     */
    QList<DeviceTemplate> getTemplatesByCategory(const QString& category) const;
    
    /**
     * @brief 获取所有类别
     */
    QStringList getCategories() const;
    
    /**
     * @brief 根据ID获取模板
     */
    DeviceTemplate getTemplate(const QString& id) const;
    
    /**
     * @brief 检查模板是否存在
     */
    bool hasTemplate(const QString& id) const;
    
    /**
     * @brief 将模板转换为采集器配置 JSON
     */
    QJsonObject templateToCollectorConfig(const DeviceTemplate& tmpl,
                                          const QString& name,
                                          const QString& ip) const;
    
    /**
     * @brief 获取类别显示名称
     */
    static QString categoryDisplayName(const QString& category);
    
    /**
     * @brief 获取类别图标
     */
    static QString categoryIcon(const QString& category);
    
signals:
    void templatesLoaded(int count);
    void templateLoadError(const QString& file, const QString& error);
    
private:
    TemplateManager(QObject* parent = nullptr);
    ~TemplateManager() = default;
    
    // 禁用拷贝
    TemplateManager(const TemplateManager&) = delete;
    TemplateManager& operator=(const TemplateManager&) = delete;
    
    // 解析单个模板文件
    bool parseTemplateFile(const QString& filePath, DeviceTemplate& tmpl);
    
    // 解析映射规则
    CollectorMappingRule parseMappingRule(const QJsonObject& json) const;
    
    // 模板存储
    QMap<QString, DeviceTemplate> m_templates;
    QStringList m_categories;
};

} // namespace ModbusPlexLink

#endif // TEMPLATEMANAGER_H
