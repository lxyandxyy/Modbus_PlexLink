#include "CollectorConfigDialog.h"
#include "TemplateSelectDialog.h"
#include "DialogStyles.h"
#include "utils/CsvHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QFileDialog>
#include <QSplitter>
#include <QScrollArea>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QFrame>
#include <QGraphicsDropShadowEffect>

namespace ModbusPlexLink {

CollectorConfigDialog::CollectorConfigDialog(const QJsonObject& config, QWidget *parent)
    : QDialog(parent)
    , m_isNewCollector(config.isEmpty())
    , m_originalConfig(config)
    , m_protocolCombo(nullptr)
    , m_nameEdit(nullptr)
    , m_tcpWidget(nullptr)
    , m_ipEdit(nullptr)
    , m_portSpin(nullptr)
    , m_rtuWidget(nullptr)
    , m_serialPortCombo(nullptr)
    , m_baudRateCombo(nullptr)
    , m_dataBitsCombo(nullptr)
    , m_parityCombo(nullptr)
    , m_stopBitsCombo(nullptr)
    , m_unitIdSpin(nullptr)
    , m_pollRateSpin(nullptr)
    , m_timeoutSpin(nullptr)
    , m_maxRetriesSpin(nullptr)
    , m_autoReconnectCheck(nullptr)
    , m_logErrorsCheck(nullptr)
    , m_mappingTable(nullptr)
    , m_addMappingBtn(nullptr)
    , m_deleteMappingBtn(nullptr)
    , m_duplicateMappingBtn(nullptr)
    , m_importCsvBtn(nullptr)
    , m_exportCsvBtn(nullptr)
    , m_isUpdatingTable(false)
{
    setWindowTitle(m_isNewCollector ? tr("📥 新建采集器") : tr("📥 编辑采集器"));
    resize(1250, 750);
    setMinimumSize(1050, 650);
    
    // 应用现代化样式
    setStyleSheet(DialogStyles::getDialogStyle());
    
    setupUi();
    loadConfig();
    
    // 禁用所有 SpinBox 和 ComboBox 的滚轮事件，防止意外修改
    DialogStyles::disableAllWheelEvents(this);
}

CollectorConfigDialog::~CollectorConfigDialog() {
}

void CollectorConfigDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // 顶部标题区域
    QWidget* headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background: transparent;");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 8);
    
    QLabel* iconLabel = new QLabel("📥", headerWidget);
    iconLabel->setStyleSheet("font-size: 26px;");
    headerLayout->addWidget(iconLabel);
    
    QVBoxLayout* titleVLayout = new QVBoxLayout();
    titleVLayout->setSpacing(2);
    QLabel* titleLabel = new QLabel(m_isNewCollector ? tr("创建数据采集器") : tr("编辑采集器配置"), headerWidget);
    titleLabel->setStyleSheet("font-size: 17px; font-weight: bold; color: #1E293B;");
    titleVLayout->addWidget(titleLabel);
    
    QLabel* subtitleLabel = new QLabel(tr("配置连接参数和数据点映射规则"), headerWidget);
    subtitleLabel->setStyleSheet("color: #64748B; font-size: 10px;");
    titleVLayout->addWidget(subtitleLabel);
    headerLayout->addLayout(titleVLayout);
    
    headerLayout->addStretch();
    mainLayout->addWidget(headerWidget);
    
    // 使用分割器分隔基本信息和映射表格
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    splitter->setStyleSheet(R"(
        QSplitter::handle {
            background-color: #E2E8F0;
            height: 3px;
        }
        QSplitter::handle:hover {
            background-color: #3B82F6;
        }
    )");
    
    // 基本信息区域
    QWidget* basicInfoWidget = createBasicInfoSection();
    splitter->addWidget(basicInfoWidget);
    
    // 映射表格区域
    QWidget* mappingWidget = createMappingTableSection();
    splitter->addWidget(mappingWidget);
    
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    
    mainLayout->addWidget(splitter, 1);
    
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
    QPushButton* okBtn = DialogStyles::createPrimaryButton(m_isNewCollector ? tr("创建采集器") : tr("保存更改"), this);
    
    connect(cancelBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onRejected);
    connect(okBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onAccepted);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(okBtn);
    
    mainLayout->addLayout(buttonLayout);
}

