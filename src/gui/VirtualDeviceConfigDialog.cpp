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
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QFrame>

namespace ModbusPlexLink {

VirtualDeviceConfigDialog::VirtualDeviceConfigDialog(const QJsonObject& config, QWidget *parent)
    : QDialog(parent)
    , m_initialConfig(config)
    , m_isNewDevice(config.isEmpty())
    , m_isUpdatingTable(false)
    , m_variableCompleter(nullptr)
{
    setWindowTitle(m_isNewDevice ? tr("🖥️ 新建虚拟设备") : tr("🖥️ 编辑虚拟设备"));
    resize(1280, 750);
    setMinimumSize(1100, 650);
    
    // 应用现代化样式
    setStyleSheet(DialogStyles::getDialogStyle());
    
    setupUi();
    loadConfig();
    
    // 禁用所有 SpinBox 和 ComboBox 的滚轮事件，防止意外修改
    DialogStyles::disableAllWheelEvents(this);
}

VirtualDeviceConfigDialog::~VirtualDeviceConfigDialog() {
}

void VirtualDeviceConfigDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // 顶部标题区域
    QWidget* headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: transparent;");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 8);
    
    QLabel* iconLabel = new QLabel("🖥️", headerWidget);
    iconLabel->setStyleSheet("font-size: 26px;");
    headerLayout->addWidget(iconLabel);
    
    QVBoxLayout* titleVLayout = new QVBoxLayout();
    titleVLayout->setSpacing(2);
    QLabel* titleLabel = new QLabel(m_isNewDevice ? tr("创建虚拟设备") : tr("编辑虚拟设备"), headerWidget);
    titleLabel->setStyleSheet("font-size: 17px; font-weight: bold; color: #1E293B;");
    titleVLayout->addWidget(titleLabel);
    
    QLabel* subtitleLabel = new QLabel(tr("配置从站地址和数据点映射规则"), headerWidget);
    subtitleLabel->setStyleSheet("color: #64748B; font-size: 10px;");
    titleVLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleVLayout);
    
    headerLayout->addStretch();
    mainLayout->addWidget(headerWidget);
    
    // 基本信息部分
    QWidget* basicInfoWidget = createBasicInfoSection();
    mainLayout->addWidget(basicInfoWidget);
    
    // 映射表格部分
    QWidget* mappingWidget = createMappingTableSection();
    mainLayout->addWidget(mappingWidget, 1);
    
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
    QPushButton* okBtn = DialogStyles::createPrimaryButton(m_isNewDevice ? tr("创建设备") : tr("保存更改"), this);
    
    connect(cancelBtn, &QPushButton::clicked, this, &VirtualDeviceConfigDialog::onRejected);
    connect(okBtn, &QPushButton::clicked, this, &VirtualDeviceConfigDialog::onAccepted);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(okBtn);
    
    mainLayout->addLayout(buttonLayout);
}

QWidget* VirtualDeviceConfigDialog::createBasicInfoSection() {
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
    
    QLabel* sectionTitle = new QLabel(tr("设备配置"), container);
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
    formContainer->setSpacing(40);
    
    // 左侧表单
    QFormLayout* leftForm = new QFormLayout();
    leftForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    leftForm->setHorizontalSpacing(12);
    leftForm->setVerticalSpacing(12);
    
    m_nameEdit = new QLineEdit(container);
    m_nameEdit->setPlaceholderText(tr("例如：电表_01"));
    m_nameEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 8px 12px; background: #F8FAFC;");
    QLabel* nameLabel = new QLabel(tr("设备名称 *"), container);
    nameLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    leftForm->addRow(nameLabel, m_nameEdit);
    
    formContainer->addLayout(leftForm);
    
    // 中间表单
    QFormLayout* midForm = new QFormLayout();
    midForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    midForm->setHorizontalSpacing(12);
    midForm->setVerticalSpacing(12);
    
    m_virtualUnitIdSpin = new QSpinBox(container);
    m_virtualUnitIdSpin->setRange(1, 247);
    m_virtualUnitIdSpin->setValue(1);
    m_virtualUnitIdSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    QLabel* unitLabel = new QLabel(tr("从站地址 *"), container);
    unitLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    midForm->addRow(unitLabel, m_virtualUnitIdSpin);
    
    formContainer->addLayout(midForm);
    
    // 右侧表单
    QFormLayout* rightForm = new QFormLayout();
    rightForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rightForm->setHorizontalSpacing(12);
    rightForm->setVerticalSpacing(12);
    
    m_enabledCheck = new QCheckBox(tr("启用此虚拟设备"), container);
    m_enabledCheck->setChecked(true);
    m_enabledCheck->setStyleSheet("border: none; background: transparent; color: #475569;");
    QLabel* enableLabel = new QLabel(tr("状态"), container);
    enableLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rightForm->addRow(enableLabel, m_enabledCheck);
    
    formContainer->addLayout(rightForm);
    formContainer->addStretch();
    
    mainLayout->addLayout(formContainer);
    
    return container;
}

