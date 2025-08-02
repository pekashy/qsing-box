#include "main_window.h"
#include "ui_main_window.h"

#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>
#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QFile>
#include <QListWidget>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTextEdit>
#include <QProcessEnvironment>

#include "about_dialog.h"
#include "ansi_color_text.h"
#include "settings_dialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Remove the minimize and maximize buttons
    setWindowFlags(windowFlags() & ~Qt::WindowMinMaxButtonsHint);
    resize(600, 450);
    setMaximumSize(720, 720);

    m_versionLabel = new QLabel(this);
    m_versionLabel->setText("v" + QString(PROJECT_VERSION));
    m_versionLabel->setIndent(8);
    ui->statusbar->addWidget(m_versionLabel);
    ui->statusLabel->setPixmap(QPixmap(":/images/status_disabled.png").
                               scaled(QSize(48, 48)));

    ui->stopButton->setEnabled(false);
    ui->outputEdit->setReadOnly(true);
    ui->outputEdit->setMaximumBlockCount(1000);

    m_proxyManager = new ProxyManager(this);
    connect(m_proxyManager, &ProxyManager::proxyProcessStateChanged, this,
            &MainWindow::changeProxy);
    connect(m_proxyManager, &ProxyManager::proxyProcessReadyReadStandardError, this,
            &MainWindow::displayProxyOutput);

    m_configManager = new ConfigManager(this);
    connect(m_configManager, &ConfigManager::configUpdated, this,
            &MainWindow::updateConfigList);
    connect(m_configManager, &ConfigManager::configChanged, this,
            &MainWindow::changeSelectedConfig);

    // Initialize subscription functionality
    m_networkManager = new QNetworkAccessManager(this);
    m_currentReply = nullptr;
    
    // Configure SSL for better compatibility
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // For testing - consider VerifyPeer for production
    QSslConfiguration::setDefaultConfiguration(sslConfig);
    
    // Setup update timer for 1-minute intervals
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(60000); // 1 minute
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateSubscriptionConfig);
    
    // Setup config file path
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    m_configFilePath = appDataPath + "/subscription_config.json";
    
    // Initialize config preview
    ui->configPreviewEdit->setPlainText("No configuration downloaded yet");
    
    // Load saved subscription URL
    loadSubscriptionUrl();

    // Initialize configuration
    changeSelectedConfig();

    m_trayIcon = new TrayIcon(this);
    connect(m_trayIcon, &TrayIcon::disableProxyActionTriggered, this, &MainWindow::stopProxy);
    connect(m_trayIcon, &TrayIcon::enableProxyActionTriggered, this, &MainWindow::startProxy);
    connect(m_trayIcon, &TrayIcon::restoreActionTriggered, this, &QWidget::showNormal);
    connect(m_trayIcon, &TrayIcon::iconActivated, this, &MainWindow::showMainWindow);
    connect(this, &MainWindow::proxyChanged, m_trayIcon, &TrayIcon::setMenuEnabled);
    m_trayIcon->show();
}

MainWindow::~MainWindow()
{
    stopProxy();
    delete ui;
}

void MainWindow::startProxy()
{
    m_proxyManager->startProxy();
    ui->outputEdit->clear();
}

void MainWindow::stopProxy()
{
    m_proxyManager->stopProxy();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!event->spontaneous() || !isVisible())
        return;
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    }
}

void MainWindow::on_startButton_clicked()
{
    startProxy();
}

void MainWindow::on_stopButton_clicked()
{
    stopProxy();
}

void MainWindow::on_settingsButton_clicked()
{
    SettingsDialog settingsDialog(m_configManager, this);
    settingsDialog.exec();
}

