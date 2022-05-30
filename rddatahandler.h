#ifndef RDDATAHANDLER_H
#define RDDATAHANDLER_H
#include<QVector>
class RDDataHandler
{
public:
    virtual void operator()(const QVector<quint8>& rddata, quint16 width, quint16 height, bool rowAlignment){}
};

#endif // RDDATAHANDLER_H
