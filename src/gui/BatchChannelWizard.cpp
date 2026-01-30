#include "BatchChannelWizard.h"
#include "core/ChannelManager.h"
#include "core/Channel.h"
#include "DialogStyles.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace ModbusPlexLink {

// ============ BatchChannelWizard ============

BatchChannelWizard::BatchChannelWizard(ChannelManager* channelManager, QWidget* parent)
    : QWizard(parent)
    , m_channelManager(channelManager)
    , m_startIndex(1)
    , m_createdCount(0)
{
    setWindowTitle(tr("批量创建通道向导"));
    setWizardStyle(QWizard::ModernStyle);
    resize(700, 550);
    
    setStyleSheet(DialogStyles::getDialogStyle());
    
    addPage(createIntroPage());
    addPage(createTemplatePage());
    addPage(createDeviceListPage());
    addPage(createNamingPage());
    addPage(createConfirmPage());
    addPage(createResultPage());
    
    setButtonText(QWizard::NextButton, tr("下一步 >"));
    setButtonText(QWizard::BackButton, tr("< 上一步"));
    setButtonText(QWizard::FinishButton, tr("完成"));
    setButtonText(QWizard::CancelButton, tr("取消"));
}

QWizardPage* BatchChannelWizard::createIntroPage() {
    return new IntroPage(this);
}

QWizardPage* BatchChannelWizard::createTemplatePage() {
    return new TemplatePage(m_channelManager, this);
}

QWizardPage* BatchChannelWizard::createDeviceListPage() {
    return new DeviceListPage(this);
}

QWizardPage* BatchChannelWizard::createNamingPage() {
    return new NamingPage(this);
}

QWizardPage* BatchChannelWizard::createConfirmPage() {
    return new ConfirmPage(this);
}

QWizardPage* BatchChannelWizard::createResultPage() {
    return new ResultPage(this, this);
}

void BatchChannelWizard::createChannels() {
    m_createdCount = 0;
    m_createdChannelNames.clear();
    m_failedChannels.clear();
    
    // 获取模板配置
    TemplatePage* templatePage = qobject_cast<TemplatePage*>(page(1));
    m_templateConfig = templatePage->getTemplateConfig();
    
    // 获取设备列表
    DeviceListPage* devicePage = qobject_cast<DeviceListPage*>(page(2));
    m_devices = devicePage->getDevices();
    
    // 获取命名规则
    NamingPage* namingPage = qobject_cast<NamingPage*>(page(3));
    m_namePrefix = namingPage->getNamePrefix();
    m_nameTemplate = namingPage->getNameTemplate();
    m_startIndex = namingPage->getStartIndex();
    
    // 批量创建
    for (int i = 0; i < m_devices.size(); ++i) {
        const DeviceInfo& device = m_devices[i];
        
        // 生成通道名称
        QString channelName = generateChannelName(m_nameTemplate, i, device);
        
        // 检查名称是否已存在
        if (m_channelManager->getChannel(channelName)) {
            m_failedChannels.append(tr("%1 (名称已存在)").arg(channelName));
            continue;
        }
        
        // 复制模板配置并修改IP
        QJsonObject channelConfig = m_templateConfig;
        channelConfig["name"] = channelName;
        
        // 修改采集器的IP地址
        QJsonArray collectors = channelConfig["collectors"].toArray();
        for (int j = 0; j < collectors.size(); ++j) {
            QJsonObject collector = collectors[j].toObject();
            collector["ip"] = device.ip;
            collector["port"] = device.port;
            collector["unitId"] = device.unitId;
            collectors[j] = collector;
        }
        channelConfig["collectors"] = collectors;
        
        // 创建通道
        Channel* channel = m_channelManager->createChannel(channelName);
        if (channel && channel->configure(channelConfig)) {
            m_createdCount++;
            m_createdChannelNames.append(channelName);
        } else {
            m_failedChannels.append(tr("%1 (创建失败)").arg(channelName));
            if (channel) {
                m_channelManager->deleteChannel(channelName);
            }
        }
    }
    
    // 保存配置
    if (m_createdCount > 0) {
        m_channelManager->saveConfig("channels.json");
    }
}