QWidget* CollectorConfigDialog::createBasicInfoSection() {
    QWidget* container = new QWidget();
    container->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border: 1px solid #E2E8F0;
            border-radius: 12px;
        }
    )");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(16);
    
    // 标题栏
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* sectionIcon = new QLabel("⚙️", container);
    sectionIcon->setStyleSheet("font-size: 16px; border: none; background: transparent;");
    headerLayout->addWidget(sectionIcon);
    
    QLabel* sectionTitle = new QLabel(tr("连接配置"), container);
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
    
    // 表单区域
    QHBoxLayout* formContainer = new QHBoxLayout();
    formContainer->setSpacing(30);
    
    // 左侧：基本信息
    QFormLayout* leftForm = new QFormLayout();
    leftForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    leftForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    leftForm->setHorizontalSpacing(12);
    leftForm->setVerticalSpacing(12);
    
    // 名称
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("例如: 电表采集器_01"));
    m_nameEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 8px 12px; background: #F8FAFC;");
    connect(m_nameEdit, &QLineEdit::textChanged, this, &CollectorConfigDialog::onNameChanged);
    QLabel* nameLabel = new QLabel(tr("采集器名称 *"), container);
    nameLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    leftForm->addRow(nameLabel, m_nameEdit);
    
    // 协议选择
    m_protocolCombo = new QComboBox();
    m_protocolCombo->addItem(tr("🌐 Modbus TCP"), "modbus-tcp");
    m_protocolCombo->addItem(tr("🔌 Modbus RTU"), "modbus-rtu");
    m_protocolCombo->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CollectorConfigDialog::onProtocolChanged);
    QLabel* protoLabel = new QLabel(tr("通讯协议 *"), container);
    protoLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    leftForm->addRow(protoLabel, m_protocolCombo);
    
    // 从站ID
    m_unitIdSpin = new QSpinBox();
    m_unitIdSpin->setRange(0, 255);
    m_unitIdSpin->setValue(1);
    m_unitIdSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    QLabel* unitLabel = new QLabel(tr("从站地址"), container);
    unitLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    leftForm->addRow(unitLabel, m_unitIdSpin);
    
    formContainer->addLayout(leftForm);
    
    // 中间：连接参数
    QVBoxLayout* middleLayout = new QVBoxLayout();
    middleLayout->setSpacing(10);
    
    // TCP配置区域
    m_tcpWidget = new QWidget();
    m_tcpWidget->setStyleSheet("border: none; background: transparent;");
    QFormLayout* tcpForm = new QFormLayout(m_tcpWidget);
    tcpForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tcpForm->setHorizontalSpacing(12);
    tcpForm->setVerticalSpacing(10);
    
    m_ipEdit = new QLineEdit();
    m_ipEdit->setPlaceholderText(tr("192.168.1.10"));
    m_ipEdit->setMinimumWidth(140);
    m_ipEdit->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 8px 12px; background: #F8FAFC;");
    connect(m_ipEdit, &QLineEdit::textChanged, this, &CollectorConfigDialog::onIpChanged);
    QLabel* ipLabel = new QLabel(tr("目标IP *"), m_tcpWidget);
    ipLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    tcpForm->addRow(ipLabel, m_ipEdit);
    
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(502);
    m_portSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    QLabel* portLabel = new QLabel(tr("端口"), m_tcpWidget);
    portLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    tcpForm->addRow(portLabel, m_portSpin);
    
    middleLayout->addWidget(m_tcpWidget);
    
    // RTU配置区域
    m_rtuWidget = new QWidget();
    m_rtuWidget->setStyleSheet("border: none; background: transparent;");
    QFormLayout* rtuForm = new QFormLayout(m_rtuWidget);
    rtuForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rtuForm->setHorizontalSpacing(12);
    rtuForm->setVerticalSpacing(8);
    
    m_serialPortCombo = new QComboBox();
    m_serialPortCombo->setEditable(true);
    m_serialPortCombo->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
#ifdef Q_OS_WIN
    for (int i = 1; i <= 16; ++i) {
        m_serialPortCombo->addItem(QString("COM%1").arg(i));
    }
#else
    m_serialPortCombo->addItems({"/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyS0", "/dev/ttyS1"});