void MainWindow::on_aboutButton_clicked()
{
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

// Old profile management methods - no longer used
void MainWindow::on_addButton_clicked()
{
    // No longer used - UI element removed
}

void MainWindow::on_editButton_clicked()
{
    // No longer used - UI element removed
}

void MainWindow::on_deleteButton_clicked()
{
    // No longer used - UI element removed
}

void MainWindow::on_importButton_clicked()
{
    // No longer used - UI element removed
}

void MainWindow::on_switchButton_clicked()
{
    // No longer used - UI element removed
}

void MainWindow::enableButton(int currentRow)
{
    // No longer used - UI element removed
    Q_UNUSED(currentRow)
}

void MainWindow::displayProxyOutput()
{
    QByteArray outputData = m_proxyManager->readProxyProcessAllStandardError();
    QString outputText = QString::fromUtf8(outputData);
    // Parsing ANSI colors and dispalys
    AnsiColorText::appendAnsiColorText(ui->outputEdit, outputText);
    // Scroll to latest content
    QScrollBar *scrollBar = ui->outputEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void MainWindow::updateConfigList()
{
    // No longer used - config list UI removed in favor of subscription
}

void MainWindow::changeProxy(int newState)
{
    if (m_proxyManager->proxyProcessState() == QProcess::Running)
    {
        m_trayIcon->setIcon(QIcon(":/images/app_enable_proxy.ico"));
        setWindowIcon(QIcon(":/images/app_enable_proxy.ico"));
        ui->statusLabel->setPixmap(QPixmap(":/images/status_enabled.png").
                                   scaled(QSize(48, 48)));
        ui->startButton->setEnabled(false);
        ui->stopButton->setEnabled(true);
        emit proxyChanged(true);
    } else if (newState == QProcess::NotRunning) {
        m_trayIcon->setIcon(QIcon(":/images/app.ico"));
        setWindowIcon(QIcon(":/images/app.ico"));
        ui->statusLabel->setPixmap(QPixmap(":/images/status_disabled.png").
                                   scaled(QSize(48, 48)));
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        emit proxyChanged(false);
        m_proxyManager->clearSystemProxy();
    }
}

void MainWindow::showMainWindow(int reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::MiddleClick:
        this->show();
        break;
    default:
        ;
    }
}

void MainWindow::changeSelectedConfig()
{
    // Use subscription config if available and valid
    if (!m_subscriptionUrl.isEmpty() && QFile::exists(m_configFilePath)) {
        // Verify the config file has content and is valid JSON
        QFile file(m_configFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray content = file.readAll();
            file.close();
            
            if (!content.isEmpty()) {
                // Validate JSON structure
                QJsonParseError error;
                QJsonDocument jsonDoc = QJsonDocument::fromJson(content, &error);
                
                if (error.error == QJsonParseError::NoError && jsonDoc.isObject()) {
                    QJsonObject config = jsonDoc.object();
                    
                    // Check for essential sing-box config elements
                    if (config.contains("inbounds") || config.contains("outbounds")) {
                        ui->configStatusLabel->setText(tr("Status: Using subscription config"));
                        m_proxyManager->setConfigFilePath(m_configFilePath);
                        
                        // Load and display config preview
                        QString configText = jsonDoc.toJson(QJsonDocument::Indented);
                        ui->configPreviewEdit->setPlainText(configText);
                        
                        if (m_proxyManager->proxyProcessState() == QProcess::Running) {
                            stopProxy();
                            startProxy();
                        }
                        return;
                    } else {
                        // Valid JSON but not a proper sing-box config
                        ui->configStatusLabel->setText(tr("Status: Invalid config - missing inbounds/outbounds"));
                        ui->configPreviewEdit->setPlainText("Error: Configuration appears to be invalid.\nMissing required 'inbounds' or 'outbounds' sections.");
                        if (m_proxyManager->proxyProcessState() == QProcess::Running) {
                            stopProxy();
                        }
                        return;
                    }
                } else {
                    // Invalid JSON
                    ui->configStatusLabel->setText(tr("Status: Invalid JSON config"));
                    ui->configPreviewEdit->setPlainText(QString("Error: Invalid JSON configuration.\nParse error: %1").arg(error.errorString()));
                    if (m_proxyManager->proxyProcessState() == QProcess::Running) {
                        stopProxy();
                    }
                    return;
                }
            } else {
                // Empty file
                ui->configStatusLabel->setText(tr("Status: Empty config file"));
                ui->configPreviewEdit->setPlainText("Error: Configuration file is empty");
                if (m_proxyManager->proxyProcessState() == QProcess::Running) {
                    stopProxy();
                }
                return;
            }
        } else {
            // Cannot read file
            ui->configStatusLabel->setText(tr("Status: Cannot read config file"));
            ui->configPreviewEdit->setPlainText("Error: Cannot read configuration file");
            if (m_proxyManager->proxyProcessState() == QProcess::Running) {
                stopProxy();
            }
            return;
        }
    }
    
    // Fallback to local configs or show no config available
    if (m_configManager->configCount() == 0) {
        ui->configStatusLabel->setText(tr("Status: No configuration available"));
        ui->configPreviewEdit->setPlainText("No configuration available.\nPlease enter a subscription URL or import a configuration file.");
        if (m_proxyManager->proxyProcessState() == QProcess::Running)
        {
            stopProxy();
        }
    } else {
        QString name = m_configManager->configName();
        int index = m_configManager->configIndex();
        ui->configStatusLabel->setText(QString("Status: Using local config: %1").arg(name));
        ui->configPreviewEdit->setPlainText("Using local configuration file");
        m_proxyManager->setConfigFilePath(m_configManager->configFilePath());
        if (m_proxyManager->proxyProcessState() == QProcess::Running)
        {
            stopProxy();
            startProxy();
        }
    }
}

// New subscription slot implementations
void MainWindow::on_saveUrlButton_clicked()
{
    QString url = ui->subscriptionUrlEdit->text().trimmed();
    
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please enter a subscription URL."));
        return;
    }
    
    if (!isValidUrl(url)) {
        QMessageBox::warning(this, tr("Warning"), tr("Please enter a valid URL."));
        return;
    }
    
    m_subscriptionUrl = url;
    saveSubscriptionUrl();
    
    // Start the timer and fetch config immediately
    updateConfigStatus(tr("Saving URL and fetching config..."));
    updateSubscriptionConfig();
    m_updateTimer->start();
    
    QMessageBox::information(this, tr("Success"), tr("Subscription URL saved successfully!"));
}

