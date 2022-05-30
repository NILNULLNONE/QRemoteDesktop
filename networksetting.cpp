#include "networksetting.h"
#include "ui_networksetting.h"
#include <QHostAddress>
#include <QNetworkInterface>
#include <cctype>
#include "logger.h"
quint64 ComputeNumBytes(quint64 numPixelsPerBlock, QImage::Format fmt)
{

    switch (fmt) {
    case QImage::Format_RGB888:
        return numPixelsPerBlock * 3;
    case QImage::Format_RGBA8888:
        return numPixelsPerBlock * 4;
    default:
        assert(0);
    }
    return 0;
}

TcpConnectionProxy::TcpConnectionProxy(QTcpSocket* client, TcpConnReadState state)
    : clientConn(client),
      readState(state)
{
    SwitchState(readState);
    if(!clientConn)return;
    connect(clientConn, &QAbstractSocket::disconnected,this, &TcpConnectionProxy::on_clientDisconnected);
    connect(clientConn, &QAbstractSocket::readyRead, this, &TcpConnectionProxy::on_clientDataReady);
}

void TcpConnectionProxy::on_clientDisconnected()
{
    clientConn->deleteLater();
    delete this;
}

void TcpConnectionProxy::on_clientDataReady()
{
    this->ReadData();
}

char dataBuf[BUF_SIZE];
void TcpConnectionProxy::ReadData()
{
    qint64 bytesRead = clientConn->read(dataBuf, sizeof(dataBuf));
    switch (readState)
    {
    case TcpConnReadState::WaitForRequestPacket:
        //ReadRequestPacket(dataBuf, bytesRead);
        ReadPacket<RequestDesktopPacket>(dataBuf, bytesRead);
        break;
    case TcpConnReadState::WaitForResponsePacket:
        ReadPacket<ResponseDesktopPacket>(dataBuf,bytesRead);
        break;
    case TcpConnReadState::WaitForBlockPacket:
        ReadPacket<DesktopBlockPacket>(dataBuf, bytesRead);
        break;
    case TcpConnReadState::None:
        assert(0);
        break;
    default:
        break;
    }
}

//template<typename PType>
//void THandlePacket(const PType& packet, TcpConnectionProxy* proxy)
//{
//    proxy->HandlePacket(packet);
//}

template<typename PType>
void TcpConnectionProxy::ReadPacket(char* dataBuf, qint64 dataSize)
{
    if(dataSize < remainBytesToRead)
    {
        remainBytesToRead -= dataSize;
        memcpy(dataPtr, dataBuf, dataSize);
        dataPtr += dataSize;
    }
    else
    {
        memcpy(dataPtr, dataBuf, remainBytesToRead);
        dataBuf += remainBytesToRead;
        dataSize -= remainBytesToRead;

        PType packet;
        memcpy(&packet, dataCache, sizeof(PType));
        //dataPtr = dataCache;

        //THandlePacket(packet, this);
        this->HandlePacket(packet);

        if(dataSize > 0)
        {
            switch (packet.type) {
            case PacketType::Empty:
                break;
            case PacketType::RequestDesktop:
                assert(0);
                break;
            case PacketType::ResponseDesktop:
                ReadPacket<DesktopBlockPacket>(dataBuf, dataSize);
                break;
            case PacketType::DesktopBlock:
                ReadPacket<DesktopBlockPacket>(dataBuf, dataSize);
                break;
            }
        }

//        switch (packet.type) {
//        case PacketType::Empty:
////            remainBytesToRead = sizeof(Packet);
////            if(leaveBytes > 0)
////            {
////                ReadRequestPacket(dataBuf, leaveBytes);
////            }
//            break;
//        case PacketType::RequestDesktop:
//            for(auto handler : requestDesktopPacketHandleres)
//            {
//                (*handler)(packet);
//            }
//            break;
//        case PacketType::ResponseDesktop:
//            for(auto handler : responseDesktopPacketHandleres)
//            {
//                (*handler)(packet);
//            }
//            break;
//        case PacketType::DesktopBlock:

//            break;
//        }
    }
}


//void TcpConnectionProxy::ReadRequestPacket(char* dataBuf, qint64 dataSize)
//{
//    if(dataSize < remainBytesToRead)
//    {
//        remainBytesToRead -= dataSize;
//        memcpy(dataPtr, dataBuf, dataSize);
//        dataPtr += dataSize;
//    }
//    else
//    {
//        qint64 leaveBytes = dataSize - remainBytesToRead;
//        memcpy(dataPtr, dataBuf, remainBytesToRead);
//        dataBuf += remainBytesToRead;

//        RequestDesktopPacket packet;
//        memcpy(&packet, dataCache, sizeof(Packet));
//        dataPtr = dataCache;
////        emit FinishPacket(this, packet);
//        for(auto handler : requestDesktopPacketHandleres)
//        {
//            (*handler)(packet);
//        }


