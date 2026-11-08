#include <QGuiApplication>
#include <QVulkanInstance>
#include <QVulkanWindow>
#include <QVulkanFunctions>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>

class SceneRenderer final : public QVulkanWindowRenderer {
    QVulkanWindow* window;
    QVulkanDeviceFunctions* functions=nullptr;
    QElapsedTimer clock;
    quint64* frames;
public:
    SceneRenderer(QVulkanWindow* target,quint64* counter):window(target),frames(counter){}
    void initResources() override {functions=window->vulkanInstance()->deviceFunctions(window->device());clock.start();}
    void startNextFrame() override {
        const float time=clock.elapsed()/1000.f;
        VkClearValue values[2]{};
        values[0].color={{.035f,.1f+.04f*std::sin(time),.13f,1.f}};
        values[1].depthStencil={1.f,0};
        VkRenderPassBeginInfo pass{};pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass=window->defaultRenderPass();pass.framebuffer=window->currentFramebuffer();
        const auto size=window->swapChainImageSize();pass.renderArea.extent={uint32_t(size.width()),uint32_t(size.height())};
        pass.clearValueCount=2;pass.pClearValues=values;
        functions->vkCmdBeginRenderPass(window->currentCommandBuffer(),&pass,VK_SUBPASS_CONTENTS_INLINE);
        functions->vkCmdEndRenderPass(window->currentCommandBuffer());
        ++*frames;window->frameReady();window->requestUpdate();
    }
};
class SceneWindow final : public QVulkanWindow {
public:
    quint64 frames=0;
    QVulkanWindowRenderer* createRenderer() override {return new SceneRenderer(this,&frames);}
};
int main(int argc,char** argv){
    QGuiApplication application(argc,argv);application.setApplicationName("FramoryxVulkanScene");
    QVulkanInstance instance;instance.setApiVersion(QVersionNumber(1,0,0));
    if(!instance.create()){qCritical("A Vulkan loader and usable driver are required.");return 2;}
    SceneWindow window;window.setVulkanInstance(&instance);window.resize(960,600);window.setTitle("Framoryx Vulkan test scene");window.show();
    const auto arguments=application.arguments();const int secondsIndex=arguments.indexOf("--seconds"),reportIndex=arguments.indexOf("--report");
    bool valid=false;const int seconds=arguments.value(secondsIndex+1).toInt(&valid);
    if(secondsIndex>=0&&valid&&seconds>0&&seconds<=600)QTimer::singleShot(seconds*1000,&application,&QGuiApplication::quit);
    QElapsedTimer elapsed;elapsed.start();const int status=application.exec();
    if(reportIndex>=0){QSaveFile file(arguments.value(reportIndex+1));const QJsonObject result{{"api","Vulkan"},{"frames",qint64(window.frames)},{"elapsedMs",elapsed.elapsed()},{"valid",window.isValid()}};if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(result).toJson())<0||!file.commit())return 3;}
    return status?status:window.frames>0?0:1;
}
