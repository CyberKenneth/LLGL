/*
 * Canvas.cpp
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015-2018 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include <LLGL/Canvas.h>
#include "../Core/Helper.h"


namespace LLGL
{


/* ----- Canvas EventListener class ----- */

void Canvas::EventListener::OnProcessEvents(Canvas& sender)
{
    // dummy
}


/* ----- Window class ----- */

#define FOREACH_LISTENER_CALL(FUNC) \
    for (const auto& lst : eventListeners_) { lst->FUNC; }

#ifndef LLGL_MOBILE_PLATFORM

std::unique_ptr<Canvas> Canvas::Create(const CanvasDescriptor& desc)
{
    /* For non-mobile platforms this function always returns null */
    return nullptr;
}

#endif

bool Canvas::AdaptForVideoMode(VideoModeDescriptor& videoModeDesc)
{
    /* Default implementation of this function always return false for the Canvas class */
    return false;
}

void Canvas::ProcessEvents()
{
    FOREACH_LISTENER_CALL( OnProcessEvents(*this) );
    OnProcessEvents();
}

/* --- Event handling --- */

void Canvas::AddEventListener(const std::shared_ptr<EventListener>& eventListener)
{
    AddOnceToSharedList(eventListeners_, eventListener);
}

void Canvas::RemoveEventListener(const EventListener* eventListener)
{
    RemoveFromSharedList(eventListeners_, eventListener);
}

#undef FOREACH_LISTENER_CALL


} // /namespace LLGL



// ================================================================================
