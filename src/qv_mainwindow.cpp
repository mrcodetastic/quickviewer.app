
#include <QNetworkInterface>
#include <QApplication>
#include <QCommonStyle>
#include <QSettings>
#include <QHostInfo>
#include <QThread>
#include <QDebug>
#include <QUuid>
#include <QSysInfo>
#include <QDesktopServices>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include "ui_qv_mainwindow.h"
#include "qv_mainwindow.h"


QV_MainWindow::QV_MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::QV_MainWindow),
    m_proxySocketHandler(Q_NULLPTR),
    m_graberClass(new ScreenCapture(this)),
    m_inputSimulator(new InputSimulator(this)),
    m_trayMenu(new QMenu),
    m_trayIcon(new QSystemTrayIcon(this)),
    m_isConnectedToProxy(false),

    m_device_access_id(QString("UNKNOWN")),
    m_device_access_pw(QString("UNKNOWN"))
{

    m_ui->setupUi(this);

    QCommonStyle style;
    // m_trayMenu->addAction(QIcon(style.standardPixmap(QStyle::SP_DialogCancelButton)),"Exit");
    //m_trayIcon->setContextMenu(m_trayMenu);

    m_trayIcon->setIcon(QIcon(":/res/favicon.ico"));
    m_trayIcon->setToolTip("QuickViewer.app");
    m_trayIcon->show();

    connect(m_trayMenu,SIGNAL(triggered(QAction*)),this,SLOT(actionTriggered(QAction*)));

    // Set default statusbar    
    m_ui->statusBar->setStyleSheet(("background-color: #CCC;"));
    m_ui->statusBar->showMessage(QString("Not Ready. Awaiting Internet."));

    // Setup validator on 'Connect to remote device'
    // regexp: optional '-' followed by between 1 and 3 digits
    QRegularExpression rx("\\d*");
    QValidator *validator = new QRegularExpressionValidator(rx, this);

    //QLineEdit *edit = new QLineEdit(this);
    m_ui->text_remote_id->setValidator(validator);

    loadSettings();

    m_graberClass->start();

} // Qv Main Window

void QV_MainWindow::actionTriggered(QAction *action)
{
    if(action->text() == "Help")
        showInfoMessage();
    else if(action->text() == "Exit")
        emit closeAppSignal();
}

void QV_MainWindow::showInfoMessage()
{

    if (m_isConnectedToProxy)
    {
        if ( m_remoteClientsList.size() == 0) // no active connections.
        {
            m_ui->statusBar->setStyleSheet(("background-color: #baffcd;"));
            m_ui->statusBar->showMessage(QString("Ready for remote connections."));
        } else
        {
            m_ui->statusBar->setStyleSheet(("background-color: #00FF00;"));
            m_ui->statusBar->showMessage(QString("A remote device is connected to this computer!"));
        }

        // Need a disconnect notification

        m_ui->text_this_id->setText(m_device_access_id);
        m_ui->text_this_pw->setText(m_device_access_pw);


        m_trayIcon->showMessage("QuickViewer.app", QString("Connected and ready for remote access to this PC."), QSystemTrayIcon::Information);
    }
    else
    {
        m_ui->statusBar->setStyleSheet(("background-color: #CCC;"));
        m_ui->statusBar->showMessage(QString("No Internet Connection."));

    }

} // showInfoMessage



void QV_MainWindow::showTextMessageDialog(QString message)
{

    QMessageBox msgBox;
    msgBox.setText(message);
    msgBox.exec();
}

