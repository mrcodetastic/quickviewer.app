#include "ws_handler.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QUuid>
#include <QJsonArray>

/*  https://stackoverflow.com/questions/50257210/convert-qbytearray-from-big-endian-to-little-endian/50257560
 *  The concept of endianness only applies to multi-byte data types. So the "right" order depends on what the data type is.
 *  But if you just want to reverse the array, I'd use std::reverse from the C++ algorithms library. – bnaecker May 9 '18 at 15:54
 */

// Packet & Proxy Control
static const QByteArray KEY_PKT_HEADR           = QString("1111").toUtf8(); // 8bit ASCI CODE 219 - https://theasciicode.com.ar/extended-ascii-code/block-graphic-character-ascii-code-219.html
static const QByteArray KEY_REGISTER            = QString("REGO").toUtf8();

// Actual Desktop Sharing
static const QByteArray KEY_SET_NAME            = QString("STNM").toUtf8(); // i.e. 'DESKTOP-XYZ'
static const QByteArray KEY_GET_IMAGE           = QString("GIMG").toUtf8();
static const QByteArray KEY_IMAGE_PARAM         = QString("IMGP").toUtf8();
static const QByteArray KEY_IMAGE_TILE          = QString("IMGT").toUtf8();
static const QByteArray KEY_IMAGE_SCREEN        = QString("IMGS").toUtf8();
static const QByteArray KEY_SET_KEY_STATE       = QString("SKST").toUtf8();
static const QByteArray KEY_SET_CURSOR_POS      = QString("SCUP").toUtf8();
static const QByteArray KEY_SET_CURSOR_DELTA    = QString("SCUD").toUtf8();
static const QByteArray KEY_SET_MOUSE_KEY       = QString("SMKS").toUtf8();
static const QByteArray KEY_SET_MOUSE_WHEEL     = QString("SMWH").toUtf8();
static const QByteArray KEY_CHANGE_DISPLAY      = QString("CHDP").toUtf8();
static const QByteArray KEY_REFRESH_DISPLAY     = QString("REFH").toUtf8();
static const QByteArray KEY_TILE_RECEIVED       = QString("TLRD").toUtf8();

// Authentication etc.
static const QByteArray KEY_CONNECT_UUID                = QString("CTUU").toUtf8();
static const QByteArray KEY_SET_NONCE                   = QString("STNC").toUtf8();
static const QByteArray KEY_SET_AUTH_REQUEST            = QString("SARQ").toUtf8();
static const QByteArray KEY_SET_AUTH_RESPONSE           = QString("SARP").toUtf8();

static const QByteArray KEY_CONNECTED_PROXY_CLIENT      = QString("CNPC").toUtf8();
static const QByteArray KEY_DISCONNECTED_PROXY_CLIENT   = QString("DNPC").toUtf8();

const int CLIENT_VERSION    = 2;

WebSocketHandler::WebSocketHandler(QObject *parent) : QObject(parent),
    m_webSocket(Q_NULLPTR),
    m_timerReconnect(Q_NULLPTR),
    m_client_isAuthenticated(false),
    m_ws_stream(QByteArray())
{

}

void WebSocketHandler::createSocket()
{
    if(m_webSocket)
        return;

    m_webSocket = new QWebSocket("",QWebSocketProtocol::VersionLatest,  this);

    // Socket State
    connect(m_webSocket, &QWebSocket::stateChanged,          this, &WebSocketHandler::WSocketStateChanged);

    // Signal: We're up and running?
    connect(m_webSocket, &QWebSocket::connected,             this, &WebSocketHandler::WSocketConnected);

    // Signal: Proxy closes connection
    connect(m_webSocket, &QWebSocket::disconnected,             this, &WebSocketHandler::WSocketDisconnected);

    // WS message recieved
    connect(m_webSocket, &QWebSocket::textMessageReceived,  this, &WebSocketHandler::textMessageReceived);
    connect(m_webSocket, &QWebSocket::binaryMessageReceived,this, &WebSocketHandler::binaryMessageReceived);

    if(!m_timerReconnect) {
        m_timerReconnect = new QTimer(this);
        connect(m_timerReconnect, &QTimer::timeout, this, &WebSocketHandler::timerReconnectTick);
    }

    // qDebug()<<"WebSocketHandler::createSocket: "<< m_url;

    m_webSocket->open(QUrl(m_url)); // open socket @ url

} // create socket


