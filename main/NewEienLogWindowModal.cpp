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
static std::string __name = std::string(u8"��־") + __ch;
static std::string __strWaitTimeout = "500";
static std::string __strUpdateTimeout = "3000";
static bool __vScrollToBottom = true;

void NewEienLogWindowModal::renderComponents()
{
    ImGui::Text( u8"�½�һ����־���ڣ�������־��Ϣ" );
    ImGui::Separator();

    // �����ǩ�ı�
    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"��ַ"); // �⽫�����ı�ǩ
    ImGui::SameLine( 0.0f, 1.0f );
    // ���������Ŀ��
    ImGui::PushItemWidth(160);
    ImGui::InputText( u8"##addr", &__addr );
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"�˿�");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::PushItemWidth(120);
    ImGui::InputInt( u8"##port", &__port, 1 );
    ImGui::PopItemWidth();

    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"����");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::InputText( u8"##name", &__name );

    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"�ȴ���ʱ");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::PushItemWidth(80);
    ImGui::InputText( u8"##wait_timeout", &__strWaitTimeout );
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text(u8"���³�ʱ");
    ImGui::SameLine( 0.0f, 1.0f );
    ImGui::PushItemWidth(80);
    ImGui::InputText( u8"##update_timeout", &__strUpdateTimeout );
    ImGui::PopItemWidth();

    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0, 0 ) );
    ImGui::Checkbox( u8"�Զ��������ײ�", &__vScrollToBottom );
    ImGui::PopStyleVar();
}

void NewEienLogWindowModal::onOk()
{
    this->_manager->addWindow( __name, __addr, (USHORT)__port, winux::Mixed(__strWaitTimeout), winux::Mixed(__strUpdateTimeout), __vScrollToBottom );

    __ch[0]++;
    __name = std::string(u8"��־") + __ch;
    __port++;
}

void NewEienLogWindowModal::onCancel()
{

}