QWidget* VirtualDeviceConfigDialog::createMappingTableSection() {
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
    
    QLabel* sectionIcon = new QLabel("📊", container);
    sectionIcon->setStyleSheet("font-size: 16px; border: none; background: transparent;");
    headerLayout->addWidget(sectionIcon);
    
    QLabel* titleLabel = new QLabel(tr("数据点映射规则"), container);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #1E293B; border: none; background: transparent;");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    QLabel* countLabel = new QLabel(tr("0 个数据点"), container);
    countLabel->setObjectName("mappingCountLabel");
    countLabel->setStyleSheet(R"(
        color: #7C3AED;
        font-weight: 600;
        font-size: 10pt;
        padding: 4px 12px;
        background-color: #EDE9FE;
        border-radius: 12px;
        border: none;
    )");
    headerLayout->addWidget(countLabel);
    
    layout->addLayout(headerLayout);
    
    // 提示卡片
    QWidget* hintCard = new QWidget(container);
    hintCard->setStyleSheet(R"(
        QWidget {
            background-color: #EFF6FF;
            border: 1px solid #BFDBFE;
            border-radius: 8px;
            border-left: 3px solid #3B82F6;
        }
    )");
    QHBoxLayout* hintLayout = new QHBoxLayout(hintCard);
    hintLayout->setContentsMargins(12, 8, 12, 8);
    
    QLabel* hintIcon = new QLabel("💡", hintCard);
    hintIcon->setStyleSheet("font-size: 14px; border: none; background: transparent;");
    hintLayout->addWidget(hintIcon);
    
    QLabel* hintLabel = new QLabel(
        tr("源变量格式为 \"采集器名:标签名\"，如 \"电表采集器:有功电能\"。输出类型可与源不同，实现自动类型转换。"),
        hintCard);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #1E40AF; font-size: 9pt; border: none; background: transparent;");
    hintLayout->addWidget(hintLabel, 1);
    
    layout->addWidget(hintCard);
    
    // 操作按钮栏
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    
    m_addMappingBtn = DialogStyles::createSecondaryButton(tr("＋ 手动添加"), container);
    m_addFromVariableBtn = DialogStyles::createPrimaryButton(tr("📋 从变量添加"), container);
    m_addFromVariableBtn->setToolTip(tr("从采集器变量列表中选择添加，自动填充数据类型"));
    m_deleteMappingBtn = DialogStyles::createDangerButton(tr("删除"), container);
    m_duplicateMappingBtn = DialogStyles::createSecondaryButton(tr("复制"), container);
    m_importCsvBtn = DialogStyles::createSecondaryButton(tr("📄 CSV导入"), container);
    m_exportCsvBtn = DialogStyles::createSecondaryButton(tr("💾 CSV导出"), container);
    
    m_deleteMappingBtn->setEnabled(false);
    m_duplicateMappingBtn->setEnabled(false);
    
    connect(m_addMappingBtn, &QPushButton::clicked,
            this, &VirtualDeviceConfigDialog::onAddMapping);
    connect(m_addFromVariableBtn, &QPushButton::clicked,
            this, &VirtualDeviceConfigDialog::onAddFromVariable);
    connect(m_deleteMappingBtn, &QPushButton::clicked,
            this, &VirtualDeviceConfigDialog::onDeleteMapping);
    connect(m_duplicateMappingBtn, &QPushButton::clicked,
            this, &VirtualDeviceConfigDialog::onDuplicateMapping);
    connect(m_importCsvBtn, &QPushButton::clicked,
            this, &VirtualDeviceConfigDialog::onImportCsv);
    connect(m_exportCsvBtn, &QPushButton::clicked,
            this, &VirtualDeviceConfigDialog::onExportCsv);
    
    buttonLayout->addWidget(m_addFromVariableBtn);
    buttonLayout->addWidget(m_addMappingBtn);
    buttonLayout->addWidget(m_deleteMappingBtn);
    buttonLayout->addWidget(m_duplicateMappingBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_importCsvBtn);
    buttonLayout->addWidget(m_exportCsvBtn);
    
    layout->addLayout(buttonLayout);
    
    // 映射表格
    m_mappingTable = new QTableWidget(container);
    m_mappingTable->setColumnCount(12);
    m_mappingTable->setHorizontalHeaderLabels({
        tr("#"), tr("源变量"), tr("输出标签"), tr("功能码"), tr("地址"),
        tr("输出类型"), tr("字节序"), tr("倍率"), tr("偏移"), tr("表达式"),
        tr("可写"), tr("注释")
    });
    
    // 应用现代表格样式
    DialogStyles::setupModernTable(m_mappingTable);
    m_mappingTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_mappingTable->horizontalHeader()->setStretchLastSection(true);
    
    m_mappingTable->setColumnWidth(0, 36);   // 序号
    m_mappingTable->setColumnWidth(1, 160);  // 源变量
    m_mappingTable->setColumnWidth(2, 110);  // 输出标签
    m_mappingTable->setColumnWidth(3, 170);   // 功能码
    m_mappingTable->setColumnWidth(4, 65);   // 地址
    m_mappingTable->setColumnWidth(5, 140);   // 输出类型
    m_mappingTable->setColumnWidth(6, 140);   // 字节序
    m_mappingTable->setColumnWidth(7, 65);   // 倍率
    m_mappingTable->setColumnWidth(8, 65);   // 偏移
    m_mappingTable->setColumnWidth(9, 80);   // 表达式
    m_mappingTable->setColumnWidth(10, 130);  // 可写
    
    connect(m_mappingTable, &QTableWidget::itemSelectionChanged,
            this, &VirtualDeviceConfigDialog::onMappingTableSelectionChanged);
    connect(m_mappingTable, &QTableWidget::cellChanged,
            this, &VirtualDeviceConfigDialog::onMappingTableCellChanged);
    
    layout->addWidget(m_mappingTable, 1);
    
    return container;
}

