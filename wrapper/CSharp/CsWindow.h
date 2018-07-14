/*
 * CsWindow.h
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015-2018 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#pragma once

#include <vcclr.h>
#include <LLGL/Window.h>
#include "CsHelper.h"
#include "CsTypes.h"

#using <System.dll>
#using <System.Core.dll>
#using <System.Runtime.InteropServices.dll>


using namespace System;
using namespace System::Runtime::InteropServices;


namespace LHermanns
{

namespace LLGL
{


public ref class Window
{

    public:

        /* ----- Common ----- */

        Window(::LLGL::Window* instance);

        #if 0
        static Window^ Create(WindowDescriptor^ desc);
        #endif

        property Offset2D^ Position
        {
            Offset2D^ get();
            void set(Offset2D^ position);
        };

        property Extent2D^ Size
        {
            Extent2D^ get();
            void set(Extent2D^ size);
        }

        property Extent2D^ ClientAreaSize
        {
            Extent2D^ get();
            void set(Extent2D^ size);
        }

        property String^ Title
        {
            String^ get();
            void set(String^ title);
        }

        property bool Shown
        {
            bool get();
            void set(bool shown);
        }

        #if 0
        property WindowDescriptor^ Desc;

        property WindowBehavior^ Behavior;
        #endif

        property bool HasFocus
        {
            bool get();
        }

        #if 0
        bool AdaptForVideoMode(VideoModeDescriptor^ videoModeDesc);
        #endif

        bool ProcessEvents();

        #if 0
        /* --- Event handling --- */

        void AddEventListener(EventListener^ eventListener);
        void RemoveEventListener(EventListener^ eventListener);

        void PostKeyDown(Key keyCode);
        void PostKeyUp(Key keyCode);
        void PostDoubleClick(Key keyCode);
        void PostChar(wchar_t chr);
        void PostWheelMotion(int motion);
        void PostLocalMotion(Offset2D^ position);
        void PostGlobalMotion(Offset2D^ motion);
        void PostResize(Extent2D^ clientAreaSize);
        void PostGetFocus();
        void PostLoseFocus();
        void PostQuit();
        void PostTimer(std::uint32_t timerID);
        #endif

    private:

        ::LLGL::Window* instance_ = nullptr;

};


} // /namespace LLGL

} // /namespace LHermanns



// ================================================================================
