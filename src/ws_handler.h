#ifndef WS_HANDLER_H
#define WS_HANDLER_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QtWebSockets/qwebsocket.h>
#include <QSize>
#include <QMap>

class WebSocketHandler : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketHandler(QObject *parent = Q_NULLPTR);

private:
    QWebSocket *m_webSocket;
    QTimer *m_timerReconnect;
    QTimer *m_timerWaitResponse;
    int     m_waitType;

    QString m_url;
    QString m_name;
    QString m_login;
    QString m_pass;

    bool       m_client_isAuthenticated;  // remote client
    QByteArray m_client_uuid;

    QByteArray m_uuid; // not used
    QByteArray m_nonce;

    QByteArray m_ws_stream;

signals:
    void finished();
    void getDesktop();
    void connectedStatus(bool);
    void authenticatedStatus(bool);
    void receivedTileNum(quint16 num);
    void changeDisplayNum();
    void setKeyPressed(quint16 keyCode, bool state);
    void setMousePressed(quint16 keyCode, bool state);
    void setWheelChanged(bool deltaPos);
    void setMouseMove(quint16 posX, quint16 posY);
    void setMouseDelta(qint16 deltaX, qint16 deltaY);
    void refreshDisplay();

    void disconnected(WebSocketHandler *pointer);
    void disconnectedUuid(const QByteArray &uuid);

    void newProxyConnection(WebSocketHandler *handler, const QByteArray &uuidSrc, const QByteArray &uuidDst);
    void proxyConnectionCreated(bool state);
    void connectedProxyClient(const QByteArray &uuid);
    void disconnectedProxyClient(const QByteArray &uuid);

    void sendTextMessage(QString message);

public slots:
    void createSocket();
    void removeSocket();
    void setUrl(const QString &url);
    void setName(const QString &name);
    QString getName();
    QByteArray getUuid();

    void setLoginPass(const QString &login, const QString &pass);

    QWebSocket *getSocket();

    void sendLoginNonce();

    void sendImageParameters(const QSize &imageSize, int rectWidth);
    void sendImageTile(quint16 posX, quint16 posY, const QByteArray &imageData, quint16 tileNum);
    void sendImageScreen(const QByteArray &imageData);
    void sendName(const QString &name);
    //void createProxyConnection(WebSocketHandler *handler, const QByteArray &uuid); // not used
    //void proxyHandlerDisconnected(const QByteArray &uuid);

private slots:
    void textMessageReceived(const QString &message);
    void binaryMessageReceived(const QByteArray &data);
    void newData(const QByteArray &command, const QByteArray &data);
    void sendBinaryMessage(const QByteArray &data);

    void sendAuthenticationResponse(bool state);
    QByteArray getHashSum(const QByteArray &nonce, const QString &login, const QString &pass);
    void WSocketStateChanged(QAbstractSocket::SocketState state);

    void WSocketConnected();
    void WSocketDisconnected();

    void timerReconnectTick();
    void startWaitResponseTimer(int msec, int type);
    void stopWaitResponseTimer();
    void timerWaitResponseTick();

    void debugHexData(const QByteArray &data);

public:
    //static QByteArray arrayFromUint16(quint16 number);
    static QByteArray arrayFromUint32(quint32 number);
    static quint16 uint16FromArray(const QByteArray &buf);
    static quint32 uint32FromArray(const QByteArray &buf);
};

#endif // WS_HANDLER_H
