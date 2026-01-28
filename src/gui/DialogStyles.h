#ifndef DIALOGSTYLES_H
#define DIALOGSTYLES_H

#include <QString>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QGraphicsDropShadowEffect>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QAbstractSpinBox>

namespace ModbusPlexLink {

/**
 * @brief 禁用滚轮事件的事件过滤器
 * 
 * 用于防止用户在滚动页面时意外修改 SpinBox 和 ComboBox 的值
 */
class WheelEventFilter : public QObject {
    Q_OBJECT
public:
    explicit WheelEventFilter(QObject* parent = nullptr) : QObject(parent) {}
    
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Wheel) {
            // 只有当控件获得焦点时才允许滚轮操作
            QWidget* widget = qobject_cast<QWidget*>(obj);
            if (widget && !widget->hasFocus()) {
                // 如果没有焦点，忽略滚轮事件
                event->ignore();
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

/**
 * @brief 对话框样式管理类
 * 
 * 提供统一的现代化样式，用于所有配置对话框
 */
class DialogStyles {
public:
    // 获取主对话框样式
    static QString getDialogStyle() {
        return R"(
            QDialog {
                background-color: #F8FAFC;
            }
            
            /* 分组框样式 */
            QGroupBox {
                font-weight: bold;
                font-size: 11pt;
                color: #1E293B;
                border: 1px solid #E2E8F0;
                border-radius: 12px;
                margin-top: 16px;
                padding: 20px 16px 16px 16px;
                background-color: white;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 16px;
                top: 4px;
                padding: 0 8px;
                background-color: white;
            }
            
            /* 输入框样式 */
            QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
                padding: 8px 12px;
                border: 1px solid #E2E8F0;
                border-radius: 8px;
                background-color: white;
                color: #1E293B;
                font-size: 10pt;
                min-height: 20px;
            }
            QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
                border: 2px solid #3B82F6;
                background-color: #F0F9FF;
            }
            QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {
                background-color: #F1F5F9;
                color: #94A3B8;
            }
            QLineEdit::placeholder {
                color: #94A3B8;
            }
            
            /* 下拉框样式 */
            QComboBox {
                padding-right: 30px;
            }
            QComboBox::drop-down {
                border: none;
                width: 30px;
            }
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 6px solid #64748B;
                margin-right: 10px;
            }
            QComboBox QAbstractItemView {
                border: 1px solid #E2E8F0;
                border-radius: 8px;
                background-color: white;
                selection-background-color: #3B82F6;
                selection-color: white;
                padding: 4px;
            }
            
            /* SpinBox 箭头 */
            QSpinBox::up-button, QDoubleSpinBox::up-button {
                subcontrol-origin: border;
                subcontrol-position: top right;
                width: 20px;
                border: none;
                background: transparent;
            }
            QSpinBox::down-button, QDoubleSpinBox::down-button {
                subcontrol-origin: border;
                subcontrol-position: bottom right;
                width: 20px;
                border: none;
                background: transparent;
            }
            
            /* 复选框样式 */
            QCheckBox {
                spacing: 8px;
                color: #1E293B;
                font-size: 10pt;
            }
            QCheckBox::indicator {
                width: 20px;
                height: 20px;
                border-radius: 6px;
                border: 2px solid #CBD5E1;
                background-color: white;
            }
            QCheckBox::indicator:checked {
                background-color: #3B82F6;
                border-color: #3B82F6;
                image: url(:/icons/check-white.png);
            }
            QCheckBox::indicator:hover {
                border-color: #3B82F6;
            }
            
            /* 标签样式 */
            QLabel {
                color: #475569;
                font-size: 10pt;
            }
            
            /* 文本框样式 */
            QTextEdit {
                padding: 10px;
                border: 1px solid #E2E8F0;
                border-radius: 8px;
                background-color: white;
                color: #1E293B;
                font-size: 10pt;
            }
            QTextEdit:focus {
                border: 2px solid #3B82F6;
            }
            
            /* Tab 样式 */
            QTabWidget::pane {
                border: 1px solid #E2E8F0;
                border-radius: 12px;
                background-color: white;
                padding: 8px;
            }
            QTabBar::tab {
                padding: 10px 24px;
                margin-right: 4px;
                border: none;
                border-top-left-radius: 8px;
                border-top-right-radius: 8px;
                background-color: #F1F5F9;
                color: #64748B;
                font-weight: 500;
            }
            QTabBar::tab:selected {
                background-color: white;
                color: #3B82F6;
                font-weight: 600;
            }
            QTabBar::tab:hover:!selected {
                background-color: #E2E8F0;
            }
            
            /* 分割器 */
            QSplitter::handle {
                background-color: #E2E8F0;
            }
            QSplitter::handle:vertical {
                height: 4px;
            }
            QSplitter::handle:horizontal {
                width: 4px;
            }
            QSplitter::handle:hover {
                background-color: #3B82F6;
            }
            
            /* 滚动条 */
            QScrollBar:vertical {
                border: none;
                background: #F1F5F9;
                width: 10px;
                margin: 0;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical {
                background: #CBD5E1;
                min-height: 30px;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical:hover {
                background: #94A3B8;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollBar:horizontal {
                border: none;
                background: #F1F5F9;
                height: 10px;
                margin: 0;
                border-radius: 5px;
            }
            QScrollBar::handle:horizontal {
                background: #CBD5E1;
                min-width: 30px;
                border-radius: 5px;
            }
            QScrollBar::handle:horizontal:hover {
                background: #94A3B8;
            }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
                width: 0;
            }
            
            /* 对话框按钮框 */
            QDialogButtonBox {
                button-layout: 3;
            }
        )";
    }
    
