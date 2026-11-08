#include "controller.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QAction>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSGRendererInterface>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMenu>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QCryptographicHash>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

int main(int argc,char** argv){
#ifdef Q_OS_WIN
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
#endif
    QQuickStyle::setStyle("Basic");
    QApplication application(argc,argv);
    QCoreApplication::setApplicationName("Framoryx");
    QCoreApplication::setApplicationVersion("0.3.0");
    QCoreApplication::setOrganizationName("Framoryx");
    QGuiApplication::setDesktopFileName("io.framoryx.Framoryx");
    QApplication::setWindowIcon(QIcon(":/art/icon.png"));
    QPalette palette;palette.setColor(QPalette::Window,QColor("#131e29"));palette.setColor(QPalette::WindowText,QColor("#edf5fa"));palette.setColor(QPalette::Base,QColor("#152330"));palette.setColor(QPalette::Text,QColor("#edf5fa"));palette.setColor(QPalette::Button,QColor("#20333e"));palette.setColor(QPalette::ButtonText,QColor("#edf5fa"));palette.setColor(QPalette::Highlight,QColor("#7df2c6"));palette.setColor(QPalette::HighlightedText,QColor("#081916"));application.setPalette(palette);
    QFont font=application.font();font.setFamily(
#ifdef Q_OS_WIN
        "Segoe UI"
#else
        "Noto Sans"
#endif
    );font.setPixelSize(14);application.setFont(font);
    QCommandLineParser parser;parser.setApplicationDescription("Framoryx gaming toolkit");parser.addHelpOption();parser.addVersionOption();
    parser.addOption({"test-root","Use an isolated configuration directory.","path"});
    parser.addOption({"self-test","Run headless model checks without audio or game changes."});
    parser.addOption({"ui-smoke","Run bounded UI checks on the isolated CI runner."});
    parser.addOption({"snapshot-dir","Write CI UI captures and results here.","path"});
    parser.process(application);
    const bool modelTest=parser.isSet("self-test"),smoke=parser.isSet("ui-smoke");
    if((modelTest||smoke)&&!parser.isSet("test-root")){qCritical("Diagnostics require --test-root.");return 2;}
    if(smoke&&qEnvironmentVariable("GITHUB_ACTIONS")!="true"){qCritical("UI diagnostics are restricted to the isolated CI runner.");return 2;}
    const QString root=parser.isSet("test-root")?QFileInfo(parser.value("test-root")).absoluteFilePath():QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(root);
    QLockFile lock(root+"/desktop.lock");lock.setStaleLockTime(0);
    const auto key=QCryptographicHash::hash(root.toUtf8(),QCryptographicHash::Sha256).toHex().left(24);
    const QString serverName="Framoryx.Desktop."+QString::fromLatin1(key);
    if(!modelTest&&!smoke&&!lock.tryLock()){
        QLocalSocket other;other.connectToServer(serverName);if(other.waitForConnected(800)){other.write("show\n");other.waitForBytesWritten(200);}return 0;
    }
    Controller controller(root,modelTest||smoke);
    if(modelTest){QString error;if(!controller.selfTest(&error)){qCritical("FAIL: %s",qPrintable(error));return 1;}return 0;}
    QLocalServer server;server.setSocketOptions(QLocalServer::UserAccessOption);
    if(!smoke){QLocalServer::removeServer(serverName);server.listen(serverName);QObject::connect(&server,&QLocalServer::newConnection,&controller,[&]{while(auto socket=server.nextPendingConnection()){QObject::connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{if(socket->readAll().startsWith("show"))emit controller.showRequested();socket->disconnectFromServer();});QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);}});}
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("app",&controller);
    bool qmlFailure=false;QStringList qmlWarnings;
    QObject::connect(&engine,&QQmlEngine::warnings,&application,[&](const QList<QQmlError>& warnings){for(const auto& warning:warnings){qWarning().noquote()<<warning.toString();qmlWarnings<<warning.toString();}qmlFailure=true;});
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window)return 1;
#ifdef Q_OS_WIN
    BOOL dark=TRUE;DwmSetWindowAttribute(HWND(window->winId()),20,&dark,sizeof(dark));
