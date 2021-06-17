#include "screen_capture.h"

#include <QPixmap>
#include <QScreen>
#include <QApplication>
#include <QWindow>
#include <QDesktopWidget>
#include <QDebug>
#include <QBuffer>

ScreenCapture::ScreenCapture(QObject *parent) : QObject(parent),
    m_grabTimer(Q_NULLPTR),
    m_grabInterval(300),
    m_rectSize(200),
    m_screenNumber(0),
    m_currentTileNum(0),
    m_receivedTileNum(0),
    m_permitCounter(0)
{
    m_meanCounter.resize(4);
    m_meanCounter.fill(1);
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

    m_permitCounter = 0;
    m_receivedTileNum = 0;
    m_currentTileNum = 0;

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
    /*

    if(!isSendTilePermit())
    {
        // qDebug()<<"GraberClass::updateImage - send not permitted.";
        return;

    }
    */


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

    /*
    if (currentImage != m_lastImage)
    {
         // qDebug()<<"A lot has changed, so sending a full image.";
         sendImage(currentImage);
         m_lastImage = currentImage;
         setReceivedTileNum(0);
         return;

         // Can't send these as payload too large
    }
    */


    /* Pre-check to see if > 50% of the tiles have changed, if so, just send a complete new image instead. */
    quint16 numTiles        = columnCount*rowCount;
    quint16 numDirtyTiles   = 0;

    for(int i=0;i<columnCount;++i) {
        for(int j=0;j<rowCount;++j) {

            QImage image = currentImage.copy(i*m_rectSize, j*m_rectSize, m_rectSize, m_rectSize);
            QImage lastImage = m_lastImage.copy(i*m_rectSize, j*m_rectSize, m_rectSize, m_rectSize);

            if(lastImage != image)
                numDirtyTiles++;
        }
    }

    // More than half is dirty, easier to just send a full screen image

    if (numDirtyTiles > numTiles/2)
    {
         // qDebug()<<"A lot has changed, so sending a full image.";
         sendImage(currentImage);
         m_lastImage = currentImage;
       //  setReceivedTileNum(0);
         return;

         // Can't send these as payload too large
    }


    /* Else, send only the tiles that have changed. */
    for(int i=0;i<columnCount;++i)
    {
        for(int j=0;j<rowCount;++j)
        {
            QImage image = currentImage.copy(i*m_rectSize, j*m_rectSize, m_rectSize, m_rectSize);
            QImage lastImage = m_lastImage.copy(i*m_rectSize, j*m_rectSize, m_rectSize, m_rectSize);

            if(lastImage != image)
            {
                sendImage(i,j,tileNum,image);
                m_currentTileNum = tileNum;
                ++tileNum;
            }
        }
    }
    m_lastImage = currentImage;
}

void ScreenCapture::setReceivedTileNum(quint16 num)
{
    m_permitCounter = 0;
    m_receivedTileNum = num;
}

/* Send a tile image */
void ScreenCapture::sendImage(int posX, int posY, int tileNum, const QImage &image)
{
    QByteArray bArray;
    QBuffer buffer(&bArray);
    buffer.open(QIODevice::WriteOnly);
    //image.save(&buffer, "PNG");
    image.save(&buffer, "WEBP", 15);
    //bArray.remove(0,PNG_HEADER_SIZE);

    emit imageTile(static_cast<quint16>(posX),static_cast<quint16>(posY),bArray,static_cast<quint16>(tileNum));
}

/* Send a full screen image */
void ScreenCapture::sendImage(const QImage &image)
{
    QByteArray bArray;
    QBuffer buffer(&bArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "WEBP", 25);

    emit imageScreen(bArray); // full screen one
}



bool ScreenCapture::isSendTilePermit()
{
    bool result = false;

    if(m_currentTileNum <= (m_receivedTileNum))
        result = true;

    if(!result)
    {
        ++m_permitCounter;

        if(m_permitCounter > 20)
        {
            m_permitCounter = 0;
            result = true;
        }
    }

    return result;
}
