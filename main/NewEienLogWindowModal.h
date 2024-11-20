#pragma once
#include "WindowModal.h"

struct EienLogWindows;
struct NewEienLogWindowModal : public WindowModal
{
    NewEienLogWindowModal( EienLogWindows * manager, std::string const & name );

    void renderComponents() override;
    void onOk() override;
    void onCancel() override;

    EienLogWindows * _manager;
};

