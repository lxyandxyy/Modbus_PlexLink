#ifndef MODESTATUSBANNER_H
#define MODESTATUSBANNER_H

#include <QFrame>
#include <QLabel>
#include <QHBoxLayout>

namespace ModbusPlexLink {

/**
 * @brief 模式状态Banner组件
 * 
 * 在告警管理、录波等界面顶部显示当前系统运行模式
 * - 本地模式：绿色
 * - 本地+API模式：蓝色
 * - 远程模式（已连接）：橙色
 * - 远程模式（断开）：红色
 */
class ModeStatusBanner : public QFrame {
    Q_OBJECT
    
public:
    explicit ModeStatusBanner(QWidget* parent = nullptr);
    ~ModeStatusBanner() = default;
    
    /**
     * @brief 设置为纯本地模式
     */
    void setLocalMode();
    
    /**
     * @brief 设置为本地+API模式
     * @param httpPort HTTP端口
     * @param wsPort WebSocket端口
     */
    void setLocalWithApiMode(quint16 httpPort, quint16 wsPort);
    
    /**
     * @brief 设置为远程模式
     * @param remoteHost 远程主机地址
     * @param connected 是否已连接
     */
    void setRemoteMode(const QString& remoteHost, bool connected);
    
    /**
     * @brief 设置为断开状态
     */
    void setDisconnected();
    
    /**
     * @brief 隐藏Banner
     */
    void hideBanner();
    
    /**
     * @brief 显示Banner
     */
    void showBanner();
    
private:
    void updateStyle(const QString& bgColor, const QString& borderColor, 
                     const QString& textColor, const QString& icon);
    
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_detailLabel;
};

} // namespace ModbusPlexLink

#endif // MODESTATUSBANNER_H