void QV_MainWindow::loadSettings()
{

#ifdef QT_DEBUG
    //QString     proxyHost                   = "ws://192.168.8.100:8080";
    QString     proxyHost                   = "ws://quickviewer.app:8080";
#else
    QString     proxyHost                   = "ws://quickviewer.app:8080";
#endif

    QString     machineName                 = QHostInfo::localHostName();

    // Calculate machine ID
    QByteArray access_id = QSysInfo::machineUniqueId();

    if (access_id.size() == 0) {
        access_id = QUuid::createUuid().toByteArray();
    }

    // qDebug() << "Random UUID is: "  << QUuid::createUuid();
    // qDebug() << "OS UUID is: "      << access_id;

    quint32 machine_id = 0;
    for (int i = 0; i < access_id.size(); i++) {
        machine_id += access_id[i];

        if (i % 4 == 0) {
            machine_id *= 6.499;
        }
    }

    QString machine_id_str = QString::number(machine_id);
    // qDebug() << "Obfuscated ID is: " << machine_id_str;


    // Calculate a password
    QString password = QUuid::createUuid().toString().mid(1,7);

    // qDebug() << "Password is: " << password;

    m_device_access_id = machine_id_str;
    m_device_access_pw = password;

    startOutboundProxyConnection(proxyHost, machineName, machine_id_str, password); // try connect to a proxy

} // loadSettings


// outgoing proxy connection
void QV_MainWindow::startOutboundProxyConnection(const QString &host, const QString &name, const QString &login, const QString &pass)
{
    // qDebug() << "RemoteDesktopMain::startOutboundProxyConnection to host: "<< host;

    QThread *thread = new QThread;
    m_proxySocketHandler = new WebSocketHandler;
    m_proxySocketHandler->setUrl(host);
    m_proxySocketHandler->setName(name);
    m_proxySocketHandler->setLoginPass(login, pass);

    connect(thread,                 &QThread::started,                  m_proxySocketHandler,   &WebSocketHandler::createSocket); // this kicks things off on thread start
    connect(this,                   &QV_MainWindow::closeAppSignal,     m_proxySocketHandler,   &WebSocketHandler::removeSocket);

    connect(m_proxySocketHandler,   &WebSocketHandler::finished,        this,                   &QV_MainWindow::finishedWebSockeHandler);
    connect(m_proxySocketHandler,   &WebSocketHandler::finished,        thread,                 &QThread::quit);

    connect(thread,                 &QThread::finished,                 m_proxySocketHandler,   &WebSocketHandler::deleteLater);
    connect(thread,                 &QThread::finished,                 thread,                 &QThread::deleteLater);

    // Connect to proxy
    connect(m_proxySocketHandler,   &WebSocketHandler::connectedStatus,         this,           &QV_MainWindow::connectedToProxyServer); // UI update only

    // Wait for incoming requests
    connect(m_proxySocketHandler,   &WebSocketHandler::connectedProxyClient,    this,           &QV_MainWindow::remoteClientConnected); //?
    connect(m_proxySocketHandler,   &WebSocketHandler::disconnectedProxyClient, this,           &QV_MainWindow::remoteClientDisconnected);

    // message
    connect(m_proxySocketHandler,   &WebSocketHandler::sendTextMessage,     this,               &QV_MainWindow::showTextMessageDialog);


    // Get the socket up and running now hand over to the handler
    createConnectionToHandler(m_proxySocketHandler);

    m_proxySocketHandler->moveToThread(thread);
    thread->start();

} //startOutboundProxyConnection


