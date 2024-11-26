#pragma once
#include "WindowModal.h"

struct EienLogWindowsManager;
struct NewListenWindowModal : public WindowModal
{
    NewListenWindowModal( EienLogWindowsManager * manager, winux::Utf8String const & name );

    void renderComponents() override;
    void onOk() override;
    void onCancel() override;

    EienLogWindowsManager * _manager;
};

