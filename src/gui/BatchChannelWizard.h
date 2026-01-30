#ifndef BATCHCHANNELWIZARD_H
#define BATCHCHANNELWIZARD_H

#include <QWizard>
#include <QWizardPage>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QLabel>
#include <QRadioButton>
#include <QJsonObject>

namespace ModbusPlexLink {

class ChannelManager;

/**
 * @brief 设备信息结构
 */
struct DeviceInfo {
    QString ip;
    int port = 502;
    int unitId = 1;
    QString description;
};

/**
 * @brief 批量创建通道向导
 * 
 * 功能：
 * - 选择模板通道或配置点表
 * - 输入设备列表（IP地址）
 * - 配置通道命名规则
 * - 批量创建通道
 */
class BatchChannelWizard : public QWizard {
    Q_OBJECT
    
public:
    explicit BatchChannelWizard(ChannelManager* channelManager, QWidget* parent = nullptr);
    ~BatchChannelWizard() = default;
    
    /**
     * @brief 获取创建的通道数量
     */
    int getCreatedCount() const { return m_createdCount; }
    
    /**
     * @brief 获取创建的通道名称列表
     */
    QStringList getCreatedChannelNames() const { return m_createdChannelNames; }
    
    /**
     * @brief 获取创建失败的通道列表
     */
    QStringList getFailedChannels() const { return m_failedChannels; }
    
    /**
     * @brief 执行批量创建
     */
    void createChannels();
    
private:
    QWizardPage* createIntroPage();
    QWizardPage* createTemplatePage();
    QWizardPage* createDeviceListPage();
    QWizardPage* createNamingPage();
    QWizardPage* createConfirmPage();
    QWizardPage* createResultPage();
    QString generateChannelName(const QString& nameTemplate, int index, const DeviceInfo& device);
    
    ChannelManager* m_channelManager;
    
    // 配置数据
    QJsonObject m_templateConfig;
    QList<DeviceInfo> m_devices;
    QString m_nameTemplate;
    QString m_namePrefix;
    int m_startIndex;
    
    // 结果
    int m_createdCount;
    QStringList m_createdChannelNames;
    QStringList m_failedChannels;
};

// ============ 向导页面 ============

/**
 * @brief 介绍页面
 */
class IntroPage : public QWizardPage {
    Q_OBJECT
public:
    explicit IntroPage(QWidget* parent = nullptr);
};

/**
 * @brief 模板选择页面
 */
class TemplatePage : public QWizardPage {
    Q_OBJECT
public:
    explicit TemplatePage(ChannelManager* channelManager, QWidget* parent = nullptr);
    
    bool validatePage() override;
    QJsonObject getTemplateConfig() const;
    
private slots:
    void onTemplateSourceChanged();
    void onTemplateChannelChanged(int index);
    
private:
    void setupUi();
    void loadTemplateChannels();
    
    ChannelManager* m_channelManager;
    QRadioButton* m_useExistingRadio;
    QRadioButton* m_useFileRadio;
    QComboBox* m_templateChannelCombo;
    QLineEdit* m_templateFileEdit;
    QPushButton* m_browseBtn;
    QTextEdit* m_previewEdit;
};

/**
 * @brief 设备列表页面
 */
class DeviceListPage : public QWizardPage {
    Q_OBJECT
public:
    explicit DeviceListPage(QWidget* parent = nullptr);
    
    bool validatePage() override;
    QList<DeviceInfo> getDevices() const;
    
private slots:
    void onInputMethodChanged();
    void onGenerateDevices();
    void onImportCsv();
    void onAddDevice();
    void onRemoveDevice();
    void onClearDevices();
    
private:
    void setupUi();
    void updateDeviceTable();
    
    QRadioButton* m_manualRadio;
    QRadioButton* m_rangeRadio;
    QRadioButton* m_csvRadio;
    
    // 手动输入
    QTableWidget* m_deviceTable;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;
    QPushButton* m_clearBtn;
    
    // IP范围生成
    QLineEdit* m_baseIpEdit;
    QSpinBox* m_startIpSpin;
    QSpinBox* m_endIpSpin;
    QSpinBox* m_portSpin;
    QSpinBox* m_unitIdSpin;
    QPushButton* m_generateBtn;
    
    // CSV导入
    QLineEdit* m_csvFileEdit;
    QPushButton* m_importBtn;
    
    QLabel* m_countLabel;
    QList<DeviceInfo> m_devices;
};

/**
 * @brief 命名规则页面
 */
class NamingPage : public QWizardPage {
    Q_OBJECT
public:
    explicit NamingPage(QWidget* parent = nullptr);
    
    bool validatePage() override;
    QString getNameTemplate() const;
    QString getNamePrefix() const;
    int getStartIndex() const;
    
private slots:
    void onPreviewUpdate();
    
private:
    void setupUi();
    
    QLineEdit* m_prefixEdit;
    QComboBox* m_patternCombo;
    QSpinBox* m_startIndexSpin;
    QSpinBox* m_digitsSpin;
    QTextEdit* m_previewEdit;
};

/**
 * @brief 确认页面
 */
class ConfirmPage : public QWizardPage {
    Q_OBJECT
public:
    explicit ConfirmPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    
private:
    void setupUi();
    
    QTextEdit* m_summaryEdit;
};

/**
 * @brief 结果页面
 */
class ResultPage : public QWizardPage {
    Q_OBJECT
public:
    explicit ResultPage(BatchChannelWizard* wizard, QWidget* parent = nullptr);
    
    void initializePage() override;
    
private:
    void setupUi();
    
    BatchChannelWizard* m_wizard;
    QTextEdit* m_resultEdit;
    QLabel* m_statusLabel;
};

} // namespace ModbusPlexLink

#endif // BATCHCHANNELWIZARD_H
