#include "App.h"
#include "WindowModal.h"

WindowModal::WindowModal( std::string const & name ) : _name(name), _toggled(false)
{
}

void WindowModal::toggle()
{
    _toggled = true;
}

void WindowModal::render()
{
    if ( _toggled )
    {
        _toggled = false;
        ImGui::OpenPopup( _name.c_str() );
    }

    if ( ImGui::BeginPopupModal( _name.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings ) )
    {
        this->renderComponents();

        ImGui::Separator();
        if ( ImGui::Button( u8"确定", ImVec2( 120, 0 ) ) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) )
        {
            ImGui::CloseCurrentPopup();
            this->onOk();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if ( ImGui::Button( u8"取消", ImVec2( 120, 0 ) ) )
        {
            ImGui::CloseCurrentPopup();
            this->onCancel();
        }

        ImGui::EndPopup();
    }
}

void WindowModal::renderComponents()
{
}
