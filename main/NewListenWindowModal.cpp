#include "App.h"
#include "MainWindow.h"
#include "WindowModal.h"
#include "NewListenWindowModal.h"
#include "EienLogWindows.h"

// struct NewListenWindowModal ---------------------------------------------------------------
NewListenWindowModal::NewListenWindowModal( EienLogWindowsManager * manager, winux::Utf8String const & name ) : WindowModal(name), _manager(manager)
{
}

static winux::Utf8String __addr = u8"";
static int __port = 22345;
static char __ch[] = { 'A', '\0' };
static winux::Utf8String __name = winux::Utf8String(u8"日志") + __ch;
static winux::Utf8String __strWaitTimeout = u8"50";
static winux::Utf8String __strUpdateTimeout = u8"300";
static bool __vScrollToBottom = true;

void NewListenWindowModal::renderComponents()
{
    ImGui::Text( u8"新建一个日志窗口，监听日志信息" );
    ImGui::Separator();

    // 对齐标签文本
    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"地址"); // 这将是左侧的标签
    ImGui::SameLine( 0.0f, 1.0f );
    // 设置输入框的宽度
    ImGui::PushItemWidth(160);
    ImGui::InputText( u8"##addr", &__addr );
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"端口");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::PushItemWidth(120);
    ImGui::InputInt( u8"##port", &__port, 1 );
    ImGui::PopItemWidth();

    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"名称");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::InputText( u8"##name", &__name );

    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"等待超时");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::PushItemWidth(80);
    ImGui::InputText( u8"##wait_timeout", &__strWaitTimeout );
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"更新超时");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::PushItemWidth(80);
    ImGui::InputText( u8"##update_timeout", &__strUpdateTimeout );
    ImGui::PopItemWidth();

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
    ImGui::Checkbox( u8"自动滚动到底部", &__vScrollToBottom );
    ImGui::PopStyleVar();
}

void NewListenWindowModal::onOk()
{
    App::ListenParams lparams;
    lparams.name = __name;
    lparams.addr = __addr;
    lparams.port = (winux::ushort)__port;
    lparams.waitTimeout = winux::Mixed(__strWaitTimeout);
    lparams.updateTimeout = winux::Mixed(__strUpdateTimeout);
    lparams.vScrollToBottom = __vScrollToBottom;

    this->_manager->addWindow(lparams);

    __ch[0]++;
    __name = winux::Utf8String(u8"日志") + __ch;
    __port++;
}

void NewListenWindowModal::onCancel()
{

}
