#include<QtCore>
#include<QDebug>
#include"lz4/lz4.h"
qint32 lz4CalculateCompressedSize(qint32 uncompressedSize)
{
    return LZ4_compressBound(uncompressedSize);
//    return 0;
}

qint32 lz4Compress(const void* src, void* dst, qint32 srcSize, qint32 maxOutputSize)
{
    qint32 res = LZ4_compress_fast((char*)src, (char*)dst, srcSize, maxOutputSize, 1);
    if(res <= 0)
    {
        qDebug()<<"fail to compress data";
    }
    return res;
//    return 0;
}

qint32 lz4Decompress(const void* src, void* dst, qint32 srcSize, qint32 dstSize)
{
    qint32 res = LZ4_decompress_safe((char*)src, (char*)dst, srcSize, dstSize);
    if(res <= 0)
    {
        qDebug()<<"fail to decompress data "<< res;
    }
    return res;
//    return 0;
}
