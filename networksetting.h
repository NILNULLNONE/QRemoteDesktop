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
enum class PacketType
{
    Empty,
    RequestDesktop,
    ResponseDesktop,
    DesktopBlock
};

struct Packet
{
    PacketType type;
};

struct RequestDesktopPacket
{
    PacketType type = PacketType::RequestDesktop;
    double scale;
//    quint16 width;
//    quint16 height;
};

struct ResponseDesktopPacket
{
    PacketType type = PacketType::ResponseDesktop;
    quint16 width;
    quint16 height;
    QImage::Format pixelFormt = QImage::Format_RGB888;
    bool alignRow;// 4 byte alignment
    qint32 size;
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
    quint64 GetNumBlocks()const{return (size + GetNumBytesPerBlock() - 1) / GetNumBytesPerBlock();}
};

struct DesktopBlockPacket
{
    PacketType type = PacketType::DesktopBlock;
    quint8 block[PACKET_BLOCK_BYTES];
};

struct RequestDesktopPacketHandler
{
    virtual void operator()(const RequestDesktopPacket& packet){}
};

struct ResponseDesktopPacketHandler
{
    virtual void operator()(const ResponseDesktopPacket& packet){}
};

struct DesktopBlockPacketHandler
{
    virtual void operator()(const DesktopBlockPacket& packet){}
};

enum class TcpConnReadState
{
    None,
    WaitForRequestPacket,
    WaitForResponsePacket,
    WaitForBlockPacket
};

class TcpConnectionProxy;
class TcpConnectionProxy : public QObject
{
    Q_OBJECT
public:
    TcpConnectionProxy(QTcpSocket* client, TcpConnReadState state);
    ~TcpConnectionProxy()
    {
        if(clientConn)
        {
            clientConn->disconnect();
            if(clientConn->isOpen())
            {
                clientConn->close();
                if(clientConn->state() != QAbstractSocket::SocketState::UnconnectedState)
                    clientConn->waitForDisconnected();
            }

            delete clientConn;
            clientConn = nullptr;
        }
    }
    QTcpSocket* GetSocket()const{return clientConn;}
    void SwitchState(TcpConnReadState state)
    {
        readState = state;
        switch (readState) {
        case TcpConnReadState::WaitForRequestPacket:
            remainBytesToRead = sizeof(RequestDesktopPacket);
            break;
        case TcpConnReadState::WaitForResponsePacket:
            remainBytesToRead = sizeof(ResponseDesktopPacket);
            break;
        case TcpConnReadState::WaitForBlockPacket:
            remainBytesToRead = sizeof(DesktopBlockPacket);
            break;
        case TcpConnReadState::None:
            assert(0);
            break;
        }
        dataPtr = dataCache;
    }
    void RegisterRequestDesktopPacketHandler(RequestDesktopPacketHandler* handler)
    {
        requestDesktopPacketHandleres.push_back(handler);
    }
    void RegisterResponseDesktopPacketHandler(ResponseDesktopPacketHandler* handler)
    {
        responseDesktopPacketHandleres.push_back(handler);
    }
    void RegisterDesktopBlockPacketHandler(DesktopBlockPacketHandler* handler)
    {
        desktopBlockPacketHandleres.push_back(handler);
    }
    void HandlePacket(const RequestDesktopPacket& packet)
    {
        this->SwitchState(TcpConnReadState::WaitForRequestPacket);
        for(auto& handler : requestDesktopPacketHandleres)
            (*handler)(packet);
    }
    void HandlePacket(const ResponseDesktopPacket& packet)
    {
        this->SwitchState(TcpConnReadState::WaitForBlockPacket);
        for(auto& handler : responseDesktopPacketHandleres)
            (*handler)(packet);
    }
    void HandlePacket(const DesktopBlockPacket& packet)
    {
        this->SwitchState(TcpConnReadState::WaitForBlockPacket);
        for(auto& handler : desktopBlockPacketHandleres)
            (*handler)(packet);
    }
signals:
    void sig_SocketError(QAbstractSocket::SocketError err);
    void sig_SocketStateChanged(QAbstractSocket::SocketState state);
private:
    void ReadData();
    template<typename PType>
    void ReadPacket(char* dataBuf, qint64 dataSize);
private slots:
    void on_clientDisconnected();
    void on_clientDataReady();
private:
    QTcpSocket* clientConn;
    char dataCache[BUF_SIZE];
    char* dataPtr;
    TcpConnReadState readState;
    qint64 remainBytesToRead;
    QVector<RequestDesktopPacketHandler*> requestDesktopPacketHandleres;
    QVector<ResponseDesktopPacketHandler*> responseDesktopPacketHandleres;
    QVector<DesktopBlockPacketHandler*> desktopBlockPacketHandleres;
};

//extern void OnSocketError(QAbstractSocket::SocketError err);

