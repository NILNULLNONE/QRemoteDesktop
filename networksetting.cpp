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
    connect(clientConn, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &TcpConnectionProxy::sig_SocketError);
    connect(clientConn, &QTcpSocket::stateChanged, this, &TcpConnectionProxy::sig_SocketStateChanged);
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
    }
}

NetworkSetting::NetworkSetting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NetworkSetting)
{
    ui->setupUi(this);
    server = new RDServer(this);
    client = new ClientThreadWorker();

//    ui->resoBox->addItem("1920x1080");
//    ui->resoBox->addItem("1024x768");
//    ui->resoBox->addItem("800x600");

    connect(ui->scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            client, &ClientThreadWorker::UpdateRequestScale);
    connect(client, &ClientThreadWorker::sig_UpdateSize,
            this, &NetworkSetting::sig_UpdateSize);
}

//void NetworkSetting::Init()
//{
//    ui->resoBox->setCurrentIndex(1);
//}

NetworkSetting::~NetworkSetting()
{
    delete ui;
    delete server;
    delete client;
    qDebug("delete network setting");
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
    if(server->isListening())
        return;
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
    emit sig_buildConn();
    if(client->IsConnected())
    {
        logd("alread connected");
        return;
    }
    client->SetHostIp(hostIp);
    emit client->startWork();
}

void ServerConnThreadWorker::work()
{
    QTcpSocket* clientConn = new QTcpSocket();
    clientConn->setSocketDescriptor(descriptor);
    clientProxy = new TcpConnectionProxy(clientConn, TcpConnReadState::WaitForRequestPacket);
    clientProxy->RegisterRequestDesktopPacketHandler(this);
    connect(clientConn, &QTcpSocket::disconnected,
            this, &ServerConnThreadWorker::on_ClientDisconnect);
//    connect(clientProxy->GetSocket(),
//            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
//            this, &ServerConnThreadWorker::on_SocketError);
}


void ClientThreadWorker::operator()(const ResponseDesktopPacket& packet)
{
    respPacket = packet;
    crntBlkIdx = 0;
    rddata.resize(packet.GetNumBytes());
    compressedRddata.resize(packet.GetNumBlocks() * packet.GetNumBytesPerBlock());

    emit sig_UpdateSize(respPacket.width, respPacket.height);
//    epTimer.restart();
}

void ClientThreadWorker::operator()(const DesktopBlockPacket& packet)
{
    memcpy(compressedRddata.data() + crntBlkIdx * respPacket.GetNumBytesPerBlock(),
           packet.block, sizeof(packet.block));
    crntBlkIdx++;
    if(crntBlkIdx == respPacket.GetNumBlocks())
    {
        extern qint32 lz4Decompress(const void* src, void* dst, qint32 srcSize, qint32 maxOutputSize);
        lz4Decompress(compressedRddata.data(), rddata.data(),
                      respPacket.size, rddata.size());
//        qDebug()<< "recv total data use"  << epTimer.elapsed() << " ms";
        crntBlkIdx = 0;
        for(auto handler : rddataHandleres)
        {
            (*handler)(rddata, respPacket.width, respPacket.height, respPacket.alignRow);
        }
        clientProxy->SwitchState(TcpConnReadState::WaitForResponsePacket);
        this->SendRequestDesktopPacket();
    }
}

void ClientThreadWorker::SendRequestDesktopPacket()
{
    RequestDesktopPacket packet;
    packet.type = PacketType::RequestDesktop;
    packet.scale = reqScale / 100.0;
//    packet.width = reqWidth;
//    packet.height = reqHeight;
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
    else if(newState == QAbstractSocket::SocketState::UnconnectedState)
    {
//        this->CleanUp();
        emit this->sig_CleanUp();
    }
}

//void OnSocketError(QAbstractSocket::SocketError err)
//{
//    logd("socket error: %d", (int)err);
//}

void ClientThreadWorker::work()
{
    clientProxy = new TcpConnectionProxy(new QTcpSocket(), TcpConnReadState::WaitForResponsePacket);
    connect(clientProxy, &TcpConnectionProxy::sig_SocketError,
            this, &ClientThreadWorker::on_clientSocketError);
    connect(clientProxy, &TcpConnectionProxy::sig_SocketStateChanged,
            this, &ClientThreadWorker::on_clientSocketStateChanged);
    clientProxy->RegisterResponseDesktopPacketHandler(this);
    clientProxy->RegisterDesktopBlockPacketHandler(this);
    logd("try connect to %s", hostIp.toStdString().c_str());
    clientProxy->GetSocket()->connectToHost(QHostAddress(hostIp), SRV_PORT);
}

void ClientThreadWorker::on_clientSocketError(QAbstractSocket::SocketError err)
{
//    this->CleanUp();
    emit this->sig_CleanUp();
}

void ClientThreadWorker::CleanUp()
{
    if(clientProxy)
    {
        delete clientProxy;
        clientProxy = nullptr;
    }

}

//void NetworkSetting::on_resoBox_currentTextChanged(const QString &resoStr)
//{
//    QStringList splits = resoStr.split("x");
//    if(splits.size() != 2)return;
//    quint16 w = static_cast<quint16>(splits[0].trimmed().toUInt());
//    quint16 h = static_cast<quint16>(splits[1].trimmed().toUInt());
//    client->UpdateRequestResolution(w, h);
//    emit sig_resoChanged(w, h);
//}