void WebSocketHandler::WSocketConnected()
{
    // qDebug()<<"WebSocketHandler::WSocketConnected - Sending Registration Request";

    // Send proxy hello and registration request -> largely pointless
    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_REGISTER);
    data.append("|");
    data.append(m_login.toUtf8());
    data.append("|");
    data.append(QString::number(CLIENT_VERSION).toUtf8());
    data.append("|");
    sendBinaryMessage(data);

}

QWebSocket *WebSocketHandler::getSocket()
{
    return m_webSocket;
}


// is used
void WebSocketHandler::removeSocket()
{
    if(m_timerReconnect)
    {
        if(m_timerReconnect->isActive())
            m_timerReconnect->stop();

        m_timerReconnect->deleteLater();
        m_timerReconnect = Q_NULLPTR;
    }

    if(m_timerWaitResponse)
    {
        if(m_timerWaitResponse->isActive())
            m_timerWaitResponse->stop();

        m_timerWaitResponse->deleteLater();
        m_timerWaitResponse = Q_NULLPTR;
    }

    if(m_webSocket)
    {
        if(m_webSocket->state() != QAbstractSocket::UnconnectedState)
            m_webSocket->close();

        m_webSocket->disconnect();
        m_webSocket->deleteLater();
        m_webSocket = Q_NULLPTR;
    }

    emit finished();
}

// is used
void WebSocketHandler::setUrl(const QString &url)
{
    m_url = url;
    // qDebug()<<"SocketWeb::setUrl"<<url<<QUrl(url).isValid();
}
// is used

// is used
void WebSocketHandler::setName(const QString &name)
{
    m_name = name;
}

// is used
QString WebSocketHandler::getName()
{
    return m_name;
}

// is used
QByteArray WebSocketHandler::getUuid()
{
    return m_uuid;
}

// is used
void WebSocketHandler::setLoginPass(const QString &login, const QString &pass)
{
    m_login = login;
    m_pass = pass;
}


void WebSocketHandler::sendLoginNonce()
{
    // qDebug() << "WebSocketHandler::sendLoginNonce";

    m_uuid  = QUuid::createUuid().toRfc4122();
    m_nonce = QUuid::createUuid().toRfc4122();

    QByteArray data;

    data.append(KEY_PKT_HEADR);
    data.append(KEY_SET_NONCE);
    data.append(arrayFromUint32(static_cast<quint32>(m_nonce.size()))); // packet size
    data.append(m_nonce);

    sendBinaryMessage(data);
}



/* Sends the full display screen dimensions */
void WebSocketHandler::sendImageParameters(const QSize &imageSize, int rectWidth)
{
    if(!m_client_isAuthenticated)
        return;

    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_IMAGE_PARAM);
    data.append(arrayFromUint32(sizeof(quint32)*3)); //payload size
    data.append(arrayFromUint32(static_cast<quint32>(imageSize.width())));
    data.append(arrayFromUint32(static_cast<quint32>(imageSize.height())));
    data.append(arrayFromUint32(static_cast<quint32>(rectWidth)));

    // qDebug()<<"WebSocketHandler::sendImageParameters - screen width: " <<imageSize.width();
    // qDebug()<<"WebSocketHandler::sendImageParameters - screen height: " <<imageSize.height();


    sendBinaryMessage(data);
}

void WebSocketHandler::sendImageTile(quint16 posX, quint16 posY, const QByteArray &imageData, quint16 tileNum)
{
    if(!m_client_isAuthenticated)
        return;

    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_IMAGE_TILE);
    data.append(arrayFromUint32(static_cast<quint32>(imageData.size() + sizeof(quint32)*3))); // payload size
    data.append(arrayFromUint32(static_cast<quint32>(posX)));
    data.append(arrayFromUint32(static_cast<quint32>(posY)));
    data.append(arrayFromUint32(static_cast<quint32>(tileNum)));
    data.append(imageData);

    sendBinaryMessage(data);
    // qDebug()<<"WebSocketHandler::sendImageTile";
}

