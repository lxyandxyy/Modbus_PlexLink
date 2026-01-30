#include "ModeStatusBanner.h"
#include <QFont>

namespace ModbusPlexLink {

ModeStatusBanner::ModeStatusBanner(QWidget* parent)
    : QFrame(parent)
    , m_iconLabel(new QLabel(this))
    , m_titleLabel(new QLabel(this))
    , m_detailLabel(new QLabel(this))
{
    // 设置布局
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);
    
    // 图标
    m_iconLabel->setFixedSize(24, 24);
    QFont iconFont = m_iconLabel->font();
    iconFont.setPointSize(14);
    m_iconLabel->setFont(iconFont);
    layout->addWidget(m_iconLabel);
    
    // 标题
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);
    
    // 详情
    m_detailLabel->setStyleSheet("color: inherit; opacity: 0.8;");
    layout->addWidget(m_detailLabel);
    
    layout->addStretch();
    
    // 默认设置为本地模式
    setLocalMode();
}

void ModeStatusBanner::setLocalMode() {
    updateStyle("#DCFCE7", "#86EFAC", "#166534", "🏠");
    m_titleLabel->setText(tr("本地模式"));
    m_detailLabel->setText(tr("数据来自本地采集"));
}

void ModeStatusBanner::setLocalWithApiMode(quint16 httpPort, quint16 wsPort) {
    updateStyle("#DBEAFE", "#93C5FD", "#1E40AF", "🌐");
    m_titleLabel->setText(tr("本地模式 + API服务"));
    m_detailLabel->setText(tr("HTTP:%1 | WS:%2 - 可被远程访问").arg(httpPort).arg(wsPort));
}

void ModeStatusBanner::setRemoteMode(const QString& remoteHost, bool connected) {
    if (connected) {
        updateStyle("#FEF3C7", "#FCD34D", "#92400E", "📡");
        m_titleLabel->setText(tr("远程模式"));
        m_detailLabel->setText(tr("已连接到 %1 - 数据来自远程网关").arg(remoteHost));
    } else {
        updateStyle("#FEE2E2", "#FCA5A5", "#991B1B", "⚠️");
        m_titleLabel->setText(tr("远程模式"));
        m_detailLabel->setText(tr("连接断开 - %1").arg(remoteHost));
    }
}

void ModeStatusBanner::setDisconnected() {
    updateStyle("#FEE2E2", "#FCA5A5", "#991B1B", "❌");
    m_titleLabel->setText(tr("连接断开"));
    m_detailLabel->setText(tr("无法连接到远程服务器"));
}

void ModeStatusBanner::hideBanner() {
    setVisible(false);
}

void ModeStatusBanner::showBanner() {
    setVisible(true);
}

void ModeStatusBanner::updateStyle(const QString& bgColor, const QString& borderColor,
                                    const QString& textColor, const QString& icon) {
    setStyleSheet(QString(
        "ModeStatusBanner {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "}"
        "QLabel {"
        "  color: %3;"
        "  background: transparent;"
        "}"
    ).arg(bgColor).arg(borderColor).arg(textColor));
    
    m_iconLabel->setText(icon);
}

} // namespace ModbusPlexLink