class ServerConnThreadWorker :
        public QObject,
        public RequestDesktopPacketHandler
{
    Q_OBJECT
public:
    ServerConnThreadWorker(qintptr d)
        : descriptor(d),
         clientProxy(nullptr)
    {
        connect(this, &ServerConnThreadWorker::sig_recvRequest,
                this, &ServerConnThreadWorker::on_recvRequest);
    }

    ~ServerConnThreadWorker()
    {
        this->CleanUp();
    }
    void work();

    void on_ClientDisconnect()
    {
        qDebug()<<"client disconnect";
    }
signals:
    void sig_recvRequest();

public:
    void operator()(const RequestDesktopPacket& packet) override
    {
//        logd("server recv request packet");
        reqPacket = packet;
        emit sig_recvRequest();
    }

    void on_recvRequest()
    {
        ResponseDesktopPacket respPacket;
        respPacket.type = PacketType::ResponseDesktop;
//        respPacket.width = reqPacket.width;
//        respPacket.height = reqPacket.height;
        respPacket.alignRow = true;
        respPacket.pixelFormt = QImage::Format_RGB888;
        GetDesktopCaptureInfo(&respPacket.width, &respPacket.height,
                              &respPacket.pixelFormt, &respPacket.alignRow);

        {
            respPacket.width *= reqPacket.scale;
            respPacket.height *= reqPacket.scale;
        }
        quint64 numBytes = respPacket.GetNumBytes();
        if(numBytes != captureData.size())
            captureData.resize(numBytes);
//        CaptureDesktop(captureData.data());
        StretchCaptureDesktop(captureData.data(), captureData.size(), respPacket.width, respPacket.height);

        // compress
        {
            extern qint32 lz4CalculateCompressedSize(qint32 uncompressedSize);
            extern qint32 lz4Compress(const void* src, void* dst, qint32 srcSize, qint32 maxOutputSize);
            qint32 compressedSize = lz4CalculateCompressedSize(static_cast<qint32>(captureData.size()));
            compressedData.resize(compressedSize);
            respPacket.size = lz4Compress(captureData.data(), compressedData.data(), captureData.size(), compressedSize);
        }


        clientProxy->GetSocket()->write(reinterpret_cast<char*>(&respPacket), sizeof(respPacket));

        quint64 bytesPerBlock = respPacket.GetNumBytesPerBlock();
        quint64 numBlocks = respPacket.GetNumBlocks();
        DesktopBlockPacket blkPacket;
        quint8* dataPtr = compressedData.data();

        for(int i = 0; i < numBlocks; ++i)
        {
            quint64 bytesToCopy = bytesPerBlock;
            if(numBytes < bytesToCopy)
                bytesToCopy = numBytes;
            memcpy(blkPacket.block, dataPtr, bytesToCopy);
            dataPtr += bytesToCopy;
            numBytes -= bytesToCopy;
            clientProxy->GetSocket()->write(reinterpret_cast<char*>(&blkPacket), sizeof(blkPacket));
        }
    }

    void on_SocketError(QAbstractSocket::SocketError err)
    {
//        this->CleanUp();
        qDebug()<<"socet err "<<err;
    }

    void CleanUp()
    {
//        if(clientProxy)
//        {
//            clientProxy->disconnect();
//            delete clientProxy;
//            clientProxy = nullptr;
//        }
    }
private:
    qintptr descriptor;
    RequestDesktopPacket reqPacket;
    TcpConnectionProxy* clientProxy;
    QVector<quint8> captureData;
    QVector<quint8> compressedData;
};

class ClientThreadWorker :
        public QObject
        , public ResponseDesktopPacketHandler
        , public DesktopBlockPacketHandler
{
    Q_OBJECT
public:
    ClientThreadWorker()
        : clientProxy(nullptr),
          reqScale(50)
//        : reqWidth(1024),
//          reqHeight(768)
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

    void on_clientSocketStateChanged(QAbstractSocket::SocketState);

    void on_clientSocketError(QAbstractSocket::SocketError err);

    virtual void operator()(const ResponseDesktopPacket& packet) override;

    virtual void operator()(const DesktopBlockPacket& packet) override;

    void RegisterRDDataHandler(RDDataHandler* handler)
    {
        rddataHandleres.push_back(handler);
    }

    void work();

    void SendRequestDesktopPacket();

    void UpdateRequestScale(double s) {reqScale = s;}

    void CleanUp();

    bool IsConnected()const
    {
        return clientProxy && clientProxy->GetSocket()->state() == QAbstractSocket::SocketState::ConnectedState;
    }
//    void UpdateRequestResolution(quint16 w, quint16 h)
//    {
//        reqWidth = w;
//        reqHeight = h;
//    }
signals:
    void startWork();
    void sig_UpdateSize(quint16 w, quint16 h);
    void sig_CleanUp();
private:
    TcpConnectionProxy* clientProxy;
    QString hostIp;
    ResponseDesktopPacket respPacket;
    int crntBlkIdx;
    QVector<quint8> rddata;
    QVector<quint8> compressedRddata;
    int numBlks;
    QVector<RDDataHandler*> rddataHandleres;
    QElapsedTimer epTimer;
//    quint16 reqWidth, reqHeight;
    double reqScale;
    QThread* clientThread;
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
        if(serverThread)
        {
            if(serverThread->isRunning())
            {
                 serverThread->quit();
                 serverThread->wait();
            }
            delete serverThread;
            serverThread = nullptr;
        }

        if(newConnThreadWorker)
        {
            delete newConnThreadWorker;
            newConnThreadWorker = nullptr;
        }
    }
signals:
    void startWork();
protected:
    void incomingConnection(qintptr socketDescriptor)override
    {
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