void VirtualDeviceConfigDialog::loadConfig() {
    if (m_initialConfig.isEmpty()) {
        return;
    }
    
    // 加载基本信息
    m_nameEdit->setText(m_initialConfig.value("name").toString());
    m_virtualUnitIdSpin->setValue(m_initialConfig.value("virtualUnitId").toInt(1));
    m_enabledCheck->setChecked(m_initialConfig.value("enabled").toBool(true));
    
    // 加载映射规则
    QJsonArray mappingsArray = m_initialConfig.value("mappings").toArray();
    for (const QJsonValue& value : mappingsArray) {
        if (value.isObject()) {
            QJsonObject mappingObj = value.toObject();
            ServerMappingRule rule;
            
            // 源变量配置
            rule.sourceCollector = mappingObj.value("sourceCollector").toString();
            rule.sourceTagName = mappingObj.value("sourceTagName").toString();
            
            // 兼容旧格式：如果没有sourceTagName，使用tagName作为源
            if (rule.sourceTagName.isEmpty()) {
                rule.sourceTagName = mappingObj.value("tagName").toString();
            }
            
            rule.tagName = mappingObj.value("tagName").toString();
            rule.comment = mappingObj.value("comment").toString();
            rule.address = mappingObj.value("address").toInt();
            rule.scale = mappingObj.value("scale").toDouble(1.0);
            rule.offset = mappingObj.value("offset").toDouble(0.0);
            rule.expression = mappingObj.value("expression").toString();
            rule.writable = mappingObj.value("writable").toBool(false);
            rule.accessLevel = mappingObj.value("accessLevel").toString();
            
            // 支持字符串和整数两种格式（兼容性）
            QJsonValue regTypeValue = mappingObj.value("registerType");
            if (regTypeValue.isString()) {
                rule.registerType = DataTypeUtils::registerTypeFromString(regTypeValue.toString());
            } else {
                rule.registerType = static_cast<RegisterType>(regTypeValue.toInt());
            }
            
            QJsonValue dataTypeValue = mappingObj.value("dataType");
            if (dataTypeValue.isString()) {
                rule.dataType = DataTypeUtils::dataTypeFromString(dataTypeValue.toString());
            } else {
                rule.dataType = static_cast<DataType>(dataTypeValue.toInt());
            }
            
            QJsonValue byteOrderValue = mappingObj.value("byteOrder");
            if (byteOrderValue.isString()) {
                rule.byteOrder = DataTypeUtils::byteOrderFromString(byteOrderValue.toString());
            } else {
                rule.byteOrder = static_cast<ByteOrder>(byteOrderValue.toInt());
            }
            
            m_mappings.append(rule);
        }
    }
    
    refreshMappingTable();
}

void VirtualDeviceConfigDialog::setAvailableVariables(const QList<AvailableVariable>& variables) {
    m_availableVariables = variables;
    m_variableFullIds.clear();
    
    for (const AvailableVariable& var : variables) {
        m_variableFullIds << var.fullId;
    }
    
    // 创建自动完成器
    if (m_variableCompleter) {
        delete m_variableCompleter;
    }
    m_variableCompleter = new QCompleter(m_variableFullIds, this);
    m_variableCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_variableCompleter->setFilterMode(Qt::MatchContains);
}