void MainWindow::on_updateConfigButton_clicked()
{
    if (m_subscriptionUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("No subscription URL configured."));
        return;
    }
    
    updateConfigStatus(tr("Manually updating config..."));
    updateSubscriptionConfig();
}

void MainWindow::updateSubscriptionConfig()
{
    if (m_subscriptionUrl.isEmpty()) {
        return;
    }
    
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    QUrl url(m_subscriptionUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "qsing-box/" + QString(PROJECT_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &MainWindow::onConfigDownloadFinished);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &MainWindow::onConfigDownloadError);
    
    updateConfigStatus(tr("Downloading config from subscription..."));
}

void MainWindow::onConfigDownloadFinished()
{
    if (!m_currentReply) {
        return;
    }
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray configData = m_currentReply->readAll();
        
        if (!configData.isEmpty()) {
            // Validate downloaded config
            QJsonParseError error;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(configData, &error);
            
            if (error.error != QJsonParseError::NoError) {
                updateConfigStatus(tr("Error: Downloaded config is not valid JSON"));
                ui->configPreviewEdit->setPlainText(QString("Error: Invalid JSON downloaded.\nParse error: %1\n\nRaw content:\n%2")
                                                  .arg(error.errorString())
                                                  .arg(QString::fromUtf8(configData)));
                m_currentReply->deleteLater();
                m_currentReply = nullptr;
                return;
            }
            
            if (!jsonDoc.isObject()) {
                updateConfigStatus(tr("Error: Downloaded config is not a JSON object"));
                ui->configPreviewEdit->setPlainText("Error: Downloaded config is not a valid JSON object");
                m_currentReply->deleteLater();
                m_currentReply = nullptr;
                return;
            }
            
            QJsonObject config = jsonDoc.object();
            if (!config.contains("inbounds") && !config.contains("outbounds")) {
                updateConfigStatus(tr("Error: Downloaded config missing required sections"));
                ui->configPreviewEdit->setPlainText("Error: Downloaded configuration is missing required 'inbounds' or 'outbounds' sections.\nThis doesn't appear to be a valid sing-box configuration.");
                m_currentReply->deleteLater();
                m_currentReply = nullptr;
                return;
            }
            
            // Config is valid, display preview
            QString configText = jsonDoc.toJson(QJsonDocument::Indented);
            ui->configPreviewEdit->setPlainText(configText);
            
            // Save to file
            QFile file(m_configFilePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(configData);
                file.close();
                
                updateConfigStatus(tr("Config updated successfully. Last update: %1")
                                  .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
                
                // Update proxy config if running
                changeSelectedConfig();
            } else {
                updateConfigStatus(tr("Error: Failed to save config file"));
                ui->configPreviewEdit->setPlainText("Error: Failed to save config file");
            }
        } else {
            updateConfigStatus(tr("Error: Empty config received"));
            ui->configPreviewEdit->setPlainText("Error: Empty config received from subscription URL");
        }
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void MainWindow::onConfigDownloadError(QNetworkReply::NetworkError error)
{
    if (m_currentReply) {
        QString errorString = m_currentReply->errorString();
        
        // Provide specific error handling for TLS issues
        if (error == QNetworkReply::SslHandshakeFailedError || 
            errorString.contains("tls initialization failed", Qt::CaseInsensitive) ||
            errorString.contains("ssl", Qt::CaseInsensitive)) {
            
            // Check if OpenSSL is available
            QString tlsStatus = checkOpenSSLStatus();
            updateConfigStatus(tr("TLS Error: %1. %2").arg(errorString).arg(tlsStatus));
            ui->configPreviewEdit->setPlainText(QString("TLS Error: %1\n\nTroubleshooting:\n%2").arg(errorString).arg(tlsStatus));
        } else {
            updateConfigStatus(tr("Error downloading config: %1").arg(errorString));
            ui->configPreviewEdit->setPlainText(QString("Download Error: %1").arg(errorString));
        }
        
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void MainWindow::loadSubscriptionUrl()
{
    QSettings settings;
    m_subscriptionUrl = settings.value("subscription/url", "").toString();
    ui->subscriptionUrlEdit->setText(m_subscriptionUrl);
    
    if (!m_subscriptionUrl.isEmpty()) {
        updateConfigStatus(tr("Subscription URL loaded. Fetching config..."));
        // Start timer and fetch config immediately
        updateSubscriptionConfig();
        m_updateTimer->start();
    } else {
        updateConfigStatus(tr("No subscription configured"));
    }
}

void MainWindow::saveSubscriptionUrl()
{
    QSettings settings;
    settings.setValue("subscription/url", m_subscriptionUrl);
    settings.sync();
}

void MainWindow::updateConfigStatus(const QString &message)
{
    ui->configStatusLabel->setText(message);
}

bool MainWindow::isValidUrl(const QString &url)
{
    QUrl qurl(url);
    bool isValid = qurl.isValid() && (qurl.scheme() == "http" || qurl.scheme() == "https");
    
    // Show warning for HTTP URLs
    if (isValid && qurl.scheme() == "http") {
        QMessageBox::information(nullptr, tr("HTTP URL"), 
                               tr("Warning: Using HTTP instead of HTTPS. Data will not be encrypted."));
    }
    
    return isValid;
}

QString MainWindow::checkOpenSSLStatus()
{
    QString diagnostics = "OpenSSL Diagnostics:\n";
    
    // Check Qt SSL support
    if (!QSslSocket::supportsSsl()) {
        diagnostics += "- Qt SSL support: NOT AVAILABLE\n";
        diagnostics += "- This is likely the main issue. Qt was built without SSL support.\n";
    } else {
        diagnostics += "- Qt SSL support: Available\n";
        diagnostics += QString("- SSL Library Build Version: %1\n").arg(QSslSocket::sslLibraryBuildVersionString());
        diagnostics += QString("- SSL Library Runtime Version: %1\n").arg(QSslSocket::sslLibraryVersionString());
    }
    
    // Check for OpenSSL installation paths
    QStringList opensslPaths = {
        "C:/Program Files/OpenSSL-Win64/bin",
        "C:/Program Files (x86)/OpenSSL-Win32/bin", 
        "C:/OpenSSL-Win64/bin",
        "C:/OpenSSL-Win32/bin"
    };
    
    bool opensslFound = false;
    for (const QString &path : opensslPaths) {
        QDir dir(path);
        if (dir.exists() && QFile::exists(dir.filePath("openssl.exe"))) {
            diagnostics += QString("- OpenSSL found at: %1\n").arg(path);
            opensslFound = true;
            break;
        }
    }
    
    if (!opensslFound) {
        diagnostics += "- OpenSSL installation: NOT FOUND in standard locations\n";
        diagnostics += "- Try reinstalling OpenSSL from https://slproweb.com/products/Win32OpenSSL.html\n";
    }
    
    // Check PATH environment
    QString pathEnv = qEnvironmentVariable("PATH");
    bool opensslInPath = false;
    for (const QString &pathDir : pathEnv.split(';')) {
        if (pathDir.contains("OpenSSL", Qt::CaseInsensitive) && QDir(pathDir).exists()) {
            diagnostics += QString("- OpenSSL in PATH: %1\n").arg(pathDir);
            opensslInPath = true;
        }
    }
    
    if (!opensslInPath) {
        diagnostics += "- OpenSSL not found in PATH environment variable\n";
    }
    
    // Suggestions
    diagnostics += "\nSuggestions:\n";
    if (!QSslSocket::supportsSsl()) {
        diagnostics += "1. Reinstall Qt with SSL support or use the installer to get OpenSSL\n";
    }
    if (!opensslFound) {
        diagnostics += "2. Install OpenSSL from https://slproweb.com/products/Win32OpenSSL.html\n";
        diagnostics += "3. Run the qsing-box installer which includes automatic OpenSSL installation\n";
    }
    diagnostics += "4. Restart the application after installing OpenSSL\n";
    
    return diagnostics;
}
