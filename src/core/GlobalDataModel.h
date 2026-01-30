#ifndef GLOBALDATAMODEL_H
#define GLOBALDATAMODEL_H

#include <QObject>
#include <QHash>
#include <QReadWriteLock>
#include <QStringList>
#include "DataPoint.h"

namespace ModbusPlexLink {

/**
 * @brief 全局数据模型单例类
 * 
 * 功能：
 * - 所有采集通道的数据都写入到全局模型
 * - 所有服务通道从全局模型读取数据
 * - 提供跨通道的数据共享能力
 * 
 * 标签命名规则：
 * - 完整标签名格式: "通道名:采集器名:变量名"
 * - 例如: "Channel1:Collector1:Temperature"
 */
class GlobalDataModel : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief 获取单例实例
     */
    static GlobalDataModel& instance();
    
    /**
     * @brief 写入数据点（采集通道调用）
     * @param channelName 通道名称
     * @param collectorName 采集器名称
     * @param tagName 变量名称
     * @param point 数据点
     */
    void updatePoint(const QString& channelName,
                    const QString& collectorName,
                    const QString& tagName,
                    const DataPoint& point);
    
    /**
     * @brief 批量写入数据点
     * @param channelName 通道名称
     * @param collectorName 采集器名称
     * @param points 数据点映射 (tagName -> DataPoint)
     */
    void updatePoints(const QString& channelName,
                     const QString& collectorName,
                     const QHash<QString, DataPoint>& points);
    
    /**
     * @brief 读取数据点（服务通道调用）
     * @param fullTagName 完整标签名 (格式: "通道名:采集器名:变量名")
     * @return 数据点
     */
    DataPoint readPoint(const QString& fullTagName) const;
    
    /**
     * @brief 读取数据点（通过分离的参数）
     * @param channelName 通道名称
     * @param collectorName 采集器名称
     * @param tagName 变量名称
     * @return 数据点
     */
    DataPoint readPoint(const QString& channelName,
                       const QString& collectorName,
                       const QString& tagName) const;
    
    /**
     * @brief 获取所有标签名
     * @return 标签名列表
     */
    QStringList getAllTags() const;
    
    /**
     * @brief 获取所有通道名
     * @return 通道名列表
     */
    QStringList getAllChannels() const;
    
    /**
     * @brief 按通道获取标签
     * @param channelName 通道名称
     * @return 该通道下的所有标签名
     */
    QStringList getTagsByChannel(const QString& channelName) const;
    
    /**
     * @brief 按采集器获取标签
     * @param channelName 通道名称
     * @param collectorName 采集器名称
     * @return 该采集器下的所有标签名
     */
    QStringList getTagsByCollector(const QString& channelName,
                                   const QString& collectorName) const;
    
    /**
     * @brief 获取数据点数量
     * @return 数据点总数
     */
    int size() const;
    
    /**
     * @brief 检查标签是否存在
     * @param fullTagName 完整标签名
     * @return 是否存在
     */
    bool contains(const QString& fullTagName) const;
    
    /**
     * @brief 清除通道数据
     * @param channelName 通道名称
     */
    void clearChannelData(const QString& channelName);
    
    /**
     * @brief 清除所有数据
     */
    void clear();
    
signals:
    /**
     * @brief 数据更新信号
     * @param fullTagName 完整标签名
     * @param point 数据点
     */
    void dataUpdated(const QString& fullTagName, const DataPoint& point);
    
    /**
     * @brief 标签添加信号
     * @param fullTagName 完整标签名
     */
    void tagAdded(const QString& fullTagName);
    
    /**
     * @brief 标签移除信号
     * @param fullTagName 完整标签名
     */
    void tagRemoved(const QString& fullTagName);
    
private:
    GlobalDataModel();
    ~GlobalDataModel() = default;
    
    // 禁止拷贝
    GlobalDataModel(const GlobalDataModel&) = delete;
    GlobalDataModel& operator=(const GlobalDataModel&) = delete;
    
    /**
     * @brief 生成完整标签名
     */
    QString makeFullTagName(const QString& channelName,
                           const QString& collectorName,
                           const QString& tagName) const;
    
    /**
     * @brief 解析完整标签名
     * @param fullTagName 完整标签名
     * @param channelName 输出：通道名
     * @param collectorName 输出：采集器名
     * @param tagName 输出：变量名
     * @return 是否解析成功
     */
    bool parseFullTagName(const QString& fullTagName,
                         QString& channelName,
                         QString& collectorName,
                         QString& tagName) const;
    
    QHash<QString, DataPoint> m_dataCache;  // 数据缓存
    mutable QReadWriteLock m_lock;          // 读写锁
};

} // namespace ModbusPlexLink

#endif // GLOBALDATAMODEL_H
