#ifndef LOGGER_H
#define LOGGER_H
#include<cstdarg>
#include<cstdio>
#include<QString>
#include<QVector>
#include<QObject>
class LoggerHandler : public QObject
{
    Q_OBJECT
signals:
   void siglog(QString log);
public:
   void HandleLog(const char* text)
   {
        emit siglog(QString(text));
   }
};

class Logger
{
public:
    void Debug(const char* format, ...)
    {
        char buf[4096];
        va_list args;
        va_start(args, format);
        vsprintf(buf, format, args);
        va_end(args);

        for(auto handler : logHandleres)
            handler->HandleLog(buf);
    }

    void Debugln(const char* format, ...)
    {
        char buf[4096];
        va_list args;
        va_start(args, format);
        vsprintf(buf, format, args);
        va_end(args);
        for(auto handler : logHandleres)
        {
            handler->HandleLog(buf);
            handler->HandleLog("\n");
        }

    }

    void RegisterHandler(LoggerHandler* handler)
    {
        logHandleres.append(handler);
    }

    static Logger& Inst()
    {
        static Logger logger;
        return logger;
    }

private:
    QVector<LoggerHandler*> logHandleres;
};
#define logd(...) Logger::Inst().Debugln(__VA_ARGS__)
#endif // LOGGER_H
