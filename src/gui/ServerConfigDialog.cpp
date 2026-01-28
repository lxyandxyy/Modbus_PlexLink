#include "ServerConfigDialog.h"
#include "VirtualDeviceConfigDialog.h"
#include "DialogStyles.h"
#include "utils/CsvHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QFileDialog>
#include <QMap>
#include <QFrame>

namespace ModbusPlexLink {

ServerConfigDialog::ServerConfigDialog(const QJsonObject& config, QWidget *parent)
    : QDialog(parent)
    , m_initialConfig(config)
    , m_isNewServer(config.isEmpty())
{
    setWindowTitle(m_isNewServer ? tr("📤 新建服务器") : tr("📤 编辑服务器"));
    resize(800, 680);
    setMinimumSize(720, 580);
    
    // 应用现代化样式
    setStyleSheet(DialogStyles::getDialogStyle());
    
    setupUi();
    loadConfig();
    
    // 禁用所有 SpinBox 和 ComboBox 的滚轮事件，防止意外修改
    DialogStyles::disableAllWheelEvents(this);
}

ServerConfigDialog::~ServerConfigDialog() {
}

void ServerConfigDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // 顶部标题区域
    QWidget* headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: transparent;");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 8);
    
    QLabel* iconLabel = new QLabel("📤", headerWidget);
    iconLabel->setStyleSheet("font-size: 26px;");
    headerLayout->addWidget(iconLabel);
    
    QVBoxLayout* titleVLayout = new QVBoxLayout();
    titleVLayout->setSpacing(2);
    QLabel* titleLabel = new QLabel(m_isNewServer ? tr("创建Modbus服务器") : tr("编辑服务器配置"), headerWidget);
    titleLabel->setStyleSheet("font-size: 17px; font-weight: bold; color: #1E293B;");
    titleVLayout->addWidget(titleLabel);
    
    QLabel* subtitleLabel = new QLabel(tr("配置监听端口和虚拟设备映射"), headerWidget);
    subtitleLabel->setStyleSheet("color: #64748B; font-size: 10px;");
    titleVLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleVLayout);
    
    headerLayout->addStretch();
    mainLayout->addWidget(headerWidget);
    
    // 基本信息部分
    QWidget* basicInfoWidget = createBasicInfoSection();
    mainLayout->addWidget(basicInfoWidget);
    
    // 虚拟设备表格部分
    QWidget* virtualDeviceWidget = createVirtualDeviceTableSection();
    mainLayout->addWidget(virtualDeviceWidget, 1);
    
    // 分隔线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #E2E8F0;");
    line->setFixedHeight(1);
    mainLayout->addWidget(line);
    
    // 底部按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    QPushButton* cancelBtn = DialogStyles::createSecondaryButton(tr("取消"), this);
    QPushButton* okBtn = DialogStyles::createPrimaryButton(m_isNewServer ? tr("创建服务器") : tr("保存更改"), this);
    
    connect(cancelBtn, &QPushButton::clicked, this, &ServerConfigDialog::onRejected);
    connect(okBtn, &QPushButton::clicked, this, &ServerConfigDialog::onAccepted);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(okBtn);
    
    mainLayout->addLayout(buttonLayout);
}

