#pragma once
#include "WindowModal.h"

struct LogWindowsManager;
struct NewLogListenWindowModal : public WindowModal
{
    NewLogListenWindowModal( LogWindowsManager * manager, winux::Utf8String const & name );

    void renderComponents() override;
    void onOk() override;
    void onCancel() override;

    LogWindowsManager * _manager;
};
