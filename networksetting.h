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
//#define BLOCK_WIDTH 128
//#define BLOCK_HEIGHT 128
//#define BLOCK_SIZE (BLOCK_WIDTH * BLOCK_HEIGHT)
//#define NUM_PIXELS_PER_BLOCK (1024 * 8 * 4 * 2)
//#define NUM_PIXELS_PER_BLOCK 1024*64
//#define PACKET_BLOCK_BYTES (1024 * 8 * 4 * 4 * 2)
#define PACKET_BLOCK_BYTES 1024*400
//#define BUF_SIZE (65536 * 4 * 2)
//#define BUF_SIZE 65536
#define BUF_SIZE 1024*1024
extern quint64 ComputeNumBytes(quint64 numPixelsPerBlock, QImage::Format fmt);
extern void GetDesktopCaptureInfo(quint16* width, quint16* height, QImage::Format* format, bool* rowAlign);
extern void CaptureDesktop(quint8* bytes);

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
    //quint64 GetNumBytesPerBlock()const{return ComputeNumBytes(NUM_PIXELS_PER_BLOCK, pixelFormt);}
    quint64 GetNumBytesPerBlock()const{return PACKET_BLOCK_BYTES;}
    quint64 GetNumBlocks()const{return (size + GetNumBytesPerBlock() - 1) / GetNumBytesPerBlock();}
//    quint16 rows;
//    quint16 cols;
//    quint32 numBlocks;
};

struct DesktopBlockPacket
{
    PacketType type = PacketType::DesktopBlock;
    quint8 block[PACKET_BLOCK_BYTES];
};

//typedef void (*RequestDesktopPacketHandler)(const RequestDesktopPacket&, void*);
//typedef void (*ResponseDesktopPacketHandler)(const ResponseDesktopPacket&, void*);
//typedef void (*DesktopBlockPacketHandler)(const DesktopBlockPacket&, void*);
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
//signals:
//    void FinishPacket(TcpConnectionProxy* proxy, const Packet& packet);
public:
    TcpConnectionProxy(QTcpSocket* client, TcpConnReadState state);
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
//    static TcpConnectionProxy* CreateProxy(QTcpSocket* client);
private:
    void ReadData();
//    void ReadRequestPacket(char* dataBuf, qint64 dataSize);
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

extern void OnSocketError(QAbstractSocket::SocketError err);

class ServerConnThreadWorker :
        public QObject,
        public RequestDesktopPacketHandler
{
    Q_OBJECT
public:
    ServerConnThreadWorker(qintptr d) : descriptor(d)
    {
        connect(this, &ServerConnThreadWorker::sig_recvRequest,
                this, &ServerConnThreadWorker::on_recvRequest);
    }
    void work();

signals:
    void sig_recvRequest();

public:
    void operator()(const RequestDesktopPacket& packet) override
    {
//        logd("server recv request packet");
        recvRequestPacket = packet;
        emit sig_recvRequest();
    }

//    void GetDesktopCapture()
//    {
//        QScreen *screen = QGuiApplication::primaryScreen();
////        if (const QWindow *window = windowHandle())
////            screen = window->screen();
//        if (!screen)
//                return;
//        QPixmap originalPixmap = screen->grabWindow(0);
//        originalPixmap.data_ptr();
//    }

    void on_recvRequest()
    {
        ResponseDesktopPacket respPacket;
        respPacket.type = PacketType::ResponseDesktop;
        GetDesktopCaptureInfo(&respPacket.width, &respPacket.height,
                              &respPacket.pixelFormt, &respPacket.alignRow);
        quint64 numBytes = respPacket.GetNumBytes();
        if(numBytes != captureData.size())
            captureData.resize(numBytes);
        CaptureDesktop(captureData.data());

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
        //quint64 numBlocks = respPacket.GetNumBlocks();
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
private:
//    void on_serverConnRecvPacket(TcpConnectionProxy* clientProxy, const Packet& packet);
    qintptr descriptor;
    RequestDesktopPacket recvRequestPacket;
    TcpConnectionProxy* clientProxy;
    QVector<quint8> captureData;
    QVector<quint8> compressedData;
};

//class ClientThread : public QObject
class ClientThreadWorker :
        public QObject
        , public ResponseDesktopPacketHandler
        , public DesktopBlockPacketHandler
{
    Q_OBJECT
public:
    ClientThreadWorker()
    {

    }

    void SetHostIp(const QString& hip){hostIp = hip;}

//    void on_clientRecvPacket(TcpConnectionProxy* clientProxy, const Packet& packet);

    void on_clientSocketStateChanged(QAbstractSocket::SocketState);

    void on_clientSocketError(QAbstractSocket::SocketError err);

    virtual void operator()(const ResponseDesktopPacket& packet) override;

    virtual void operator()(const DesktopBlockPacket& packet) override;

    void RegisterRDDataHandler(RDDataHandler* handler)
    {
        rddataHandleres.push_back(handler);
    }
//    void on_recvRespPacket();

//    void on_recvBlockPacket();

    void work();
//    void run();

    void SendRequestDesktopPacket();
signals:
    void startWork();
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
};

namespace Ui {
class NetworkSetting;
}

class RDServer : public QTcpServer
{
    Q_OBJECT
public:
    RDServer(QObject* parent) : QTcpServer(parent)
    {
        serverThread.start();
    }
    RDServer():RDServer(nullptr){}
    ~RDServer()
    {
        serverThread.terminate();
    }
signals:
    void startWork();
protected:
    void incomingConnection(qintptr socketDescriptor)override
    {
        ServerConnThreadWorker* newConnThreadWorker = new ServerConnThreadWorker(socketDescriptor);
//        newConnThread->start();
        newConnThreadWorker->moveToThread(&serverThread);
        connect(this, &RDServer::startWork, newConnThreadWorker, &ServerConnThreadWorker::work);
        emit startWork();

    }
private:
    QThread serverThread;
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
private slots:
    void on_waitConnBtn_clicked(bool checked);

    void on_buildConnBtn_clicked(bool checked);
private:
    Ui::NetworkSetting *ui;
    RDServer* server;
    ClientThreadWorker* client;
};

#endif // NETWORKSETTING_H