//        switch (packet.type) {
//        case PacketType::Empty:
////            remainBytesToRead = sizeof(Packet);
////            if(leaveBytes > 0)
////            {
////                ReadRequestPacket(dataBuf, leaveBytes);
////            }
//            break;
//        case PacketType::RequestDesktop:
//            break;
//        case PacketType::ResponseDesktop:
//            break;
//        case PacketType::DesktopBlock:
//            break;
//        }
//    }
//}

//TcpConnectionProxy* TcpConnectionProxy::CreateProxy(QTcpSocket* client)
//{
//    TcpConnectionProxy* proxy = new TcpConnectionProxy(client);
//    return proxy;
//}

NetworkSetting::NetworkSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NetworkSetting)
{
    ui->setupUi(this);
    server = new RDServer(this);
    client = new ClientThreadWorker();
//    clientThread.start();
}

NetworkSetting::~NetworkSetting()
{
    delete ui;
//    clientThread.terminate();
    delete server;
}

bool CheckIp(const QString& ipStr)
{
    if(ipStr.count('.') != 3)return false;
    for(auto ch : ipStr)
    {
        if(!ch.isDigit() && ch != '.')
        {
            logd("invalid char: %c", ch.toLatin1());
            return false;
        }
     }
    return true;
}

void NetworkSetting::on_waitConnBtn_clicked(bool checked)
{
    QString myIp = ui->ipEdit->text();
    if(!CheckIp(myIp))
    {
        logd("local host ip is invalid: %s", myIp.toStdString().c_str());
        return;
    }
    logd("listen to %s", myIp.toStdString().c_str());
    if(!server->listen(QHostAddress(myIp), SRV_PORT))
    {
        logd("fail to start the server %s", server->errorString().toStdString().c_str());
    }
}

void NetworkSetting::on_buildConnBtn_clicked(bool checked)
{
    QString hostIp = ui->ipEdit->text();
    if(!CheckIp(hostIp))
    {
        logd("remote host ip is invalid: %s", hostIp.toStdString().c_str());
        return;
    }
    client->SetHostIp(hostIp);
    QThread* clientThread = new QThread();
    clientThread->start();
    client->moveToThread(clientThread);
    connect(client, &ClientThreadWorker::startWork,
            client, &ClientThreadWorker::work);
    emit client->startWork();
}

//void ServerConnThreadWorker::on_serverConnRecvPacket(TcpConnectionProxy* clientProxy, const Packet& recvPacket)
//{
//    logd("server recv packet %d", (int)recvPacket.type);
//    logd("server send a packet");
//    Packet sendPacket;
//    sendPacket.type = PacketType::Empty;
//    clientProxy->GetSocket()->write((char*)&sendPacket, sizeof(sendPacket));
//}

//void ClientThreadWorker::on_clientRecvPacket(TcpConnectionProxy* clientProxy, const Packet& packet)
//{
//    logd("client recv packet %d", (int)packet.type);
//}

//struct ServerRequestPacketHandler : public RequestDesktopPacketHandler
//{
//public:
//    ServerRequestPacketHandler(ServerConnThreadWorker* w) : worker(w){}
//    void operator()(const RequestDesktopPacket& packet)
//    {

//    }
//private:
//    ServerConnThreadWorker* worker;
//};

void ServerConnThreadWorker::work()
{
    QTcpSocket* clientConn = new QTcpSocket();
    clientConn->setSocketDescriptor(descriptor);
//        TcpConnectionProxy* proxy = TcpConnectionProxy::CreateProxy(clientConn);
    clientProxy = new TcpConnectionProxy(clientConn, TcpConnReadState::WaitForRequestPacket);
    clientProxy->RegisterRequestDesktopPacketHandler(this);
    connect(clientProxy->GetSocket(),
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            OnSocketError);
}


void ClientThreadWorker::operator()(const ResponseDesktopPacket& packet)
{
    respPacket = packet;
    crntBlkIdx = 0;
//    logd("client recv response packet");
//    logd("width: %hu, height: %hu, rows: %hu, cols: %hu, blocks: %u",
//         packet.width, packet.height, packet.rows, packet.cols, packet.numBlocks);
//    logd("width: %hu, height: %hu",
//         packet.width, packet.height);
//    rddata.resize(packet.GetNumBlocks() * packet.GetNumBytesPerBlock());
    rddata.resize(packet.GetNumBytes());
    compressedRddata.resize(packet.GetNumBlocks() * packet.GetNumBytesPerBlock());
    epTimer.restart();
}