bool VirtualDeviceConfigDialog::validateConfig() {
    // 验证设备名称
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请输入设备名称"));
        m_nameEdit->setFocus();
        return false;
    }
    
    // 验证映射规则
    if (m_mappings.isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), 
            tr("至少需要添加一个映射规则\n\n提示：点击\"添加映射\"按钮创建映射"));
        return false;
    }
    
    // 检查地址是否重复
    QMap<int, QString> addressMap;
    for (const ServerMappingRule& rule : m_mappings) {
        int registerCount = DataTypeUtils::getRegisterCount(rule.dataType);
        for (int i = 0; i < registerCount; ++i) {
            int addr = rule.address + i;
            if (addressMap.contains(addr)) {
                QMessageBox::warning(this, tr("验证失败"),
                    tr("地址 %1 重复\n\n标签 '%2' 和 '%3' 的地址范围重叠")
                    .arg(addr).arg(addressMap[addr]).arg(rule.tagName));
                return false;
            }
            addressMap[addr] = rule.tagName;
        }
    }
    
    return true;
}

QJsonObject VirtualDeviceConfigDialog::getConfig() const {
    QJsonObject config;
    config["name"] = m_nameEdit->text().trimmed();
    config["virtualUnitId"] = m_virtualUnitIdSpin->value();
    config["enabled"] = m_enabledCheck->isChecked();
    
    QJsonArray mappingsArray;
    for (const ServerMappingRule& rule : m_mappings) {
        QJsonObject mappingObj;
        
        // 源变量配置
        mappingObj["sourceCollector"] = rule.sourceCollector;
        mappingObj["sourceTagName"] = rule.sourceTagName;
        
        // 目标配置
        mappingObj["tagName"] = rule.tagName;
        mappingObj["comment"] = rule.comment;
        mappingObj["registerType"] = DataTypeUtils::registerTypeToString(rule.registerType);
        mappingObj["address"] = rule.address;
        mappingObj["dataType"] = DataTypeUtils::dataTypeToString(rule.dataType);
        mappingObj["byteOrder"] = DataTypeUtils::byteOrderToString(rule.byteOrder);
        
        // 数据转换
        mappingObj["scale"] = rule.scale;
        mappingObj["offset"] = rule.offset;
        if (!rule.expression.isEmpty()) {
            mappingObj["expression"] = rule.expression;
        }
        
        mappingObj["writable"] = rule.writable;
        mappingObj["accessLevel"] = rule.accessLevel;
        
        mappingsArray.append(mappingObj);
    }
    config["mappings"] = mappingsArray;
    
    return config;
}

void VirtualDeviceConfigDialog::refreshMappingTable() {
    m_isUpdatingTable = true;
    
    m_mappingTable->setRowCount(m_mappings.size());
    
    for (int i = 0; i < m_mappings.size(); ++i) {
        setupMappingTableRow(i, m_mappings[i]);
    }
    
    // 更新计数标签
    QLabel* countLabel = findChild<QLabel*>("mappingCountLabel");
    if (countLabel) {
        countLabel->setText(tr("%1 个数据点").arg(m_mappings.size()));
    }
    
    m_isUpdatingTable = false;
    updateMappingButtons();
}

