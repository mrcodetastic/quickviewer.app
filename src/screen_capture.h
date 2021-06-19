#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <QObject>
#include <QTimer>
#include <QImage>
#include <QMap>
#include <QTime>

class ScreenCapture : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCapture(QObject *parent = Q_NULLPTR);

private:

    struct TileStruct
    {
        int x;
        int y;
        QImage image;

        TileStruct(int posX, int posY, const QImage &image) :
        x(posX), y(posY), image(image){}

        TileStruct(){}
    };

    QTimer *m_grabTimer;
    int m_grabInterval;
    int m_rectSize;
    int m_screenNumber;
    QImage m_lastImage;
    QTime m_time;

    // tile response ack time
    QMap <quint16, quint64>  m_tilePendingAck;  // tile, and time sent
    QVector<quint64>         m_AckLatencyMs;
    quint64                  m_meanAckLatencyMs;

signals: // 'emit'
    void finished();
    void imageParameters(const QSize &imageSize, int rectWidth);
    void imageTile(quint16 posX, quint16 posY, const QByteArray &imageData, quint16 tileNum);
    void imageScreen(const QByteArray &imageData); // full screen image
    void screenPositionChanged(const QPoint &pos);

public slots:
    void start();
    void stop();
    void setInterval(int msec){m_grabInterval = msec;}
    void setRectSize(int size){m_rectSize = size;}
    void changeScreenNum();

    void startSending();
    void stopSending();
    void updateImage();
    void updateScreen();
    void setReceivedTileNum(quint16 num);

private slots:
    void sendImage(int posX, int posY, quint16 tileNum, const QImage& image);
    void sendImage(const QImage& image); // full screen image
};

#endif // SCREEN_CAPTURE_H
