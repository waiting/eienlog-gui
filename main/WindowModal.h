#pragma once
struct WindowModal
{
    WindowModal( std::string const & name );

    void toggle();

    void render();

    virtual void renderComponents();

    DEFINE_CUSTOM_EVENT( Ok, (), () )
    DEFINE_CUSTOM_EVENT( Cancel, (), () )

    std::string _name;
    bool _toggled;
};
