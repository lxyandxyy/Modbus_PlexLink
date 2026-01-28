#ifndef COLLECTORCONFIGDIALOG_H
#define COLLECTORCONFIGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QJsonObject>
#include "core/DataTypes.h"
#include "utils/TemplateManager.h"

namespace ModbusPlexLink {

/**
 * @brief 采集器配置对话框
 * 
 * 功能：
 * - 配置采集器基本信息（名称、IP、端口、UnitID等）
 * - 配置采集器映射规则（标签名、地址、数据类型等）
 * - 支持表格内联编辑
 * - 支持CSV导入导出（在阶段6实现）
 */
class CollectorConfigDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit CollectorConfigDialog(const QJsonObject& config = QJsonObject(), 
                                   QWidget *parent = nullptr);
    ~CollectorConfigDialog();
    
    /**
     * @brief 获取采集器配置（JSON格式）
     */
    QJsonObject getConfig() const;
    
    /**
     * @brief 设置采集器配置
     */
    void setConfig(const QJsonObject& config);
    
private slots:
    // 基本信息
    void onNameChanged(const QString& text);
    void onIpChanged(const QString& text);
    
    // 映射规则操作
    void onAddMapping();
    void onDeleteMapping();
    void onDuplicateMapping();
    void onMappingTableSelectionChanged();
    void onMappingTableCellChanged(int row, int column);
    
    // CSV导入导出（阶段6实现）
    void onImportFromCSV();
    void onExportToCSV();
    
    // 模板加载
    void onLoadFromTemplate();
    void applyTemplateDefaults(const DeviceTemplate& tmpl);
    
    // 对话框按钮
    void onAccepted();
    void onRejected();
    
private:
    void setupUi();
    QWidget* createBasicInfoSection();
    QWidget* createMappingTableSection();
    
    void loadConfig();
    bool validateConfig();
    
    void refreshMappingTable();
    void updateMappingButtons();
    
    void setupMappingTableRow(int row, const CollectorMappingRule& rule);
    CollectorMappingRule getMappingRuleFromRow(int row) const;
    
    // 创建表格单元格编辑器
    QWidget* createRegisterTypeComboBox(RegisterType currentType);
    QWidget* createDataTypeComboBox(DataType currentType);
    QWidget* createByteOrderComboBox(ByteOrder currentOrder);
    
private slots:
    // 协议类型切换
    void onProtocolChanged(int index);
    
private:
    bool m_isNewCollector;
    QJsonObject m_originalConfig;
    
    // 协议选择
    QComboBox* m_protocolCombo;
    
    // 基本信息控件
    QLineEdit* m_nameEdit;
    
    // TCP专用控件
    QWidget* m_tcpWidget;
    QLineEdit* m_ipEdit;
    QSpinBox* m_portSpin;
    
    // RTU专用控件
    QWidget* m_rtuWidget;
    QComboBox* m_serialPortCombo;
    QComboBox* m_baudRateCombo;
    QComboBox* m_dataBitsCombo;
    QComboBox* m_parityCombo;
    QComboBox* m_stopBitsCombo;
    
    // 通用控件
    QSpinBox* m_unitIdSpin;
    QSpinBox* m_pollRateSpin;
    QSpinBox* m_timeoutSpin;
    QSpinBox* m_maxRetriesSpin;
    QCheckBox* m_autoReconnectCheck;
    QCheckBox* m_logErrorsCheck;
    
    // 映射规则表格
    QTableWidget* m_mappingTable;
    QPushButton* m_addMappingBtn;
    QPushButton* m_deleteMappingBtn;
    QPushButton* m_duplicateMappingBtn;
    QPushButton* m_importCsvBtn;
    QPushButton* m_exportCsvBtn;
    QPushButton* m_loadTemplateBtn;
    
    // 映射规则数据
    QList<CollectorMappingRule> m_mappings;
    
    // 防止递归触发cellChanged信号
    bool m_isUpdatingTable;
};

} // namespace ModbusPlexLink

#endif // COLLECTORCONFIGDIALOG_H

