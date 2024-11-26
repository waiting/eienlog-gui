#pragma once
struct WindowModal
{
    WindowModal( winux::Utf8String const & name );

    void toggle();

    void render();

    virtual void renderComponents();

    DEFINE_CUSTOM_EVENT( Ok, (), () )
    DEFINE_CUSTOM_EVENT( Cancel, (), () )

    winux::Utf8String _name;
    bool _toggled;
};