#endif
    QLabel* serialLabel = new QLabel(tr("串口 *"), m_rtuWidget);
    serialLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rtuForm->addRow(serialLabel, m_serialPortCombo);
    
    QHBoxLayout* serialParams = new QHBoxLayout();
    serialParams->setSpacing(8);
    
    m_baudRateCombo = new QComboBox();
    m_baudRateCombo->addItems({"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"});
    m_baudRateCombo->setCurrentText("9600");
    m_baudRateCombo->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 4px 8px; background: white;");
    serialParams->addWidget(m_baudRateCombo);
    
    m_dataBitsCombo = new QComboBox();
    m_dataBitsCombo->addItems({"7", "8"});
    m_dataBitsCombo->setCurrentText("8");
    m_dataBitsCombo->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 4px 8px; background: white;");
    serialParams->addWidget(m_dataBitsCombo);
    
    m_parityCombo = new QComboBox();
    m_parityCombo->addItem(tr("N"), "N");
    m_parityCombo->addItem(tr("E"), "E");
    m_parityCombo->addItem(tr("O"), "O");
    m_parityCombo->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 4px 8px; background: white;");
    serialParams->addWidget(m_parityCombo);
    
    m_stopBitsCombo = new QComboBox();
    m_stopBitsCombo->addItems({"1", "2"});
    m_stopBitsCombo->setCurrentText("1");
    m_stopBitsCombo->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 4px 8px; background: white;");
    serialParams->addWidget(m_stopBitsCombo);
    
    QLabel* paramsLabel = new QLabel(tr("参数"), m_rtuWidget);
    paramsLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rtuForm->addRow(paramsLabel, serialParams);
    
    middleLayout->addWidget(m_rtuWidget);
    m_rtuWidget->hide();
    
    formContainer->addLayout(middleLayout);
    
    // 右侧：时序和选项
    QFormLayout* rightForm = new QFormLayout();
    rightForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rightForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    rightForm->setHorizontalSpacing(12);
    rightForm->setVerticalSpacing(10);
    
    m_pollRateSpin = new QSpinBox();
    m_pollRateSpin->setRange(100, 60000);
    m_pollRateSpin->setValue(1000);
    m_pollRateSpin->setSuffix(tr(" ms"));
    m_pollRateSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    QLabel* pollLabel = new QLabel(tr("轮询周期"), container);
    pollLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rightForm->addRow(pollLabel, m_pollRateSpin);
    
    m_timeoutSpin = new QSpinBox();
    m_timeoutSpin->setRange(100, 10000);
    m_timeoutSpin->setValue(3000);
    m_timeoutSpin->setSuffix(tr(" ms"));
    m_timeoutSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    QLabel* timeoutLabel = new QLabel(tr("超时时间"), container);
    timeoutLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rightForm->addRow(timeoutLabel, m_timeoutSpin);
    
    m_maxRetriesSpin = new QSpinBox();
    m_maxRetriesSpin->setRange(0, 10);
    m_maxRetriesSpin->setValue(3);
    m_maxRetriesSpin->setStyleSheet("border: 1px solid #E2E8F0; border-radius: 6px; padding: 6px 10px; background: white;");
    QLabel* retryLabel = new QLabel(tr("重试次数"), container);
    retryLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rightForm->addRow(retryLabel, m_maxRetriesSpin);
    
    // 选项
    QVBoxLayout* optionsLayout = new QVBoxLayout();
    optionsLayout->setSpacing(6);
    
    m_autoReconnectCheck = new QCheckBox(tr("自动重连"));
    m_autoReconnectCheck->setChecked(true);
    m_autoReconnectCheck->setStyleSheet("border: none; background: transparent; color: #475569;");
    
    m_logErrorsCheck = new QCheckBox(tr("记录日志"));
    m_logErrorsCheck->setChecked(true);
    m_logErrorsCheck->setStyleSheet("border: none; background: transparent; color: #475569;");
    
    optionsLayout->addWidget(m_autoReconnectCheck);
    optionsLayout->addWidget(m_logErrorsCheck);
    
    QLabel* optLabel = new QLabel(tr("选项"), container);
    optLabel->setStyleSheet("font-weight: 600; color: #374151; border: none; background: transparent;");
    rightForm->addRow(optLabel, optionsLayout);
    
    formContainer->addLayout(rightForm);
    formContainer->addStretch();
    
    mainLayout->addLayout(formContainer);
    
    return container;
}

void CollectorConfigDialog::onProtocolChanged(int index) {
    QString protocol = m_protocolCombo->itemData(index).toString();
    bool isTcp = (protocol == "modbus-tcp");
    
    m_tcpWidget->setVisible(isTcp);
    m_rtuWidget->setVisible(!isTcp);
}

QWidget* CollectorConfigDialog::createMappingTableSection() {
    QWidget* container = new QWidget();
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
    
    // 说明
    QLabel* hintLabel = new QLabel(
        tr("定义从物理设备读取数据并转换为UDM标签的规则。双击单元格可编辑，也可从模板或CSV导入。"),
        container);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("color: #64748B; font-size: 9pt; border: none; background: transparent; padding-bottom: 4px;");
    layout->addWidget(hintLabel);
    
    // 操作按钮栏
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    
    m_loadTemplateBtn = DialogStyles::createPrimaryButton(tr("📦 从模板加载"), container);
    m_loadTemplateBtn->setToolTip(tr("从预配置的设备模板加载配置"));
    
    m_addMappingBtn = DialogStyles::createSecondaryButton(tr("＋ 添加"), container);
    m_deleteMappingBtn = DialogStyles::createDangerButton(tr("删除"), container);
    m_duplicateMappingBtn = DialogStyles::createSecondaryButton(tr("复制"), container);
    m_importCsvBtn = DialogStyles::createSecondaryButton(tr("📄 CSV导入"), container);
    m_exportCsvBtn = DialogStyles::createSecondaryButton(tr("💾 CSV导出"), container);
    
    connect(m_loadTemplateBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onLoadFromTemplate);
    connect(m_addMappingBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onAddMapping);
    connect(m_deleteMappingBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onDeleteMapping);
    connect(m_duplicateMappingBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onDuplicateMapping);
    connect(m_importCsvBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onImportFromCSV);
    connect(m_exportCsvBtn, &QPushButton::clicked, this, &CollectorConfigDialog::onExportToCSV);
    
    btnLayout->addWidget(m_loadTemplateBtn);
    btnLayout->addSpacing(16);
    btnLayout->addWidget(m_addMappingBtn);
    btnLayout->addWidget(m_deleteMappingBtn);
    btnLayout->addWidget(m_duplicateMappingBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_importCsvBtn);
    btnLayout->addWidget(m_exportCsvBtn);
    
    layout->addLayout(btnLayout);
    
    // 映射表格
    m_mappingTable = new QTableWidget();
    m_mappingTable->setColumnCount(11);
    m_mappingTable->setHorizontalHeaderLabels({
        tr("#"), tr("标签名 *"), tr("注释"), tr("功能码"), tr("地址"), 
        tr("数据类型"), tr("字节序"), tr("倍率"), tr("偏移"), 
        tr("单位"), tr("启用")
    });
    
    // 应用现代表格样式
    DialogStyles::setupModernTable(m_mappingTable);
    m_mappingTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_mappingTable->horizontalHeader()->setStretchLastSection(false);
    
    // 设置列宽
    m_mappingTable->setColumnWidth(0, 36);   // 行号
    m_mappingTable->setColumnWidth(1, 140);  // 标签名
    m_mappingTable->setColumnWidth(2, 110);  // 注释
    m_mappingTable->setColumnWidth(3, 210);  // 功能码
    m_mappingTable->setColumnWidth(4, 70);   // 地址
    m_mappingTable->setColumnWidth(5, 140);   // 数据类型
    m_mappingTable->setColumnWidth(6, 140);   // 字节序
    m_mappingTable->setColumnWidth(7, 60);   // 倍率
    m_mappingTable->setColumnWidth(8, 60);   // 偏移
    m_mappingTable->setColumnWidth(9, 55);   // 单位
    m_mappingTable->setColumnWidth(10, 50);  // 启用
    
    connect(m_mappingTable, &QTableWidget::itemSelectionChanged,
            this, &CollectorConfigDialog::onMappingTableSelectionChanged);
    connect(m_mappingTable, &QTableWidget::cellChanged,
            this, &CollectorConfigDialog::onMappingTableCellChanged);
    
    layout->addWidget(m_mappingTable, 1);
    
    updateMappingButtons();
    
    return container;
}

