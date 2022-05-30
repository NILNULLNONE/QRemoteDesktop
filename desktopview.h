#ifndef DESKTOPVIEW_H
#define DESKTOPVIEW_H
#include<QOpenGLWidget>
#include<QOpenGLFunctions_3_2_Core>
#include"rddatahandler.h"
#include<QThread>
#include<QQueue>
#include<QMutex>
#include<QTimer>
#include<QDebug>
class DesktopView : public QOpenGLWidget, public RDDataHandler
{
    Q_OBJECT
public:
    DesktopView(QWidget* parent = nullptr)
        : QOpenGLWidget(parent)
    {
        QSurfaceFormat format;
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);
        format.setVersion(3, 2);
        format.setProfile(QSurfaceFormat::CoreProfile);
        this->setFormat(format); // must be called before the widget or its parent window gets shown
        connect(&updateTimer, &QTimer::timeout, this, QOverload<void>::of(&DesktopView::update));
        updateTimer.start(33);
    }

    void CreateProgram()
    {
        const char *vsrc =
            "#version 300 es\n"
            "in vec2 in_Vertex;\n"
            "in vec2 vertTexCoord;\n"
            "out vec2 fragTexCoord;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(in_Vertex, 0.0, 1.0);\n"
            "    fragTexCoord = vertTexCoord;\n"
            "}\n";
        const char *fsrc =
                "#version 300 es\n"
                "#ifdef GL_ES\n"
                "precision highp float;\n"
                "#endif\n"
                "uniform sampler2D tex;\n"
                "in vec2 fragTexCoord;\n"
                "out vec4 out_Color;\n"
                "void main()\n"
                "{\n"
                "    out_Color = texture(tex,fragTexCoord);\n"
                "    //out_Color = vec4(fragTexCoord, 0, 1);\n"
                "    //out_Color = vec4(1, 0.5, 0, 1);\n"
                "}\n";
        GLuint vsId = f->glCreateShader(GL_VERTEX_SHADER);
        GLuint fsId = f->glCreateShader(GL_FRAGMENT_SHADER);
        f->glShaderSource(vsId, 1, &vsrc, nullptr);
        f->glShaderSource(fsId, 1, &fsrc, nullptr);
        f->glCompileShader(vsId);
        GLint cmpResult;
        int infoLen;
        f->glGetShaderiv(vsId, GL_COMPILE_STATUS, &cmpResult);
        f->glGetShaderiv(vsId, GL_INFO_LOG_LENGTH, &infoLen);
        char errBuf[1024] = {0};
        if ( infoLen > 0 )
        {

            f->glGetShaderInfoLog(vsId, infoLen, NULL, errBuf);
            qDebug()<<errBuf;
        }
        else
        {
            qDebug()<<"compile vs success";
        }

        f->glCompileShader(fsId);
        f->glGetShaderiv(fsId, GL_COMPILE_STATUS, &cmpResult);
        f->glGetShaderiv(fsId, GL_INFO_LOG_LENGTH, &infoLen);
        if ( infoLen > 0 )
        {
            f->glGetShaderInfoLog(fsId, infoLen, NULL, errBuf);
            qDebug()<<errBuf;
        }
        else
        {
            qDebug()<<"compile fs success";
        }

        programId = f->glCreateProgram();
        f->glAttachShader(programId, vsId);
        f->glAttachShader(programId, fsId);
        f->glLinkProgram(programId);

        f->glGetProgramiv(programId, GL_LINK_STATUS, &cmpResult);
        f->glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLen);
        if ( infoLen > 0 )
        {
            f->glGetProgramInfoLog(programId, infoLen, NULL, errBuf);
            qDebug()<<errBuf;
        }
        else
        {
            qDebug()<<"link program success";
        }



        f->glDetachShader(programId, vsId);
        f->glDetachShader(programId, fsId);
        f->glDeleteShader(vsId);
        f->glDeleteShader(fsId);
    }

    void CreateBuffer()
    {
        f->glGenVertexArrays(1, &quadVAO);
        f->glBindVertexArray(quadVAO);


        f->glEnableVertexAttribArray(0);
        f->glGenBuffers(1, &quadVertexPosBufferId);
        f->glBindBuffer(GL_ARRAY_BUFFER, quadVertexPosBufferId);
        f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        float pos[] = {
            -1.0,-1.0,  1.0,-1.0,  1.0,1.0,
            1.0,1.0, -1, 1, -1, -1
        };
        f->glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);


        f->glGenBuffers(1, &quadVertexUVBufferId);
        f->glBindBuffer(GL_ARRAY_BUFFER, quadVertexUVBufferId);
        f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        float uv[] = {0.0,0.0,  1.0,0.0,  1.0,1.0,
                      1, 1, 0.0,1.0, 0, 0};
        f->glBufferData(GL_ARRAY_BUFFER, sizeof(uv), uv, GL_STATIC_DRAW);
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void CreateTexture()
    {
        f->glGenTextures(1, &desktex);
        f->glBindTexture(GL_TEXTURE_2D, desktex);
        uint8_t pixels[256][256][3];
        memset(pixels, 0, sizeof(pixels));
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_BGR, GL_UNSIGNED_BYTE, pixels);
        f->glBindTexture(GL_TEXTURE_2D, 0);
    }

    virtual void initializeGL()
    {
        f = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_3_2_Core>();
        f->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        CreateBuffer();
        CreateProgram();
        CreateTexture();
    }

    void UpdateDesktopTexture()
    {
        rddataQueueLock.lock();
        if(!rddataQueue.empty())
        {
            f->glBindTexture(GL_TEXTURE_2D, desktex);
            if(rddataQueue.back().rowAlign)
                f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                            rddataQueue.back().width,
                            rddataQueue.back().height,
                            0, GL_BGR, GL_UNSIGNED_BYTE,
                            rddataQueue.back().data.data());
            f->glBindTexture(GL_TEXTURE_2D, 0);


            rddataQueue.pop_back();
        }
        rddataQueueLock.unlock();
    }

    virtual void paintGL()
    {
        f->glClear(GL_COLOR_BUFFER_BIT);
        UpdateDesktopTexture();

        f->glBindVertexArray(quadVAO);
        f->glUseProgram(programId);
        f->glEnableVertexAttribArray(0);
        f->glEnableVertexAttribArray(1);

        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, desktex);
        f->glUniform1i(f->glGetUniformLocation(programId, "tex"), 0);
        f->glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    virtual void resizeGL(int w, int h)
    {
        QOpenGLWidget::resizeGL(w, h);
    }

public:
    virtual void operator()(const QVector<quint8>& data, quint16 width, quint16 height, bool rowAlign)override
    {
        rddataQueueLock.lock();
        rddataQueue.push_back({});
        rddataQueue.back().data = data;
        rddataQueue.back().width = width;
        rddataQueue.back().height = height;
        rddataQueue.back().rowAlign = rowAlign;
        rddataQueueLock.unlock();
    }
protected:
    virtual void closeEvent(QCloseEvent* evt)override
    {
        emit sig_close();
        QOpenGLWidget::closeEvent(evt);
    }

signals:
    void sig_close();
private:
    QOpenGLFunctions_3_2_Core *f;
    GLuint desktex;
    GLuint quadVertexPosBufferId;
    GLuint quadVertexUVBufferId;
    GLuint quadVAO;
    GLuint programId;
    struct RDDataInfo
    {
        QVector<quint8> data;
        quint16 width;
        quint16 height;
        bool rowAlign;
    };
    QQueue<RDDataInfo> rddataQueue;
    QMutex rddataQueueLock;
    QTimer updateTimer;
};

#endif // DESKTOPVIEW_H
