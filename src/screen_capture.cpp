#include "screen_capture.h"

#include <QPixmap>
#include <QScreen>
#include <QApplication>
#include <QWindow>
#include <QDesktopWidget>
#include <QDebug>
#include <QBuffer>
#include <QTime>

ScreenCapture::ScreenCapture(QObject *parent) : QObject(parent),
    m_grabTimer(Q_NULLPTR),
    m_grabInterval(300),
    m_rectSize(300),
    m_screenNumber(0)
{
    m_AckLatencyMs.resize(10);
    m_AckLatencyMs.fill(5000);
}

void ScreenCapture::start()
{
    if(!m_grabTimer)
    {
        m_grabTimer = new QTimer(this);
        connect(m_grabTimer,SIGNAL(timeout()),this,SLOT(updateImage()));
    }
}

void ScreenCapture::stop()
{
    stopSending();
    emit finished();
}

void ScreenCapture::changeScreenNum()
{
    QList<QScreen *> screens = QApplication::screens();

    if(screens.size() > m_screenNumber+1)
        ++m_screenNumber;
    else m_screenNumber = 0;

    QScreen* screen = screens.at(m_screenNumber);
    emit screenPositionChanged(QPoint(screen->geometry().x(),screen->geometry().y()));

    startSending();
}

void ScreenCapture::startSending()
{
    // // qDebug()<<"GraberClass::startSending";

    if(m_grabTimer)
        if(!m_grabTimer->isActive())
            m_grabTimer->start(m_grabInterval);

    m_lastImage = QImage();
    updateImage();
}

void ScreenCapture::stopSending()
{
    // qDebug()<<"GraberClass::stopSending";

    if(m_grabTimer)
        if(m_grabTimer->isActive())
            m_grabTimer->stop();
}

void ScreenCapture::updateScreen()
{
    QScreen *screen = QApplication::screens().at(m_screenNumber);
    QImage currentImage = screen->grabWindow(0).toImage().convertToFormat(QImage::Format_RGB888);
    sendImage(currentImage);
}

void ScreenCapture::updateImage()
{

    QScreen *screen = QApplication::screens().at(m_screenNumber);
    //QImage currentImage = screen->grabWindow(0).toImage().convertToFormat(QImage::Format_RGB444);
    QImage currentImage = screen->grabWindow(0).toImage().convertToFormat(QImage::Format_RGB888);

    int columnCount = currentImage.width() / m_rectSize;
    int rowCount = currentImage.height() / m_rectSize;

    // Deal with fractionals
    if(currentImage.width() % m_rectSize > 0)
        ++columnCount;

    if(currentImage.height() % m_rectSize > 0)
        ++rowCount;

    if(m_lastImage.isNull())
    {
        m_lastImage = QImage(currentImage.size(),currentImage.format());
        m_lastImage.fill(QColor(Qt::blue));

        emit imageParameters(currentImage.size(), m_rectSize);
    }


    quint16 tileNum = 0;

    /* Pre-check to see if > 50% of the tiles have changed, if so, just send a complete new image instead. */
    quint16 numTiles        = columnCount*rowCount;
    quint16 numDirtyTiles   = 0;

    m_time        = QTime::currentTime();
    quint64 dtime = m_time.msecsSinceStartOfDay();


    for(int i=0;i<columnCount;++i) {
        for(int j=0;j<rowCount;++j) {

            tileNum = (i*rowCount)+j;
            QImage image        = currentImage.copy(i*m_rectSize, j*m_rectSize, m_rectSize, m_rectSize);
            QImage lastImage     = m_lastImage.copy(i*m_rectSize, j*m_rectSize, m_rectSize, m_rectSize);

            // when was the last ackowlegement of a tile, more than a second ago?
            bool missingAck = false;

            if ( m_tilePendingAck.contains(tileNum) ) {
                missingAck = ( (dtime - m_tilePendingAck.value(tileNum) ) > 1000) ? true:false;
            }

            if (missingAck) {
                qDebug() << "Tile was last acknowleged more than a second ago.";
            }


            if(lastImage != image) {
                numDirtyTiles++;
                sendImage(i,j,tileNum,image);
                m_tilePendingAck.insert(tileNum, dtime );
#ifdef Q_DEBUG
                qDebug() << "Send tile " << tileNum << " at ms" << dtime;
#endif

            }
            else if ( missingAck)
            {
                sendImage(i,j,tileNum,image);
                m_tilePendingAck.remove(tileNum); // need to do this here or we'll cause a race condition
            }

            if (numDirtyTiles > numTiles/3)
            {
                sendImage(currentImage);
                break;
            }
        }
    }

    m_lastImage = currentImage;


    /*
    if ( m_lastImage != currentImage) {
            m_lastImage = currentImage;
            sendImage(currentImage);
            return;
    }
    */

}

void ScreenCapture::setReceivedTileNum(quint16 tileNum)
{
#ifdef QT_DEBUG
    qDebug() << "Client recieved tile " << tileNum;
#endif

    if (tileNum == 9999) // within displayField.js line ~500
    {
        m_tilePendingAck.clear();
    }

    m_time        =     QTime::currentTime();
    quint64 dtime = m_time.msecsSinceStartOfDay();

    if (m_tilePendingAck.contains(tileNum)) {
         quint64 sentMs     = m_tilePendingAck.value(tileNum);
         quint64 latencyMs  = dtime - sentMs;

         m_AckLatencyMs.pop_back();
         m_AckLatencyMs.push_front(latencyMs);

//         qDebug() << "Adding latency of:" << latencyMs;

         // Update mean latency, NOT USED ANYMORE
         m_meanAckLatencyMs = std::accumulate(m_AckLatencyMs.begin(), m_AckLatencyMs.end(), .0) / m_AckLatencyMs.size();

#ifdef QT_DEBUG
         qDebug()<<"GraberClass::setReceivedTileNum - Current client tile acknowledge latency (ms) is: " << m_meanAckLatencyMs;
#endif
         // Remove from pending list
         m_tilePendingAck.remove(tileNum);

    }

}

/* Send a tile image */
void ScreenCapture::sendImage(int posX, int posY, quint16 tileNum, const QImage &image)
{    
    QByteArray bArray;
    QBuffer buffer(&bArray);
    buffer.open(QIODevice::WriteOnly);
    //image.save(&buffer, "PNG");
    image.save(&buffer, "WEBP", 25);
    //bArray.remove(0,PNG_HEADER_SIZE);

    emit imageTile(static_cast<quint16>(posX),static_cast<quint16>(posY),bArray,static_cast<quint16>(tileNum));
}

/* Send a full screen image */
void ScreenCapture::sendImage(const QImage &image)
{
    QByteArray bArray;
    QBuffer buffer(&bArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "WEBP", 35);

    m_tilePendingAck.clear();

    emit imageScreen(bArray); // full screen one
}