void VirtualDeviceConfigDialog::setupMappingTableRow(int row, const ServerMappingRule& rule) {
    // 0: 序号（只读）
    QTableWidgetItem* rowNumItem = new QTableWidgetItem(QString::number(row + 1));
    rowNumItem->setTextAlignment(Qt::AlignCenter);
    rowNumItem->setFlags(rowNumItem->flags() & ~Qt::ItemIsEditable);
    rowNumItem->setBackground(QBrush(QColor(240, 240, 240)));
    m_mappingTable->setItem(row, 0, rowNumItem);
    
    // 1: 源变量（可编辑，带自动完成）
    QLineEdit* sourceEdit = new QLineEdit();
    sourceEdit->setText(rule.getFullSourceId());
    sourceEdit->setPlaceholderText(tr("采集器:标签名"));
    if (m_variableCompleter) {
        sourceEdit->setCompleter(m_variableCompleter);
    }
    // 源变量改变时更新输出标签
    connect(sourceEdit, &QLineEdit::editingFinished, this, [this, row]() {
        onSourceTagChanged(row);
    });
    m_mappingTable->setCellWidget(row, 1, sourceEdit);
    
    // 2: 输出标签（可编辑）
    QTableWidgetItem* tagNameItem = new QTableWidgetItem(rule.tagName);
    m_mappingTable->setItem(row, 2, tagNameItem);
    
    // 3: 功能码（下拉框）
    QComboBox* registerTypeCombo = new QComboBox();
    registerTypeCombo->addItem("01 (Coil)", static_cast<int>(RegisterType::Coil));
    registerTypeCombo->addItem("02 (Discrete)", static_cast<int>(RegisterType::DiscreteInput));
    registerTypeCombo->addItem("03 (Holding)", static_cast<int>(RegisterType::HoldingRegister));
    registerTypeCombo->addItem("04 (Input)", static_cast<int>(RegisterType::InputRegister));
    registerTypeCombo->setCurrentIndex(registerTypeCombo->findData(static_cast<int>(rule.registerType)));
    m_mappingTable->setCellWidget(row, 3, registerTypeCombo);
    
    // 4: 地址（可编辑）
    QTableWidgetItem* addressItem = new QTableWidgetItem(QString::number(rule.address));
    addressItem->setTextAlignment(Qt::AlignCenter);
    m_mappingTable->setItem(row, 4, addressItem);
    
    // 5: 输出数据类型（下拉框）
    QComboBox* dataTypeCombo = new QComboBox();
    dataTypeCombo->addItem("UInt16", static_cast<int>(DataType::UInt16));
    dataTypeCombo->addItem("Int16", static_cast<int>(DataType::Int16));
    dataTypeCombo->addItem("UInt32", static_cast<int>(DataType::UInt32));
    dataTypeCombo->addItem("Int32", static_cast<int>(DataType::Int32));
    dataTypeCombo->addItem("Float32", static_cast<int>(DataType::Float32));
    dataTypeCombo->addItem("UInt64", static_cast<int>(DataType::UInt64));
    dataTypeCombo->addItem("Int64", static_cast<int>(DataType::Int64));
    dataTypeCombo->addItem("Float64", static_cast<int>(DataType::Float64));
    dataTypeCombo->setCurrentIndex(dataTypeCombo->findData(static_cast<int>(rule.dataType)));
    m_mappingTable->setCellWidget(row, 5, dataTypeCombo);
    
    // 6: 字节序（下拉框）
    QComboBox* byteOrderCombo = new QComboBox();
    byteOrderCombo->addItem("AB", static_cast<int>(ByteOrder::AB));
    byteOrderCombo->addItem("BA", static_cast<int>(ByteOrder::BA));
    byteOrderCombo->addItem("ABCD", static_cast<int>(ByteOrder::ABCD));
    byteOrderCombo->addItem("DCBA", static_cast<int>(ByteOrder::DCBA));
    byteOrderCombo->addItem("BADC", static_cast<int>(ByteOrder::BADC));
    byteOrderCombo->addItem("CDAB", static_cast<int>(ByteOrder::CDAB));
    byteOrderCombo->setCurrentIndex(byteOrderCombo->findData(static_cast<int>(rule.byteOrder)));
    m_mappingTable->setCellWidget(row, 6, byteOrderCombo);
    
    // 7: 倍率（可编辑）
    QTableWidgetItem* scaleItem = new QTableWidgetItem(QString::number(rule.scale, 'f', 3));
    scaleItem->setTextAlignment(Qt::AlignCenter);
    m_mappingTable->setItem(row, 7, scaleItem);
    
    // 8: 偏移（可编辑）
    QTableWidgetItem* offsetItem = new QTableWidgetItem(QString::number(rule.offset, 'f', 3));
    offsetItem->setTextAlignment(Qt::AlignCenter);
    m_mappingTable->setItem(row, 8, offsetItem);
    
    // 9: 表达式（可编辑）
    QTableWidgetItem* exprItem = new QTableWidgetItem(rule.expression);
    exprItem->setToolTip(tr("表达式示例：x*0.1, (x-32)*5/9\nx代表源值"));
    m_mappingTable->setItem(row, 9, exprItem);
    
    // 10: 可写（下拉框）
    QComboBox* writableCombo = new QComboBox();
    writableCombo->addItem(tr("否"), false);
    writableCombo->addItem(tr("是"), true);
    writableCombo->setCurrentIndex(rule.writable ? 1 : 0);
    m_mappingTable->setCellWidget(row, 10, writableCombo);
    
    // 11: 注释（可编辑）
    QTableWidgetItem* commentItem = new QTableWidgetItem(rule.comment);
    m_mappingTable->setItem(row, 11, commentItem);
}