QString BatchChannelWizard::generateChannelName(const QString& nameTemplate, int index, const DeviceInfo& device) {
    QString name = nameTemplate;
    
    // 替换变量
    name.replace("{prefix}", m_namePrefix);
    name.replace("{index}", QString::number(m_startIndex + index));
    
    // 格式化索引（带前导零）
    NamingPage* namingPage = qobject_cast<NamingPage*>(page(3));
    int digits = 3; // 默认3位
    if (namingPage) {
        // 从模板中解析位数
        QRegularExpression re("\\{index:(\\d+)d\\}");
        QRegularExpressionMatch match = re.match(nameTemplate);
        if (match.hasMatch()) {
            digits = match.captured(1).toInt();
            name.replace(match.captured(0), QString("%1").arg(m_startIndex + index, digits, 10, QChar('0')));
        }
    }
    
    // 替换IP相关变量
    name.replace("{ip}", device.ip);
    name.replace("{ip_last}", device.ip.section('.', -1));
    name.replace("{port}", QString::number(device.port));
    name.replace("{unitId}", QString::number(device.unitId));
    
    return name;
}

// ============ IntroPage ============

IntroPage::IntroPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("批量创建通道"));
    setSubTitle(tr("此向导将帮助您批量创建多个配置相同但IP不同的采集通道"));
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    QLabel* descLabel = new QLabel(this);
    descLabel->setWordWrap(true);
    descLabel->setText(tr(
        "<h3>📦 批量创建通道向导</h3>"
        "<p>当您有多台相同型号的设备需要采集时，可以使用此向导快速创建多个通道。</p>"
        "<p><b>使用场景示例：</b></p>"
        "<ul>"
        "<li>100台电表，点表相同，只是IP地址不同</li>"
        "<li>多台PLC设备，寄存器配置相同</li>"
        "<li>同一厂家的多台传感器</li>"
        "</ul>"
        "<p><b>向导步骤：</b></p>"
        "<ol>"
        "<li>选择模板通道或导入配置文件</li>"
        "<li>输入设备IP地址列表</li>"
        "<li>配置通道命名规则</li>"
        "<li>确认并创建</li>"
        "</ol>"
    ));
    layout->addWidget(descLabel);
    layout->addStretch();
}

// ============ TemplatePage ============

TemplatePage::TemplatePage(ChannelManager* channelManager, QWidget* parent)
    : QWizardPage(parent)
    , m_channelManager(channelManager)
{
    setTitle(tr("选择模板"));
    setSubTitle(tr("选择一个现有通道作为模板，或导入配置文件"));
    
    setupUi();
    loadTemplateChannels();
}

void TemplatePage::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // 模板来源选择
    QGroupBox* sourceGroup = new QGroupBox(tr("模板来源"), this);
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceGroup);
    
    m_useExistingRadio = new QRadioButton(tr("使用现有通道作为模板"), sourceGroup);
    m_useExistingRadio->setChecked(true);
    connect(m_useExistingRadio, &QRadioButton::toggled, this, &TemplatePage::onTemplateSourceChanged);
    sourceLayout->addWidget(m_useExistingRadio);
    
    QHBoxLayout* existingLayout = new QHBoxLayout();
    existingLayout->addSpacing(20);
    m_templateChannelCombo = new QComboBox(sourceGroup);
    m_templateChannelCombo->setMinimumWidth(200);
    connect(m_templateChannelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TemplatePage::onTemplateChannelChanged);
    existingLayout->addWidget(m_templateChannelCombo);
    existingLayout->addStretch();
    sourceLayout->addLayout(existingLayout);
    
    m_useFileRadio = new QRadioButton(tr("从配置文件导入"), sourceGroup);
    connect(m_useFileRadio, &QRadioButton::toggled, this, &TemplatePage::onTemplateSourceChanged);
    sourceLayout->addWidget(m_useFileRadio);
    
    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->addSpacing(20);
    m_templateFileEdit = new QLineEdit(sourceGroup);
    m_templateFileEdit->setPlaceholderText(tr("选择配置文件..."));
    m_templateFileEdit->setEnabled(false);
    fileLayout->addWidget(m_templateFileEdit, 1);
    m_browseBtn = new QPushButton(tr("浏览..."), sourceGroup);
    m_browseBtn->setEnabled(false);
    connect(m_browseBtn, &QPushButton::clicked, this, [this]() {
        QString filename = QFileDialog::getOpenFileName(this, tr("选择配置文件"),
            QString(), tr("JSON文件 (*.json)"));
        if (!filename.isEmpty()) {
            m_templateFileEdit->setText(filename);
        }
    });
    fileLayout->addWidget(m_browseBtn);
    sourceLayout->addLayout(fileLayout);
    
    layout->addWidget(sourceGroup);
    
    // 预览
    QGroupBox* previewGroup = new QGroupBox(tr("配置预览"), this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewEdit = new QTextEdit(previewGroup);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(200);
    previewLayout->addWidget(m_previewEdit);
    
    layout->addWidget(previewGroup);
    layout->addStretch();
}

