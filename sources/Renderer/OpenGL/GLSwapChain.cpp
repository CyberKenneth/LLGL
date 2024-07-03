/*
 * GLSwapChain.cpp
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include "GLSwapChain.h"
#include "../TextureUtils.h"
#include "Platform/GLContextManager.h"
#include <LLGL/TypeInfo.h>
#include <LLGL/Platform/Platform.h>

#ifdef LLGL_MOBILE_PLATFORM
#   include <LLGL/Canvas.h>
#else
#   include <LLGL/Window.h>
#endif

#ifdef LLGL_OS_LINUX
#   include <LLGL/Platform/NativeHandle.h>
#endif


namespace LLGL
{


GLSwapChain::GLSwapChain(
    const SwapChainDescriptor&      desc,
    const std::shared_ptr<Surface>& surface,
    GLContextManager&               contextMngr)
:
    SwapChain { desc }
{
    /* Set up pixel format for GL context */
    GLPixelFormat pixelFormat;
    pixelFormat.colorBits   = desc.colorBits;
    pixelFormat.depthBits   = desc.depthBits;
    pixelFormat.stencilBits = desc.stencilBits;
    pixelFormat.samples     = static_cast<int>(GetClampedSamples(desc.samples));

    #ifdef LLGL_OS_LINUX

    /* Set up surface for the swap-chain and pass native context handle */
    NativeHandle windowContext = {};
    ChooseGLXVisualAndGetX11WindowContext(pixelFormat, windowContext);
    SetOrCreateSurface(surface, UTF8String{}, desc.resolution, desc.fullscreen, &windowContext, sizeof(windowContext));

    #else

    /* Setup surface for the swap-chain */
    SetOrCreateSurface(surface, UTF8String{}, desc.resolution, desc.fullscreen);

    #endif

    /*
    Cache resolution height after surface has been created,
    since high-resolution displays might provide a multiple of the input size.
    */
    framebufferHeight_ = static_cast<GLint>(GetResolution().height);

    /* Create platform dependent OpenGL context */
    context_ = contextMngr.AllocContext(&pixelFormat, &GetSurface());
    swapChainContext_ = GLSwapChainContext::Create(*context_, GetSurface());
    GLSwapChainContext::MakeCurrent(swapChainContext_.get());

    /* Get state manager and reset current framebuffer height */
    GetStateManager().ResetFramebufferHeight(framebufferHeight_);

    /* Show default surface */
    if (!surface)
        ShowSurface();
}

void GLSwapChain::Present()
{
    swapChainContext_->SwapBuffers();
}

std::uint32_t GLSwapChain::GetCurrentSwapIndex() const
{
    return 0; // dummy
}

std::uint32_t GLSwapChain::GetNumSwapBuffers() const
{
    return 1; // dummy
}

std::uint32_t GLSwapChain::GetSamples() const
{
    return static_cast<std::uint32_t>(context_->GetSamples());
}

Format GLSwapChain::GetColorFormat() const
{
    return context_->GetColorFormat();
}

Format GLSwapChain::GetDepthStencilFormat() const
{
    return context_->GetDepthStencilFormat();
}

const RenderPass* GLSwapChain::GetRenderPass() const
{
    return nullptr; // dummy
}

bool GLSwapChain::SetVsyncInterval(std::uint32_t vsyncInterval)
{
    return SetSwapInterval(static_cast<int>(vsyncInterval));
}

bool GLSwapChain::MakeCurrent(GLSwapChain* swapChain)
{
    if (swapChain)
    {
        /* Make OpenGL context of the specified render contex current and notify the state manager */
        bool result = GLSwapChainContext::MakeCurrent(swapChain->swapChainContext_.get());
        GLStateManager::Get().ResetFramebufferHeight(swapChain->framebufferHeight_);
        return result;
    }
    else
        return GLSwapChainContext::MakeCurrent(nullptr);
}

void GLSwapChain::BuildAndSetDefaultSurfaceTitle(const RendererInfo& info)
{
    #ifdef LLGL_MOBILE_PLATFORM

    /* Set Canvas title for mobile platforms */
    CastTo<Canvas>(GetSurface()).SetTitle(SwapChain::BuildDefaultSurfaceTitle(info));

    #else // LLGL_MOBILE_PLATFORM

    /* Set Window title for desktop platforms */
    CastTo<Window>(GetSurface()).SetTitle(SwapChain::BuildDefaultSurfaceTitle(info));

    #endif // /LLGL_MOBILE_PLATFORM
}


/*
 * ======= Private: =======
 */

bool GLSwapChain::ResizeBuffersPrimary(const Extent2D& resolution)
{
    /* Notify GL context of a resize */
    swapChainContext_->Resize(resolution);

    /* Update context height */
    const GLint height = static_cast<GLint>(resolution.height);
    GetStateManager().ResetFramebufferHeight(height);
    framebufferHeight_ = height;

    return true;
}

bool GLSwapChain::SetSwapInterval(int swapInterval)
{
    GLSwapChainContext::MakeCurrent(swapChainContext_.get());
    return GLContext::SetCurrentSwapInterval(swapInterval);
}


} // /namespace LLGL



// ================================================================================