#endif
    QObject::connect(&controller,&Controller::quitRequested,&application,&QApplication::quit);
    QSystemTrayIcon tray(QIcon(":/art/icon.png"));QMenu menu;
    auto show=menu.addAction("Open Framoryx");QObject::connect(show,&QAction::triggered,&controller,[&]{emit controller.showRequested();});
    auto hide=menu.addAction("Hide window");QObject::connect(hide,&QAction::triggered,window,&QWindow::hide);
    menu.addSeparator();auto overlay=menu.addAction("Toggle overlay");QObject::connect(overlay,&QAction::triggered,&controller,[&]{controller.action("overlay");});
    auto quit=menu.addAction("Quit Framoryx");QObject::connect(quit,&QAction::triggered,&application,&QApplication::quit);
    tray.setContextMenu(&menu);tray.setToolTip("Framoryx — Play at your best");
    QObject::connect(&tray,&QSystemTrayIcon::activated,&controller,[&](QSystemTrayIcon::ActivationReason reason){if(reason==QSystemTrayIcon::Trigger||reason==QSystemTrayIcon::DoubleClick)emit controller.showRequested();});
    if(QSystemTrayIcon::isSystemTrayAvailable()&&!smoke)tray.show();
    if(smoke){
        const QString destination=QFileInfo(parser.value("snapshot-dir")).absoluteFilePath();QDir().mkpath(destination);
        window->show();window->resize(1440,980);
        window->setProperty("diagnosticAnimating",true);
#ifdef Q_OS_WIN
        ShowWindow(HWND(window->winId()),SW_SHOWNOACTIVATE);
#endif
        window->requestUpdate();
        auto timer=new QTimer(&application);timer->setInterval(950);int* step=new int(0);auto frames=std::make_shared<int>(0);
        QObject::connect(window,&QQuickWindow::frameSwapped,&application,[frames]{++*frames;});
        QObject::connect(timer,&QTimer::timeout,&application,[&,timer,step,frames,destination]{
            if(*step<6){window->setProperty("page",*step);}
            else if(*step<12){const int page=*step-6;window->setProperty("page",page);if(!window->grabWindow().save(destination+QString("/page-%1.png").arg(page)))qmlFailure=true;}
            else if(*step==12){controller.setOption("style",1);window->setProperty("page",1);}
            else if(*step==13){window->grabWindow().save(destination+"/studio-compact.png");controller.setOption("style",2);}
            else if(*step==14){window->grabWindow().save(destination+"/studio-telemetry.png");controller.setOption("uiScale",125);window->showMaximized();}
            else if(*step==15){window->grabWindow().save(destination+"/maximized-125.png");window->showNormal();window->resize(900,700);controller.setOption("uiScale",0);}
            else if(*step==16){window->grabWindow().save(destination+"/compact-auto.png");controller.setOption("motion",false);}
            else if(*step==17){
                timer->stop();const bool pass=!qmlFailure&&*frames>10;
                QSaveFile file(destination+"/ui-result.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"passed",pass},{"qmlWarnings",qmlFailure},{"warnings",QJsonArray::fromStringList(qmlWarnings)},{"frames",*frames},{"exposed",window->isExposed()},{"visible",window->isVisible()},{"graphicsApi",int(window->rendererInterface()->graphicsApi())},{"pages",6},{"screenshots",10},{"platform",controller.platformName()}}).toJson());file.commit();delete step;application.exit(pass?0:1);return;
            }
            ++*step;
        });timer->start();
        QTimer::singleShot(35000,&application,[&]{qCritical("UI diagnostic timeout");application.exit(1);});
    }
    const int result=application.exec();return result;
}