void CollectorConfigDialog::loadConfig() {
    if (m_isNewCollector) {
        // 新建采集器，使用默认值
        m_nameEdit->setText(tr("新采集器"));
        m_nameEdit->selectAll();
        m_nameEdit->setFocus();
        return;
    }
    
    // 加载基本信息
    m_nameEdit->setText(m_originalConfig.value("name").toString());
    
    // 加载协议类型
    QString protocol = m_originalConfig.value("protocol").toString("modbus-tcp").toLower();
    if (protocol == "modbus-rtu" || protocol == "modbus_rtu") {
        m_protocolCombo->setCurrentIndex(1);  // RTU
        
        // 加载RTU参数
        m_serialPortCombo->setCurrentText(m_originalConfig.value("serialPort").toString("COM1"));
        m_baudRateCombo->setCurrentText(QString::number(m_originalConfig.value("baudRate").toInt(9600)));
        m_dataBitsCombo->setCurrentText(QString::number(m_originalConfig.value("dataBits").toInt(8)));
        m_stopBitsCombo->setCurrentText(QString::number(m_originalConfig.value("stopBits").toInt(1)));
        
        QString parity = m_originalConfig.value("parity").toString("N");
        for (int i = 0; i < m_parityCombo->count(); ++i) {
            if (m_parityCombo->itemData(i).toString() == parity) {
                m_parityCombo->setCurrentIndex(i);
                break;
            }
        }
    } else {
        m_protocolCombo->setCurrentIndex(0);  // TCP
        
        // 加载TCP参数
    m_ipEdit->setText(m_originalConfig.value("ip").toString());
    m_portSpin->setValue(m_originalConfig.value("port").toInt(502));
    }
    
    // 加载通用参数
    m_unitIdSpin->setValue(m_originalConfig.value("unitId").toInt(1));
    m_pollRateSpin->setValue(m_originalConfig.value("pollRate").toInt(1000));
    m_timeoutSpin->setValue(m_originalConfig.value("timeout").toInt(3000));
    m_maxRetriesSpin->setValue(m_originalConfig.value("maxRetries").toInt(3));
    m_autoReconnectCheck->setChecked(m_originalConfig.value("autoReconnect").toBool(true));
    m_logErrorsCheck->setChecked(m_originalConfig.value("logErrors").toBool(true));
    
    // 加载映射规则
    m_mappings.clear();
    QJsonArray mappingsArray = m_originalConfig.value("mappings").toArray();
    
    for (const QJsonValue& value : mappingsArray) {
        if (!value.isObject()) continue;
        
        QJsonObject mappingObj = value.toObject();
        CollectorMappingRule rule;
        
        rule.tagName = mappingObj.value("tagName").toString();
        rule.comment = mappingObj.value("comment").toString();
        rule.registerType = DataTypeUtils::registerTypeFromString(
            mappingObj.value("registerType").toString("Holding"));
        rule.address = mappingObj.value("address").toInt(0);
        rule.count = mappingObj.value("count").toInt(0);
        rule.dataType = DataTypeUtils::dataTypeFromString(
            mappingObj.value("dataType").toString("UInt16"));
        rule.byteOrder = DataTypeUtils::byteOrderFromString(
            mappingObj.value("byteOrder").toString("AB"));
        rule.scale = mappingObj.value("scale").toDouble(1.0);
        rule.offset = mappingObj.value("offset").toDouble(0.0);
        rule.unit = mappingObj.value("unit").toString();
        rule.enabled = mappingObj.value("enabled").toBool(true);
        
        m_mappings.append(rule);
    }
    
    refreshMappingTable();
}

