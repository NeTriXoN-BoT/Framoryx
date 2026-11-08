#include <QCoreApplication>
#include <QCommandLineParser>
#include <QSocketNotifier>
#include <QTimer>
#include <xcb/xcb.h>
#include <xcb/present.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QCommandLineParser parser;parser.addHelpOption();parser.addOption({"seconds","Close after this many seconds.","count","0"});parser.process(app);
    int screenIndex=0;auto connection=xcb_connect(nullptr,&screenIndex);if(!connection||xcb_connection_has_error(connection))return 2;
    const auto extension=xcb_get_extension_data(connection,&xcb_present_id);if(!extension||!extension->present){xcb_disconnect(connection);return 2;}
    auto screens=xcb_setup_roots_iterator(xcb_get_setup(connection));for(int i=0;i<screenIndex&&screens.rem;++i)xcb_screen_next(&screens);if(!screens.rem){xcb_disconnect(connection);return 2;}const auto screen=screens.data;
    auto atom=[&](const char* text){auto reply=xcb_intern_atom_reply(connection,xcb_intern_atom(connection,0,uint16_t(strlen(text)),text),nullptr);const auto value=reply?reply->atom:0;free(reply);return value;};
    const auto window=xcb_generate_id(connection);const uint32_t attrs[]{screen->black_pixel,XCB_EVENT_MASK_STRUCTURE_NOTIFY|XCB_EVENT_MASK_EXPOSURE};
    xcb_create_window(connection,screen->root_depth,window,screen->root,80,80,640,360,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,screen->root_visual,XCB_CW_BACK_PIXEL|XCB_CW_EVENT_MASK,attrs);
    const char title[]="Framoryx · external capture test";xcb_change_property(connection,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,sizeof(title)-1,title);
    const char klass[]="FramoryxPresentScene\0FramoryxPresentScene\0";xcb_change_property(connection,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_CLASS,XCB_ATOM_STRING,8,sizeof(klass)-1,klass);
    const uint32_t pid=getpid();xcb_change_property(connection,XCB_PROP_MODE_REPLACE,window,atom("_NET_WM_PID"),XCB_ATOM_CARDINAL,32,1,&pid);
    const auto protocol=atom("WM_PROTOCOLS"),close=atom("WM_DELETE_WINDOW");xcb_change_property(connection,XCB_PROP_MODE_REPLACE,window,protocol,XCB_ATOM_ATOM,32,1,&close);
    xcb_pixmap_t pixmaps[2]{xcb_generate_id(connection),xcb_generate_id(connection)};for(auto pixmap:pixmaps)xcb_create_pixmap(connection,screen->root_depth,pixmap,window,640,360);
    const auto gc=xcb_generate_id(connection);xcb_create_gc(connection,gc,window,0,nullptr);bool available[2]{true,true};uint32_t serial=0;
    const auto subscription=xcb_generate_id(connection);xcb_present_select_input(connection,subscription,window,XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY);xcb_map_window(connection,window);xcb_flush(connection);
    QSocketNotifier events(xcb_get_file_descriptor(connection),QSocketNotifier::Read);
    QObject::connect(&events,&QSocketNotifier::activated,&app,[&]{if(xcb_connection_has_error(connection)){app.quit();return;}while(auto event=xcb_poll_for_event(connection)){
        const auto type=event->response_type&127;
        if(type==XCB_CLIENT_MESSAGE&&reinterpret_cast<xcb_client_message_event_t*>(event)->data.data32[0]==close)app.quit();
        if(type==XCB_DESTROY_NOTIFY)app.quit();
        if(type==XCB_GE_GENERIC){auto generic=reinterpret_cast<xcb_ge_generic_event_t*>(event);if(generic->event_type==XCB_PRESENT_IDLE_NOTIFY){auto idle=reinterpret_cast<xcb_present_idle_notify_event_t*>(event);for(int i=0;i<2;++i)if(idle->pixmap==pixmaps[i])available[i]=true;}}
        free(event);
    }});
    QTimer frames;frames.setTimerType(Qt::PreciseTimer);frames.setInterval(16);
    QObject::connect(&frames,&QTimer::timeout,&app,[&]{int slot=available[0]?0:available[1]?1:-1;if(slot<0)return;available[slot]=false;const uint32_t background=0x101e2b,accent=0x7df2c6;xcb_change_gc(connection,gc,XCB_GC_FOREGROUND,&background);const xcb_rectangle_t full{0,0,640,360};xcb_poly_fill_rectangle(connection,pixmaps[slot],gc,1,&full);xcb_change_gc(connection,gc,XCB_GC_FOREGROUND,&accent);const xcb_rectangle_t bar{int16_t((serial*3)%560),150,80,60};xcb_poly_fill_rectangle(connection,pixmaps[slot],gc,1,&bar);
        xcb_present_pixmap(connection,window,pixmaps[slot],++serial,0,0,0,0,0,0,0,XCB_PRESENT_OPTION_COPY,0,0,0,0,nullptr);xcb_flush(connection);
    });frames.start();const int seconds=parser.value("seconds").toInt();if(seconds>0)QTimer::singleShot(std::min(seconds,300)*1000,&app,&QCoreApplication::quit);
    const int result=app.exec();frames.stop();events.setEnabled(false);xcb_disconnect(connection);return result;
}