void ClientThreadWorker::operator()(const DesktopBlockPacket& packet)
{
//    logd("client recv block packet #%d", crntBlkIdx);

//    for(int i = 0; i < 10; ++i)
//    {
//        Logger::Inst().Debug("%02x", packet.block[i]);
//        if(i!=9)Logger::Inst().Debug(" ");
//    }
//    Logger::Inst().Debug("\n");

//    memcpy(rddata.data() + crntBlkIdx * respPacket.GetNumBytesPerBlock(),
//           packet.block, sizeof(packet.block));
    memcpy(compressedRddata.data() + crntBlkIdx * respPacket.GetNumBytesPerBlock(),
           packet.block, sizeof(packet.block));
    crntBlkIdx++;
    if(crntBlkIdx == respPacket.GetNumBlocks())
    {
        extern qint32 lz4Decompress(const void* src, void* dst, qint32 srcSize, qint32 maxOutputSize);
        lz4Decompress(compressedRddata.data(), rddata.data(),
                      respPacket.size, rddata.size());
        qDebug()<< "recv total data use"  << epTimer.elapsed() << " ms";
        crntBlkIdx = 0;
        for(auto handler : rddataHandleres)
        {
            (*handler)(rddata, respPacket.width, respPacket.height, respPacket.alignRow);
        }
        clientProxy->SwitchState(TcpConnReadState::WaitForResponsePacket);
        this->SendRequestDesktopPacket();
        // save data
//        QImage img(respPacket.width, respPacket.height, respPacket.pixelFormt);
//        quint64 paddingBytes = 0;
//        for(int i = 0; i < respPacket.height; ++i)
//        {
//            for(int j = 0; j < respPacket.width; ++j)
//            {
//                switch (respPacket.pixelFormt) {
//                case QImage::Format_RGB888:
//                {
//                    int pidx = (i * respPacket.width + j) * 3 + respPacket.GetNumPaddingBytesPerRow() * i;
//                    int x = j;
//                    int y = i;
//                    y = respPacket.height - y - 1;
//                    //x = respPacket.width - x - 1;
//                    //img.setPixelColor(x, y, QColor(rddata[pidx], rddata[pidx+1], rddata[pidx+2]));
//                    img.setPixelColor(x, y, QColor(rddata[pidx+2], rddata[pidx+1], rddata[pidx]));
//                    break;
//                }
//                default:
//                    assert(0);
//                }

//            }
//        }
//        img.save("test.png");
    }


}

void ClientThreadWorker::SendRequestDesktopPacket()
{
    RequestDesktopPacket packet;
    packet.type = PacketType::RequestDesktop;
//    logd("client send the first request packet");
    clientProxy->GetSocket()->write((char*)&packet, sizeof(packet));
    clientProxy->SwitchState(TcpConnReadState::WaitForResponsePacket);
}

void ClientThreadWorker::on_clientSocketStateChanged(QAbstractSocket::SocketState newState)
{
    logd("client socket state changed to: %d", (int)newState);
    if(newState == QAbstractSocket::SocketState::ConnectedState)
    {
        this->SendRequestDesktopPacket();
    }
}

void OnSocketError(QAbstractSocket::SocketError err)
{
    logd("socket error: %d", (int)err);
}

//struct ClientResponsePacketHandler : public ResponseDesktopPacketHandler
//{
//public:
//    ClientResponsePacketHandler(ClientThreadWorker* w) : worker(w){}
//    virtual void operator()(const ResponseDesktopPacket& packet) override
//    {

//    }
//private:
//    ClientThreadWorker* worker;
//};

//struct ClientDesktopBlockPacketHandler : public DesktopBlockPacketHandler
//{
//public:
//    ClientDesktopBlockPacketHandler(ClientThreadWorker* w) : worker(w){}
//    virtual void operator()(const DesktopBlockPacket& packet) override
//    {

//    }
//private:
//    ClientThreadWorker* worker;
//};

void ClientThreadWorker::work()
{
    clientProxy = new TcpConnectionProxy(new QTcpSocket(), TcpConnReadState::WaitForResponsePacket);
//    connect(clientProxy, &TcpConnectionProxy::FinishPacket,
//            this, &ClientThreadWorker::on_clientRecvPacket);
    connect(clientProxy->GetSocket(), QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, OnSocketError);
    connect(clientProxy->GetSocket(), &QTcpSocket::stateChanged,
            this, &ClientThreadWorker::on_clientSocketStateChanged);
//    clientProxy->RegisterResponseDesktopPacketHandler(new ClientResponsePacketHandler(this));
//    clientProxy->RegisterDesktopBlockPacketHandler(new ClientDesktopBlockPacketHandler(this));
    clientProxy->RegisterResponseDesktopPacketHandler(this);
    clientProxy->RegisterDesktopBlockPacketHandler(this);
    logd("try connect to %s", hostIp.toStdString().c_str());
    clientProxy->GetSocket()->connectToHost(QHostAddress(hostIp), SRV_PORT);
}