QWidget* ServerConfigDialog::createBasicInfoSection() {
    QWidget* container = new QWidget(this);
    container->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(14);
    
    // 标题栏
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* sectionIcon = new QLabel("⚙️", container);
    sectionIcon->setStyleSheet("font-size: 16px; border: none; background: transparent;");
    headerLayout->addWidget(sectionIcon);
    
    QLabel* sectionTitle = new QLabel(tr("服务器配置"), container);
    sectionTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #1E293B; border: none; background: transparent;");
    headerLayout->addWidget(sectionTitle);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);
    
    // 分隔线
    QFrame* sep = new QFrame(container);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background-color: #E2E8F0; border: none;");
    sep->setFixedHeight(1);
    mainLayout->addWidget(sep);
    
    // 表单区域 - 水平布局
    QHBoxLayout* formContainer = new QHBoxLayout();
    formContainer->setSpacing(30);
    
    // 左侧表单
    QFormLayout* leftForm = new QFormLayout();
    leftForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    leftForm->setHorizontalSpacing(12);
    leftForm->setVerticalSpacing(12);
    
    m_nameEdit = new QLineEdit(container);
    m_nameEdit->setPlaceholderText(tr("例如：电表服务器_01"));
    m_nameEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 8px 12px; background: #F8FAFC;");
    QLabel* nameLabel = new QLabel(tr("服务器名称 *"), container);
    nameLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    leftForm->addRow(nameLabel, m_nameEdit);
    
    m_enabledCheck = new QCheckBox(tr("启用此服务器"), container);
    m_enabledCheck->setChecked(true);
    m_enabledCheck->setStyleSheet("border: none; background: transparent; color: #475569;");
    QLabel* enableLabel = new QLabel(tr("状态"), container);
    enableLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    leftForm->addRow(enableLabel, m_enabledCheck);
    
    formContainer->addLayout(leftForm);
    
    // 右侧表单
    QFormLayout* rightForm = new QFormLayout();
    rightForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rightForm->setHorizontalSpacing(12);
    rightForm->setVerticalSpacing(12);
    
    m_listenIpEdit = new QLineEdit(container);
    m_listenIpEdit->setPlaceholderText(tr("0.0.0.0 表示监听所有网卡"));
    m_listenIpEdit->setText("0.0.0.0");
    m_listenIpEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 8px 12px; background: #F8FAFC;");
    QLabel* ipLabel = new QLabel(tr("监听地址"), container);
    ipLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rightForm->addRow(ipLabel, m_listenIpEdit);
    
    m_listenPortSpin = new QSpinBox(container);
    m_listenPortSpin->setRange(1, 65535);
    m_listenPortSpin->setValue(502);
    m_listenPortSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    QLabel* portLabel = new QLabel(tr("监听端口 *"), container);
    portLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rightForm->addRow(portLabel, m_listenPortSpin);
    
    formContainer->addLayout(rightForm);
    formContainer->addStretch();
    
    mainLayout->addLayout(formContainer);
    
    return container;
}

