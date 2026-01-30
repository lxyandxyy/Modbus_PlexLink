#ifndef SYSTEMVARIABLEMANAGERDIALOG_H
#define SYSTEMVARIABLEMANAGERDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace ModbusPlexLink {

class ChannelManager;

/**
 * @brief 系统变量管理对话框
 * 
 * 功能：
 * - 显示所有系统变量列表
 * - 按通道/采集器筛选
 * - 搜索变量
 * - 导出变量列表到CSV
 */
class SystemVariableManagerDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SystemVariableManagerDialog(ChannelManager* channelManager,
                                         QWidget* parent = nullptr);
    ~SystemVariableManagerDialog() = default;
    
private slots:
    void onRefresh();
    void onExportCsv();
    void onFilterChanged();
    void onSearchTextChanged(const QString& text);
    void onCopyFullId();
    
private:
    void setupUi();
    void refreshTable();
    void updateFilterOptions();
    
    ChannelManager* m_channelManager;
    
    // UI组件
    QTableWidget* m_variableTable;
    QComboBox* m_channelFilter;
    QComboBox* m_collectorFilter;
    QLineEdit* m_searchEdit;
    QPushButton* m_refreshBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_copyBtn;
    QLabel* m_countLabel;
};

} // namespace ModbusPlexLink

#endif // SYSTEMVARIABLEMANAGERDIALOG_H
