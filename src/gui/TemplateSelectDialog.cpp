#include "TemplateSelectDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QDir>

namespace ModbusPlexLink {

TemplateSelectDialog::TemplateSelectDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    loadTemplates();
}

void TemplateSelectDialog::setupUI() {
    setWindowTitle(tr("选择设备模板"));
    setMinimumSize(800, 500);
    resize(900, 600);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    
    // 顶部搜索栏
    QHBoxLayout* searchLayout = new QHBoxLayout();
    
    QLabel* searchLabel = new QLabel(tr("🔍 搜索:"));
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("输入设备名称、型号或制造商..."));
    m_searchEdit->setClearButtonEnabled(true);
    
    m_btnRefresh = new QPushButton(tr("🔄 刷新"));
    m_btnRefresh->setToolTip(tr("重新加载模板"));
    
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_btnRefresh);
    
    mainLayout->addLayout(searchLayout);
    
    // 中间分割区域
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    
    // 左侧树形列表
    m_treeWidget = new QTreeWidget();
    m_treeWidget->setHeaderLabels({tr("设备"), tr("制造商"), tr("型号")});
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->header()->setStretchLastSection(true);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->setMinimumWidth(350);
    
    // 右侧预览区
    m_previewBrowser = new QTextBrowser();
    m_previewBrowser->setOpenExternalLinks(true);
    m_previewBrowser->setMinimumWidth(300);
    
    splitter->addWidget(m_treeWidget);
    splitter->addWidget(m_previewBrowser);
    splitter->setSizes({500, 400});
    
    mainLayout->addWidget(splitter, 1);
    
    // 底部状态和按钮
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: gray;");
    
    m_btnOk = new QPushButton(tr("确定"));
    m_btnOk->setEnabled(false);
    m_btnOk->setDefault(true);
    
    m_btnCancel = new QPushButton(tr("取消"));
    
    bottomLayout->addWidget(m_statusLabel, 1);
    bottomLayout->addWidget(m_btnOk);
    bottomLayout->addWidget(m_btnCancel);
    
    mainLayout->addLayout(bottomLayout);
    
    // 连接信号
    connect(m_searchEdit, &QLineEdit::textChanged, 
            this, &TemplateSelectDialog::onSearchTextChanged);
    connect(m_treeWidget, &QTreeWidget::currentItemChanged,
            this, &TemplateSelectDialog::onTreeItemChanged);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &TemplateSelectDialog::onTreeItemDoubleClicked);
    connect(m_btnRefresh, &QPushButton::clicked,
            this, &TemplateSelectDialog::onRefreshClicked);
    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void TemplateSelectDialog::loadTemplates() {
    // 获取模板目录
    QString templatesDir = QApplication::applicationDirPath() + "/templates";
    
    // 如果运行目录没有，尝试项目目录
    if (!QDir(templatesDir).exists()) {
        templatesDir = QDir::currentPath() + "/templates";
    }
    
    // 如果还没有，尝试上级目录
    if (!QDir(templatesDir).exists()) {
        QDir dir(QApplication::applicationDirPath());
        dir.cdUp();
        templatesDir = dir.filePath("templates");
    }
    
    int count = TemplateManager::instance().loadTemplates(templatesDir);
    populateTree();
    
    m_statusLabel->setText(tr("已加载 %1 个设备模板").arg(count));
}

void TemplateSelectDialog::populateTree(const QString& filter) {
    m_treeWidget->clear();
    m_selectedTemplateId.clear();
    m_btnOk->setEnabled(false);
    clearPreview();
    
    TemplateManager& manager = TemplateManager::instance();
    QStringList categories = manager.getCategories();
    
    for (const QString& category : categories) {
        QList<DeviceTemplate> templates = manager.getTemplatesByCategory(category);
        
        // 过滤
        QList<DeviceTemplate> filtered;
        for (const DeviceTemplate& tmpl : templates) {
            if (filter.isEmpty() ||
                tmpl.name.contains(filter, Qt::CaseInsensitive) ||
                tmpl.manufacturer.contains(filter, Qt::CaseInsensitive) ||
                tmpl.model.contains(filter, Qt::CaseInsensitive)) {
                filtered.append(tmpl);
            }
        }
        
        if (filtered.isEmpty()) continue;
        
        // 创建类别节点
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem();
        QString displayName = TemplateManager::categoryIcon(category) + " " +
                              TemplateManager::categoryDisplayName(category);
        categoryItem->setText(0, displayName);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable);
        
        QFont font = categoryItem->font(0);
        font.setBold(true);
        categoryItem->setFont(0, font);
        
        m_treeWidget->addTopLevelItem(categoryItem);
        
        // 添加模板
        for (const DeviceTemplate& tmpl : filtered) {
            QTreeWidgetItem* item = new QTreeWidgetItem(categoryItem);
            item->setText(0, tmpl.name);
            item->setText(1, tmpl.manufacturer);
            item->setText(2, tmpl.model);
            item->setData(0, Qt::UserRole, tmpl.id);
            item->setToolTip(0, tmpl.description);
        }
        
        categoryItem->setExpanded(true);
    }
    
    // 调整列宽
    m_treeWidget->resizeColumnToContents(1);
    m_treeWidget->resizeColumnToContents(2);
}