QWidget* ServerConfigDialog::createVirtualDeviceTableSection() {
    QWidget* container = new QWidget(this);
    container->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);
    
    // 标题栏
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* sectionIcon = new QLabel("🖥️", container);
    sectionIcon->setStyleSheet("font-size: 16px; border: none; background: transparent;");
    headerLayout->addWidget(sectionIcon);
    
    QLabel* titleLabel = new QLabel(tr("虚拟设备（从站）"), container);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #1E293B; border: none; background: transparent;");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    QLabel* countLabel = new QLabel(tr("0 个设备"), container);
    countLabel->setObjectName("deviceCountLabel");
    countLabel->setStyleSheet(R"(
        color: #059669;
        font-weight: 600;
        font-size: 10pt;
        padding: 4px 12px;
        background-color: #ECFDF5;
        border-radius: 12px;
        border: none;
    )");
    headerLayout->addWidget(countLabel);
    
    layout->addLayout(headerLayout);
    
    // 说明
    QLabel* hintLabel = new QLabel(
        tr("每个虚拟设备对应一个Modbus从站地址，可配置多个数据点映射规则。双击可编辑。"),
        container);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #64748B; font-size: 9pt; border: none; background: transparent; padding-bottom: 4px;");
    layout->addWidget(hintLabel);
    
    // 操作按钮栏
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    
    m_addVirtualDeviceBtn = DialogStyles::createPrimaryButton(tr("＋ 添加设备"), container);
    m_editVirtualDeviceBtn = DialogStyles::createSecondaryButton(tr("✏️ 编辑"), container);
    m_deleteVirtualDeviceBtn = DialogStyles::createDangerButton(tr("删除"), container);
    m_duplicateVirtualDeviceBtn = DialogStyles::createSecondaryButton(tr("复制"), container);
    m_batchImportBtn = DialogStyles::createSecondaryButton(tr("📄 批量导入"), container);
    m_batchExportBtn = DialogStyles::createSecondaryButton(tr("💾 批量导出"), container);
    
    m_editVirtualDeviceBtn->setEnabled(false);
    m_deleteVirtualDeviceBtn->setEnabled(false);
    m_duplicateVirtualDeviceBtn->setEnabled(false);
    m_batchExportBtn->setEnabled(false);
    
    connect(m_addVirtualDeviceBtn, &QPushButton::clicked,
            this, &ServerConfigDialog::onAddVirtualDevice);
    connect(m_editVirtualDeviceBtn, &QPushButton::clicked,
            this, &ServerConfigDialog::onEditVirtualDevice);
    connect(m_deleteVirtualDeviceBtn, &QPushButton::clicked,
            this, &ServerConfigDialog::onDeleteVirtualDevice);
    connect(m_duplicateVirtualDeviceBtn, &QPushButton::clicked,
            this, &ServerConfigDialog::onDuplicateVirtualDevice);
    connect(m_batchImportBtn, &QPushButton::clicked,
            this, &ServerConfigDialog::onBatchImportVirtualDevices);
    connect(m_batchExportBtn, &QPushButton::clicked,
            this, &ServerConfigDialog::onBatchExportVirtualDevices);
    
    buttonLayout->addWidget(m_addVirtualDeviceBtn);
    buttonLayout->addWidget(m_editVirtualDeviceBtn);
    buttonLayout->addWidget(m_deleteVirtualDeviceBtn);
    buttonLayout->addWidget(m_duplicateVirtualDeviceBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_batchImportBtn);
    buttonLayout->addWidget(m_batchExportBtn);
    
    layout->addLayout(buttonLayout);
    
    // 虚拟设备表格
    m_virtualDeviceTable = new QTableWidget(container);
    m_virtualDeviceTable->setColumnCount(4);
    m_virtualDeviceTable->setHorizontalHeaderLabels({
        tr("从站ID"), tr("设备名称"), tr("映射数量"), tr("启用")
    });
    
    // 应用现代表格样式
    DialogStyles::setupModernTable(m_virtualDeviceTable);
    m_virtualDeviceTable->horizontalHeader()->setStretchLastSection(true);
    m_virtualDeviceTable->setColumnWidth(0, 80);
    m_virtualDeviceTable->setColumnWidth(1, 220);
    m_virtualDeviceTable->setColumnWidth(2, 100);
    
    connect(m_virtualDeviceTable, &QTableWidget::itemSelectionChanged,
            this, &ServerConfigDialog::onVirtualDeviceTableSelectionChanged);
    connect(m_virtualDeviceTable, &QTableWidget::cellDoubleClicked,
            this, &ServerConfigDialog::onVirtualDeviceTableCellDoubleClicked);
    
    layout->addWidget(m_virtualDeviceTable, 1);
    
    return container;
}

void ServerConfigDialog::loadConfig() {
    if (m_initialConfig.isEmpty()) {
        return;
    }
    
    // 加载基本信息
    m_nameEdit->setText(m_initialConfig.value("name").toString());
    
    // 兼容旧字段名 (listenIp) 和新字段名 (listenAddress)
    QString listenAddress = m_initialConfig.value("listenAddress").toString();
    if (listenAddress.isEmpty()) {
        listenAddress = m_initialConfig.value("listenIp").toString("0.0.0.0");
    }
    m_listenIpEdit->setText(listenAddress);
    
    // 兼容旧字段名 (listenPort) 和新字段名 (port)
    int port = m_initialConfig.value("port").toInt(0);
    if (port == 0) {
        port = m_initialConfig.value("listenPort").toInt(502);
    }
    m_listenPortSpin->setValue(port);
    
    m_enabledCheck->setChecked(m_initialConfig.value("enabled").toBool(true));
    
    // 加载虚拟设备列表
    QJsonArray virtualDevicesArray = m_initialConfig.value("virtualDevices").toArray();
    for (const QJsonValue& value : virtualDevicesArray) {
        if (value.isObject()) {
            m_virtualDevices.append(value.toObject());
        }
    }
    
    refreshVirtualDeviceTable();
}

