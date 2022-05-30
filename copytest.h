#ifndef COPYTEST_H
#define COPYTEST_H
#include<QObject>
#include<QDebug>
class TestCopy
{
public:
    TestCopy(){qDebug()<<"construct test copy";}
    TestCopy(const TestCopy& t){qDebug()<<"copy test copy";}
    TestCopy(TestCopy&& t){qDebug()<<"move test copy";}
};

class TestObj : public QObject
{
    Q_OBJECT
public:
signals:
    void sig1(TestCopy);
    void sig2(TestCopy);
public:
    void handle(TestCopy)
    {
        qDebug()<<"handle";
    }
    TestObj()
    {
        connect(this, &TestObj::sig1, this, &TestObj::sig2, Qt::ConnectionType::QueuedConnection);
        connect(this, &TestObj::sig2, this, &TestObj::handle, Qt::ConnectionType::QueuedConnection);
    }
    void Test()
    {
        qDebug()<<"TEST()";
        emit sig1(tc);
    }
    TestCopy tc;
};

#endif // COPYTEST_H