void WebSocketHandler::sendImageScreen(const QByteArray &imageData)
{
    if(!m_client_isAuthenticated)
        return;

    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_IMAGE_SCREEN);
    data.append(arrayFromUint32(static_cast<quint32>(imageData.size()))); // UINT 32!!!!!!!!!
    data.append(imageData);

    sendBinaryMessage(data);
    // qDebug()<<"WebSocketHandler::sendImageScreen";
     // qDebug()<<"WebSocketHandler::sendImageScreen - payload size: " << (quint32)imageData.size() << " bytes.";
}

void WebSocketHandler::sendName(const QString &name)
{
    if(!m_client_isAuthenticated)
        return;

    QByteArray nameArray = name.toUtf8();
    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_SET_NAME);
    data.append(arrayFromUint32(static_cast<quint32>(nameArray.size()))); // payload size
    data.append(nameArray);

    sendBinaryMessage(data);
}

/*
void WebSocketHandler::createProxyConnection(WebSocketHandler *handler, const QByteArray &uuid)
{

    // qDebug()<<"WebSocketHandler::createProxyConnection";

    disconnect(handler->getSocket(), &QWebSocket::binaryMessageReceived,handler, &WebSocketHandler::binaryMessageReceived);

    // Bidirectonal
    connect(m_webSocket, &QWebSocket::binaryMessageReceived, handler->getSocket(), &QWebSocket::sendBinaryMessage);
    connect(handler->getSocket(), &QWebSocket::binaryMessageReceived, m_webSocket, &QWebSocket::sendBinaryMessage);


    connect(handler, &WebSocketHandler::disconnectedUuid, this, &WebSocketHandler::proxyHandlerDisconnected);
    connect(this, &WebSocketHandler::proxyConnectionCreated, handler, &WebSocketHandler::sendAuthenticationResponse);
    emit proxyConnectionCreated(true);

    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_CONNECTED_PROXY_CLIENT);
    data.append(arrayFromUint32(static_cast<quint32>(uuid.size()))); // payload size
    data.append(uuid);
    sendBinaryMessage(data);
}


void WebSocketHandler::proxyHandlerDisconnected(const QByteArray &uuid)
{
    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_DISCONNECTED_PROXY_CLIENT);
    data.append(arrayFromUint32(static_cast<quint32>(uuid.size()))); // payload size
    data.append(uuid);
    sendBinaryMessage(data);
}
*/