void VirtualDeviceConfigDialog::onSourceTagChanged(int row) {
    if (row < 0 || row >= m_mappingTable->rowCount()) {
        return;
    }
    
    // 获取源变量
    QLineEdit* sourceEdit = qobject_cast<QLineEdit*>(m_mappingTable->cellWidget(row, 1));
    if (!sourceEdit) return;
    
    QString fullId = sourceEdit->text();
    
    // 如果输出标签为空，自动填充源标签名
    QTableWidgetItem* tagItem = m_mappingTable->item(row, 2);
    if (tagItem && tagItem->text().isEmpty()) {
        // 提取标签名部分
        int colonPos = fullId.indexOf(':');
        QString tagName = (colonPos > 0) ? fullId.mid(colonPos + 1) : fullId;
        tagItem->setText(tagName);
    }
    
    // 查找源变量信息，自动设置数据类型
    for (const AvailableVariable& var : m_availableVariables) {
        if (var.fullId == fullId) {
            // 可以自动设置输出类型与源类型相同（可选）
            break;
        }
    }
}

ServerMappingRule VirtualDeviceConfigDialog::getMappingRuleFromRow(int row) const {
    ServerMappingRule rule;
    
    if (row < 0 || row >= m_mappingTable->rowCount()) {
        return rule;
    }
    
    // 1: 源变量
    QLineEdit* sourceEdit = qobject_cast<QLineEdit*>(m_mappingTable->cellWidget(row, 1));
    if (sourceEdit) {
        rule.setFullSourceId(sourceEdit->text());
    }
    
    // 2: 输出标签名
    QTableWidgetItem* tagNameItem = m_mappingTable->item(row, 2);
    rule.tagName = tagNameItem ? tagNameItem->text() : "";
    
    // 3: 功能码
    QComboBox* registerTypeCombo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 3));
    rule.registerType = registerTypeCombo ? 
        static_cast<RegisterType>(registerTypeCombo->currentData().toInt()) : 
        RegisterType::HoldingRegister;
    
    // 4: 地址
    QTableWidgetItem* addressItem = m_mappingTable->item(row, 4);
    rule.address = addressItem ? addressItem->text().toInt() : 0;
    
    // 5: 输出数据类型
    QComboBox* dataTypeCombo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 5));
    rule.dataType = dataTypeCombo ? 
        static_cast<DataType>(dataTypeCombo->currentData().toInt()) : 
        DataType::UInt16;
    
    // 6: 字节序
    QComboBox* byteOrderCombo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 6));
    rule.byteOrder = byteOrderCombo ? 
        static_cast<ByteOrder>(byteOrderCombo->currentData().toInt()) : 
        ByteOrder::AB;
    
    // 7: 倍率
    QTableWidgetItem* scaleItem = m_mappingTable->item(row, 7);
    rule.scale = scaleItem ? scaleItem->text().toDouble() : 1.0;
    
    // 8: 偏移
    QTableWidgetItem* offsetItem = m_mappingTable->item(row, 8);
    rule.offset = offsetItem ? offsetItem->text().toDouble() : 0.0;
    
    // 9: 表达式
    QTableWidgetItem* exprItem = m_mappingTable->item(row, 9);
    rule.expression = exprItem ? exprItem->text() : "";
    
    // 10: 可写
    QComboBox* writableCombo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 10));
    rule.writable = writableCombo ? writableCombo->currentData().toBool() : false;
    
    // 11: 注释
    QTableWidgetItem* commentItem = m_mappingTable->item(row, 11);
    rule.comment = commentItem ? commentItem->text() : "";
    
    return rule;
}

void VirtualDeviceConfigDialog::updateMappingButtons() {
    bool hasSelection = m_mappingTable->currentRow() >= 0;
    bool hasMappings = m_mappings.size() > 0;
    
    m_deleteMappingBtn->setEnabled(hasSelection);
    m_duplicateMappingBtn->setEnabled(hasSelection);
    m_exportCsvBtn->setEnabled(hasMappings);
}

void VirtualDeviceConfigDialog::onAddMapping() {
    ServerMappingRule rule;
    rule.tagName = tr("NewTag%1").arg(m_mappings.size() + 1);
    rule.sourceTagName = rule.tagName;
    rule.registerType = RegisterType::HoldingRegister;
    rule.dataType = DataType::UInt16;
    rule.byteOrder = ByteOrder::AB;
    rule.scale = 1.0;
    rule.offset = 0.0;
    rule.writable = false;
    
    // 智能地址递增
    if (!m_mappings.isEmpty()) {
        const ServerMappingRule& lastRule = m_mappings.last();
        int lastRegisterCount = DataTypeUtils::getRegisterCount(lastRule.dataType);
        rule.address = lastRule.address + lastRegisterCount;
    } else {
        rule.address = 0;
    }
    
    m_mappings.append(rule);
    refreshMappingTable();
    m_mappingTable->selectRow(m_mappings.size() - 1);
    
    // 焦点在源变量输入框
    QLineEdit* sourceEdit = qobject_cast<QLineEdit*>(m_mappingTable->cellWidget(m_mappings.size() - 1, 1));
    if (sourceEdit) {
        sourceEdit->setFocus();
        sourceEdit->selectAll();
    }
}

