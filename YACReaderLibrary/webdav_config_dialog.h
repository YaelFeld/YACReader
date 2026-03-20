#ifndef WEBDAV_CONFIG_DIALOG_H
#define WEBDAV_CONFIG_DIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QCheckBox;

/**
 * @brief Dialog for configuring WebDAV/Nextcloud library connection
 * 
 * This dialog allows users to configure connection settings for
 * a WebDAV-based library (e.g., Nextcloud, ownCloud, or any WebDAV server)
 */
class WebDAVConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebDAVConfigDialog(QWidget *parent = nullptr);
    ~WebDAVConfigDialog() override;

    // Getters for configuration values
    QString serverUrl() const;
    QString username() const;
    QString password() const;
    QString basePath() const;
    QString libraryName() const;
    bool savePassword() const;

    // Setters for editing existing configuration
    void setServerUrl(const QString &url);
    void setUsername(const QString &username);
    void setBasePath(const QString &path);
    void setLibraryName(const QString &name);
    void setSavePassword(bool save);

public slots:
    void accept() override;

private slots:
    void testConnection();
    void onTestConnectionFinished(bool success);
    void updateUIState();

private:
    void setupUI();
    void createConnections();
    bool validateInputs();

    // UI Components
    QLineEdit *m_serverUrlEdit;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_basePathEdit;
    QLineEdit *m_libraryNameEdit;
    QCheckBox *m_savePasswordCheck;
    
    QPushButton *m_testButton;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;

    // State
    bool m_testingConnection;
};

#endif // WEBDAV_CONFIG_DIALOG_H