    // 获取表格样式
    static QString getTableStyle() {
        return R"(
            QTableWidget {
                border: 1px solid #E2E8F0;
                border-radius: 8px;
                background-color: white;
                gridline-color: #F1F5F9;
                selection-background-color: #DBEAFE;
                selection-color: #1E40AF;
                font-size: 10pt;
            }
            QTableWidget::item {
                padding: 8px 12px;
                border-bottom: 1px solid #F1F5F9;
            }
            QTableWidget::item:selected {
                background-color: #DBEAFE;
                color: #1E40AF;
            }
            QTableWidget::item:hover {
                background-color: #F8FAFC;
            }
            QHeaderView::section {
                background-color: #F8FAFC;
                color: #475569;
                font-weight: 600;
                font-size: 9pt;
                padding: 10px 12px;
                border: none;
                border-bottom: 2px solid #E2E8F0;
                border-right: 1px solid #E2E8F0;
            }
            QHeaderView::section:last {
                border-right: none;
            }
            QHeaderView::section:hover {
                background-color: #F1F5F9;
            }
            QTableCornerButton::section {
                background-color: #F8FAFC;
                border: none;
            }
        )";
    }
    
    // 获取主要按钮样式
    static QString getPrimaryButtonStyle() {
        return R"(
            QPushButton {
                background-color: #3B82F6;
                color: white;
                border: none;
                border-radius: 8px;
                padding: 10px 20px;
                font-weight: 600;
                font-size: 10pt;
                min-width: 100px;
            }
            QPushButton:hover {
                background-color: #2563EB;
            }
            QPushButton:pressed {
                background-color: #1D4ED8;
            }
            QPushButton:disabled {
                background-color: #CBD5E1;
                color: #94A3B8;
            }
        )";
    }
    
    // 获取次要按钮样式
    static QString getSecondaryButtonStyle() {
        return R"(
            QPushButton {
                background-color: white;
                color: #475569;
                border: 1px solid #E2E8F0;
                border-radius: 8px;
                padding: 10px 20px;
                font-weight: 500;
                font-size: 10pt;
                min-width: 100px;
            }
            QPushButton:hover {
                background-color: #F8FAFC;
                border-color: #CBD5E1;
                color: #1E293B;
            }
            QPushButton:pressed {
                background-color: #F1F5F9;
            }
            QPushButton:disabled {
                background-color: #F8FAFC;
                color: #CBD5E1;
                border-color: #F1F5F9;
            }
        )";
    }
    
    // 获取危险按钮样式
    static QString getDangerButtonStyle() {
        return R"(
            QPushButton {
                background-color: white;
                color: #DC2626;
                border: 1px solid #FCA5A5;
                border-radius: 8px;
                padding: 10px 20px;
                font-weight: 500;
                font-size: 10pt;
                min-width: 100px;
            }
            QPushButton:hover {
                background-color: #FEF2F2;
                border-color: #F87171;
            }
            QPushButton:pressed {
                background-color: #FEE2E2;
            }
            QPushButton:disabled {
                background-color: #F8FAFC;
                color: #CBD5E1;
                border-color: #F1F5F9;
            }
        )";
    }
    
    // 获取成功按钮样式
    static QString getSuccessButtonStyle() {
        return R"(
            QPushButton {
                background-color: #10B981;
                color: white;
                border: none;
                border-radius: 8px;
                padding: 10px 20px;
                font-weight: 600;
                font-size: 10pt;
                min-width: 100px;
            }
            QPushButton:hover {
                background-color: #059669;
            }
            QPushButton:pressed {
                background-color: #047857;
            }
            QPushButton:disabled {
                background-color: #CBD5E1;
                color: #94A3B8;
            }
        )";
    }
    
    // 获取图标按钮样式
    static QString getIconButtonStyle() {
        return R"(
            QPushButton {
                background-color: transparent;
                border: 1px solid transparent;
                border-radius: 6px;
                padding: 8px;
                min-width: 36px;
                min-height: 36px;
                max-width: 36px;
                max-height: 36px;
            }
            QPushButton:hover {
                background-color: #F1F5F9;
                border-color: #E2E8F0;
            }
            QPushButton:pressed {
                background-color: #E2E8F0;
            }
            QPushButton:disabled {
                color: #CBD5E1;
            }
        )";
    }
    
    // 获取提示标签样式
    static QString getHintLabelStyle() {
        return R"(
            QLabel {
                color: #64748B;
                font-size: 9pt;
                padding: 8px 12px;
                background-color: #F1F5F9;
                border-radius: 6px;
                border-left: 3px solid #3B82F6;
            }
        )";
    }
    
    // 获取警告提示标签样式
    static QString getWarningLabelStyle() {
        return R"(
            QLabel {
                color: #92400E;
                font-size: 9pt;
                padding: 8px 12px;
                background-color: #FEF3C7;
                border-radius: 6px;
                border-left: 3px solid #F59E0B;
            }
        )";
    }
    
    // 获取成功提示标签样式
    static QString getSuccessLabelStyle() {
        return R"(
            QLabel {
                color: #065F46;
                font-size: 9pt;
                padding: 8px 12px;
                background-color: #D1FAE5;
                border-radius: 6px;
                border-left: 3px solid #10B981;
            }
        )";
    }
    
    // 获取章节标题样式
    static QString getSectionTitleStyle() {
        return R"(
            QLabel {
                color: #1E293B;
                font-size: 12pt;
                font-weight: bold;
                padding: 4px 0;
            }
        )";
    }
    
    // 获取统计标签样式
    static QString getStatLabelStyle() {
        return R"(
            QLabel {
                color: #64748B;
                font-size: 9pt;
                padding: 4px 12px;
                background-color: #F1F5F9;
                border-radius: 12px;
            }
        )";
    }
    
    // 应用阴影效果
    static void applyShadow(QWidget* widget, int blur = 20, int offsetY = 4) {
        QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setBlurRadius(blur);
        shadow->setXOffset(0);
        shadow->setYOffset(offsetY);
        shadow->setColor(QColor(0, 0, 0, 25));
        widget->setGraphicsEffect(shadow);
    }
    
    // 设置表格
    static void setupModernTable(QTableWidget* table) {
        table->setStyleSheet(getTableStyle());
        table->setAlternatingRowColors(false);
        table->setShowGrid(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setHighlightSections(false);
        table->horizontalHeader()->setMinimumSectionSize(60);
        table->verticalHeader()->setDefaultSectionSize(44);
        table->setFocusPolicy(Qt::NoFocus);
    }
    
    // 创建带图标的按钮
    static QPushButton* createIconButton(const QString& icon, const QString& tooltip, QWidget* parent = nullptr) {
        QPushButton* btn = new QPushButton(icon, parent);
        btn->setStyleSheet(getIconButtonStyle());
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    }
    
    // 创建主要按钮
    static QPushButton* createPrimaryButton(const QString& text, QWidget* parent = nullptr) {
        QPushButton* btn = new QPushButton(text, parent);
        btn->setStyleSheet(getPrimaryButtonStyle());
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    }
    
    // 创建次要按钮
    static QPushButton* createSecondaryButton(const QString& text, QWidget* parent = nullptr) {
        QPushButton* btn = new QPushButton(text, parent);
        btn->setStyleSheet(getSecondaryButtonStyle());
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    }
    
    // 创建危险按钮
    static QPushButton* createDangerButton(const QString& text, QWidget* parent = nullptr) {
        QPushButton* btn = new QPushButton(text, parent);
        btn->setStyleSheet(getDangerButtonStyle());
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    }
    
    // 创建提示标签
    static QLabel* createHintLabel(const QString& text, QWidget* parent = nullptr) {
        QLabel* label = new QLabel(text, parent);
        label->setStyleSheet(getHintLabelStyle());
        label->setWordWrap(true);
        return label;
    }
    
    // 创建章节标题
    static QLabel* createSectionTitle(const QString& text, QWidget* parent = nullptr) {
        QLabel* label = new QLabel(text, parent);
        label->setStyleSheet(getSectionTitleStyle());
        return label;
    }
    
    /**
     * @brief 禁用控件的滚轮事件（只有获得焦点时才响应滚轮）
     * 
     * 使用方式：在创建 SpinBox/ComboBox 后调用此函数
     * @param widget 需要禁用滚轮的控件
     * @param parent 事件过滤器的父对象（用于内存管理）
     */
    static void disableWheelEvent(QWidget* widget, QObject* parent = nullptr) {
        if (!widget) return;
        
        // 设置焦点策略：只有点击时才获得焦点
        widget->setFocusPolicy(Qt::StrongFocus);
        
        // 创建并安装事件过滤器
        static WheelEventFilter* filter = nullptr;
        if (!filter) {
            filter = new WheelEventFilter();  // 单例，不会被销毁
        }
        widget->installEventFilter(filter);
    }
    
    /**
     * @brief 批量禁用控件的滚轮事件
     * 
     * 递归查找并禁用所有 SpinBox 和 ComboBox 的滚轮事件
     * @param parent 父控件
     */
    static void disableAllWheelEvents(QWidget* parent) {
        if (!parent) return;
        
        // 查找所有 SpinBox 和 ComboBox
        QList<QAbstractSpinBox*> spinBoxes = parent->findChildren<QAbstractSpinBox*>();
        for (QAbstractSpinBox* spin : spinBoxes) {
            disableWheelEvent(spin, parent);
        }
        
        QList<QComboBox*> comboBoxes = parent->findChildren<QComboBox*>();
        for (QComboBox* combo : comboBoxes) {
            disableWheelEvent(combo, parent);
        }
    }
};

} // namespace ModbusPlexLink

#endif // DIALOGSTYLES_H