bool CollectorConfigDialog::validateConfig() {
    // 验证名称
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("采集器名称不能为空"));
        m_nameEdit->setFocus();
        return false;
    }
    
    // 根据协议类型验证
    QString protocol = m_protocolCombo->currentData().toString();
    if (protocol == "modbus-tcp") {
    // 验证IP地址
    QString ip = m_ipEdit->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("IP地址不能为空"));
        m_ipEdit->setFocus();
        return false;
        }
    } else if (protocol == "modbus-rtu") {
        // 验证串口
        QString serialPort = m_serialPortCombo->currentText().trimmed();
        if (serialPort.isEmpty()) {
            QMessageBox::warning(this, tr("验证失败"), tr("串口不能为空"));
            m_serialPortCombo->setFocus();
            return false;
        }
    }
    
    // 验证映射规则
    for (int i = 0; i < m_mappings.size(); ++i) {
        const CollectorMappingRule& rule = m_mappings[i];
        if (rule.tagName.trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("验证失败"), 
                tr("映射规则 #%1 的标签名不能为空").arg(i + 1));
            m_mappingTable->selectRow(i);
            return false;
        }
    }
    
    return true;
}

QJsonObject CollectorConfigDialog::getConfig() const {
    QJsonObject config;
    
    // 基本信息
    config["name"] = m_nameEdit->text().trimmed();
    
    // 协议类型
    QString protocol = m_protocolCombo->currentData().toString();
    config["protocol"] = protocol;
    
    if (protocol == "modbus-rtu") {
        // RTU参数
        config["serialPort"] = m_serialPortCombo->currentText();
        config["baudRate"] = m_baudRateCombo->currentText().toInt();
        config["dataBits"] = m_dataBitsCombo->currentText().toInt();
        config["parity"] = m_parityCombo->currentData().toString();
        config["stopBits"] = m_stopBitsCombo->currentText().toInt();
    } else {
        // TCP参数
    config["ip"] = m_ipEdit->text().trimmed();
    config["port"] = m_portSpin->value();
    }
    
    // 通用参数
    config["unitId"] = m_unitIdSpin->value();
    config["pollRate"] = m_pollRateSpin->value();
    config["timeout"] = m_timeoutSpin->value();
    config["maxRetries"] = m_maxRetriesSpin->value();
    config["autoReconnect"] = m_autoReconnectCheck->isChecked();
    config["logErrors"] = m_logErrorsCheck->isChecked();
    
    // 映射规则
    QJsonArray mappingsArray;
    for (const CollectorMappingRule& rule : m_mappings) {
        QJsonObject mappingObj;
        mappingObj["tagName"] = rule.tagName;
        mappingObj["comment"] = rule.comment;
        mappingObj["registerType"] = DataTypeUtils::registerTypeToString(rule.registerType);
        mappingObj["address"] = rule.address;
        mappingObj["count"] = rule.count > 0 ? rule.count : DataTypeUtils::getRegisterCount(rule.dataType);
        mappingObj["dataType"] = DataTypeUtils::dataTypeToString(rule.dataType);
        mappingObj["byteOrder"] = DataTypeUtils::byteOrderToString(rule.byteOrder);
        mappingObj["scale"] = rule.scale;
        mappingObj["offset"] = rule.offset;
        mappingObj["unit"] = rule.unit;
        mappingObj["enabled"] = rule.enabled;
        
        mappingsArray.append(mappingObj);
    }
    config["mappings"] = mappingsArray;
    
    return config;
}

void CollectorConfigDialog::setConfig(const QJsonObject& config) {
    m_originalConfig = config;
    m_isNewCollector = false;
    loadConfig();
}