void VirtualDeviceConfigDialog::onAddFromVariable() {
    if (m_availableVariables.isEmpty()) {
        QMessageBox::information(this, tr("提示"), 
            tr("当前没有可用的采集器变量。\n\n请先在采集器中配置变量映射规则。"));
        return;
    }
    
    // 创建变量选择对话框
    QDialog selectDialog(this);
    selectDialog.setWindowTitle(tr("选择采集器变量"));
    selectDialog.resize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&selectDialog);
    
    // 搜索框
    QLineEdit* searchEdit = new QLineEdit(&selectDialog);
    searchEdit->setPlaceholderText(tr("搜索变量..."));
    layout->addWidget(searchEdit);
    
    // 变量列表
    QTableWidget* varTable = new QTableWidget(&selectDialog);
    varTable->setColumnCount(4);
    varTable->setHorizontalHeaderLabels({tr("采集器"), tr("标签名"), tr("数据类型"), tr("注释")});
    varTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    varTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    varTable->horizontalHeader()->setStretchLastSection(true);
    varTable->setRowCount(m_availableVariables.size());
    
    for (int i = 0; i < m_availableVariables.size(); ++i) {
        const AvailableVariable& var = m_availableVariables[i];
        varTable->setItem(i, 0, new QTableWidgetItem(var.collectorName));
        varTable->setItem(i, 1, new QTableWidgetItem(var.tagName));
        varTable->setItem(i, 2, new QTableWidgetItem(DataTypeUtils::dataTypeToString(var.dataType)));
        varTable->setItem(i, 3, new QTableWidgetItem(var.comment));
    }
    
    layout->addWidget(varTable);
    
    // 过滤功能
    connect(searchEdit, &QLineEdit::textChanged, &selectDialog, [varTable, this](const QString& text) {
        for (int i = 0; i < varTable->rowCount(); ++i) {
            bool match = text.isEmpty();
            if (!match) {
                for (int j = 0; j < 4; ++j) {
                    QTableWidgetItem* item = varTable->item(i, j);
                    if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            varTable->setRowHidden(i, !match);
        }
    });
    
    // 按钮
    QDialogButtonBox* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &selectDialog);
    connect(btnBox, &QDialogButtonBox::accepted, &selectDialog, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &selectDialog, &QDialog::reject);
    layout->addWidget(btnBox);
    
    if (selectDialog.exec() == QDialog::Accepted) {
        QList<int> selectedRows;
        for (const QModelIndex& idx : varTable->selectionModel()->selectedRows()) {
            selectedRows << idx.row();
        }
        
        if (selectedRows.isEmpty()) {
            return;
        }
        
        // 添加选中的变量
        for (int row : selectedRows) {
            const AvailableVariable& var = m_availableVariables[row];
            
            ServerMappingRule rule;
            rule.sourceCollector = var.collectorName;
            rule.sourceTagName = var.tagName;
            rule.tagName = var.tagName;
            rule.comment = var.comment;
            rule.dataType = var.dataType;  // 默认使用源数据类型
            rule.registerType = RegisterType::HoldingRegister;
            rule.byteOrder = ByteOrder::AB;
            rule.scale = 1.0;
            rule.offset = 0.0;
            
            // 智能地址递增
            if (!m_mappings.isEmpty()) {
                const ServerMappingRule& lastRule = m_mappings.last();
                int lastRegisterCount = DataTypeUtils::getRegisterCount(lastRule.dataType);
                rule.address = lastRule.address + lastRegisterCount;
            } else {
                rule.address = 0;
            }
            
            m_mappings.append(rule);
        }
        
        refreshMappingTable();
        m_mappingTable->selectRow(m_mappings.size() - 1);
        
        QMessageBox::information(this, tr("添加成功"), 
            tr("已添加 %1 个变量映射。").arg(selectedRows.size()));
    }
}

void VirtualDeviceConfigDialog::onDeleteMapping() {
    int row = m_mappingTable->currentRow();
    if (row < 0 || row >= m_mappings.size()) {
        return;
    }
    
    m_mappings.removeAt(row);
    refreshMappingTable();
    
    // 选中下一行或上一行
    if (row < m_mappings.size()) {
        m_mappingTable->selectRow(row);
    } else if (m_mappings.size() > 0) {
        m_mappingTable->selectRow(m_mappings.size() - 1);
    }
}

