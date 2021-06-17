#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <QObject>
#include <QTimer>
#include <QImage>
#include <QList>

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
    int m_currentTileNum;
    int m_receivedTileNum;
    int m_permitCounter;
    QImage m_lastImage;
    QVector<int> m_meanCounter;

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
    void sendImage(int posX, int posY, int tileNum, const QImage& image);
    void sendImage(const QImage& image); // full screen image
    bool isSendTilePermit();
};

#endif // SCREEN_CAPTURE_H