bool ServerConfigDialog::validateConfig() {
    // 验证服务器名称
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请输入服务器名称"));
        m_nameEdit->setFocus();
        return false;
    }
    
    // 验证监听IP
    if (m_listenIpEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请输入监听IP地址"));
        m_listenIpEdit->setFocus();
        return false;
    }
    
    // 验证虚拟设备列表
    if (m_virtualDevices.isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), 
            tr("至少需要添加一个虚拟设备\n\n提示：点击\"添加设备\"按钮创建虚拟设备"));
        return false;
    }
    
    // 检查虚拟从站ID是否重复
    QSet<int> unitIds;
    for (const QJsonObject& vd : m_virtualDevices) {
        int unitId = vd.value("virtualUnitId").toInt();
        if (unitIds.contains(unitId)) {
            QMessageBox::warning(this, tr("验证失败"), 
                tr("虚拟从站ID %1 重复，请确保每个虚拟设备的从站ID唯一").arg(unitId));
            return false;
        }
        unitIds.insert(unitId);
    }
    
    return true;
}

QJsonObject ServerConfigDialog::getConfig() const {
    QJsonObject config;
    config["protocol"] = "modbus-tcp";  // 添加协议类型
    config["name"] = m_nameEdit->text().trimmed();
    config["listenAddress"] = m_listenIpEdit->text().trimmed();  // 使用标准字段名
    config["port"] = m_listenPortSpin->value();                   // 使用标准字段名
    config["enabled"] = m_enabledCheck->isChecked();
    
    // ModbusTcpServer 需要的其他字段（使用默认值）
    config["timeout"] = 1000;
    config["maxClients"] = 10;
    config["logRequests"] = false;
    config["logErrors"] = true;
    
    QJsonArray virtualDevicesArray;
    for (const QJsonObject& vd : m_virtualDevices) {
        virtualDevicesArray.append(vd);
    }
    config["virtualDevices"] = virtualDevicesArray;
    
    return config;
}

void ServerConfigDialog::refreshVirtualDeviceTable() {
    m_virtualDeviceTable->setRowCount(m_virtualDevices.size());
    
    for (int i = 0; i < m_virtualDevices.size(); ++i) {
        const QJsonObject& vd = m_virtualDevices[i];
        
        // 虚拟从站ID
        QTableWidgetItem* unitIdItem = new QTableWidgetItem(
            QString::number(vd.value("virtualUnitId").toInt()));
        unitIdItem->setTextAlignment(Qt::AlignCenter);
        m_virtualDeviceTable->setItem(i, 0, unitIdItem);
        
        // 设备名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(
            vd.value("name").toString());
        m_virtualDeviceTable->setItem(i, 1, nameItem);
        
        // 映射数量
        int mappingCount = vd.value("mappings").toArray().size();
        QTableWidgetItem* mappingItem = new QTableWidgetItem(
            QString::number(mappingCount));
        mappingItem->setTextAlignment(Qt::AlignCenter);
        m_virtualDeviceTable->setItem(i, 2, mappingItem);
        
        // 启用状态
        QTableWidgetItem* enabledItem = new QTableWidgetItem(
            vd.value("enabled").toBool(true) ? tr("✓ 是") : tr("✗ 否"));
        enabledItem->setTextAlignment(Qt::AlignCenter);
        enabledItem->setForeground(vd.value("enabled").toBool(true) ? 
            QBrush(QColor("#059669")) : QBrush(QColor("#DC2626")));
        m_virtualDeviceTable->setItem(i, 3, enabledItem);
    }
    
    // 更新设备计数标签
    QLabel* countLabel = findChild<QLabel*>("deviceCountLabel");
    if (countLabel) {
        countLabel->setText(tr("%1 个设备").arg(m_virtualDevices.size()));
    }
    
    updateVirtualDeviceButtons();
}

void ServerConfigDialog::updateVirtualDeviceButtons() {
    bool hasSelection = m_virtualDeviceTable->currentRow() >= 0;
    bool hasDevices = !m_virtualDevices.isEmpty();
    
    m_editVirtualDeviceBtn->setEnabled(hasSelection);
    m_deleteVirtualDeviceBtn->setEnabled(hasSelection);
    m_duplicateVirtualDeviceBtn->setEnabled(hasSelection);
    m_batchExportBtn->setEnabled(hasDevices);  // 有设备时启用批量导出
}

