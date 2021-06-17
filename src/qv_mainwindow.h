#ifndef QV_MAINWINDOW_H
#define QV_MAINWINDOW_H

#include <QObject>
#include <QWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMainWindow>

#include "ws_handler.h"
#include "screen_capture.h"
#include "input_simulator.h"

QT_BEGIN_NAMESPACE
namespace Ui { class QV_MainWindow; }
QT_END_NAMESPACE


class QV_MainWindow : public QMainWindow
{
    Q_OBJECT

public:
  explicit QV_MainWindow(QWidget *parent = Q_NULLPTR);

private:
    Ui::QV_MainWindow *m_ui;

    WebSocketHandler *m_proxySocketHandler;
    ScreenCapture *m_graberClass;
    InputSimulator *m_inputSimulator;
    QMenu *m_trayMenu;
    QSystemTrayIcon *m_trayIcon;

    bool    m_isConnectedToProxy;
    QString m_device_access_id;
    QString m_device_access_pw;

    QList<QByteArray> m_remoteClientsList;

signals:
    void closeAppSignal();

public slots:

private slots:
    void actionTriggered(QAction* action);
    void showInfoMessage();
    void loadSettings();

    void startOutboundProxyConnection(const QString &host, const QString &name, const QString &login, const QString &pass);
    void createConnectionToHandler(WebSocketHandler *webSocketHandler);

    void finishedWebSockeTransfer();
    void finishedWebSockeHandler();

    void remoteClientConnected(const QByteArray &uuid);
    void remoteClientDisconnected(const QByteArray &uuid);

    void showTextMessageDialog(QString message);

    void connectedToProxyServer(bool state);
    void on_btn_remote_connect_released();
    void on_text_remote_id_textChanged(const QString &arg1);
};


#endif // QV_MAINWINDOW_H
