#ifndef SERVERCONFIGDIALOG_H
#define SERVERCONFIGDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include "VirtualDeviceConfigDialog.h"

namespace ModbusPlexLink {

class ServerConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit ServerConfigDialog(const QJsonObject& config = QJsonObject(), QWidget *parent = nullptr);
    ~ServerConfigDialog();

    QJsonObject getConfig() const;
    
    // 设置可用的采集器变量列表（用于虚拟设备配置）
    void setAvailableVariables(const QList<AvailableVariable>& variables);

private slots:
    void onAddVirtualDevice();
    void onEditVirtualDevice();
    void onDeleteVirtualDevice();
    void onDuplicateVirtualDevice();
    void onBatchImportVirtualDevices();  // 批量导入虚拟设备
    void onBatchExportVirtualDevices();  // 批量导出虚拟设备
    void onVirtualDeviceTableSelectionChanged();
    void onVirtualDeviceTableCellDoubleClicked(int row, int column);

    void onAccepted();
    void onRejected();

private:
    void setupUi();
    QWidget* createBasicInfoSection();
    QWidget* createVirtualDeviceTableSection();

    void loadConfig();
    bool validateConfig();

    void refreshVirtualDeviceTable();
    void updateVirtualDeviceButtons();

private:
    QJsonObject m_initialConfig;
    QList<QJsonObject> m_virtualDevices;
    QList<AvailableVariable> m_availableVariables;
    bool m_isNewServer;

    // 基本信息 UI
    QLineEdit* m_nameEdit;
    QLineEdit* m_listenIpEdit;
    QSpinBox* m_listenPortSpin;
    QCheckBox* m_enabledCheck;

    // 虚拟设备表格 UI
    QTableWidget* m_virtualDeviceTable;
    QPushButton* m_addVirtualDeviceBtn;
    QPushButton* m_editVirtualDeviceBtn;
    QPushButton* m_deleteVirtualDeviceBtn;
    QPushButton* m_duplicateVirtualDeviceBtn;
    QPushButton* m_batchImportBtn;   // 批量导入按钮
    QPushButton* m_batchExportBtn;   // 批量导出按钮
};

} // namespace ModbusPlexLink

#endif // SERVERCONFIGDIALOG_H