void ServerConfigDialog::setAvailableVariables(const QList<AvailableVariable>& variables) {
    m_availableVariables = variables;
}

void ServerConfigDialog::onAddVirtualDevice() {
    VirtualDeviceConfigDialog dialog(QJsonObject(), this);
    dialog.setWindowTitle(tr("新建虚拟设备"));
    dialog.setAvailableVariables(m_availableVariables);

    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject config = dialog.getConfig();
        m_virtualDevices.append(config);
        refreshVirtualDeviceTable();
        m_virtualDeviceTable->selectRow(m_virtualDevices.size() - 1);
    }
}

void ServerConfigDialog::onEditVirtualDevice() {
    int row = m_virtualDeviceTable->currentRow();
    if (row < 0 || row >= m_virtualDevices.size()) {
        return;
    }

    VirtualDeviceConfigDialog dialog(m_virtualDevices[row], this);
    dialog.setWindowTitle(tr("编辑虚拟设备 - %1")
        .arg(m_virtualDevices[row].value("name").toString()));
    dialog.setAvailableVariables(m_availableVariables);

    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject config = dialog.getConfig();
        m_virtualDevices[row] = config;
        refreshVirtualDeviceTable();
    }
}

void ServerConfigDialog::onDeleteVirtualDevice() {
    int row = m_virtualDeviceTable->currentRow();
    if (row < 0 || row >= m_virtualDevices.size()) {
        return;
    }
    
    QString deviceName = m_virtualDevices[row].value("name").toString();
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("确认删除"), 
        tr("确定要删除虚拟设备 '%1' 吗？").arg(deviceName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_virtualDevices.removeAt(row);
        refreshVirtualDeviceTable();
    }
}

void ServerConfigDialog::onDuplicateVirtualDevice() {
    int row = m_virtualDeviceTable->currentRow();
    if (row < 0 || row >= m_virtualDevices.size()) {
        return;
    }
    
    // 复制选中的虚拟设备
    QJsonObject vd = m_virtualDevices[row];
    
    // 智能递增虚拟从站ID
    int maxUnitId = 0;
    for (const QJsonObject& device : m_virtualDevices) {
        maxUnitId = qMax(maxUnitId, device.value("virtualUnitId").toInt());
    }
    vd["virtualUnitId"] = maxUnitId + 1;
    
    // 修改名称
    QString originalName = vd.value("name").toString();
    vd["name"] = originalName + tr("_副本");
    
    m_virtualDevices.append(vd);
    refreshVirtualDeviceTable();
    m_virtualDeviceTable->selectRow(m_virtualDevices.size() - 1);
}

void ServerConfigDialog::onVirtualDeviceTableSelectionChanged() {
    updateVirtualDeviceButtons();
}

void ServerConfigDialog::onVirtualDeviceTableCellDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row >= 0 && row < m_virtualDevices.size()) {
        onEditVirtualDevice();
    }
}

void ServerConfigDialog::onAccepted() {
    if (validateConfig()) {
        accept();
    }
}

void ServerConfigDialog::onRejected() {
    reject();
}