void TemplatePage::loadTemplateChannels() {
    m_templateChannelCombo->clear();
    
    if (!m_channelManager) return;
    
    for (const QString& name : m_channelManager->getChannelNames()) {
        Channel* channel = m_channelManager->getChannel(name);
        if (channel && channel->getType() == ChannelType::Collector) {
            m_templateChannelCombo->addItem(name, name);
        }
    }
    
    if (m_templateChannelCombo->count() == 0) {
        m_templateChannelCombo->addItem(tr("(无可用的采集通道)"), "");
    }
}

void TemplatePage::onTemplateSourceChanged() {
    bool useExisting = m_useExistingRadio->isChecked();
    m_templateChannelCombo->setEnabled(useExisting);
    m_templateFileEdit->setEnabled(!useExisting);
    m_browseBtn->setEnabled(!useExisting);
    
    if (useExisting) {
        onTemplateChannelChanged(m_templateChannelCombo->currentIndex());
    } else {
        m_previewEdit->clear();
    }
}

void TemplatePage::onTemplateChannelChanged(int index) {
    Q_UNUSED(index);
    
    QString channelName = m_templateChannelCombo->currentData().toString();
    if (channelName.isEmpty()) {
        m_previewEdit->setText(tr("请选择一个有效的通道"));
        return;
    }
    
    Channel* channel = m_channelManager->getChannel(channelName);
    if (!channel) {
        m_previewEdit->setText(tr("通道不存在"));
        return;
    }
    
    ChannelConfig config = channel->getConfig();
    
    QString preview;
    preview += tr("通道名称: %1\n").arg(config.name);
    preview += tr("采集器数量: %1\n").arg(config.collectors.size());
    
    for (const QJsonObject& collector : config.collectors) {
        preview += tr("\n  采集器: %1\n").arg(collector["name"].toString());
        preview += tr("    类型: %1\n").arg(collector["type"].toString());
        preview += tr("    IP: %1:%2\n").arg(collector["ip"].toString()).arg(collector["port"].toInt());
        
        QJsonArray mappings = collector["mappings"].toArray();
        preview += tr("    映射点数: %1\n").arg(mappings.size());
    }
    
    m_previewEdit->setText(preview);
}

bool TemplatePage::validatePage() {
    if (m_useExistingRadio->isChecked()) {
        QString channelName = m_templateChannelCombo->currentData().toString();
        if (channelName.isEmpty()) {
            QMessageBox::warning(this, tr("验证失败"), tr("请选择一个有效的模板通道"));
            return false;
        }
    } else {
        if (m_templateFileEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("验证失败"), tr("请选择配置文件"));
            return false;
        }
    }
    return true;
}

