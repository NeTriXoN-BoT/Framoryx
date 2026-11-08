#include "passive_linux.hpp"
#include <QSocketNotifier>
#include <QByteArray>
#include <cstdlib>
#include <xcb/xcb.h>
#include <xcb/present.h>

struct PassiveCapture::Data {
    xcb_connection_t* connection=nullptr;
    xcb_special_event_t* events=nullptr;
    xcb_window_t root=0,target=0;
    xcb_atom_t activeAtom=0;
    uint32_t subscription=0,stamp=0;
    uint64_t previous=0;
    std::unique_ptr<QSocketNotifier> notifier;
    QList<double> pending;
};
namespace {
xcb_atom_t atom(xcb_connection_t* connection,const QByteArray& name){
    auto reply=xcb_intern_atom_reply(connection,xcb_intern_atom(connection,1,uint16_t(name.size()),name.constData()),nullptr);
    if(!reply)return 0;const auto result=reply->atom;free(reply);return result;
}
quint32 property(xcb_connection_t* connection,xcb_window_t window,xcb_atom_t key,xcb_atom_t type){
    if(!key)return 0;
    auto reply=xcb_get_property_reply(connection,xcb_get_property(connection,0,window,key,type,0,1),nullptr);
    quint32 result=0;if(reply&&reply->format==32&&xcb_get_property_value_length(reply)==4)result=*static_cast<const uint32_t*>(xcb_get_property_value(reply));free(reply);return result;
}
}
PassiveCapture::PassiveCapture(QObject* parent):QObject(parent),d(std::make_unique<Data>()){}
PassiveCapture::~PassiveCapture(){stop();}
bool PassiveCapture::start(quint32 window,quint32 pid,QString* error){
    stop();int screen=0;d->connection=xcb_connect(nullptr,&screen);
    auto fail=[&](const QString& text){*error=text;stop();return false;};
    if(!d->connection||xcb_connection_has_error(d->connection))return fail("The X11 display is unavailable. Native Wayland surfaces cannot be monitored through X Present.");
    auto screens=xcb_setup_roots_iterator(xcb_get_setup(d->connection));for(int i=0;i<screen&&screens.rem;++i)xcb_screen_next(&screens);
    if(!screens.rem)return fail("The X11 screen is unavailable.");d->root=screens.data->root;
    const auto extension=xcb_get_extension_data(d->connection,&xcb_present_id);
    if(!extension||!extension->present)return fail("This X server does not provide the Present extension.");
    const auto pidAtom=atom(d->connection,"_NET_WM_PID");
    if(!window||!pid||property(d->connection,window,pidAtom,XCB_ATOM_CARDINAL)!=pid)return fail("The game window identity changed. Refresh the game list before capturing.");
    d->activeAtom=atom(d->connection,"_NET_ACTIVE_WINDOW");d->target=window;d->subscription=xcb_generate_id(d->connection);
    d->events=xcb_register_for_special_xge(d->connection,&xcb_present_id,d->subscription,&d->stamp);
    if(!d->events)return fail("Present event monitoring could not be registered.");
    auto failure=xcb_request_check(d->connection,xcb_present_select_input_checked(d->connection,d->subscription,window,XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY));
    if(failure){free(failure);return fail("The X server refused Present events for this window.");}
    xcb_flush(d->connection);
    d->notifier=std::make_unique<QSocketNotifier>(xcb_get_file_descriptor(d->connection),QSocketNotifier::Read,this);
    connect(d->notifier.get(),&QSocketNotifier::activated,this,[this]{drain();});
    return true;
}
void PassiveCapture::drain(){
    if(!d->connection||!d->events)return;
    int count=0;
    while(count++<8192){auto event=xcb_poll_for_special_event(d->connection,d->events);if(!event)break;
        const auto generic=reinterpret_cast<xcb_ge_generic_event_t*>(event);
        if(generic->event_type==XCB_PRESENT_COMPLETE_NOTIFY){const auto complete=reinterpret_cast<xcb_present_complete_notify_event_t*>(event);
            if(complete->window==d->target&&complete->kind==XCB_PRESENT_COMPLETE_KIND_PIXMAP&&complete->mode!=XCB_PRESENT_COMPLETE_MODE_SKIP){
                if(d->previous&&complete->ust>d->previous&&d->pending.size()<8192)d->pending<<double(complete->ust-d->previous)/1000.;d->previous=complete->ust;
            }
        }free(event);
    }
}
void PassiveCapture::stop(){
    d->notifier.reset();
    if(d->connection){if(d->events)xcb_unregister_for_special_event(d->connection,d->events);xcb_disconnect(d->connection);}
    d->connection=nullptr;d->events=nullptr;d->target=0;d->root=0;d->previous=0;d->pending.clear();
}
bool PassiveCapture::running()const{return d->connection&&d->target&&!xcb_connection_has_error(d->connection);}
quint32 PassiveCapture::window()const{return d->target;}
quint32 PassiveCapture::foreground()const{return d->connection?property(d->connection,d->root,d->activeAtom,XCB_ATOM_WINDOW):0;}
QList<double> PassiveCapture::takeFrames(){drain();QList<double> result;result.swap(d->pending);return result;}