void TemplateSelectDialog::onTreeItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous) {
    Q_UNUSED(previous);
    
    if (!current || !current->parent()) {
        // 选中的是类别节点或空
        m_selectedTemplateId.clear();
        m_btnOk->setEnabled(false);
        clearPreview();
        return;
    }
    
    QString templateId = current->data(0, Qt::UserRole).toString();
    m_selectedTemplateId = templateId;
    m_btnOk->setEnabled(true);
    
    DeviceTemplate tmpl = TemplateManager::instance().getTemplate(templateId);
    updatePreview(tmpl);
}

void TemplateSelectDialog::onTreeItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    if (item && item->parent()) {
        // 双击模板项，直接确定
        accept();
    }
}

void TemplateSelectDialog::onSearchTextChanged(const QString& text) {
    populateTree(text);
}

void TemplateSelectDialog::onRefreshClicked() {
    loadTemplates();
}

void TemplateSelectDialog::updatePreview(const DeviceTemplate& tmpl) {
    QString html;
    
    html += "<h2>" + tmpl.name + "</h2>";
    html += "<p><b>制造商:</b> " + tmpl.manufacturer + "</p>";
    html += "<p><b>型号:</b> " + tmpl.model + "</p>";
    html += "<p><b>描述:</b> " + tmpl.description + "</p>";
    
    html += "<hr>";
    html += "<h3>默认配置</h3>";
    html += "<ul>";
    html += "<li>协议: " + tmpl.protocol + "</li>";
    html += "<li>端口: " + QString::number(tmpl.port) + "</li>";
    html += "<li>从站地址: " + QString::number(tmpl.unitId) + "</li>";
    html += "<li>采集周期: " + QString::number(tmpl.pollRate) + " ms</li>";
    html += "<li>默认字节序: " + tmpl.defaultByteOrder + "</li>";
    html += "</ul>";
    
    html += "<hr>";
    html += "<h3>数据点 (" + QString::number(tmpl.mappings.size()) + " 个)</h3>";
    html += "<table border='1' cellpadding='4' cellspacing='0' width='100%'>";
    html += "<tr style='background:#f0f0f0;'>";
    html += "<th>标签名</th><th>类型</th><th>地址</th><th>单位</th>";
    html += "</tr>";
    
    int displayCount = qMin(tmpl.mappings.size(), 10);
    for (int i = 0; i < displayCount; ++i) {
        const CollectorMappingRule& rule = tmpl.mappings[i];
        html += "<tr>";
        html += "<td>" + rule.tagName + "</td>";
        html += "<td>" + DataTypeUtils::dataTypeToString(rule.dataType) + "</td>";
        html += "<td>" + QString::number(rule.address) + "</td>";
        html += "<td>" + rule.unit + "</td>";
        html += "</tr>";
    }
    
    if (tmpl.mappings.size() > 10) {
        html += "<tr><td colspan='4' style='text-align:center; color:gray;'>";
        html += "... 还有 " + QString::number(tmpl.mappings.size() - 10) + " 个数据点";
        html += "</td></tr>";
    }
    
    html += "</table>";
    
    if (!tmpl.notes.isEmpty()) {
        html += "<hr>";
        html += "<h3>注意事项</h3>";
        html += "<p style='white-space:pre-wrap;'>" + tmpl.notes.toHtmlEscaped() + "</p>";
    }
    
    if (!tmpl.manualUrl.isEmpty()) {
        html += "<hr>";
        html += "<p><a href='" + tmpl.manualUrl + "'>📖 查看设备手册</a></p>";
    }
    
    m_previewBrowser->setHtml(html);
}

void TemplateSelectDialog::clearPreview() {
    QString html = "<div style='color:gray; text-align:center; padding:50px;'>";
    html += "<h3>选择左侧的设备模板查看详情</h3>";
    html += "<p>双击模板可快速选择</p>";
    html += "</div>";
    m_previewBrowser->setHtml(html);
}

DeviceTemplate TemplateSelectDialog::selectedTemplate() const {
    return TemplateManager::instance().getTemplate(m_selectedTemplateId);
}

bool TemplateSelectDialog::hasSelection() const {
    return !m_selectedTemplateId.isEmpty();
}

} // namespace ModbusPlexLink
