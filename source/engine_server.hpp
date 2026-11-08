// Authenticated parent/child stdio bridge. No network listener or shared command file.
class EnginePipe {
    std::thread inputThread, outputThread;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<nlohmann::json> commands;
    std::optional<std::string> outgoing;
    std::atomic<bool> stopped{false};
    std::atomic<DWORD> outputThreadId{0};
    HWND window = nullptr;
    HANDLE input = nullptr, output = nullptr;
  public:
    bool start(HWND target) {
        window=target;input=GetStdHandle(STD_INPUT_HANDLE);output=GetStdHandle(STD_OUTPUT_HANDLE);
        if(GetFileType(input)!=FILE_TYPE_PIPE||GetFileType(output)!=FILE_TYPE_PIPE)return false;
        try {
            inputThread=std::thread([this]{
                std::string pending;char buffer[4096];
                while(!stopped){DWORD available=0;if(!PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr))break;
                    if(!available){Sleep(15);continue;}
                    DWORD count=0;if(!ReadFile(input,buffer,std::min<DWORD>(sizeof(buffer),available),&count,nullptr)||!count)break;
                    pending.append(buffer,count);if(pending.size()>65536)break;
                    size_t end;while((end=pending.find('\n'))!=std::string::npos){
                        auto value=nlohmann::json::parse(pending.substr(0,end),nullptr,false);pending.erase(0,end+1);
                        if(!value.is_object()||!value.contains("token")||!value["token"].is_string()||value["token"].get<std::string>()!=fg::utf8(app.engineToken))continue;
                        {std::lock_guard lock(mutex);if(commands.size()>=64)commands.pop_front();commands.push_back(std::move(value));}
                        PostMessageW(window,WM_APP+70,0,0);
                    }
                }
                if(!stopped)PostMessageW(window,WM_CLOSE,0,0);
            });
            outputThread=std::thread([this]{
                outputThreadId=GetCurrentThreadId();
                std::unique_lock lock(mutex);
                while(!stopped){wake.wait(lock,[this]{return stopped||outgoing.has_value();});if(stopped)break;
                    auto text=std::move(*outgoing);outgoing.reset();lock.unlock();
                    DWORD written=0;bool ok=WriteFile(output,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size();
                    lock.lock();if(!ok){if(!stopped)PostMessageW(window,WM_CLOSE,0,0);break;}
                }
            });
        }catch(const std::exception&){stop();return false;}
        return true;
    }
    void publish(nlohmann::json value){if(stopped)return;std::lock_guard lock(mutex);outgoing=value.dump()+'\n';wake.notify_one();}
    std::deque<nlohmann::json> take(){std::lock_guard lock(mutex);std::deque<nlohmann::json> result;result.swap(commands);return result;}
    void stop(){stopped=true;wake.notify_all();const DWORD id=outputThreadId.load();if(id){fg::Handle thread(OpenThread(THREAD_TERMINATE,FALSE,id));if(thread)CancelSynchronousIo(thread.h);}if(inputThread.joinable())inputThread.join();if(outputThread.joinable())outputThread.join();}
    ~EnginePipe(){stop();}
} enginePipe;
void engineSelect(const nlohmann::json& request){
    refreshTargets();std::string id=request.value("id",std::string());
    const auto wanted=fg::lower(request.value("exe",std::string()));app.engineWantedExe=fg::validRule(wanted)?wanted:std::string();
    int index=0;
    for(size_t i=0;i<app.targets.size();++i){const auto& target=app.targets[i];
        const auto candidate=std::to_string(target.pid)+":"+std::to_string(reinterpret_cast<uintptr_t>(target.hwnd));
        if(candidate==id||(!request.value("exe",std::string()).empty()&&fg::lower(target.exe)==fg::lower(request.value("exe",std::string())))){index=int(i+1);break;}
    }
    SendMessageW(app.controls[Target],CB_SETCURSEL,index,0);SendMessageW(app.window,WM_COMMAND,MAKEWPARAM(Target,CBN_SELCHANGE),reinterpret_cast<LPARAM>(app.controls[Target]));
}
void engineOptions(const nlohmann::json& request){
    const auto options=request.value("options",nlohmann::json::object());const auto key=request.value("key",std::string());
    const auto profileKey=fg::lower(options.value("profileExe",app.activeProfile));if(!profileKey.empty()&&!fg::validRule(profileKey))return;
    const auto previousOptions=app.hudOptions;const int previousMode=app.hudMode,previousCorner=app.corner;
    const auto stored=app.hudProfiles.find(profileKey);const auto targetProfile=profileKey.empty()||stored==app.hudProfiles.end()?app.hudBase:stored->second;
    app.hudOptions=targetProfile.options;app.hudMode=targetProfile.mode;app.corner=targetProfile.corner;
    auto changed=[&](const std::string& name){if(key==name)return true;if(request.contains("keys")&&request["keys"].is_array())for(const auto& item:request["keys"])if(item.is_string()&&item.get<std::string>()==name)return true;return false;};
    const auto previousAllow=app.rules.allow,previousDeny=app.rules.deny;
    auto integer=[&](const char* name,int fallback,int low,int high){const auto value=options.find(name);if(value==options.end()||!value->is_number_integer())return fallback;const auto n=value->get<int64_t>();return int(std::clamp<int64_t>(n,low,high));};
    auto boolean=[&](const char* name,bool fallback){const auto value=options.find(name);return value!=options.end()&&value->is_boolean()?value->get<bool>():fallback;};
    int nextCap=integer("cap",app.cap,0,1000);if(nextCap>0&&nextCap<10)nextCap=10;
    if(!profileKey.empty())app.gameLimits[profileKey]=nextCap;
    if(profileKey==app.activeProfile){app.cap=nextCap;SetWindowTextW(app.controls[CapValue],std::to_wstring(app.cap).c_str());updateCap(false);}
    else if(!profileKey.empty()&&fg::lower(app.hook.name())==profileKey)app.hook.cap(nextCap);
    app.hudOptions.fields=uint32_t(integer("fields",int(app.hudOptions.fields),0,65535));
    app.hudOptions.scale=integer("overlayScale",app.hudOptions.scale,0,300);app.hudOptions.opacity=integer("opacity",app.hudOptions.opacity,0,100);app.hudOptions.style=integer("style",app.hudOptions.style,0,4);app.hudOptions.accent=integer("accent",app.hudOptions.accent,0,9);app.hudMode=integer("mode",app.hudMode,0,2);
    app.hudOptions.fontScale=integer("fontScale",app.hudOptions.fontScale,80,140);
    if(options.contains("order")&&options["order"].is_array()&&options["order"].size()==16){uint64_t order=0;bool valid=true;for(int i=0;i<16;++i){if(!options["order"][i].is_number_integer()){valid=false;break;}const int n=options["order"][i].get<int>();if(n<0||n>15){valid=false;break;}order|=uint64_t(n)<<(i*4);}if(valid)app.hudOptions.order=order;}
    auto coordinate=[&](const char* name,int fallback){if(!options.contains(name)||!options[name].is_number())return fallback;const double n=options[name].get<double>();return std::isfinite(n)?int(std::clamp(n,0.,1.)*10000):fallback;};
    app.hudOptions.x=coordinate("hudX",app.hudOptions.x);app.hudOptions.y=coordinate("hudY",app.hudOptions.y);
    app.autoBorderless=boolean("autoBorderless",app.autoBorderless);app.autoNoiseUpdates=boolean("autoNoise",app.autoNoiseUpdates);
    if(options.contains("allow")&&options["allow"].is_array()){app.rules.allow.clear();for(const auto& value:options["allow"])if(value.is_string()&&fg::validRule(value.get<std::string>()))app.rules.allow.push_back(value.get<std::string>());}
    if(options.contains("deny")&&options["deny"].is_array()){app.rules.deny.clear();for(const auto& value:options["deny"])if(value.is_string()&&fg::validRule(value.get<std::string>()))app.rules.deny.push_back(value.get<std::string>());}
    if(previousAllow!=app.rules.allow||previousDeny!=app.rules.deny){app.capture.setRules(app.rules);refreshTargets();app.borderless.restoreAll();if(app.hook.active()&&!app.rules.hookAllowed(app.hook.name()))app.hook.disable();}
    if(changed("micId")||changed("noise")||changed("eq")||changed("low")||changed("mid")||changed("high")||changed("voicePreset")||changed("noisePreset"))cancelPendingMicrophoneEdit();
    if(options.contains("micDevice")&&options["micDevice"].is_string()){
        const auto device=fg::wide(options["micDevice"].get<std::string>());const auto name=fg::wide(options.value("micName",std::string()));int match=-1;for(size_t i=0;i<app.microphones.size();++i)if(app.microphones[i].id==device)match=int(i);if(match<0&&!name.empty()){int count=0;for(size_t i=0;i<app.microphones.size();++i)if(app.microphones[i].name==name){match=int(i);++count;}if(count!=1)match=-1;}if(match>=0){SendMessageW(app.controls[MicDevice],CB_SETCURSEL,match,0);app.microphoneId=app.microphones[match].id;selectAutomaticNoisePlugin();}else if(!device.empty()){SendMessageW(app.controls[MicDevice],CB_SETCURSEL,-1,0);app.microphoneId.clear();app.audioNotice=L"The selected microphone is unavailable. Refresh devices before applying effects.";}
    }
    app.microphoneOptions.noise=boolean("noise",app.microphoneOptions.noise);app.microphoneOptions.equalizer=boolean("eq",app.microphoneOptions.equalizer);
    app.microphoneOptions.preset=integer("noisePreset",app.microphoneOptions.preset,0,2);app.microphoneOptions.low=integer("low",app.microphoneOptions.low,-12,12);app.microphoneOptions.mid=integer("mid",app.microphoneOptions.mid,-12,12);app.microphoneOptions.high=integer("high",app.microphoneOptions.high,-12,12);
    syncMicrophoneControls();selectComboValue(BassAmount,integer("bass",3,0,9));
    // The bass combo stores 0/3/6/9 by index rather than its descriptive caption.
    SendMessageW(app.controls[BassAmount],CB_SETCURSEL,integer("bass",3,0,9)/3,0);
    app.hudOptions.validate();const fg::HudProfile profile{app.hudOptions,app.corner,app.hudMode};if(profileKey.empty())app.hudBase=profile;else app.hudProfiles[profileKey]=profile;if(profileKey!=app.activeProfile){app.hudOptions=previousOptions;app.hudMode=previousMode;app.corner=previousCorner;}pushHookPresentation();positionHud();app.engineDirty=true;
}
void engineCommands(){
    if(!app.engineMode)return;
    for(const auto& request:enginePipe.take())try{
        const auto action=request.value("action",std::string());
        if(action=="options")engineOptions(request);
        else if(action=="telemetry"){const std::array<std::string,6> keys{"gpu","vramPercent","gpuTemp","cpuTemp","gpuPower","gpuClock"};for(size_t i=0;i<keys.size();++i){const auto found=request.find(keys[i]);app.sensorValues[i]=found!=request.end()&&found->is_number()?found->get<double>():-1;}app.hook.sensors(app.sensorValues);}
        else if(action=="safetyFallback"){app.safetyFallback=request.value("enabled",true);if(app.safetyFallback&&app.hook.pid()&&app.hook.status()!=2){app.hook.disable();setNotice(L"Switched to the external overlay because anti-cheat inspection is detected or uncertain. Restart the game to remove a previously loaded module completely.");}pushHookPresentation();positionHud();}
        else if(action=="select")engineSelect(request);
        else if(action=="refresh"){refreshTargets();refreshMicrophones();refreshAudioStatus();}
        else if(action=="captureStart"){if(!app.capture.running())SendMessageW(app.window,WM_COMMAND,Capture,0);}
        else if(action=="capture")SendMessageW(app.window,WM_COMMAND,Capture,0);
        else if(action=="overlay")SendMessageW(app.window,WM_COMMAND,Overlay,0);
        else if(action=="disableHook")SendMessageW(app.window,WM_COMMAND,Disable,0);
        else if(action=="enableHook"&&request.value("confirmed",false))attachHook(true);
        else if(action=="volume"){
            const int value=std::clamp(request.value("value",100),0,200);SendMessageW(app.controls[Volume],TBM_SETPOS,TRUE,value);SendMessageW(app.window,WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(app.controls[Volume]));
        }
        else if(action=="micApply")audioCommand(MicApply,BN_CLICKED);
        else if(action=="micDisable")audioCommand(MicDisable,BN_CLICKED);
        else if(action=="bassApply")audioCommand(BassApply,BN_CLICKED);
        else if(action=="bassDisable")audioCommand(BassDisable,BN_CLICKED);
        else if(action=="noiseUpdate")startNoiseUpdate(request.value("force",false));
        else if(action=="soundSettings")SendMessageW(app.window,WM_COMMAND,SoundSettings,0);
        else if(action=="getApo")audioCommand(AudioGetAPO,BN_CLICKED);
        else if(action=="apoConfigurator")audioCommand(AudioConfigurator,BN_CLICKED);
        else if(action=="apoConfig"){
            const auto path=fs::path(fg::wide(request.value("path",std::string())));
            if(fg::lower(fg::utf8(path.filename().wstring()))=="config.txt"&&app.boost.directory(path.parent_path().wstring())){saveSettings();app.audioNotice=L"APO configuration selected. Verify device setup before applying effects.";}else app.audioNotice=L"Choose config.txt in a local Equalizer APO folder.";
        }
        else if(action=="borderless")makeBorderless();
        else if(action=="restore")SendMessageW(app.window,WM_COMMAND,Restore,0);
        else if(action=="fixture")launchDemo();
        else if(action=="shortcut")setNotice(createDesktopShortcut());
        else if(action=="export"){
            const auto path=fg::wide(request.value("path",std::string()));if(!path.empty()){std::ofstream file(fs::path(path),std::ios::binary);const bool ok=fg::writeCaptureCsv(file,app.stats);file.close();setNotice(ok&&file?L"Rolling capture exported.":L"The capture could not be exported.");}
        }
        else if(action=="quit")PostMessageW(app.window,WM_CLOSE,0,0);
    }catch(const std::exception&){setNotice(L"The requested operation contained invalid settings.");}
}
void enginePublish(){
    if(!app.engineMode)return;
    using Json=nlohmann::json;Json windows=Json::array(),microphones=Json::array(),graph=Json::array();
    for(const auto& target:app.targets){if(windows.size()>=200)break;windows.push_back({{"id",std::to_string(target.pid)+":"+std::to_string(reinterpret_cast<uintptr_t>(target.hwnd))},{"name",fg::utf8(target.title)},{"exe",target.exe},{"pid",target.pid}});}
    for(const auto& mic:app.microphones)microphones.push_back({{"id",fg::utf8(mic.id)},{"name",fg::utf8(mic.name)},{"rate",mic.rate},{"channels",mic.channels}});
    const size_t first=app.stats.frames.size()>180?app.stats.frames.size()-180:0;for(size_t i=first;i<app.stats.frames.size();++i)graph.push_back(app.stats.frames[i].ms);
    enginePipe.publish({{"profileExe",app.activeProfile},{"profilePid",app.selectedPid},{"protocol",3},{"connected",true},{"games",windows},{"microphones",microphones},{"capture",app.capture.running()},{"live",app.stats.live},{"overlay",app.overlay},{"fps",app.stats.fps},{"average",app.stats.average},{"low",app.stats.low},{"frameTime",app.stats.ms},{"graph",graph},{"cpu",app.cpu},{"ram",app.ramGB},{"totalRam",app.totalGB},{"game",app.stats.app},{"rendererHint",fg::utf8(app.compatibility.runtime)},{"compatibilityNote",fg::utf8(app.compatibility.note)},{"inGameSupported",!app.compatibility.inspected||app.compatibility.dxgi||(!app.compatibility.vulkan&&!app.compatibility.opengl&&!app.compatibility.dx9)},{"renderer",app.hook.active()?(app.hook.renderer()==12?"DX12 · in-game":"DX11 · in-game"):"PresentMon · desktop"},{"hookActive",app.hook.active()},{"notice",fg::utf8(app.notice)},{"audioStatus",fg::utf8(app.audioNotice)},{"noiseStatus",fg::utf8(app.noiseUpdateStatus)},{"noiseBusy",app.noiseUpdater.running()},{"pendingMic",!app.pendingMicrophoneApply.empty()},{"volume",app.outputPercent},{"outputName",fg::utf8(app.audio.device)},{"micConfigured",app.cachedNoise||app.cachedEqualizer},{"bassConfigured",app.cachedBass.has_value()},{"boostConfigured",app.cachedGain.value_or(0)>.001},{"apoAvailable",app.boost.available()}});
}