void CollectorConfigDialog::refreshMappingTable() {
    m_isUpdatingTable = true;
    
    m_mappingTable->setRowCount(0);
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

void CollectorConfigDialog::setupMappingTableRow(int row, const CollectorMappingRule& rule) {
    // 0: 行号（不可编辑）
    QTableWidgetItem* rowNumItem = new QTableWidgetItem(QString::number(row + 1));
    rowNumItem->setTextAlignment(Qt::AlignCenter);
    rowNumItem->setFlags(rowNumItem->flags() & ~Qt::ItemIsEditable);
    rowNumItem->setBackground(QBrush(QColor(240, 240, 240)));
    m_mappingTable->setItem(row, 0, rowNumItem);
    
    // 1: 标签名（可编辑文本）
    QTableWidgetItem* tagItem = new QTableWidgetItem(rule.tagName);
    m_mappingTable->setItem(row, 1, tagItem);
    
    // 2: 注释（可编辑文本）
    QTableWidgetItem* commentItem = new QTableWidgetItem(rule.comment);
    m_mappingTable->setItem(row, 2, commentItem);
    
    // 3: 寄存器类型（下拉框）- 使用功能码
    QComboBox* regTypeCombo = new QComboBox();
    regTypeCombo->addItem("01 (Coil)", static_cast<int>(RegisterType::Coil));
    regTypeCombo->addItem("02 (Discrete)", static_cast<int>(RegisterType::DiscreteInput));
    regTypeCombo->addItem("03 (Holding)", static_cast<int>(RegisterType::HoldingRegister));
    regTypeCombo->addItem("04 (Input)", static_cast<int>(RegisterType::InputRegister));
    
    // 根据枚举值设置当前项
    int currentTypeValue = static_cast<int>(rule.registerType);
    for (int i = 0; i < regTypeCombo->count(); ++i) {
        if (regTypeCombo->itemData(i).toInt() == currentTypeValue) {
            regTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    m_mappingTable->setCellWidget(row, 3, regTypeCombo);
    
    // 4: 地址（可编辑数字）
    QTableWidgetItem* addrItem = new QTableWidgetItem(QString::number(rule.address));
    addrItem->setTextAlignment(Qt::AlignCenter);
    m_mappingTable->setItem(row, 4, addrItem);
    
    // 5: 数据类型（下拉框）
    QComboBox* dataTypeCombo = new QComboBox();
    dataTypeCombo->addItem("UInt16", static_cast<int>(DataType::UInt16));
    dataTypeCombo->addItem("Int16", static_cast<int>(DataType::Int16));
    dataTypeCombo->addItem("UInt32", static_cast<int>(DataType::UInt32));
    dataTypeCombo->addItem("Int32", static_cast<int>(DataType::Int32));
    dataTypeCombo->addItem("Float32", static_cast<int>(DataType::Float32));
    dataTypeCombo->addItem("UInt64", static_cast<int>(DataType::UInt64));
    dataTypeCombo->addItem("Int64", static_cast<int>(DataType::Int64));
    dataTypeCombo->addItem("Float64", static_cast<int>(DataType::Float64));
    
    // 根据枚举值设置当前项
    int currentDataTypeValue = static_cast<int>(rule.dataType);
    for (int i = 0; i < dataTypeCombo->count(); ++i) {
        if (dataTypeCombo->itemData(i).toInt() == currentDataTypeValue) {
            dataTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    m_mappingTable->setCellWidget(row, 5, dataTypeCombo);
    
    // 6: 字节序（下拉框）
    QComboBox* byteOrderCombo = new QComboBox();
    byteOrderCombo->addItem("AB", static_cast<int>(ByteOrder::AB));
    byteOrderCombo->addItem("BA", static_cast<int>(ByteOrder::BA));
    byteOrderCombo->addItem("ABCD", static_cast<int>(ByteOrder::ABCD));
    byteOrderCombo->addItem("DCBA", static_cast<int>(ByteOrder::DCBA));
    byteOrderCombo->addItem("CDAB", static_cast<int>(ByteOrder::CDAB));
    byteOrderCombo->addItem("BADC", static_cast<int>(ByteOrder::BADC));
    
    // 根据枚举值设置当前项
    int currentByteOrderValue = static_cast<int>(rule.byteOrder);
    for (int i = 0; i < byteOrderCombo->count(); ++i) {
        if (byteOrderCombo->itemData(i).toInt() == currentByteOrderValue) {
            byteOrderCombo->setCurrentIndex(i);
            break;
        }
    }
    m_mappingTable->setCellWidget(row, 6, byteOrderCombo);
    
    // 7: 倍率（可编辑数字）
    QTableWidgetItem* scaleItem = new QTableWidgetItem(QString::number(rule.scale));
    scaleItem->setTextAlignment(Qt::AlignCenter);
    m_mappingTable->setItem(row, 7, scaleItem);
    
    // 8: 偏移（可编辑数字）
    QTableWidgetItem* offsetItem = new QTableWidgetItem(QString::number(rule.offset));
    offsetItem->setTextAlignment(Qt::AlignCenter);
    m_mappingTable->setItem(row, 8, offsetItem);
    
    // 9: 单位（可编辑文本）
    QTableWidgetItem* unitItem = new QTableWidgetItem(rule.unit);
    unitItem->setTextAlignment(Qt::AlignCenter);
    m_mappingTable->setItem(row, 9, unitItem);
    
    // 10: 启用（复选框）
    QCheckBox* enabledCheck = new QCheckBox();
    enabledCheck->setChecked(rule.enabled);
    QWidget* checkWidget = new QWidget();
    QHBoxLayout* checkLayout = new QHBoxLayout(checkWidget);
    checkLayout->addWidget(enabledCheck);
    checkLayout->setAlignment(Qt::AlignCenter);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    m_mappingTable->setCellWidget(row, 10, checkWidget);
}

CollectorMappingRule CollectorConfigDialog::getMappingRuleFromRow(int row) const {
    CollectorMappingRule rule;
    
    // 跳过第0列（行号）
    
    // 标签名
    QTableWidgetItem* tagItem = m_mappingTable->item(row, 1);
    rule.tagName = tagItem ? tagItem->text() : "";
    
    // 注释
    QTableWidgetItem* commentItem = m_mappingTable->item(row, 2);
    rule.comment = commentItem ? commentItem->text() : "";
    
    // 寄存器类型
    QComboBox* regTypeCombo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 3));
    rule.registerType = regTypeCombo ? 
        static_cast<RegisterType>(regTypeCombo->currentData().toInt()) : 
        RegisterType::HoldingRegister;
    
    // 地址
    QTableWidgetItem* addrItem = m_mappingTable->item(row, 4);
    rule.address = addrItem ? addrItem->text().toInt() : 0;
    
    // 数据类型
    QComboBox* dataTypeCombo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 5));
    rule.dataType = dataTypeCombo ? 
        static_cast<DataType>(dataTypeCombo->currentData().toInt()) : 
        DataType::UInt16;
    
    // 字节序
    QComboBox* byteOrderCombo = qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 6));
    rule.byteOrder = byteOrderCombo ? 
        static_cast<ByteOrder>(byteOrderCombo->currentData().toInt()) : 
        ByteOrder::AB;
    
    // 倍率
    QTableWidgetItem* scaleItem = m_mappingTable->item(row, 7);
    rule.scale = scaleItem ? scaleItem->text().toDouble() : 1.0;
    
    // 偏移
    QTableWidgetItem* offsetItem = m_mappingTable->item(row, 8);
    rule.offset = offsetItem ? offsetItem->text().toDouble() : 0.0;
    
    // 单位
    QTableWidgetItem* unitItem = m_mappingTable->item(row, 9);
    rule.unit = unitItem ? unitItem->text() : "";
    
    // 启用
    QWidget* checkWidget = m_mappingTable->cellWidget(row, 10);
    QCheckBox* enabledCheck = checkWidget ? checkWidget->findChild<QCheckBox*>() : nullptr;
    rule.enabled = enabledCheck ? enabledCheck->isChecked() : true;
    
    // 自动计算count
    rule.count = DataTypeUtils::getRegisterCount(rule.dataType);
    
    return rule;
}