QJsonObject TemplatePage::getTemplateConfig() const {
    if (m_useExistingRadio->isChecked()) {
        QString channelName = m_templateChannelCombo->currentData().toString();
        Channel* channel = m_channelManager->getChannel(channelName);
        if (channel) {
            ChannelConfig config = channel->getConfig();
            QJsonObject json;
            json["name"] = config.name;
            json["type"] = "collector";
            json["enabled"] = config.enabled;
            
            QJsonArray collectors;
            for (const QJsonObject& c : config.collectors) {
                collectors.append(c);
            }
            json["collectors"] = collectors;
            
            return json;
        }
    } else {
        QFile file(m_templateFileEdit->text());
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            return doc.object();
        }
    }
    return QJsonObject();
}

// ============ DeviceListPage ============

DeviceListPage::DeviceListPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("设备列表"));
    setSubTitle(tr("输入要创建通道的设备IP地址列表"));
    
    setupUi();
}

void DeviceListPage::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // 输入方式选择
    QGroupBox* methodGroup = new QGroupBox(tr("输入方式"), this);
    QHBoxLayout* methodLayout = new QHBoxLayout(methodGroup);
    
    m_manualRadio = new QRadioButton(tr("手动输入"), methodGroup);
    m_manualRadio->setChecked(true);
    connect(m_manualRadio, &QRadioButton::toggled, this, &DeviceListPage::onInputMethodChanged);
    methodLayout->addWidget(m_manualRadio);
    
    m_rangeRadio = new QRadioButton(tr("IP范围生成"), methodGroup);
    connect(m_rangeRadio, &QRadioButton::toggled, this, &DeviceListPage::onInputMethodChanged);
    methodLayout->addWidget(m_rangeRadio);
    
    m_csvRadio = new QRadioButton(tr("CSV导入"), methodGroup);
    connect(m_csvRadio, &QRadioButton::toggled, this, &DeviceListPage::onInputMethodChanged);
    methodLayout->addWidget(m_csvRadio);
    
    methodLayout->addStretch();
    layout->addWidget(methodGroup);
    
    // IP范围生成区域
    QGroupBox* rangeGroup = new QGroupBox(tr("IP范围设置"), this);
    rangeGroup->setVisible(false);
    QFormLayout* rangeLayout = new QFormLayout(rangeGroup);
    
    m_baseIpEdit = new QLineEdit(rangeGroup);
    m_baseIpEdit->setPlaceholderText(tr("例如: 192.168.1."));
    m_baseIpEdit->setText("192.168.1.");
    rangeLayout->addRow(tr("基础IP:"), m_baseIpEdit);
    
    QHBoxLayout* rangeNumLayout = new QHBoxLayout();
    m_startIpSpin = new QSpinBox(rangeGroup);
    m_startIpSpin->setRange(1, 254);
    m_startIpSpin->setValue(100);
    rangeNumLayout->addWidget(m_startIpSpin);
    rangeNumLayout->addWidget(new QLabel(tr("到"), rangeGroup));
    m_endIpSpin = new QSpinBox(rangeGroup);
    m_endIpSpin->setRange(1, 254);
    m_endIpSpin->setValue(110);
    rangeNumLayout->addWidget(m_endIpSpin);
    rangeLayout->addRow(tr("IP范围:"), rangeNumLayout);
    
    m_portSpin = new QSpinBox(rangeGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(502);
    rangeLayout->addRow(tr("端口:"), m_portSpin);
    
    m_unitIdSpin = new QSpinBox(rangeGroup);
    m_unitIdSpin->setRange(1, 247);
    m_unitIdSpin->setValue(1);
    rangeLayout->addRow(tr("Unit ID:"), m_unitIdSpin);
    
    m_generateBtn = new QPushButton(tr("生成设备列表"), rangeGroup);
    connect(m_generateBtn, &QPushButton::clicked, this, &DeviceListPage::onGenerateDevices);
    rangeLayout->addRow("", m_generateBtn);
    
    layout->addWidget(rangeGroup);
    
    // CSV导入区域
    QGroupBox* csvGroup = new QGroupBox(tr("CSV文件导入"), this);
    csvGroup->setVisible(false);
    QHBoxLayout* csvLayout = new QHBoxLayout(csvGroup);
    
    m_csvFileEdit = new QLineEdit(csvGroup);
    m_csvFileEdit->setPlaceholderText(tr("选择CSV文件..."));
    csvLayout->addWidget(m_csvFileEdit, 1);
    
    m_importBtn = new QPushButton(tr("导入"), csvGroup);
    connect(m_importBtn, &QPushButton::clicked, this, &DeviceListPage::onImportCsv);
    csvLayout->addWidget(m_importBtn);
    
    layout->addWidget(csvGroup);
    
    // 设备表格
    QGroupBox* tableGroup = new QGroupBox(tr("设备列表"), this);
    QVBoxLayout* tableLayout = new QVBoxLayout(tableGroup);
    
    // 按钮栏
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton(tr("➕ 添加"), tableGroup);
    connect(m_addBtn, &QPushButton::clicked, this, &DeviceListPage::onAddDevice);
    btnLayout->addWidget(m_addBtn);
    
    m_removeBtn = new QPushButton(tr("➖ 删除"), tableGroup);
    connect(m_removeBtn, &QPushButton::clicked, this, &DeviceListPage::onRemoveDevice);
    btnLayout->addWidget(m_removeBtn);
    
    m_clearBtn = new QPushButton(tr("🗑️ 清空"), tableGroup);
    connect(m_clearBtn, &QPushButton::clicked, this, &DeviceListPage::onClearDevices);
    btnLayout->addWidget(m_clearBtn);
    
    btnLayout->addStretch();
    
    m_countLabel = new QLabel(tr("共 0 个设备"), tableGroup);
    btnLayout->addWidget(m_countLabel);
    
    tableLayout->addLayout(btnLayout);
    
    m_deviceTable = new QTableWidget(tableGroup);
    m_deviceTable->setColumnCount(4);
    m_deviceTable->setHorizontalHeaderLabels({tr("IP地址"), tr("端口"), tr("Unit ID"), tr("描述")});
    m_deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_deviceTable->setColumnWidth(1, 80);
    m_deviceTable->setColumnWidth(2, 80);
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setAlternatingRowColors(true);
    tableLayout->addWidget(m_deviceTable);
    
    layout->addWidget(tableGroup, 1);
    
    // 保存对组的引用用于显示/隐藏
    connect(m_rangeRadio, &QRadioButton::toggled, rangeGroup, &QGroupBox::setVisible);
    connect(m_csvRadio, &QRadioButton::toggled, csvGroup, &QGroupBox::setVisible);
}