void QV_MainWindow::createConnectionToHandler(WebSocketHandler *webSocketHandler)
{
    if(!webSocketHandler)
        return;

    connect(m_graberClass, &ScreenCapture::imageParameters,   webSocketHandler, &WebSocketHandler::sendImageParameters);
    connect(m_graberClass, &ScreenCapture::imageTile,         webSocketHandler, &WebSocketHandler::sendImageTile);
    connect(m_graberClass, &ScreenCapture::imageScreen,       webSocketHandler, &WebSocketHandler::sendImageScreen);
    connect(m_graberClass, &ScreenCapture::screenPositionChanged,     m_inputSimulator, &InputSimulator::setScreenPosition);

    connect(webSocketHandler, &WebSocketHandler::getDesktop,        m_graberClass, &ScreenCapture::startSending); // only on get desktop
    connect(webSocketHandler, &WebSocketHandler::disconnected,      m_graberClass, &ScreenCapture::stopSending); // even though this occurs on proxy client disconnect

    connect(webSocketHandler, &WebSocketHandler::changeDisplayNum,  m_graberClass, &ScreenCapture::changeScreenNum);
    connect(webSocketHandler, &WebSocketHandler::refreshDisplay,    m_graberClass, &ScreenCapture::updateScreen);
    connect(webSocketHandler, &WebSocketHandler::receivedTileNum,   m_graberClass, &ScreenCapture::setReceivedTileNum);

    connect(webSocketHandler, &WebSocketHandler::setKeyPressed,     m_inputSimulator, &InputSimulator::simulateKeyboard);
    connect(webSocketHandler, &WebSocketHandler::setMousePressed,   m_inputSimulator, &InputSimulator::simulateMouseKeys);
    connect(webSocketHandler, &WebSocketHandler::setMouseMove,      m_inputSimulator, &InputSimulator::simulateMouseMove);
    connect(webSocketHandler, &WebSocketHandler::setWheelChanged,   m_inputSimulator, &InputSimulator::simulateWheelEvent);
    connect(webSocketHandler, &WebSocketHandler::setMouseDelta,     m_inputSimulator, &InputSimulator::setMouseDelta);

}

void QV_MainWindow::finishedWebSockeTransfer()
{
    //m_webSocketTransfer = Q_NULLPTR;

    if(!m_proxySocketHandler)
        QApplication::quit();
}

void QV_MainWindow::finishedWebSockeHandler()
{
    m_proxySocketHandler = Q_NULLPTR;

    //if(!m_webSocketTransfer)
     //   QApplication::quit();
}

void QV_MainWindow::remoteClientConnected(const QByteArray &uuid)
{
    // qDebug() << "Remote Client Connected.";

    if(!m_remoteClientsList.contains(uuid))
        m_remoteClientsList.append(uuid);

    showInfoMessage();

    // qDebug()<<"RemoteDesktopUniting::m_remoteClientsList"<<m_remoteClientsList.size();

}

void QV_MainWindow::remoteClientDisconnected(const QByteArray &uuid)
{
    // qDebug() << "Number of remote clients connected: " << m_remoteClientsList.size();

    // qDebug() << "Remote Client Disconnected.";

    if(m_remoteClientsList.contains(uuid))
    {
        m_remoteClientsList.removeOne(uuid);

        if(m_remoteClientsList.size() == 0)
            m_graberClass->stopSending();
    }

    // showInfoMessage();
    if ( m_isConnectedToProxy )
    {
        m_ui->statusBar->setStyleSheet(("background-color: #CCC;"));
        m_ui->statusBar->showMessage(QString("A remote device has DISCONNECTED from this computer."));
    }

    m_isConnectedToProxy = false;
    // qDebug()<<"RemoteDesktopUniting::remoteClientDisconnected"<<m_remoteClientsList.size();

}

void QV_MainWindow::connectedToProxyServer(bool state)
{
    if(m_isConnectedToProxy != state)
    {
        m_isConnectedToProxy = state;

        if(state)
            showInfoMessage();
    }
}

void QV_MainWindow::on_btn_remote_connect_released()
{
//    QString remote_id = m_ui->text_remote_id
    QString remote_id = m_ui->text_remote_id->text();

    QDesktopServices::openUrl(QUrl("https://quickviewer.app/go/?id=" + remote_id, QUrl::TolerantMode));
}

void QV_MainWindow::on_text_remote_id_textChanged(const QString &arg1)
{
   // // qDebug() << "Text Changed";
    if (arg1.length() > 5) {
        m_ui->btn_remote_connect->setEnabled(true);
    } else {
        m_ui->btn_remote_connect->setEnabled(false);
    }
}