void VirtualDeviceConfigDialog::onDuplicateMapping() {
    int row = m_mappingTable->currentRow();
    if (row < 0 || row >= m_mappings.size()) {
        return;
    }
    
    // 先保存当前行的数据
    m_mappings[row] = getMappingRuleFromRow(row);
    
    // 复制映射规则
    ServerMappingRule rule = m_mappings[row];
    
    // 智能地址递增和标签命名
    int registerCount = DataTypeUtils::getRegisterCount(rule.dataType);
    rule.address += registerCount;
    rule.tagName += tr("_副本");
    
    m_mappings.insert(row + 1, rule);
    refreshMappingTable();
    m_mappingTable->selectRow(row + 1);
}

void VirtualDeviceConfigDialog::onImportCsv() {
    // 创建菜单选择导入方式
    QMenu menu(this);
    QAction* importAction = menu.addAction(tr("导入CSV文件"));
    QAction* templateAction = menu.addAction(tr("下载CSV模板"));
    
    QAction* selectedAction = menu.exec(QCursor::pos());
    
    if (selectedAction == templateAction) {
        // 生成CSV模板
        QString fileName = QFileDialog::getSaveFileName(
            this,
            tr("保存CSV模板"),
            "server_mapping_template.csv",
            tr("CSV文件 (*.csv)")
        );
        
        if (!fileName.isEmpty()) {
            if (CsvHelper::generateServerTemplate(fileName)) {
                QMessageBox::information(this, tr("成功"), 
                    tr("CSV模板已生成：\n%1\n\n请参考模板格式填写数据后导入。").arg(fileName));
            } else {
                QMessageBox::warning(this, tr("错误"), 
                    tr("无法生成CSV模板文件。"));
            }
        }
        return;
    }
    
    if (selectedAction == importAction) {
        // 导入CSV文件
        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("导入CSV文件"),
            "",
            tr("CSV文件 (*.csv)")
        );
        
        if (fileName.isEmpty()) {
            return;
        }
        
        // 询问是否清空现有数据
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("导入模式"),
            tr("是否清空现有映射规则？\n\n"
               "点击【是】：清空现有规则，只保留导入的数据\n"
               "点击【否】：追加导入的数据到现有规则"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
        );
        
        if (reply == QMessageBox::Cancel) {
            return;
        }
        
        bool clearExisting = (reply == QMessageBox::Yes);
        
        // 导入CSV
        QList<ServerMappingRule> importedMappings;
        QString errorMsg;
        
        if (!CsvHelper::importServerMappings(fileName, importedMappings, errorMsg)) {
            QMessageBox::critical(this, tr("导入失败"), 
                tr("无法导入CSV文件：\n%1").arg(errorMsg));
            return;
        }
        
        // 更新映射列表
        if (clearExisting) {
            m_mappings.clear();
        }
        
        m_mappings.append(importedMappings);
        
        // 刷新表格
        refreshMappingTable();
        
        QMessageBox::information(this, tr("导入成功"), 
            tr("成功导入 %1 条映射规则。").arg(importedMappings.size()));
    }
}

void VirtualDeviceConfigDialog::onExportCsv() {
    if (m_mappings.isEmpty()) {
        QMessageBox::information(this, tr("提示"), 
            tr("当前没有映射规则可导出。"));
        return;
    }
    
    // 先同步表格数据到m_mappings
    for (int i = 0; i < m_mappingTable->rowCount(); ++i) {
        if (i < m_mappings.size()) {
            m_mappings[i] = getMappingRuleFromRow(i);
        }
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出到CSV"),
        QString("server_mappings_%1.csv").arg(m_nameEdit->text()),
        tr("CSV文件 (*.csv)")
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    if (CsvHelper::exportServerMappings(m_mappings, fileName)) {
        QMessageBox::information(this, tr("导出成功"), 
            tr("成功导出 %1 条映射规则到：\n%2").arg(m_mappings.size()).arg(fileName));
    } else {
        QMessageBox::critical(this, tr("导出失败"), 
            tr("无法导出CSV文件。"));
    }
}

void VirtualDeviceConfigDialog::onMappingTableSelectionChanged() {
    updateMappingButtons();
}

void VirtualDeviceConfigDialog::onMappingTableCellChanged(int row, int column) {
    if (m_isUpdatingTable || row < 0 || row >= m_mappings.size()) {
        return;
    }
    
    // 保存表格中的修改到内部数据结构
    m_mappings[row] = getMappingRuleFromRow(row);
}

void VirtualDeviceConfigDialog::onAccepted() {
    // 在验证前，保存所有行的修改
    for (int i = 0; i < m_mappings.size(); ++i) {
        m_mappings[i] = getMappingRuleFromRow(i);
    }
    
    if (validateConfig()) {
        accept();
    }
}

void VirtualDeviceConfigDialog::onRejected() {
    reject();
}

} // namespace ModbusPlexLink

