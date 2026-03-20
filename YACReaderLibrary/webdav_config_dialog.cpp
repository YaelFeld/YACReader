#include "webdav_config_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QCheckBox>
#include <QMessageBox>
#include <QSettings>
#include <QtConcurrent>

#include "webdav_client.h"
#include "yacreader_global.h"

WebDAVConfigDialog::WebDAVConfigDialog(QWidget *parent)
    : QDialog(parent),
      m_testingConnection(false)
{
    setWindowTitle(tr("Configure WebDAV Library"));
    setMinimumWidth(450);
    
    setupUI();
    createConnections();
    updateUIState();
}

WebDAVConfigDialog::~WebDAVConfigDialog() = default;

void WebDAVConfigDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    auto *gridLayout = new QGridLayout();
    
    // Server URL
    auto *urlLabel = new QLabel(tr("Server URL:"));
    m_serverUrlEdit = new QLineEdit();
    m_serverUrlEdit->setPlaceholderText(tr("https://cloud.example.com"));
    urlLabel->setBuddy(m_serverUrlEdit);
    gridLayout->addWidget(urlLabel, 0, 0);
    gridLayout->addWidget(m_serverUrlEdit, 0, 1);
    
    // Username
    auto *usernameLabel = new QLabel(tr("Username:"));
    m_usernameEdit = new QLineEdit();
    usernameLabel->setBuddy(m_usernameEdit);
    gridLayout->addWidget(usernameLabel, 1, 0);
    gridLayout->addWidget(m_usernameEdit, 1, 1);
    
    // Password
    auto *passwordLabel = new QLabel(tr("Password:"));
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    passwordLabel->setBuddy(m_passwordEdit);
    gridLayout->addWidget(passwordLabel, 2, 0);
    gridLayout->addWidget(m_passwordEdit, 2, 1);
    
    // Base Path
    auto *pathLabel = new QLabel(tr("Base Path:"));
    m_basePathEdit = new QLineEdit();
    m_basePathEdit->setPlaceholderText(tr("/Comics (optional)"));
    pathLabel->setBuddy(m_basePathEdit);
    gridLayout->addWidget(pathLabel, 3, 0);
    gridLayout->addWidget(m_basePathEdit, 3, 1);
    
    // Library Name
    auto *nameLabel = new QLabel(tr("Library Name:"));
    m_libraryNameEdit = new QLineEdit();
    m_libraryNameEdit->setPlaceholderText(tr("My Nextcloud Comics"));
    nameLabel->setBuddy(m_libraryNameEdit);
    gridLayout->addWidget(nameLabel, 4, 0);
    gridLayout->addWidget(m_libraryNameEdit, 4, 1);
    
    // Save password checkbox
    m_savePasswordCheck = new QCheckBox(tr("Save password (not recommended)"));
    m_savePasswordCheck->setChecked(false);
    gridLayout->addWidget(m_savePasswordCheck, 5, 0, 1, 2);
    
    mainLayout->addLayout(gridLayout);
    
    // Info label
    auto *infoLabel = new QLabel(tr(
        "<p><b>Note:</b> For Nextcloud, use your server URL (e.g., https://cloud.example.com). "
        "The base path is the folder in your Nextcloud where comics are stored.</p>"
    ));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: gray; font-size: 11px;");
    mainLayout->addWidget(infoLabel);
    
    // Status and progress
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);
    
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0);  // Indeterminate
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    mainLayout->addStretch();
    
    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_testButton = new QPushButton(tr("Test Connection"));
    m_okButton = new QPushButton(tr("OK"));
    m_okButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("Cancel"));
    
    buttonLayout->addWidget(m_testButton);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