void WebSocketHandler::newData(const QByteArray &command, const QByteArray &data)
{
    #ifdef QT_DEBUG
        qDebug()<<"DataParser::newData"<<command<<data;
    #endif

    if(!m_client_isAuthenticated)
    {
        if(command == KEY_REGISTER) // register request response from server
        {
            if (m_login == QString(data))
            {
                // qDebug() << "Proxy server has acknowledged our ID.";
                emit connectedStatus(true);
            }
        }
        else if(command == KEY_CONNECT_UUID) // we have an incoming request from a web client via. proxy server
        {
            // qDebug() << "Remote ID connecting: " << QString(data);
            m_client_uuid = data; //QString(data).left(36); // UUID is 36 chars: Remote ID connecting:  "7f0d63be-03bd-4894-9b48-069f2e93ae2d"
            sendLoginNonce(); // Send and then see what happens next
        }
        else if(command == KEY_SET_AUTH_REQUEST)
        {
            if(data.toBase64() == getHashSum(m_nonce, m_login, m_pass))
            {
                if(!m_client_isAuthenticated)
                {
                    m_client_isAuthenticated = true;
                }
                    // qDebug() << "Remote Client is authenticted!";
                    emit connectedProxyClient(m_client_uuid);

            } else {
                m_client_isAuthenticated = false;
            }

            // qDebug()<<"data.toBase64: "<< data.toBase64();
            // qDebug()<<"getHash: "<< getHashSum(m_nonce, m_login, m_pass);

            sendAuthenticationResponse(m_client_isAuthenticated);
            // qDebug()<<"Authentication attempt: "<<m_client_isAuthenticated;

            return;
        }
        else
        {
            // qDebug() << "Unknown command.";
            debugHexData(data);
        }

        return;

    } // end is Not Authenticated code

    if(command == KEY_IMAGE_TILE)
    {
        return;
    }
    else if(command == KEY_IMAGE_SCREEN)
    {
        return;
    }
    else if(command == KEY_GET_IMAGE)
    {
        sendName(m_name);
        emit getDesktop();
    }
    else if(command == KEY_TILE_RECEIVED)
    {
        quint16 tileNum = uint16FromArray(data.mid(0,2));
        emit receivedTileNum(tileNum);
    }
    else if(command == KEY_CHANGE_DISPLAY)
    {
        emit changeDisplayNum();
    }
    else if(command == KEY_REFRESH_DISPLAY)
    {
        emit refreshDisplay();
    }
    else if(command == KEY_SET_CURSOR_POS)
    {
        if(data.size() >= 4)
        {
            quint16 posX = uint16FromArray(data.mid(0,2));
            quint16 posY = uint16FromArray(data.mid(2,2));
            emit setMouseMove(posX, posY);
        }
    }
    else if(command == KEY_SET_CURSOR_DELTA)
    {
        if(data.size() >= 4)
        {
            qint16 deltaX = static_cast<qint16>(uint16FromArray(data.mid(0,2)));
            qint16 deltaY = static_cast<qint16>(uint16FromArray(data.mid(2,2)));
            emit setMouseDelta(deltaX, deltaY);
        }
    }
    else if(command == KEY_SET_KEY_STATE)
    {
        if(data.size() >= 4)
        {
            quint16 keyCode = uint16FromArray(data.mid(0,2));
            quint16 keyState = uint16FromArray(data.mid(2,2));
            emit setKeyPressed(keyCode,static_cast<bool>(keyState));
        }
    }
    else if(command == KEY_SET_MOUSE_KEY)
    {
        if(data.size() >= 4)
        {
            quint16 keyCode = uint16FromArray(data.mid(0,2));
            quint16 keyState = uint16FromArray(data.mid(2,2));
            emit setMousePressed(keyCode,static_cast<bool>(keyState));
        }
    }
    else if(command == KEY_SET_MOUSE_WHEEL)
    {
        if(data.size() >= 4)
        {
            quint16 keyState = uint16FromArray(data.mid(2,2));
            emit setWheelChanged(static_cast<bool>(keyState));
        }
    }
    else if(command == KEY_SET_NAME)
    {
        m_name = QString::fromUtf8(data);
        // qDebug()<<"New desktop connected:"<<m_name;
    }

    else if(command == KEY_CONNECTED_PROXY_CLIENT)
    {
        //emit connectedProxyClient(data);
    }
    else if(command == KEY_DISCONNECTED_PROXY_CLIENT)
    {
        //emit disconnectedProxyClient(data);
    }

    else
    {

        qDebug()<<"DataParser::newData unknown command & data: "<<command<<data;
        debugHexData(data);
    }
}

void WebSocketHandler::sendAuthenticationResponse(bool state)
{
    QByteArray data;
    data.append(KEY_PKT_HEADR);
    data.append(KEY_SET_AUTH_RESPONSE);
    data.append(arrayFromUint32(sizeof(quint32))); // payload size
    data.append(arrayFromUint32(static_cast<quint32>(state)));

    sendBinaryMessage(data);

    WebSocketHandler *handler = static_cast<WebSocketHandler*>(sender());
    disconnect(handler, &WebSocketHandler::proxyConnectionCreated, this, &WebSocketHandler::sendAuthenticationResponse);
}


