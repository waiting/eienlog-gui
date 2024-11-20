#include "App.h"
#include "WindowModal.h"
#include "NewEienLogWindowModal.h"
#include "EienLogWindows.h"

// struct NewEienLogWindowModal ---------------------------------------------------------------
NewEienLogWindowModal::NewEienLogWindowModal( EienLogWindows * manager, std::string const & name ) : WindowModal(name), _manager(manager)
{
}

static std::string __addr = u8"";
static int __port = 22345;
static char __ch[] = { 'A', '\0' };
static std::string __name = std::string(u8"日志") + __ch;
static std::string __strWaitTimeout = "50";
static std::string __strUpdateTimeout = "300";
static bool __vScrollToBottom = true;

void NewEienLogWindowModal::renderComponents()
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

void NewEienLogWindowModal::onOk()
{
    this->_manager->addWindow( __name, __addr, (USHORT)__port, winux::Mixed(__strWaitTimeout), winux::Mixed(__strUpdateTimeout), __vScrollToBottom );

    __ch[0]++;
    __name = std::string(u8"日志") + __ch;
    __port++;
}

void NewEienLogWindowModal::onCancel()
{

}