void CollectorConfigDialog::updateMappingButtons() {
    bool hasSelection = m_mappingTable->currentRow() >= 0;
    bool hasRows = m_mappings.size() > 0;
    
    m_deleteMappingBtn->setEnabled(hasSelection);
    m_duplicateMappingBtn->setEnabled(hasSelection);
    m_exportCsvBtn->setEnabled(hasRows);
}

// ========== 槽函数实现 ==========

void CollectorConfigDialog::onNameChanged(const QString& text) {
    Q_UNUSED(text);
}

void CollectorConfigDialog::onIpChanged(const QString& text) {
    Q_UNUSED(text);
}

void CollectorConfigDialog::onAddMapping() {
    // 创建默认映射规则
    CollectorMappingRule rule;
    rule.tagName = tr("NewTag%1").arg(m_mappings.size() + 1);  // 自动编号
    rule.registerType = RegisterType::HoldingRegister;
    rule.dataType = DataType::UInt16;
    rule.byteOrder = ByteOrder::AB;
    rule.scale = 1.0;
    rule.offset = 0.0;
    rule.enabled = true;
    
    // 智能地址递增：基于上一条映射的地址和数据类型
    if (!m_mappings.isEmpty()) {
        const CollectorMappingRule& lastRule = m_mappings.last();
        // 下一个地址 = 上一个地址 + 上一个数据类型占用的寄存器数
        int lastRegisterCount = DataTypeUtils::getRegisterCount(lastRule.dataType);
        rule.address = lastRule.address + lastRegisterCount;
    } else {
        rule.address = 0;  // 第一条从0开始
    }
    
    m_mappings.append(rule);
    refreshMappingTable();
    
    // 选中新添加的行
    m_mappingTable->selectRow(m_mappings.size() - 1);
    
    // 自动聚焦到标签名单元格（注意列索引改为1）
    m_mappingTable->editItem(m_mappingTable->item(m_mappings.size() - 1, 1));
}

void CollectorConfigDialog::onDeleteMapping() {
    int row = m_mappingTable->currentRow();
    if (row < 0 || row >= m_mappings.size()) {
        return;
    }
    
    m_mappings.removeAt(row);
    refreshMappingTable();
}

void CollectorConfigDialog::onDuplicateMapping() {
    int row = m_mappingTable->currentRow();
    if (row < 0 || row >= m_mappings.size()) {
        return;
    }
    
    // 复制当前行的映射规则
    CollectorMappingRule rule = getMappingRuleFromRow(row);
    
    // 智能命名：在原标签名后加序号
    rule.tagName = QString("%1_Copy%2").arg(rule.tagName).arg(m_mappings.size() + 1);
    
    // 智能地址递增：基于最后一条的地址
    if (!m_mappings.isEmpty()) {
        const CollectorMappingRule& lastRule = m_mappings.last();
        int lastRegisterCount = DataTypeUtils::getRegisterCount(lastRule.dataType);
        rule.address = lastRule.address + lastRegisterCount;
    }
    
    m_mappings.append(rule);
    refreshMappingTable();
    
    // 选中新复制的行
    m_mappingTable->selectRow(m_mappings.size() - 1);
}