QByteArray WebSocketHandler::getHashSum(const QByteArray &nonce, const QString &login, const QString &pass)
{
    QString sum = login + pass;
    QByteArray concatFirst = QCryptographicHash::hash(sum.toUtf8(),QCryptographicHash::Md5).toBase64();
    concatFirst.append(nonce.toBase64());
    QByteArray result = QCryptographicHash::hash(concatFirst,QCryptographicHash::Md5).toBase64();
    return result;
}

void WebSocketHandler::WSocketStateChanged(QAbstractSocket::SocketState state)
{
    switch(state)
    {
        case QAbstractSocket::UnconnectedState:
        {
            if(m_timerReconnect)
                if(!m_timerReconnect->isActive())
                    m_timerReconnect->start(5000);
            break;
        }
        case QAbstractSocket::HostLookupState:
        {
            break;
        }
        case QAbstractSocket::ConnectingState:
        {
            if(m_timerReconnect)
                if(!m_timerReconnect->isActive())
                    m_timerReconnect->start(5000);
            break;
        }
        case QAbstractSocket::ConnectedState:
        {
            // qDebug()<<"WebSocketHandler::socketStateChanged: Connected to server.";
            //emit connectedStatus(true);
            // We aren't connected until we get a confirmation from the server.
            break;
        }
        case QAbstractSocket::ClosingState:
        {
            // qDebug()<<"WebSocketHandler::socketStateChanged: Disconnected from server.";
            emit connectedStatus(false);
            m_client_isAuthenticated = false;

            if(m_timerReconnect)
                if(!m_timerReconnect->isActive())
                    m_timerReconnect->start(5000);

            break;
        }
        default:
        {
            break;
        }
    }
}

void WebSocketHandler::WSocketDisconnected()
{
    //emit disconnectedUuid(m_uuid);
    //emit disconnected(this);
    emit disconnectedProxyClient(m_client_uuid);
    m_client_isAuthenticated = false;
    m_client_uuid = QByteArray();

    emit disconnected(this);

}

void WebSocketHandler::sendBinaryMessage(const QByteArray &data)
{
    if(m_webSocket)
        if(m_webSocket->state() == QAbstractSocket::ConnectedState)
            m_webSocket->sendBinaryMessage(data);
}

void WebSocketHandler::textMessageReceived(const QString &message)
{
    // qDebug() << "WebSocketHandler::textMessageReceived:" << message;
    if (message.startsWith("TXTMSG:") && message.length() > 16)
    {
        emit sendTextMessage(message.mid(7));
    }
}


// It's a fucking stream protocol like TCP. Treat it as such.
// QT client does not recieve data with a packet start header, javascript Web Client Does
void WebSocketHandler::binaryMessageReceived(const QByteArray &data)
{
#ifdef QT_DEBUG
    qDebug() << "WebSocketHandler::binaryMessageReceived!";
    debugHexData(data);
#endif

    /*

 //   if (!data.startsWith(KEY_PKT_HEADR)) {
   //         // qDebug() << "Did not get packet start header.";
    //        return;
  //  }


    if (data.length() < 4) { // always have a message header and then the COMMAND_SIZE
            // qDebug() << "Recieved binary message is too short.";
            return;
    }
    else if (data.length() > 1024) {
        // qDebug() << "Recieved binary message is too large.";
        return;
    }
    else if ( data.length() == 4 ) { // packet header only

        QByteArray command = QByteArray(data.mid(0,4));
        newData(command, QByteArray());
    }
    else
    {
        QByteArray command = data.mid(0,4);
        quint32 size       = uint32FromArray(data.mid(4,4)); // ignored to be honest
        QByteArray payload = data.mid(8);

        // qDebug() << "Command recieved: " + command;
        //// qDebug() << "Payload Size: "     + size;
        // qDebug() << "Payload recieved: " + payload;

        newData(command, payload);

    }
    */

    const quint32 msg_command_size_bytes = 4;
    const quint32 msg_payload_size_bytes = 4;



    QByteArray activeBuf = m_ws_stream;
    activeBuf.append(data);
    m_ws_stream.clear();

    quint32 size = activeBuf.size(); // How much do we have in the stream buffer so far

    if (size < msg_command_size_bytes) { // need to wait for more data
            // qDebug() << "Recieved binary message is too short.";
            return;
    }
    else if (size == msg_command_size_bytes) // command only, no size, so 4 bytes.
    {
        newData(data, QByteArray());
        return;
    }
    else // search for command + size + payload
    {
        quint32 dataStep = 0;

        for(quint32 i=0;i<size;++i) // iterate through every received byte of the appended buffer
        {
            QByteArray command = activeBuf.mid(dataStep, msg_command_size_bytes);
            quint32 dataSize = uint32FromArray(activeBuf.mid(dataStep + msg_command_size_bytes, msg_payload_size_bytes));

            //qDebug() << "Data size is: " << dataSize;

            if(size >= (dataStep + msg_command_size_bytes + dataSize))
            {
                QByteArray payload = activeBuf.mid(dataStep + msg_command_size_bytes + msg_payload_size_bytes, dataSize);
                dataStep += msg_command_size_bytes + msg_payload_size_bytes + dataSize;
                newData(command, payload);

                i = dataStep;
            }
            else
            {
                debugHexData(activeBuf);

                m_ws_stream = activeBuf.mid(dataStep, size - dataStep);

                if(m_ws_stream.size() > 2000)
                    m_ws_stream.clear();

                break;
            }
         }
    }







} //binaryMessageRecieved

