#include "x11_window.hpp"
#include <QFile>
#include <cstdlib>
#include <cstring>
#include <xcb/xcb.h>

namespace {
struct Display {
    xcb_connection_t* connection=nullptr;xcb_window_t root=0;int screen=0;
    Display(){connection=xcb_connect(nullptr,&screen);if(!connection||xcb_connection_has_error(connection))return;auto iter=xcb_setup_roots_iterator(xcb_get_setup(connection));for(int i=0;i<screen&&iter.rem;++i)xcb_screen_next(&iter);if(iter.rem)root=iter.data->root;}
    ~Display(){if(connection)xcb_disconnect(connection);}
    xcb_atom_t atom(const QByteArray& name){auto reply=xcb_intern_atom_reply(connection,xcb_intern_atom(connection,0,uint16_t(name.size()),name.constData()),nullptr);if(!reply)return 0;const auto value=reply->atom;free(reply);return value;}
    QByteArray property(quint32 id,const QByteArray& name,xcb_atom_t type,int format){const auto key=atom(name);auto reply=xcb_get_property_reply(connection,xcb_get_property(connection,0,id,key,type,0,256),nullptr);QByteArray value;if(reply&&reply->format==format)value=QByteArray(static_cast<const char*>(xcb_get_property_value(reply)),xcb_get_property_value_length(reply));free(reply);return value;}
    x11::WindowState inspect(quint32 id){
        x11::WindowState state;state.id=id;if(!root||!id)return state;
        auto attributes=xcb_get_window_attributes_reply(connection,xcb_get_window_attributes(connection,id),nullptr);if(!attributes)return state;free(attributes);
        const auto pid=property(id,"_NET_WM_PID",XCB_ATOM_CARDINAL,32);if(pid.size()!=4)return state;memcpy(&state.pid,pid.constData(),4);
        QFile stat(QString("/proc/%1/stat").arg(state.pid));if(!stat.open(QIODevice::ReadOnly))return state;const auto bytes=stat.read(4096);const auto fields=bytes.mid(bytes.lastIndexOf(')')+1).simplified().split(' ');if(fields.size()<20)return state;state.start=QString::fromLatin1(fields[19]);
        state.windowClass=property(id,"WM_CLASS",XCB_ATOM_STRING,8);const auto flags=property(id,"_NET_WM_STATE",XCB_ATOM_ATOM,32);const auto full=atom("_NET_WM_STATE_FULLSCREEN");
        for(int i=0;i+4<=flags.size();i+=4){quint32 flag;memcpy(&flag,flags.constData()+i,4);if(flag==full)state.fullscreen=true;}
        state.valid=!state.start.isEmpty();return state;
    }
};
}
namespace x11 {
quint32 foreground(){Display display;if(!display.root)return 0;const auto bytes=display.property(display.root,"_NET_ACTIVE_WINDOW",XCB_ATOM_WINDOW,32);quint32 id=0;if(bytes.size()==4)memcpy(&id,bytes.constData(),4);return id;}
bool desktopActive() {
    Display display;
    if (!display.root) return false;
    const auto active = display.property(display.root, "_NET_ACTIVE_WINDOW", XCB_ATOM_WINDOW, 32);
    if (active.size() != sizeof(xcb_window_t)) return false;
    xcb_window_t window;
    memcpy(&window, active.constData(), sizeof(window));
    if (window == XCB_NONE || window == display.root) return true;

    const auto types = display.property(window, "_NET_WM_WINDOW_TYPE", XCB_ATOM_ATOM, 32);
    if (types.size() < sizeof(xcb_atom_t)) return false;
    xcb_atom_t type;
    memcpy(&type, types.constData(), sizeof(type));
    return type != XCB_NONE && type == display.atom("_NET_WM_WINDOW_TYPE_DESKTOP");
}
WindowState inspect(quint32 id){Display display;return display.inspect(id);}
bool fullscreen(const WindowState& original,bool enabled){
    if(!original.valid)return false;Display display;const auto current=display.inspect(original.id);
    if(!current.valid||current.pid!=original.pid||current.start!=original.start||current.windowClass!=original.windowClass)return false;
    xcb_client_message_event_t message{};message.response_type=XCB_CLIENT_MESSAGE;message.format=32;message.window=original.id;message.type=display.atom("_NET_WM_STATE");
    message.data.data32[0]=enabled?1:0;message.data.data32[1]=display.atom("_NET_WM_STATE_FULLSCREEN");message.data.data32[3]=2;
    auto error=xcb_request_check(display.connection,xcb_send_event_checked(display.connection,0,display.root,XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,reinterpret_cast<const char*>(&message)));
    const bool result=!error;free(error);xcb_flush(display.connection);return result;
}
bool compositing(){Display display;if(!display.root)return false;const auto atom=display.atom("_NET_WM_CM_S"+QByteArray::number(display.screen));const auto reply=xcb_get_selection_owner_reply(display.connection,xcb_get_selection_owner(display.connection,atom),nullptr);const bool active=reply&&reply->owner!=XCB_WINDOW_NONE;free(reply);return active;}
}
