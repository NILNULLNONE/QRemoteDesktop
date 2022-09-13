#include "networksetting.h"
#include "ui_networksetting.h"
#include <QHostAddress>
#include <QNetworkInterface>
#include <cctype>
#include "logger.h"
void SocketSendData(QTcpSocket* sck, const char* data, qint64 size)
{
    sck->write(data, size);
}

template<typename MType>
void SocketSendData(QTcpSocket* sck, MType& msg)
{
    SocketSendData(sck, (char*)&msg, sizeof(msg));
}

template<typename MType>
void SocketSendBaseData(QTcpSocket* sck, QRDMessageType msgType, MType& msg)
{
    QRDMessageBase msgBase;
    QRDMessageBase::Create(msgType, msg, msgBase);
    SocketSendData(sck, msgBase);
}

void SocketRecvData(QTcpSocket* sck, char* buf, qint64 size)
{
    while(size > 0)
    {
        sck->waitForReadyRead(-1);
        qint64 cnt = sck->read(buf, size);
        size -= cnt;
        buf += cnt;
    }
}

template<typename MType>
void SocketRecvData(QTcpSocket* sck, MType& msg)
{
    SocketRecvData(sck, (char*)&msg, sizeof(msg));
}

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
    logd("server start work");
    clientConn = new QTcpSocket();
    clientConn->setSocketDescriptor(descriptor);

    QRDMessageBase baseMsg;
    QRDMsgRequestDesktopCapture req;
    while(true)
    {
        logd("server try recv data");
        SocketRecvData(clientConn, baseMsg);

        if(baseMsg.type == QRDMessageType::RequestDekstop)
        {
            baseMsg.ParseData(req);
            SendDesktopCapture(req);
        }
    }
}

void ServerConnThreadWorker::SendDesktopCapture(QRDMsgRequestDesktopCapture& req)
{
    logd("server send deskotp capture");
    QRDMsgDesktopDescription desc;
    desc.alignRow = true;
    desc.pixelFormt = QImage::Format_RGB888;
    GetDesktopCaptureInfo(&desc.width, &desc.height,
                          &desc.pixelFormt, &desc.alignRow);

    {
        desc.width *= req.scale;
        desc.height *= req.scale;
    }
    quint64 numBytes = desc.GetNumBytes();
    if(numBytes != captureData.size())
        captureData.resize(numBytes);
//        CaptureDesktop(captureData.data());
    StretchCaptureDesktop(captureData.data(), captureData.size(), desc.width, desc.height);

    // compress
    {
        extern qint32 lz4CalculateCompressedSize(qint32 uncompressedSize);
        extern qint32 lz4Compress(const void* src, void* dst, qint32 srcSize, qint32 maxOutputSize);
        qint32 compressedSize = lz4CalculateCompressedSize(static_cast<qint32>(captureData.size()));
        compressedData.resize(compressedSize);
        desc.compressedSize = lz4Compress(captureData.data(), compressedData.data(), captureData.size(), compressedSize);
    }


//        clientProxy->GetSocket()->write(reinterpret_cast<char*>(&respPacket), sizeof(respPacket));
    SocketSendBaseData(clientConn, QRDMessageType::DesktopDescription, desc);
    desc.DebugLog();

    quint64 bytesPerBlock = desc.GetNumBytesPerBlock();
    quint64 numBlocks = desc.GetNumBlocks();
//    DesktopBlockPacket blkPacket;
    QRDMsgDesktopDataBlock blkPacket;
    quint8* dataPtr = compressedData.data();

    for(int i = 0; i < numBlocks; ++i)
    {
        quint64 bytesToCopy = bytesPerBlock;
        if(numBytes < bytesToCopy)
            bytesToCopy = numBytes;
        memcpy(blkPacket.block, dataPtr, bytesToCopy);
        dataPtr += bytesToCopy;
        numBytes -= bytesToCopy;
        SocketSendData(clientConn, blkPacket);
    }
}

bool ClientThreadWorker::SendRequestDesktopPacket()
{
    logd("client send request desktop");
    QRDMsgRequestDesktopCapture msg;
    msg.scale = reqScale / 100.0;
    SocketSendBaseData(clientSocket, QRDMessageType::RequestDekstop, msg);
    return true;
}

bool ClientThreadWorker::RecvDesktopCapture(QRDMessageBase& baseMsg)
{
    logd("client recv desktop capture");
    QRDMsgDesktopDescription desc;
    baseMsg.ParseData(desc);
    desc.DebugLog();
    rddata.resize(desc.GetNumBytes());
    compressedRddata.resize(desc.GetNumBlocks() * desc.GetNumBytesPerBlock());
    emit sig_UpdateSize(desc.width, desc.height);

    int numBlocks = desc.GetNumBlocks();
    logd("total blocks: %d", numBlocks);

    QRDMsgDesktopDataBlock block;
    for(int i = 0; i < numBlocks; ++i)
    {
        logd("client try recv block data");
        SocketRecvData(clientSocket, block);
        memcpy(compressedRddata.data() + i * desc.GetNumBytesPerBlock(),
               block.block, sizeof(block.block));

        if(i == numBlocks - 1)
        {
            extern qint32 lz4Decompress(const void* src, void* dst, qint32 srcSize, qint32 maxOutputSize);
            lz4Decompress(compressedRddata.data(), rddata.data(),
                          desc.compressedSize, rddata.size());
            qDebug()<<"recv total desktop"<<rddata.size();
            for(auto handler : rddataHandleres)
            {
                (*handler)(rddata, desc.width, desc.height, desc.alignRow);
            }
        }
    }
    return true;
}

void ClientThreadWorker::work()
{
    clientSocket = new QTcpSocket();//clientProxy->GetSocket();
    clientSocket->connectToHost(QHostAddress(hostIp), SRV_PORT);
    if(clientSocket->waitForConnected(3000))
    {
        if(!SendRequestDesktopPacket())
        {
            logd("fail to send request packet");
            return;
        }

        QRDMessageBase baseMsg;
        while(true)
        {
            logd("client try recv data");
            SocketRecvData(clientSocket, baseMsg);

            if(baseMsg.type == QRDMessageType::DesktopDescription)
            {
                RecvDesktopCapture(baseMsg);
                SendRequestDesktopPacket();
            }
        }
    }
    else
    {
        logd("connect to host fail!");
    }
}

void ClientThreadWorker::CleanUp()
{

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