void WebSocketHandler::timerReconnectTick()
{
    if(m_webSocket->state() == QAbstractSocket::ConnectedState)
    {
        m_timerReconnect->stop();
        return;
    }

    if(m_webSocket->state() == QAbstractSocket::ConnectingState)
        m_webSocket->abort();

    m_webSocket->open(QUrl(m_url));
}

void WebSocketHandler::startWaitResponseTimer(int msec, int type)
{
    if(!m_timerWaitResponse)
    {
        m_timerWaitResponse = new QTimer(this);
        connect(m_timerWaitResponse, &QTimer::timeout, this, &WebSocketHandler::timerWaitResponseTick);
    }

    m_timerWaitResponse->start(msec);
    m_waitType = type;
}

void WebSocketHandler::stopWaitResponseTimer()
{
    if(m_timerWaitResponse)
        if(m_timerWaitResponse->isActive())
            m_timerWaitResponse->stop();

}

void WebSocketHandler::timerWaitResponseTick()
{
    m_timerWaitResponse->stop();

    sendAuthenticationResponse(false);
}

void WebSocketHandler::debugHexData(const QByteArray &data)
{
    QString textHex;
    int dataSize = data.size();
    for(int i=0;i<dataSize;++i)
    {
        quint8 oneByte = static_cast<quint8>(data.at(i));
        textHex.append(QString::number(oneByte,16));

        if(i < dataSize-1)
            textHex.append("|");
    }

    qDebug()<<"DataParser::debugHexData:"<<textHex<<data;
}

QByteArray WebSocketHandler::arrayFromUint32(quint32 number)
{
    QByteArray buf;
    buf.resize(4);
    buf[0] = static_cast<char>(number);
    buf[1] = static_cast<char>(number >> 8);
    buf[2] = static_cast<char>(number >> 16);
    buf[3] = static_cast<char>(number >> 24);
    return buf;
}

quint16 WebSocketHandler::uint16FromArray(const QByteArray &buf)
{
    if(buf.count() == 2)
    {
        quint16 m_number;
        m_number = static_cast<quint16>(static_cast<quint8>(buf[0]) |
                static_cast<quint8>(buf[1]) << 8);
        return m_number;
    }
    else return 0x0000;
}


quint32 WebSocketHandler::uint32FromArray(const QByteArray &buf)
{
    if(buf.count() == 4)
    {
        quint32 m_number;
        m_number = static_cast<quint16>(static_cast<quint8>(buf[0]) |
                static_cast<quint8>(buf[1]) << 8 |
                                               static_cast<quint8>(buf[2]) << 16 |
                                                                              static_cast<quint8>(buf[3]) << 24);
        return m_number;
    }
    else return 0x0000;
}