void DeviceListPage::onInputMethodChanged() {
    m_addBtn->setEnabled(m_manualRadio->isChecked());
}

void DeviceListPage::onGenerateDevices() {
    QString baseIp = m_baseIpEdit->text();
    int start = m_startIpSpin->value();
    int end = m_endIpSpin->value();
    int port = m_portSpin->value();
    int unitId = m_unitIdSpin->value();
    
    if (start > end) {
        QMessageBox::warning(this, tr("错误"), tr("起始IP必须小于等于结束IP"));
        return;
    }
    
    m_devices.clear();
    for (int i = start; i <= end; ++i) {
        DeviceInfo device;
        device.ip = baseIp + QString::number(i);
        device.port = port;
        device.unitId = unitId;
        device.description = tr("设备 %1").arg(i - start + 1);
        m_devices.append(device);
    }
    
    updateDeviceTable();
}

void DeviceListPage::onImportCsv() {
    QString filename = QFileDialog::getOpenFileName(this, tr("选择CSV文件"),
        QString(), tr("CSV文件 (*.csv)"));
    
    if (filename.isEmpty()) return;
    
    m_csvFileEdit->setText(filename);
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件"));
        return;
    }
    
    m_devices.clear();
    QTextStream in(&file);
    bool firstLine = true;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        // 跳过表头
        if (firstLine) {
            firstLine = false;
            if (line.toLower().contains("ip")) continue;
        }
        
        QStringList parts = line.split(',');
        if (parts.size() >= 1) {
            DeviceInfo device;
            device.ip = parts[0].trimmed();
            device.unitId = parts.size() > 1 ? parts[1].trimmed().toInt() : 1;
            device.port = parts.size() > 2 ? parts[2].trimmed().toInt() : 502;
            device.description = parts.size() > 3 ? parts[3].trimmed() : "";
            
            if (!device.ip.isEmpty()) {
                m_devices.append(device);
            }
        }
    }
    
    file.close();
    updateDeviceTable();
    
    QMessageBox::information(this, tr("导入成功"),
        tr("已导入 %1 个设备").arg(m_devices.size()));
}

