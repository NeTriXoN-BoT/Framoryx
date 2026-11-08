#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>
#include <QTimer>
#include <QWindow>
#include <QElapsedTimer>
#include <cmath>
int main(int argc,char** argv){
    QGuiApplication application(argc,argv);
    application.setApplicationName("FramoryxScene");
    QSurfaceFormat format;format.setVersion(2,1);format.setSwapInterval(0);
    QWindow window;window.setSurfaceType(QWindow::OpenGLSurface);window.setFormat(format);window.resize(960,600);window.setTitle("Framoryx graphics test scene");window.show();
    QOpenGLContext context;context.setFormat(format);if(!context.create()||!context.makeCurrent(&window))return 1;
    auto functions=context.functions();QElapsedTimer clock;clock.start();
    QTimer timer;QObject::connect(&timer,&QTimer::timeout,&application,[&]{if(!window.isExposed())return;if(!context.makeCurrent(&window)){application.exit(1);return;}const float t=clock.elapsed()/1000.f;functions->glViewport(0,0,int(window.width()*window.devicePixelRatio()),int(window.height()*window.devicePixelRatio()));functions->glClearColor(.035f+.02f*std::sin(t),.10f+.04f*std::sin(t*.4f),.13f,1.f);functions->glClear(GL_COLOR_BUFFER_BIT);context.swapBuffers(&window);});timer.start(0);
    const auto arguments=application.arguments();if(arguments.contains("--seconds")){const int index=arguments.indexOf("--seconds");bool ok=false;const int seconds=arguments.value(index+1).toInt(&ok);if(ok&&seconds>0&&seconds<=600)QTimer::singleShot(seconds*1000,&application,&QGuiApplication::quit);}
    return application.exec();
}