void CollectorConfigDialog::onMappingTableSelectionChanged() {
    updateMappingButtons();
}

void CollectorConfigDialog::onMappingTableCellChanged(int row, int column) {
    if (m_isUpdatingTable) {
        return;
    }
    
    // 当单元格内容变化时，更新对应的映射规则
    if (row >= 0 && row < m_mappings.size()) {
        m_mappings[row] = getMappingRuleFromRow(row);
    }
}

void CollectorConfigDialog::onImportFromCSV() {
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
            "collector_mapping_template.csv",
            tr("CSV文件 (*.csv)")
        );
        
        if (!fileName.isEmpty()) {
            if (CsvHelper::generateCollectorTemplate(fileName)) {
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
        QList<CollectorMappingRule> importedMappings;
        QString errorMsg;
        
        if (!CsvHelper::importCollectorMappings(fileName, importedMappings, errorMsg)) {
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

void CollectorConfigDialog::onExportToCSV() {
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
        QString("collector_mappings_%1.csv").arg(m_nameEdit->text()),
        tr("CSV文件 (*.csv)")
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    if (CsvHelper::exportCollectorMappings(m_mappings, fileName)) {
        QMessageBox::information(this, tr("导出成功"), 
            tr("成功导出 %1 条映射规则到：\n%2").arg(m_mappings.size()).arg(fileName));
    } else {
        QMessageBox::critical(this, tr("导出失败"), 
            tr("无法导出CSV文件。"));
    }
}

void CollectorConfigDialog::onLoadFromTemplate() {
    // 如果已有映射规则，询问是否替换
    if (!m_mappings.isEmpty()) {
        int ret = QMessageBox::question(this, tr("加载模板"),
            tr("当前已有 %1 条映射规则。\n\n选择操作：\n"
               "• 是(Y) - 替换现有规则\n"
               "• 否(N) - 追加到现有规则\n"
               "• 取消 - 不加载").arg(m_mappings.size()),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        
        if (ret == QMessageBox::Cancel) {
            return;
        }
        
        TemplateSelectDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted || !dialog.hasSelection()) {
            return;
        }
        
        DeviceTemplate tmpl = dialog.selectedTemplate();
        
        if (ret == QMessageBox::Yes) {
            // 替换模式
            m_mappings = tmpl.mappings;
        } else {
            // 追加模式
            m_mappings.append(tmpl.mappings);
        }
        
        // 应用模板的默认配置
        applyTemplateDefaults(tmpl);
        
        refreshMappingTable();
        
        QMessageBox::information(this, tr("模板已加载"),
            tr("已从模板 \"%1\" 加载 %2 条映射规则。\n\n"
               "请根据实际设备调整 IP 地址等参数。")
            .arg(tmpl.name)
            .arg(tmpl.mappings.size()));
    } else {
        // 没有现有规则，直接加载
        TemplateSelectDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted || !dialog.hasSelection()) {
            return;
        }
        
        DeviceTemplate tmpl = dialog.selectedTemplate();
        m_mappings = tmpl.mappings;
        
        // 应用模板的默认配置
        applyTemplateDefaults(tmpl);
        
        refreshMappingTable();
        
        QMessageBox::information(this, tr("模板已加载"),
            tr("已从模板 \"%1\" 加载 %2 条映射规则。\n\n"
               "请根据实际设备调整 IP 地址等参数。")
            .arg(tmpl.name)
            .arg(tmpl.mappings.size()));
    }
}

void CollectorConfigDialog::applyTemplateDefaults(const DeviceTemplate& tmpl) {
    // 设置协议
    if (tmpl.protocol == "modbus-rtu") {
        m_protocolCombo->setCurrentIndex(1);
    } else {
        m_protocolCombo->setCurrentIndex(0);
    }
    
    // 设置端口
    m_portSpin->setValue(tmpl.port);
    
    // 设置从站ID
    m_unitIdSpin->setValue(tmpl.unitId);
    
    // 设置采集周期
    m_pollRateSpin->setValue(tmpl.pollRate);
    
    // 设置超时
    m_timeoutSpin->setValue(tmpl.timeout);
    
    // 如果名称还是默认的，设置为设备名称
    if (m_nameEdit->text() == tr("新采集器") || m_nameEdit->text().isEmpty()) {
        m_nameEdit->setText(tmpl.name);
    }
}

void CollectorConfigDialog::onAccepted() {
    // 同步表格数据到m_mappings
    for (int i = 0; i < m_mappingTable->rowCount(); ++i) {
        if (i < m_mappings.size()) {
            m_mappings[i] = getMappingRuleFromRow(i);
        }
    }
    
    if (validateConfig()) {
        accept();
    }
}

void CollectorConfigDialog::onRejected() {
    reject();
}

} // namespace ModbusPlexLink