void ServerConfigDialog::onBatchImportVirtualDevices() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("批量导入虚拟设备"),
        "",
        tr("CSV文件 (*.csv)")
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // 导入CSV
    QMap<int, QList<ServerMappingRule>> devicesMap;
    QString errorMsg;
    
    if (!CsvHelper::importBatchVirtualDevices(fileName, devicesMap, errorMsg)) {
        QMessageBox::critical(this, tr("导入失败"), 
            tr("无法导入CSV文件：\n%1").arg(errorMsg));
        return;
    }
    
    // 询问是否清空现有设备
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("导入模式"),
        tr("成功解析 %1 个虚拟设备，共 %2 条映射规则。\n\n"
           "是否清空现有虚拟设备？\n\n"
           "点击【是】：清空现有设备，只保留导入的\n"
           "点击【否】：追加导入的设备到现有列表")
            .arg(devicesMap.size())
            .arg([&devicesMap](){
                int total = 0;
                for (const auto& mappings : devicesMap) {
                    total += mappings.size();
                }
                return total;
            }()),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
    );
    
    if (reply == QMessageBox::Cancel) {
        return;
    }
    
    if (reply == QMessageBox::Yes) {
        m_virtualDevices.clear();
    }
    
    // 创建虚拟设备
    QList<int> unitIds = devicesMap.keys();
    std::sort(unitIds.begin(), unitIds.end());
    
    for (int unitId : unitIds) {
        QJsonObject vd;
        vd["virtualUnitId"] = unitId;
        vd["name"] = QString("虚拟设备%1").arg(unitId);
        vd["enabled"] = true;
        
        // 转换映射规则
        QJsonArray mappingsArray;
        for (const ServerMappingRule& rule : devicesMap[unitId]) {
            QJsonObject mapping;
            mapping["tagName"] = rule.tagName;
            mapping["comment"] = rule.comment;
            mapping["registerType"] = DataTypeUtils::registerTypeToString(rule.registerType);
            mapping["address"] = rule.address;
            mapping["dataType"] = DataTypeUtils::dataTypeToString(rule.dataType);
            mapping["byteOrder"] = DataTypeUtils::byteOrderToString(rule.byteOrder);
            mapping["scale"] = rule.scale;
            mapping["offset"] = rule.offset;
            mapping["writable"] = rule.writable;
            
            mappingsArray.append(mapping);
        }
        
        vd["mappings"] = mappingsArray;
        m_virtualDevices.append(vd);
    }
    
    // 刷新表格
    refreshVirtualDeviceTable();
    
    QMessageBox::information(this, tr("导入成功"), 
        tr("成功导入 %1 个虚拟设备！").arg(devicesMap.size()));
}

void ServerConfigDialog::onBatchExportVirtualDevices() {
    if (m_virtualDevices.isEmpty()) {
        QMessageBox::information(this, tr("提示"), 
            tr("当前没有虚拟设备可导出。"));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("批量导出虚拟设备"),
        QString("batch_virtual_devices_%1.csv").arg(m_nameEdit->text()),
        tr("CSV文件 (*.csv)")
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // 收集所有虚拟设备的映射
    QMap<int, QList<ServerMappingRule>> devicesMap;
    
    for (const QJsonObject& vd : m_virtualDevices) {
        int unitId = vd.value("virtualUnitId").toInt();
        QJsonArray mappingsArray = vd.value("mappings").toArray();
        
        QList<ServerMappingRule> mappings;
        for (const QJsonValue& val : mappingsArray) {
            QJsonObject mapping = val.toObject();
            
            ServerMappingRule rule;
            rule.tagName = mapping.value("tagName").toString();
            rule.comment = mapping.value("comment").toString();
            rule.registerType = DataTypeUtils::registerTypeFromString(
                mapping.value("registerType").toString());
            rule.address = mapping.value("address").toInt();
            rule.dataType = DataTypeUtils::dataTypeFromString(
                mapping.value("dataType").toString());
            rule.byteOrder = DataTypeUtils::byteOrderFromString(
                mapping.value("byteOrder").toString());
            rule.scale = mapping.value("scale").toDouble(1.0);
            rule.offset = mapping.value("offset").toDouble(0.0);
            rule.writable = mapping.value("writable").toBool(false);
            
            mappings.append(rule);
        }
        
        devicesMap[unitId] = mappings;
    }
    
    // 导出
    if (CsvHelper::exportBatchVirtualDevices(devicesMap, fileName)) {
        int totalMappings = 0;
        for (const auto& mappings : devicesMap) {
            totalMappings += mappings.size();
        }
        QMessageBox::information(this, tr("导出成功"), 
            tr("成功导出 %1 个虚拟设备，共 %2 条映射规则到：\n%3")
                .arg(devicesMap.size())
                .arg(totalMappings)
                .arg(fileName));
    } else {
        QMessageBox::critical(this, tr("导出失败"), 
            tr("无法导出CSV文件。"));
    }
}

} // namespace ModbusPlexLink