void DeviceListPage::onAddDevice() {
    DeviceInfo device;
    device.ip = "192.168.1.1";
    device.port = 502;
    device.unitId = 1;
    m_devices.append(device);
    updateDeviceTable();
    
    // 选中新添加的行并开始编辑
    int row = m_deviceTable->rowCount() - 1;
    m_deviceTable->selectRow(row);
    m_deviceTable->editItem(m_deviceTable->item(row, 0));
}

void DeviceListPage::onRemoveDevice() {
    int row = m_deviceTable->currentRow();
    if (row >= 0 && row < m_devices.size()) {
        m_devices.removeAt(row);
        updateDeviceTable();
    }
}

void DeviceListPage::onClearDevices() {
    if (m_devices.isEmpty()) return;
    
    if (QMessageBox::question(this, tr("确认"), tr("确定要清空所有设备吗？"))
        == QMessageBox::Yes) {
        m_devices.clear();
        updateDeviceTable();
    }
}

void DeviceListPage::updateDeviceTable() {
    m_deviceTable->setRowCount(m_devices.size());
    
    for (int i = 0; i < m_devices.size(); ++i) {
        const DeviceInfo& device = m_devices[i];
        
        QTableWidgetItem* ipItem = new QTableWidgetItem(device.ip);
        QTableWidgetItem* portItem = new QTableWidgetItem(QString::number(device.port));
        QTableWidgetItem* unitIdItem = new QTableWidgetItem(QString::number(device.unitId));
        QTableWidgetItem* descItem = new QTableWidgetItem(device.description);
        
        m_deviceTable->setItem(i, 0, ipItem);
        m_deviceTable->setItem(i, 1, portItem);
        m_deviceTable->setItem(i, 2, unitIdItem);
        m_deviceTable->setItem(i, 3, descItem);
    }
    
    m_countLabel->setText(tr("共 %1 个设备").arg(m_devices.size()));
}

bool DeviceListPage::validatePage() {
    // 从表格同步数据
    m_devices.clear();
    for (int i = 0; i < m_deviceTable->rowCount(); ++i) {
        DeviceInfo device;
        device.ip = m_deviceTable->item(i, 0) ? m_deviceTable->item(i, 0)->text() : "";
        device.port = m_deviceTable->item(i, 1) ? m_deviceTable->item(i, 1)->text().toInt() : 502;
        device.unitId = m_deviceTable->item(i, 2) ? m_deviceTable->item(i, 2)->text().toInt() : 1;
        device.description = m_deviceTable->item(i, 3) ? m_deviceTable->item(i, 3)->text() : "";
        
        if (!device.ip.isEmpty()) {
            m_devices.append(device);
        }
    }
    
    if (m_devices.isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请至少添加一个设备"));
        return false;
    }
    
    return true;
}

QList<DeviceInfo> DeviceListPage::getDevices() const {
    return m_devices;
}

// ============ NamingPage ============

NamingPage::NamingPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("命名规则"));
    setSubTitle(tr("配置通道的命名规则"));
    
    setupUi();
}