void WebDAVConfigDialog::createConnections()
{
    connect(m_testButton, &QPushButton::clicked, this, &WebDAVConfigDialog::testConnection);
    connect(m_okButton, &QPushButton::clicked, this, &WebDAVConfigDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    // Update OK button state when text changes
    connect(m_serverUrlEdit, &QLineEdit::textChanged, this, &WebDAVConfigDialog::updateUIState);
    connect(m_usernameEdit, &QLineEdit::textChanged, this, &WebDAVConfigDialog::updateUIState);
    connect(m_libraryNameEdit, &QLineEdit::textChanged, this, &WebDAVConfigDialog::updateUIState);
}

void WebDAVConfigDialog::updateUIState()
{
    bool hasRequiredFields = !m_serverUrlEdit->text().isEmpty() &&
                            !m_usernameEdit->text().isEmpty() &&
                            !m_libraryNameEdit->text().isEmpty();
    
    m_okButton->setEnabled(hasRequiredFields && !m_testingConnection);
    m_testButton->setEnabled(hasRequiredFields && !m_testingConnection);
}

QString WebDAVConfigDialog::serverUrl() const
{
    return m_serverUrlEdit->text().trimmed();
}

QString WebDAVConfigDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString WebDAVConfigDialog::password() const
{
    return m_passwordEdit->text();
}

QString WebDAVConfigDialog::basePath() const
{
    QString path = m_basePathEdit->text().trimmed();
    if (path.isEmpty()) {
        return "/";
    }
    // Ensure path starts with /
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    return path;
}

QString WebDAVConfigDialog::libraryName() const
{
    return m_libraryNameEdit->text().trimmed();
}

bool WebDAVConfigDialog::savePassword() const
{
    return m_savePasswordCheck->isChecked();
}

void WebDAVConfigDialog::setServerUrl(const QString &url)
{
    m_serverUrlEdit->setText(url);
}

void WebDAVConfigDialog::setUsername(const QString &username)
{
    m_usernameEdit->setText(username);
}

void WebDAVConfigDialog::setBasePath(const QString &path)
{
    m_basePathEdit->setText(path);
}

void WebDAVConfigDialog::setLibraryName(const QString &name)
{
    m_libraryNameEdit->setText(name);
}

void WebDAVConfigDialog::setSavePassword(bool save)
{
    m_savePasswordCheck->setChecked(save);
}

bool WebDAVConfigDialog::validateInputs()
{
    QString url = serverUrl();
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Please enter a server URL."));
        m_serverUrlEdit->setFocus();
        return false;
    }
    
    // Add https:// if no scheme provided
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        url = "https://" + url;
        m_serverUrlEdit->setText(url);
    }
    
    if (username().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Please enter a username."));
        m_usernameEdit->setFocus();
        return false;
    }
    
    if (libraryName().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Please enter a library name."));
        m_libraryNameEdit->setFocus();
        return false;
    }
    
    return true;
}

void WebDAVConfigDialog::testConnection()
{
    if (!validateInputs()) {
        return;
    }
    
    m_testingConnection = true;
    m_statusLabel->setText(tr("Testing connection..."));
    m_statusLabel->setStyleSheet("color: blue;");
    m_progressBar->setVisible(true);
    updateUIState();
    
    // Run test in background thread
    QtConcurrent::run([this]() {
        WebDAVClient client;
        client.setServerUrl(serverUrl());
        client.setCredentials(username(), password());
        
        bool success = client.testConnection();
        
        QMetaObject::invokeMethod(this, [this, success]() {
            onTestConnectionFinished(success);
        });
    });
}

void WebDAVConfigDialog::onTestConnectionFinished(bool success)
{
    m_testingConnection = false;
    m_progressBar->setVisible(false);
    updateUIState();
    
    if (success) {
        m_statusLabel->setText(tr("✓ Connection successful!"));
        m_statusLabel->setStyleSheet("color: green;");
    } else {
        m_statusLabel->setText(tr("✗ Connection failed. Please check your settings."));
        m_statusLabel->setStyleSheet("color: red;");
    }
}

void WebDAVConfigDialog::accept()
{
    if (!validateInputs()) {
        return;
    }
    
    QDialog::accept();
}
