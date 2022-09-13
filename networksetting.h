#ifndef NETWORKSETTING_H
#define NETWORKSETTING_H
#include <QTcpServer>
#include <QTcpSocket>
#include <QWidget>
#include <QThread>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QElapsedTimer>
#include"rddatahandler.h"
#include "logger.h"
#define SRV_PORT 18877
#define PACKET_BLOCK_BYTES 1024*400
#define BUF_SIZE 1024*1024
extern quint64 ComputeNumBytes(quint64 numPixelsPerBlock, QImage::Format fmt);
extern void GetDesktopCaptureInfo(quint16* width, quint16* height, QImage::Format* format, bool* rowAlign);
extern void CaptureDesktop(quint8* bytes);
extern void StretchCaptureDesktop(quint8* dst, quint64 dstSize, quint16 width, quint16 height);

enum class QRDMessageType
{
    None,
    InvalidMsg,
    RequestDekstop,
    DesktopDescription,
    DesktopDataBlock
};

struct QRDMessageBase
{
    QRDMessageType type;
    char buf[4096];
    template<typename DType>
    static void Create(QRDMessageType t, const DType& dat, QRDMessageBase& outMsg)
    {
        auto s = sizeof(dat);
        if(s > sizeof(buf))
        {
            outMsg.type = QRDMessageType::InvalidMsg;
            return;
        }
        outMsg.type = t;
        memcpy(outMsg.buf, &dat, sizeof(dat));
    }

    template<typename Type>
    void ParseData(Type& outData)
    {
        if(sizeof(outData) > sizeof(buf))
        {
            logd("fail to parse data");
            return;
        }
        memcpy(&outData, buf, sizeof(outData));
    }
};

struct QRDMsgRequestDesktopCapture
{
    double scale;
};

struct QRDMsgDesktopDescription
{
    quint16 width;
    quint16 height;
    QImage::Format pixelFormt = QImage::Format_RGB888;
    bool alignRow;// 4 byte alignment
    qint32 compressedSize;
    quint64 GetNumPixels()const{return width * height;}
    quint64 GetNumPaddingBytesPerRow()const
    {
        if(alignRow)
        {
            return (((width*24 + 31)&~31) - width*24) / 8;
        }
        return 0;
    }
    quint64 GetNumBytes()const
    {
        quint64 bytes = ComputeNumBytes(GetNumPixels(), pixelFormt);
        if(alignRow)
        {
            bytes += GetNumPaddingBytesPerRow() * height;
        }
        return bytes;
    }
    quint64 GetNumBytesPerBlock()const{return PACKET_BLOCK_BYTES;}
    quint64 GetNumBlocks()const{return (compressedSize + GetNumBytesPerBlock() - 1) / GetNumBytesPerBlock();}
    void DebugLog()
    {
        logd("width: %d, height: %d, compressedSize: %d",
             (int)width, (int)height, (int)compressedSize);
    }
};

struct QRDMsgDesktopDataBlock
{
    quint8 block[PACKET_BLOCK_BYTES];
};

class ServerConnThreadWorker :
        public QObject
{
    Q_OBJECT
public:
    ServerConnThreadWorker(qintptr d)
        : descriptor(d)
    {
    }

    ~ServerConnThreadWorker()
    {
    }
    void work();

    void on_ClientDisconnect()
    {
        qDebug()<<"client disconnect";
    }
signals:
    void sig_recvRequest();

public:
    void SendDesktopCapture(QRDMsgRequestDesktopCapture& req);
private:
    qintptr descriptor;
    QVector<quint8> captureData;
    QVector<quint8> compressedData;
    QTcpSocket* clientConn;
};

class ClientThreadWorker : public QObject
{
    Q_OBJECT
public:
    ClientThreadWorker()
        : reqScale(50),
          clientSocket(nullptr)
    {
        clientThread = new QThread();
        clientThread->start();
        this->moveToThread(clientThread);
        connect(this, &ClientThreadWorker::startWork,
                this, &ClientThreadWorker::work);
        connect(this, &ClientThreadWorker::sig_CleanUp,
                this, &ClientThreadWorker::CleanUp);
    }
    ~ClientThreadWorker()
    {
//        clientThread->terminate();
        emit this->sig_CleanUp();
        clientThread->quit();
        clientThread->wait();
        delete clientThread;
    }

    void SetHostIp(const QString& hip){hostIp = hip;}

    void RegisterRDDataHandler(RDDataHandler* handler)
    {
        rddataHandleres.push_back(handler);
    }

    void work();

    bool SendRequestDesktopPacket();

    bool RecvDesktopCapture(QRDMessageBase& baseMsg);

    void UpdateRequestScale(double s) {reqScale = s;}

    void CleanUp();

    bool IsConnected()const
    {
        return clientSocket && clientSocket->state() == QAbstractSocket::SocketState::ConnectedState;
    }

signals:
    void startWork();
    void sig_UpdateSize(quint16 w, quint16 h);
    void sig_CleanUp();
private:
    QString hostIp;
    QVector<quint8> rddata;
    QVector<quint8> compressedRddata;
    QVector<RDDataHandler*> rddataHandleres;
    QElapsedTimer epTimer;
    double reqScale;
    QThread* clientThread;
    QTcpSocket* clientSocket;
};

namespace Ui {
class NetworkSetting;
}

class RDServer : public QTcpServer
{
    Q_OBJECT
public:
    RDServer(QObject* parent) :
        QTcpServer(parent),
        serverThread(nullptr),
        newConnThreadWorker(nullptr)
    {

    }
    RDServer():RDServer(nullptr){}
    ~RDServer()
    {
        this->CleanUp();
    }

    void CleanUp()
    {

    }
signals:
    void startWork();
protected:
    void incomingConnection(qintptr socketDescriptor)override
    {
        logd("server recv incoming connection");
        serverThread = new QThread();
        serverThread->start();
        newConnThreadWorker = new ServerConnThreadWorker(socketDescriptor);
        newConnThreadWorker->moveToThread(serverThread);
        connect(this, &RDServer::startWork, newConnThreadWorker, &ServerConnThreadWorker::work);
        emit startWork();

    }
private:
    QThread* serverThread;
    ServerConnThreadWorker* newConnThreadWorker;
};

class NetworkSetting : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkSetting(QWidget *parent = nullptr);
    ~NetworkSetting();
    void on_newConnection();

    void RegisterRDDataHandler(RDDataHandler* handler)
    {
        client->RegisterRDDataHandler(handler);
    }

//    void Init();
signals:
    void sig_buildConn();

    void sig_UpdateSize(quint16, quint16);
//    void sig_resoChanged(quint16 w, quint16 h);
public:
    void TriggerDisconnect()
    {
        if(client->IsConnected())
        {
//            client->CleanUp();
            emit client->sig_CleanUp();
        }
    }
private slots:
    void on_waitConnBtn_clicked(bool checked);

    void on_buildConnBtn_clicked(bool checked);

//    void on_resoBox_currentTextChanged(const QString &arg1);

private:
    Ui::NetworkSetting *ui;
    RDServer* server;
    ClientThreadWorker* client;
};

#endif // NETWORKSETTING_H
