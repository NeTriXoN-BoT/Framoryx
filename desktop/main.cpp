#include "controller.hpp"
#include "backend_checks.hpp"
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
#include <QQuickItem>
#include <QMouseEvent>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QCryptographicHash>
#include <QLibrary>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

QQuickItem* findVisual(QQuickWindow* window,const QString& name){
    QList<QQuickItem*> pending{window->contentItem()};int visited=0;
    while(!pending.isEmpty()&&visited++<16384){auto item=pending.takeLast();if(item->objectName()==name)return item;pending.append(item->childItems());}return nullptr;
}
int main(int argc,char** argv){
#ifdef Q_OS_WIN
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
#endif
    QQuickStyle::setStyle("Basic");
    QQuickWindow::setDefaultAlphaBuffer(true);
    QApplication application(argc,argv);
    application.setQuitOnLastWindowClosed(false);
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
    parser.addOption({"background","Start in the system tray when available."});
    parser.addOption({"startup","Sign-in launch; do not activate an existing window."});
    parser.addOption({"test-root","Use an isolated configuration directory.","path"});
    parser.addOption({"self-test","Run headless model checks without audio or game changes."});
    parser.addOption({"backend-test","Run live integrations on an isolated CI runner."});
    parser.addOption({"ui-smoke","Run bounded UI checks on the isolated CI runner."});
    parser.addOption({"bundle-test","Verify packaged QML and rendering without the optional input driver."});
    parser.addOption({"snapshot-dir","Write CI UI captures and results here.","path"});
    parser.process(application);
    const bool bundleTest=parser.isSet("bundle-test"),modelTest=parser.isSet("self-test"),smoke=parser.isSet("ui-smoke")||bundleTest,backendTest=parser.isSet("backend-test");
    if((modelTest||smoke||backendTest)&&!parser.isSet("test-root")){qCritical("Diagnostics require --test-root.");return 2;}
    if((smoke||backendTest)&&qEnvironmentVariable("GITHUB_ACTIONS")!="true"){qCritical("UI diagnostics are restricted to the isolated CI runner.");return 2;}
    const QString root=parser.isSet("test-root")?QFileInfo(parser.value("test-root")).absoluteFilePath():QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    for(const auto character:root)if(character.unicode()<32){qCritical("Configuration paths cannot contain control characters.");return 2;}
    QDir().mkpath(root);
    QLockFile lock(root+"/desktop.lock");lock.setStaleLockTime(0);
    const auto key=QCryptographicHash::hash(root.toUtf8(),QCryptographicHash::Sha256).toHex().left(24);
    const QString serverName="Framoryx.Desktop."+QString::fromLatin1(key);
    if(!modelTest&&!smoke&&!backendTest&&!lock.tryLock()){
        QLocalSocket other;other.connectToServer(serverName);if(!parser.isSet("startup")&&other.waitForConnected(800)){other.write("show\n");other.waitForBytesWritten(200);}return 0;
    }
    Controller controller(root,modelTest||smoke);
    if(modelTest){QString error;if(!controller.selfTest(&error)){qCritical("FAIL: %s",qPrintable(error));return 1;}return 0;}
    QLocalServer server;server.setSocketOptions(QLocalServer::UserAccessOption);
    if(!smoke&&!backendTest){QLocalServer::removeServer(serverName);server.listen(serverName);QObject::connect(&server,&QLocalServer::newConnection,&controller,[&]{while(auto socket=server.nextPendingConnection()){QObject::connect(socket,&QLocalSocket::readyRead,socket,[&,socket]{if(socket->readAll().startsWith("show"))emit controller.showRequested();socket->disconnectFromServer();});QObject::connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);}});}
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("app",&controller);
    bool qmlFailure=false;QStringList qmlWarnings;
    QObject::connect(&engine,&QQmlEngine::warnings,&application,[&](const QList<QQmlError>& warnings){for(const auto& warning:warnings){qWarning().noquote()<<warning.toString();qmlWarnings<<warning.toString();}qmlFailure=true;});
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window)return 1;
    auto displayMetrics=[&]{auto screen=window->screen();if(!screen)screen=QGuiApplication::primaryScreen();if(screen){const auto area=screen->availableGeometry();window->setProperty("autoUiScale",qBound(.75,std::min(area.width()/1600.,area.height()/1000.),1.75));}};
    QMetaObject::Connection displayConnection;
    auto followDisplay=[&]{QObject::disconnect(displayConnection);if(auto screen=window->screen())displayConnection=QObject::connect(screen,&QScreen::availableGeometryChanged,window,[&](const QRect&){displayMetrics();});displayMetrics();};
    followDisplay();QObject::connect(window,&QWindow::screenChanged,window,[&](QScreen*){followDisplay();});
    if(!smoke){auto screen=window->screen();if(screen){const auto area=screen->availableGeometry();const auto scale=controller.options()["uiScale"].toInt()?controller.options()["uiScale"].toInt()/100.:window->property("autoUiScale").toDouble();const int width=qBound(640,qRound(1360*scale),std::max(640,area.width()-32)),height=qBound(460,qRound(900*scale),std::max(460,area.height()-48));window->resize(width,height);window->setPosition(area.center()-QPoint(width/2,height/2));}}

