#ifndef SYSTEMVARIABLEMANAGER_H
#define SYSTEMVARIABLEMANAGER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QDateTime>
#include "core/DataTypes.h"

namespace ModbusPlexLink {

class ChannelManager;

/**
 * @brief 系统变量信息结构
 */
struct SystemVariable {
    QString variableName;        // 变量名
    QString sourceChannel;       // 来源通道
    QString sourceCollector;     // 来源采集器
    QString fullId;              // 完整标识: 通道:采集器:变量名
    DataType dataType;           // 数据类型
    RegisterType registerType;   // 寄存器类型
    QString comment;             // 注释
    QString unit;                // 单位
    QDateTime updateTime;        // 更新时间
    
    SystemVariable()
        : dataType(DataType::UInt16)
        , registerType(RegisterType::HoldingRegister)
    {}
};

/**
 * @brief 系统变量管理器单例类
 * 
 * 功能：
 * - 从所有采集通道的采集器映射中自动提取变量
 * - 供服务通道做映射时选择
 * - 提供变量查询和搜索功能
 */
class SystemVariableManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief 获取单例实例
     */
    static SystemVariableManager& instance();
    
    /**
     * @brief 从通道管理器同步变量
     * @param channelManager 通道管理器
     */
    void syncFromChannels(ChannelManager* channelManager);
    
    /**
     * @brief 获取所有变量
     * @return 变量列表
     */
    QList<SystemVariable> getAllVariables() const;
    
    /**
     * @brief 获取变量数量
     * @return 变量总数
     */
    int getVariableCount() const;
    
    /**
     * @brief 按通道筛选变量
     * @param channelName 通道名称
     * @return 该通道下的变量列表
     */
    QList<SystemVariable> getVariablesByChannel(const QString& channelName) const;
    
    /**
     * @brief 按采集器筛选变量
     * @param channelName 通道名称
     * @param collectorName 采集器名称
     * @return 该采集器下的变量列表
     */
    QList<SystemVariable> getVariablesByCollector(const QString& channelName,
                                                   const QString& collectorName) const;
    
    /**
     * @brief 搜索变量
     * @param keyword 搜索关键词
     * @return 匹配的变量列表
     */
    QList<SystemVariable> searchVariables(const QString& keyword) const;
    
    /**
     * @brief 获取变量
     * @param fullId 完整标识
     * @return 变量信息（如果不存在返回空变量）
     */
    SystemVariable getVariable(const QString& fullId) const;
    
    /**
     * @brief 检查变量是否存在
     * @param fullId 完整标识
     * @return 是否存在
     */
    bool hasVariable(const QString& fullId) const;
    
    /**
     * @brief 获取所有通道名称
     * @return 通道名称列表
     */
    QStringList getAllChannelNames() const;
    
    /**
     * @brief 获取指定通道的所有采集器名称
     * @param channelName 通道名称
     * @return 采集器名称列表
     */
    QStringList getCollectorNames(const QString& channelName) const;
    
    /**
     * @brief 清除所有变量
     */
    void clear();
    
signals:
    /**
     * @brief 变量更新信号
     */
    void variablesUpdated();
    
    /**
     * @brief 变量添加信号
     * @param fullId 完整标识
     */
    void variableAdded(const QString& fullId);
    
    /**
     * @brief 变量移除信号
     * @param fullId 完整标识
     */
    void variableRemoved(const QString& fullId);
    
private:
    SystemVariableManager();
    ~SystemVariableManager() = default;
    
    // 禁止拷贝
    SystemVariableManager(const SystemVariableManager&) = delete;
    SystemVariableManager& operator=(const SystemVariableManager&) = delete;
    
    /**
     * @brief 生成完整标识
     */
    QString makeFullId(const QString& channelName,
                      const QString& collectorName,
                      const QString& variableName) const;
    
    QMap<QString, SystemVariable> m_variables;  // fullId -> SystemVariable
    mutable QMutex m_mutex;
};

} // namespace ModbusPlexLink

#endif // SYSTEMVARIABLEMANAGER_H
