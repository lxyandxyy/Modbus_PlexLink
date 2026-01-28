#ifndef VIRTUALDEVICECONFIGDIALOG_H
#define VIRTUALDEVICECONFIGDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QCompleter>
#include <QStringListModel>
#include "core/DataTypes.h" // For ServerMappingRule

namespace ModbusPlexLink {

// 可用变量信息
struct AvailableVariable {
    QString collectorName;     // 采集器名称
    QString tagName;           // 标签名
    QString fullId;            // 完整标识（采集器:标签名）
    DataType dataType;         // 数据类型
    QString comment;           // 注释
};

class VirtualDeviceConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit VirtualDeviceConfigDialog(const QJsonObject& config = QJsonObject(), 
                                       QWidget *parent = nullptr);
    ~VirtualDeviceConfigDialog();

    QJsonObject getConfig() const;
    
    // 设置可用的采集器变量列表
    void setAvailableVariables(const QList<AvailableVariable>& variables);

private slots:
    void onAddMapping();
    void onAddFromVariable();   // 从变量列表添加
    void onDeleteMapping();
    void onDuplicateMapping();
    void onImportCsv();
    void onExportCsv();
    void onMappingTableSelectionChanged();
    void onMappingTableCellChanged(int row, int column);
    void onSourceTagChanged(int row);  // 源变量改变时更新

    void onAccepted();
    void onRejected();

private:
    void setupUi();
    QWidget* createBasicInfoSection();
    QWidget* createMappingTableSection();

    void loadConfig();
    bool validateConfig();

    void refreshMappingTable();
    void setupMappingTableRow(int row, const ServerMappingRule& rule);
    ServerMappingRule getMappingRuleFromRow(int row) const;
    void updateMappingButtons();

private:
    QJsonObject m_initialConfig;
    QList<ServerMappingRule> m_mappings;
    bool m_isNewDevice;
    bool m_isUpdatingTable;
    
    // 可用变量
    QList<AvailableVariable> m_availableVariables;
    QStringList m_variableFullIds;
    QCompleter* m_variableCompleter;

    // 基本信息 UI
    QLineEdit* m_nameEdit;
    QSpinBox* m_virtualUnitIdSpin;
    QCheckBox* m_enabledCheck;

    // 映射表格 UI
    QTableWidget* m_mappingTable;
    QPushButton* m_addMappingBtn;
    QPushButton* m_addFromVariableBtn;  // 从变量添加
    QPushButton* m_deleteMappingBtn;
    QPushButton* m_duplicateMappingBtn;
    QPushButton* m_importCsvBtn;
    QPushButton* m_exportCsvBtn;
};

} // namespace ModbusPlexLink

#endif // VIRTUALDEVICECONFIGDIALOG_H