void NamingPage::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    QGroupBox* namingGroup = new QGroupBox(tr("命名设置"), this);
    QFormLayout* formLayout = new QFormLayout(namingGroup);
    
    m_prefixEdit = new QLineEdit(namingGroup);
    m_prefixEdit->setText("Device");
    m_prefixEdit->setPlaceholderText(tr("通道名称前缀"));
    connect(m_prefixEdit, &QLineEdit::textChanged, this, &NamingPage::onPreviewUpdate);
    formLayout->addRow(tr("名称前缀:"), m_prefixEdit);
    
    m_patternCombo = new QComboBox(namingGroup);
    m_patternCombo->addItem(tr("{prefix}_{index:03d} (例: Device_001)"), "{prefix}_{index:03d}");
    m_patternCombo->addItem(tr("{prefix}_{ip_last} (例: Device_100)"), "{prefix}_{ip_last}");
    m_patternCombo->addItem(tr("{prefix}_{ip} (例: Device_192.168.1.100)"), "{prefix}_{ip}");
    m_patternCombo->addItem(tr("{prefix}_{index} (例: Device_1)"), "{prefix}_{index}");
    connect(m_patternCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NamingPage::onPreviewUpdate);
    formLayout->addRow(tr("命名模式:"), m_patternCombo);
    
    m_startIndexSpin = new QSpinBox(namingGroup);
    m_startIndexSpin->setRange(0, 9999);
    m_startIndexSpin->setValue(1);
    connect(m_startIndexSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NamingPage::onPreviewUpdate);
    formLayout->addRow(tr("起始索引:"), m_startIndexSpin);
    
    m_digitsSpin = new QSpinBox(namingGroup);
    m_digitsSpin->setRange(1, 6);
    m_digitsSpin->setValue(3);
    connect(m_digitsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NamingPage::onPreviewUpdate);
    formLayout->addRow(tr("索引位数:"), m_digitsSpin);
    
    layout->addWidget(namingGroup);
    
    // 预览
    QGroupBox* previewGroup = new QGroupBox(tr("名称预览"), this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewEdit = new QTextEdit(previewGroup);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(150);
    previewLayout->addWidget(m_previewEdit);
    
    layout->addWidget(previewGroup);
    layout->addStretch();
    
    onPreviewUpdate();
}

void NamingPage::onPreviewUpdate() {
    QString prefix = m_prefixEdit->text();
    QString pattern = m_patternCombo->currentData().toString();
    int startIndex = m_startIndexSpin->value();
    int digits = m_digitsSpin->value();
    
    // 替换位数
    pattern.replace("03d", QString("%1d").arg(digits));
    
    QString preview;
    preview += tr("命名模式: %1\n\n").arg(pattern);
    preview += tr("示例名称:\n");
    
    // 生成示例
    QStringList sampleIps = {"192.168.1.100", "192.168.1.101", "192.168.1.102"};
    for (int i = 0; i < sampleIps.size(); ++i) {
        QString name = pattern;
        name.replace("{prefix}", prefix);
        name.replace(QRegularExpression("\\{index:\\d+d\\}"),
                    QString("%1").arg(startIndex + i, digits, 10, QChar('0')));
        name.replace("{index}", QString::number(startIndex + i));
        name.replace("{ip}", sampleIps[i]);
        name.replace("{ip_last}", sampleIps[i].section('.', -1));
        
        preview += tr("  %1\n").arg(name);
    }
    
    m_previewEdit->setText(preview);
}

bool NamingPage::validatePage() {
    if (m_prefixEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("验证失败"), tr("请输入名称前缀"));
        return false;
    }
    return true;
}

QString NamingPage::getNameTemplate() const {
    QString pattern = m_patternCombo->currentData().toString();
    int digits = m_digitsSpin->value();
    pattern.replace("03d", QString("%1d").arg(digits));
    return pattern;
}

QString NamingPage::getNamePrefix() const {
    return m_prefixEdit->text().trimmed();
}

int NamingPage::getStartIndex() const {
    return m_startIndexSpin->value();
}

// ============ ConfirmPage ============

ConfirmPage::ConfirmPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle(tr("确认创建"));
    setSubTitle(tr("请确认以下配置信息"));
    
    setupUi();
}

void ConfirmPage::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    m_summaryEdit = new QTextEdit(this);
    m_summaryEdit->setReadOnly(true);
    layout->addWidget(m_summaryEdit);
}