#ifdef Q_OS_WIN
    BOOL dark=TRUE;DwmSetWindowAttribute(HWND(window->winId()),20,&dark,sizeof(dark));
#endif
    if(backendTest){window->hide();return runBackendChecks(&controller,parser.value("snapshot-dir"),&qmlFailure);}
    QObject::connect(&controller,&Controller::quitRequested,&application,&QApplication::quit);
    QSystemTrayIcon tray(QIcon(":/art/icon.png"));QMenu menu;
    auto show=menu.addAction("Open Framoryx");QObject::connect(show,&QAction::triggered,&controller,[&]{emit controller.showRequested();});
    auto hide=menu.addAction("Hide to tray");QObject::connect(hide,&QAction::triggered,&controller,[&]{controller.action("hideToTray");});
    menu.addSeparator();auto capture=menu.addAction("Start capture");QObject::connect(capture,&QAction::triggered,&controller,[&]{controller.action("capture");});
    QObject::connect(&controller,&Controller::stateChanged,&menu,[&]{capture->setText(controller.state()["capture"].toBool()?"Stop capture":"Start capture");});
    auto overlay=menu.addAction("Toggle overlay");QObject::connect(overlay,&QAction::triggered,&controller,[&]{controller.action("overlay");});
    auto quit=menu.addAction("Quit Framoryx");QObject::connect(quit,&QAction::triggered,&application,&QApplication::quit);
    tray.setContextMenu(&menu);tray.setToolTip("Framoryx — Play at your best");
    QObject::connect(&tray,&QSystemTrayIcon::activated,&controller,[&](QSystemTrayIcon::ActivationReason reason){if(reason==QSystemTrayIcon::Trigger||reason==QSystemTrayIcon::DoubleClick)emit controller.showRequested();});
    auto syncTray=[&]{const bool available=QSystemTrayIcon::isSystemTrayAvailable();controller.setState("trayAvailable",available);if(available&&!smoke)tray.show();else {tray.hide();if(!smoke&&!window->isVisible())window->show();}};
    syncTray();
    QObject::connect(&controller,&Controller::hideRequested,window,[&]{if(tray.isVisible())window->hide();else {window->showMinimized();controller.notify("This desktop has no system tray. Framoryx was minimized to the taskbar.");}});
    auto trayTimer=new QTimer(&application);trayTimer->setInterval(5000);QObject::connect(trayTimer,&QTimer::timeout,&application,syncTray);if(!smoke)trayTimer->start();
    if(!smoke){const bool background=parser.isSet("background")||controller.options()["startHidden"].toBool();if(!background||!tray.isVisible())window->show();else window->hide();}
    if(smoke){
        const QString destination=QFileInfo(parser.value("snapshot-dir")).absoluteFilePath();QDir().mkpath(destination);
        window->show();window->resize(1440,980);
        window->setProperty("diagnosticAnimating",true);
#ifdef Q_OS_WIN
        ShowWindow(HWND(window->winId()),SW_SHOWNOACTIVATE);
#endif
        window->requestActivate();window->requestUpdate();
        auto inputDriver=std::make_shared<QLibrary>(QCoreApplication::applicationDirPath()+"/FramoryxUiChecks");const bool driverLoaded=inputDriver->load();using Click=void(*)(QWindow*,int,int);const auto click=driverLoaded?reinterpret_cast<Click>(inputDriver->resolve("FramoryxTestClick")):nullptr;
        auto timer=new QTimer(&application);timer->setInterval(950);int* step=new int(0);auto frames=std::make_shared<int>(0);auto interactions=std::make_shared<QVariantMap>();
        QObject::connect(window,&QQuickWindow::frameSwapped,&application,[frames]{++*frames;});
        QObject::connect(timer,&QTimer::timeout,&application,[&,timer,step,frames,interactions,destination,inputDriver,click]{
            if(*step<7){window->setProperty("page",*step);}
            else if(*step<14){const int page=*step-7;window->setProperty("page",page);QTimer::singleShot(450,&application,[&,page,destination]{if(!window->grabWindow().save(destination+QString("/page-%1.png").arg(page)))qmlFailure=true;});}
            else if(*step==14){controller.setOption("style",1);window->setProperty("page",1);}
            else if(*step==15){window->grabWindow().save(destination+"/studio-compact.png");controller.setOption("style",2);}
            else if(*step==16){window->grabWindow().save(destination+"/studio-telemetry.png");controller.setOption("uiScale",125);window->showMaximized();}
            else if(*step==17){window->grabWindow().save(destination+"/maximized-125.png");window->showNormal();window->resize(900,700);controller.setOption("uiScale",0);}
            else if(*step==18){window->grabWindow().save(destination+"/compact-auto.png");controller.setOption("motion",false);}
            else if(*step==19){window->resize(1440,980);window->setProperty("page",1);controller.setOption("style",3);controller.setOption("opacity",0);}
            else if(*step==20){window->grabWindow().save(destination+"/studio-transparent.png");controller.setOption("style",4);controller.setOption("opacity",90);controller.setOption("accent",4);}
            else if(*step==21){window->grabWindow().save(destination+"/studio-dashboard.png");window->setProperty("page",2);controller.setOption("uiScale",100);}
            else if(*step==22){
                auto item=findVisual(window,"output-volume-slider");bool pass=item&&item->isVisible();QElapsedTimer elapsed;elapsed.start();
                if(pass){auto send=[&](QEvent::Type type,qreal x,Qt::MouseButton button,Qt::MouseButtons buttons){const auto position=item->mapToScene(QPointF(x,item->height()/2));QMouseEvent event(type,position,QPointF(window->mapToGlobal(position.toPoint())),button,buttons,Qt::NoModifier);QCoreApplication::sendEvent(window,&event);};
                    send(QEvent::MouseButtonPress,8,Qt::LeftButton,Qt::LeftButton);
                    for(int i=0;i<120;++i)send(QEvent::MouseMove,8+(item->width()-16)*(i%40)/39.,Qt::NoButton,Qt::LeftButton);
                    send(QEvent::MouseMove,item->width()-8,Qt::NoButton,Qt::LeftButton);send(QEvent::MouseButtonRelease,item->width()-8,Qt::LeftButton,Qt::NoButton);
                    pass=controller.options()["volume"].toInt()==200;
                }
                (*interactions)["sliderDragMs"]=elapsed.elapsed();(*interactions)["sliderFinalValue"]=controller.options()["volume"];(*interactions)["sliderPassed"]=pass;(*interactions)["sliderEvents"]=123;
            }
            else if(*step==23&&bundleTest){(*interactions)["tabNavigationChecked"]=false;}
            else if(*step==23){
                bool pass=true;QVariantList tabResults;QElapsedTimer elapsed;elapsed.start();
                if(!click){pass=false;(*interactions)["inputError"]=inputDriver->errorString();}
                for(int i=0;i<7;++i){auto item=findVisual(window,QString("navigation-%1").arg(i));if(!item){pass=false;continue;}const auto position=item->mapToScene(QPointF(item->width()/2,item->height()/2));if(click)click(window,qRound(position.x()),qRound(position.y()));const bool selected=window->property("page").toInt()==i;tabResults<<QVariantMap{{"tab",i},{"page",window->property("page")},{"visible",item->isVisible()},{"activeWindow",window->isActive()}};pass=pass&&selected;}
                (*interactions)["tabs"]=tabResults;(*interactions)["sevenTabSwitchesMs"]=elapsed.elapsed();(*interactions)["tabNavigationPassed"]=pass;(*interactions)["statePreserved"]=controller.options()["volume"].toInt()==200;
            }
            else if(*step==24){
                timer->stop();const bool pass=!qmlFailure&&*frames>10&&interactions->value("sliderPassed").toBool()&&(bundleTest||(interactions->value("tabNavigationPassed").toBool()&&interactions->value("statePreserved").toBool()));
                QSaveFile file(destination+"/ui-result.json");file.open(QIODevice::WriteOnly);file.write(QJsonDocument(QJsonObject{{"passed",pass},{"interactions",QJsonObject::fromVariantMap(*interactions)},{"qmlWarnings",qmlFailure},{"warnings",QJsonArray::fromStringList(qmlWarnings)},{"frames",*frames},{"exposed",window->isExposed()},{"visible",window->isVisible()},{"graphicsApi",int(window->rendererInterface()->graphicsApi())},{"pages",7},{"screenshots",13},{"platform",controller.platformName()}}).toJson());file.commit();delete step;application.exit(pass?0:1);return;
            }
            ++*step;
        });timer->start();
        QTimer::singleShot(35000,&application,[&]{qCritical("UI diagnostic timeout");application.exit(1);});
    }
    const int result=application.exec();return result;
}
