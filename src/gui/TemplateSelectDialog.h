#ifndef TEMPLATESELECTDIALOG_H
#define TEMPLATESELECTDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QLabel>
#include "utils/TemplateManager.h"

namespace ModbusPlexLink {

/**
 * @brief 设备模板选择对话框
 * 
 * 用于浏览和选择预配置的设备模板
 */
class TemplateSelectDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit TemplateSelectDialog(QWidget* parent = nullptr);
    ~TemplateSelectDialog() = default;
    
    /**
     * @brief 获取选中的模板
     */
    DeviceTemplate selectedTemplate() const;
    
    /**
     * @brief 是否有选中的模板
     */
    bool hasSelection() const;
    
private slots:
    void onTreeItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onSearchTextChanged(const QString& text);
    void onRefreshClicked();
    
private:
    void setupUI();
    void loadTemplates();
    void populateTree(const QString& filter = QString());
    void updatePreview(const DeviceTemplate& tmpl);
    void clearPreview();
    
    // UI 组件
    QLineEdit* m_searchEdit;
    QTreeWidget* m_treeWidget;
    QTextBrowser* m_previewBrowser;
    QLabel* m_statusLabel;
    QPushButton* m_btnOk;
    QPushButton* m_btnCancel;
    QPushButton* m_btnRefresh;
    
    // 当前选中
    QString m_selectedTemplateId;
};

} // namespace ModbusPlexLink

#endif // TEMPLATESELECTDIALOG_H