void ConfirmPage::initializePage() {
    BatchChannelWizard* wizard = qobject_cast<BatchChannelWizard*>(this->wizard());
    if (!wizard) return;
    
    TemplatePage* templatePage = qobject_cast<TemplatePage*>(wizard->page(1));
    DeviceListPage* devicePage = qobject_cast<DeviceListPage*>(wizard->page(2));
    NamingPage* namingPage = qobject_cast<NamingPage*>(wizard->page(3));
    
    QString summary;
    summary += tr("<h3>📋 创建确认</h3>");
    
    // 模板信息
    QJsonObject templateConfig = templatePage->getTemplateConfig();
    summary += tr("<p><b>模板通道:</b> %1</p>").arg(templateConfig["name"].toString());
    
    QJsonArray collectors = templateConfig["collectors"].toArray();
    summary += tr("<p><b>采集器数量:</b> %1</p>").arg(collectors.size());
    
    // 设备信息
    QList<DeviceInfo> devices = devicePage->getDevices();
    summary += tr("<p><b>设备数量:</b> %1</p>").arg(devices.size());
    
    // 命名规则
    summary += tr("<p><b>命名前缀:</b> %1</p>").arg(namingPage->getNamePrefix());
    summary += tr("<p><b>命名模式:</b> %1</p>").arg(namingPage->getNameTemplate());
    summary += tr("<p><b>起始索引:</b> %1</p>").arg(namingPage->getStartIndex());
    
    // 将要创建的通道列表
    summary += tr("<h4>将要创建的通道:</h4><ul>");
    int maxShow = qMin(10, devices.size());
    for (int i = 0; i < maxShow; ++i) {
        QString name = namingPage->getNameTemplate();
        name.replace("{prefix}", namingPage->getNamePrefix());
        name.replace(QRegularExpression("\\{index:\\d+d\\}"),
                    QString("%1").arg(namingPage->getStartIndex() + i, 3, 10, QChar('0')));
        name.replace("{index}", QString::number(namingPage->getStartIndex() + i));
        name.replace("{ip}", devices[i].ip);
        name.replace("{ip_last}", devices[i].ip.section('.', -1));
        
        summary += tr("<li>%1 (%2)</li>").arg(name, devices[i].ip);
    }
    if (devices.size() > maxShow) {
        summary += tr("<li>... 还有 %1 个</li>").arg(devices.size() - maxShow);
    }
    summary += "</ul>";
    
    m_summaryEdit->setHtml(summary);
}

// ============ ResultPage ============

ResultPage::ResultPage(BatchChannelWizard* wizard, QWidget* parent)
    : QWizardPage(parent)
    , m_wizard(wizard)
{
    setTitle(tr("创建结果"));
    setSubTitle(tr("批量创建通道完成"));
    setFinalPage(true);
    
    setupUi();
}

void ResultPage::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    layout->addWidget(m_statusLabel);
    
    m_resultEdit = new QTextEdit(this);
    m_resultEdit->setReadOnly(true);
    layout->addWidget(m_resultEdit);
}

void ResultPage::initializePage() {
    // 执行创建
    m_wizard->createChannels();
    
    int created = m_wizard->getCreatedCount();
    QStringList createdNames = m_wizard->getCreatedChannelNames();
    
    if (created > 0) {
        m_statusLabel->setText(tr("✅ 成功创建 %1 个通道").arg(created));
        m_statusLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #10B981;");
    } else {
        m_statusLabel->setText(tr("❌ 创建失败"));
        m_statusLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #EF4444;");
    }
    
    QString result;
    result += tr("<h4>创建成功的通道:</h4><ul>");
    for (const QString& name : createdNames) {
        result += tr("<li style='color: #10B981;'>✓ %1</li>").arg(name);
    }
    result += "</ul>";
    
    QStringList failedChannels = m_wizard->getFailedChannels();
    if (!failedChannels.isEmpty()) {
        result += tr("<h4>创建失败的通道:</h4><ul>");
        for (const QString& name : failedChannels) {
            result += tr("<li style='color: #EF4444;'>✗ %1</li>").arg(name);
        }
        result += "</ul>";
    }
    
    m_resultEdit->setHtml(result);
}

} // namespace ModbusPlexLink
