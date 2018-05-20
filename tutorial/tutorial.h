/*
 * tutorial.h
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef LLGL_TUTORIAL_H
#define LLGL_TUTORIAL_H


#include <LLGL/LLGL.h>
#include <LLGL/Utility.h>
#include <Gauss/Gauss.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <type_traits>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


/* ----- Global helper functions ----- */

static std::string GetSelectedRendererModule(int argc, char* argv[])
{
    /* Select renderer module */
    std::string rendererModule;

    if (argc > 1)
    {
        /* Get renderer module name from command line argument */
        rendererModule = argv[1];
    }
    else
    {
        /* Find available modules */
        auto modules = LLGL::RenderSystem::FindModules();

        if (modules.empty())
        {
            /* No modules available -> throw error */
            throw std::runtime_error("no renderer modules available on target platform");
        }
        else if (modules.size() == 1)
        {
            /* Use the only available module */
            rendererModule = modules.front();
        }
        else
        {
            /* Let user select a renderer */
            while (rendererModule.empty())
            {
                /* Print list of available modules */
                std::cout << "select renderer:" << std::endl;

                int i = 0;
                for (const auto& mod : modules)
                    std::cout << " " << (++i) << ".) " << mod << std::endl;

                /* Wait for user input */
                std::size_t selection = 0;
                std::cin >> selection;
                --selection;

                if (selection < modules.size())
                    rendererModule = modules[selection];
                else
                    std::cerr << "invalid input" << std::endl;
            }
        }
    }

    /* Choose final renderer module */
    std::cout << "selected renderer: " << rendererModule << std::endl;

    return rendererModule;
}

static std::string ReadFileContent(const std::string& filename)
{
    // Read file content into string
    std::ifstream file(filename);

    if (!file.good())
        throw std::runtime_error("failed to open file: \"" + filename + "\"");

    return std::string(
        ( std::istreambuf_iterator<char>(file) ),
        ( std::istreambuf_iterator<char>() )
    );
}

static std::vector<char> ReadFileBuffer(const std::string& filename)
{
    // Read file content into buffer
    std::ifstream file(filename, std::ios_base::binary | std::ios_base::ate);

    if (!file.good())
        throw std::runtime_error("failed to open file: \"" + filename + "\"");

    auto fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}


/* ----- Tutorial class ----- */

class Tutorial
{

public:

    static void SelectRendererModule(int argc, char* argv[])
    {
        rendererModule_ = GetSelectedRendererModule(argc, argv);
    }

    virtual ~Tutorial()
    {
    }

    void Run()
    {
        auto& window = static_cast<LLGL::Window&>(context->GetSurface());
        while (window.ProcessEvents() && !input->KeyDown(LLGL::Key::Escape))
        {
            profilerObj_->ResetCounters();
            OnDrawFrame();
        }
    }

protected:

    struct TutorialShaderDescriptor
    {
        TutorialShaderDescriptor(
            LLGL::ShaderType type, const std::string& filename) :
                type     { type     },
                filename { filename }
        {
        }

        TutorialShaderDescriptor(
            LLGL::ShaderType type, const std::string& filename, const std::string& entryPoint, const std::string& target) :
                type       { type       },
                filename   { filename   },
                entryPoint { entryPoint },
                target     { target     }
        {
        }

        LLGL::ShaderType    type;
        std::string         filename;
        std::string         entryPoint;
        std::string         target;
    };

public:

private:

    class ResizeEventHandler : public LLGL::Window::EventListener
    {

        public:

            ResizeEventHandler(
                Tutorial& tutorial,
                LLGL::RenderContext* context,
                LLGL::CommandBuffer* commands,
                Gs::Matrix4f& projection) :
                    tutorial_   { tutorial   },
                    context_    { context    },
                    commands_   { commands   },
                    projection_ { projection }
            {
            }

            void OnResize(LLGL::Window& sender, const LLGL::Extent2D& clientAreaSize) override
            {
                if (clientAreaSize.width >= 4 && clientAreaSize.height >= 4)
                {
                    // Update video mode
                    auto videoMode = context_->GetVideoMode();
                    {
                        videoMode.resolution = clientAreaSize;
                    }
                    context_->SetVideoMode(videoMode);

                    // Update projection matrix
                    auto aspectRatio = static_cast<float>(videoMode.resolution.width) / static_cast<float>(videoMode.resolution.height);
                    projection_ = tutorial_.PerspectiveProjection(aspectRatio, 0.1f, 100.0f, Gs::Deg2Rad(45.0f));

                    // Re-draw frame
                    if (tutorial_.IsLoadingDone())
                        tutorial_.OnDrawFrame();
                }
            }

            void OnTimer(LLGL::Window& sender, unsigned int timerID) override
            {
                // Re-draw frame
                if (tutorial_.IsLoadingDone())
                    tutorial_.OnDrawFrame();
            }

        private:

            Tutorial&               tutorial_;
            LLGL::RenderContext*    context_;
            LLGL::CommandBuffer*    commands_;
            Gs::Matrix4f&           projection_;

    };

    struct ShaderProgramRecall
    {
        std::vector<TutorialShaderDescriptor>   shaderDescs;
        std::vector<LLGL::Shader*>              shaders;
        std::vector<LLGL::VertexFormat>         vertexFormats;
        LLGL::StreamOutputFormat                streamOutputFormat;
    };

    std::unique_ptr<LLGL::RenderingProfiler>    profilerObj_;
    std::unique_ptr<LLGL::RenderingDebugger>    debuggerObj_;

    std::map< LLGL::ShaderProgram*,
              ShaderProgramRecall >             shaderPrograms_;

    bool                                        loadingDone_    = false;

    static std::string                          rendererModule_;

public:

    struct VertexPositionNormal
    {
        Gs::Vector3f position;
        Gs::Vector3f normal;
    };

    struct VertexPositionTexCoord
    {
        Gs::Vector3f position;
        Gs::Vector2f texCoord;
    };

protected:

    friend class ResizeEventHandler;

    const LLGL::ColorRGBAf                      defaultClearColor { 0.1f, 0.1f, 0.4f };

    // Render system
    std::unique_ptr<LLGL::RenderSystem>         renderer;

    // Main render context
    LLGL::RenderContext*                        context         = nullptr;

    // Main command buffer
    union
    {
        LLGL::CommandBuffer*                    commands        = nullptr;
        LLGL::CommandBufferExt*                 commandsExt;
    };

    // Command queue
    LLGL::CommandQueue*                         commandQueue    = nullptr;

    std::shared_ptr<LLGL::Input>                input;

    std::unique_ptr<LLGL::Timer>                timer;
    const LLGL::RenderingProfiler&              profiler;

    Gs::Matrix4f                                projection;

    virtual void OnDrawFrame() = 0;

    Tutorial(
        const std::wstring&     title,
        const LLGL::Extent2D&   resolution      = { 800, 600 },
        std::uint32_t           multiSampling   = 8,
        bool                    vsync           = true,
        bool                    debugger        = true) :
            profilerObj_ { new LLGL::RenderingProfiler() },
            debuggerObj_ { new LLGL::RenderingDebugger() },
            timer        { LLGL::Timer::Create()         },
            profiler     { *profilerObj_                 }
    {
        // Create render system
        renderer = LLGL::RenderSystem::Load(
            rendererModule_,
            (debugger ? profilerObj_.get() : nullptr),
            (debugger ? debuggerObj_.get() : nullptr)
        );

        // Create render context
        LLGL::RenderContextDescriptor contextDesc;
        {
            contextDesc.videoMode.resolution            = resolution;
            contextDesc.vsync.enabled                   = vsync;
            contextDesc.multiSampling.enabled           = (multiSampling > 1);
            contextDesc.multiSampling.samples           = multiSampling;

            #if defined __APPLE__
            contextDesc.profileOpenGL.contextProfile    = LLGL::OpenGLContextProfile::CoreProfile;
            contextDesc.profileOpenGL.majorVersion      = 4;
            contextDesc.profileOpenGL.minorVersion      = 1;
            #elif defined __linux__
            contextDesc.multiSampling.enabled           = false;
            contextDesc.multiSampling.samples           = 1;
            contextDesc.profileOpenGL.contextProfile    = LLGL::OpenGLContextProfile::CoreProfile;
            contextDesc.profileOpenGL.majorVersion      = 3;
            contextDesc.profileOpenGL.minorVersion      = 3;
            #endif
        }
        context = renderer->CreateRenderContext(contextDesc);

        // Create command buffer
        commandsExt = renderer->CreateCommandBufferExt();
        if (!commands)
            commands = renderer->CreateCommandBuffer();

        // Get command queue
        commandQueue = renderer->GetCommandQueue();

        // Initialize command buffer
        commands->SetClearColor(defaultClearColor);
        commands->SetRenderTarget(*context);
        commands->SetViewport({ { 0, 0 }, resolution });
        commands->SetScissor({ { 0, 0 }, resolution });

        // Print renderer information
        const auto& info = renderer->GetRendererInfo();

        std::cout << "renderer information:" << std::endl;
        std::cout << "  renderer:         " << info.rendererName << std::endl;
        std::cout << "  device:           " << info.deviceName << std::endl;
        std::cout << "  vendor:           " << info.vendorName << std::endl;
        std::cout << "  shading language: " << info.shadingLanguageName << std::endl;

        // Set window title
        auto& window = static_cast<LLGL::Window&>(context->GetSurface());

        auto rendererName = renderer->GetName();
        window.SetTitle(title + L" ( " + std::wstring(rendererName.begin(), rendererName.end()) + L" )");

        // Add input event listener to window
        input = std::make_shared<LLGL::Input>();
        window.AddEventListener(input);

        // Change window descriptor to allow resizing
        auto wndDesc = window.GetDesc();
        wndDesc.resizable = true;
        window.SetDesc(wndDesc);

        // Change window behavior
        auto behavior = window.GetBehavior();
        behavior.disableClearOnResize = true;
        behavior.moveAndResizeTimerID = 1;
        window.SetBehavior(behavior);

        // Add window resize listener
        window.AddEventListener(std::make_shared<ResizeEventHandler>(*this, context, commands, projection));

        // Initialize default projection matrix
        projection = PerspectiveProjection(GetAspectRatio(), 0.1f, 100.0f, Gs::Deg2Rad(45.0f));

        // Show window
        window.Show();

        // Store information that loading is done
        loadingDone_ = true;
    }

    LLGL::ShaderProgram* LoadShaderProgram(
        const std::vector<TutorialShaderDescriptor>& shaderDescs,
        const std::vector<LLGL::VertexFormat>& vertexFormats = {},
        const LLGL::StreamOutputFormat& streamOutputFormat = {})
    {
        // Create shader program
        LLGL::ShaderProgram* shaderProgram = renderer->CreateShaderProgram();

        ShaderProgramRecall recall;

        recall.shaderDescs = shaderDescs;

        for (const auto& desc : shaderDescs)
        {
            // Create shader
            auto shader = renderer->CreateShader(desc.type);

            LLGL::ShaderDescriptor shaderDesc { desc.entryPoint, desc.target, LLGL::ShaderCompileFlags::Debug };
            shaderDesc.streamOutput.format = streamOutputFormat;

            // Read shader file
            if (desc.filename.size() > 4 && desc.filename.substr(desc.filename.size() - 4) == ".spv")
            {
                // Load binary
                auto byteCode = ReadFileBuffer(desc.filename);
                shader->LoadBinary(std::move(byteCode), shaderDesc);
            }
            else
            {
                // Compile shader
                auto shaderCode = ReadFileContent(desc.filename);
                shader->Compile(shaderCode, shaderDesc);
            }

            // Print info log (warnings and errors)
            std::string log = shader->QueryInfoLog();
            if (!log.empty())
                std::cerr << log << std::endl;

            // Attach vertex- and fragment shader to the shader program
            shaderProgram->AttachShader(*shader);

            // Store shader in recall
            recall.shaders.push_back(shader);
        }

        // Bind vertex attribute layout (this is not required for a compute shader program)
        if (!vertexFormats.empty())
            shaderProgram->BuildInputLayout(static_cast<std::uint32_t>(vertexFormats.size()), vertexFormats.data());

        // Link shader program and check for errors
        if (!shaderProgram->LinkShaders())
            throw std::runtime_error(shaderProgram->QueryInfoLog());

        // Store information in call
        recall.vertexFormats = vertexFormats;
        recall.streamOutputFormat = streamOutputFormat;
        shaderPrograms_[shaderProgram] = recall;

        return shaderProgram;
    }

    // Reloads the specified shader program from the previously specified shader source files.
    bool ReloadShaderProgram(LLGL::ShaderProgram* shaderProgram)
    {
        std::cout << "reload shader program" << std::endl;

        // Find shader program in the recall map
        auto it = shaderPrograms_.find(shaderProgram);
        if (it == shaderPrograms_.end())
            return false;

        auto& recall = it->second;
        std::vector<LLGL::Shader*> shaders;

        // Detach previous shaders
        shaderProgram->DetachAll();

        try
        {
            // Recompile all shaders
            for (const auto& desc : recall.shaderDescs)
            {
                // Read shader file
                auto shaderCode = ReadFileContent(desc.filename);

                // Create shader
                auto shader = renderer->CreateShader(desc.type);

                // Compile shader
                LLGL::ShaderDescriptor shaderDesc(desc.entryPoint, desc.target, LLGL::ShaderCompileFlags::Debug);
                shaderDesc.streamOutput.format = recall.streamOutputFormat;

                shader->Compile(shaderCode, shaderDesc);

                // Print info log (warnings and errors)
                std::string log = shader->QueryInfoLog();
                if (!log.empty())
                    std::cerr << log << std::endl;

                // Attach vertex- and fragment shader to the shader program
                shaderProgram->AttachShader(*shader);

                // Store new shader
                shaders.push_back(shader);
            }

            // Bind vertex attribute layout (this is not required for a compute shader program)
            if (!recall.vertexFormats.empty())
                shaderProgram->BuildInputLayout(static_cast<std::uint32_t>(recall.vertexFormats.size()), recall.vertexFormats.data());

            // Link shader program and check for errors
            if (!shaderProgram->LinkShaders())
                throw std::runtime_error(shaderProgram->QueryInfoLog());
        }
        catch (const std::exception& err)
        {
            // Print error message
            std::cerr << err.what() << std::endl;

            // Attach all previous shaders again
            for (auto shader : recall.shaders)
                shaderProgram->AttachShader(*shader);

            // Bind vertex attribute layout (this is not required for a compute shader program)
            if (!recall.vertexFormats.empty())
                shaderProgram->BuildInputLayout(static_cast<std::uint32_t>(recall.vertexFormats.size()), recall.vertexFormats.data());

            // Link shader program and check for errors
            if (!shaderProgram->LinkShaders())
                throw std::runtime_error(shaderProgram->QueryInfoLog());

            return false;
        }

        // Delete all previous shaders
        for (auto shader : recall.shaders)
            renderer->Release(*shader);

        // Store new shaders in recall
        recall.shaders = std::move(shaders);

        return true;
    }

    // Load standard shader program (with vertex- and fragment shaders)
    LLGL::ShaderProgram* LoadStandardShaderProgram(const std::vector<LLGL::VertexFormat>& vertexFormats)
    {
        // Load shader program
        const auto& languages = renderer->GetRenderingCaps().shadingLanguages;

        if (std::find(languages.begin(), languages.end(), LLGL::ShadingLanguage::GLSL) != languages.end())
        {
            return LoadShaderProgram(
                {
                    { LLGL::ShaderType::Vertex, "vertex.glsl" },
                    { LLGL::ShaderType::Fragment, "fragment.glsl" }
                },
                vertexFormats
            );
        }
        if (std::find(languages.begin(), languages.end(), LLGL::ShadingLanguage::SPIRV) != languages.end())
        {
            return LoadShaderProgram(
                {
                    { LLGL::ShaderType::Vertex, "vertex.450core.spv" },
                    { LLGL::ShaderType::Fragment, "fragment.450core.spv" }
                },
                vertexFormats
            );
        }
        if (std::find(languages.begin(), languages.end(), LLGL::ShadingLanguage::HLSL) != languages.end())
        {
            return LoadShaderProgram(
                {
                    { LLGL::ShaderType::Vertex, "shader.hlsl", "VS", "vs_5_0" },
                    { LLGL::ShaderType::Fragment, "shader.hlsl", "PS", "ps_5_0" }
                },
                vertexFormats
            );
        }

        return nullptr;
    }

public:

    // Load image from file, create texture, upload image into texture, and generate MIP-maps.
    static LLGL::Texture* LoadTextureWithRenderer(LLGL::RenderSystem& renderSys, const std::string& filename)
    {
        // Load image data from file (using STBI library, see https://github.com/nothings/stb)
        int width = 0, height = 0, components = 0;

        auto imageBuffer = stbi_load(filename.c_str(), &width, &height, &components, 4);
        if (!imageBuffer)
            throw std::runtime_error("failed to load texture from file: \"" + filename + "\"");

        // Initialize image descriptor to upload image data onto hardware texture
        LLGL::ImageDescriptor imageDesc;
        {
            // Set image color format
            imageDesc.format    = LLGL::ImageFormat::RGBA;

            // Set image data type (unsigned char = 8-bit unsigned integer)
            imageDesc.dataType  = LLGL::DataType::UInt8;

            // Set image buffer source for texture initial data
            imageDesc.data      = imageBuffer;

            // Set image buffer size
            imageDesc.dataSize  = static_cast<std::size_t>(width*height*4);
        }

        // Create texture and upload image data onto hardware texture
        auto tex = renderSys.CreateTexture(
            LLGL::Texture2DDesc(LLGL::TextureFormat::RGBA8, width, height), &imageDesc
        );

        // Generate all MIP-maps (MIP = "Multum in Parvo", or "a multitude in a small space")
        // see https://developer.valvesoftware.com/wiki/MIP_Mapping
        // see http://whatis.techtarget.com/definition/MIP-map
        renderSys.GenerateMips(*tex);

        // Release image data
        stbi_image_free(imageBuffer);

        // Show info
        std::cout << "loaded texture: " << filename << std::endl;

        return tex;
    }

protected:

    // Load image from file, create texture, upload image into texture, and generate MIP-maps.
    LLGL::Texture* LoadTexture(const std::string& filename)
    {
        return LoadTextureWithRenderer(*renderer, filename);
    }

public:

    // Save texture image to a PNG file.
    static bool SaveTextureWithRenderer(LLGL::RenderSystem& renderSys, LLGL::Texture& texture, const std::string& filename, unsigned int mipLevel = 0)
    {
        // Get texture dimension
        auto texSize = texture.QueryMipLevelSize(0);

        // Read texture image data
        std::vector<LLGL::ColorRGBAub> imageBuffer(texSize.width*texSize.height);
        renderSys.ReadTexture(texture, mipLevel, LLGL::ImageFormat::RGBA, LLGL::DataType::UInt8, imageBuffer.data(), imageBuffer.size() * sizeof(LLGL::ColorRGBAub));

        // Save image data to file (using STBI library, see https://github.com/nothings/stb)
        auto result = stbi_write_png(
            filename.c_str(),
            static_cast<int>(texSize.width),
            static_cast<int>(texSize.height),
            4,
            imageBuffer.data(),
            static_cast<int>(texSize.width)*4
        );

        if (!result)
        {
            std::cerr << "failed to write texture to file: \"" + filename + "\"" << std::endl;
            return false;
        }

        // Show info
        std::cout << "saved texture: " << filename << std::endl;

        return true;
    }

protected:

    // Save texture image to a PNG file.
    bool SaveTexture(LLGL::Texture& texture, const std::string& filename, unsigned int mipLevel = 0)
    {
        return SaveTextureWithRenderer(*renderer, texture, filename, mipLevel);
    }

public:

    // Loads the vertices with position and normal from the specified Wavefront OBJ model file.
    static std::vector<VertexPositionNormal> LoadObjModel(const std::string& filename)
    {
        // Read obj file
        std::ifstream file(filename);
        if (!file.good())
            throw std::runtime_error("failed to load model from file: \"" + filename + "\"");

        std::vector<Gs::Vector3f> coords, normals;
        std::vector<VertexPositionNormal> vertices;

        while (!file.eof())
        {
            // Read each line
            std::string line;
            std::getline(file, line);

            std::stringstream s;
            s << line;

            // Parse line
            std::string mode;
            s >> mode;

            if (mode == "v")
            {
                Gs::Vector3f v;
                s >> v.x;
                s >> v.y;
                s >> v.z;
                coords.push_back(v);
            }
            else if (mode == "vn")
            {
                Gs::Vector3f n;
                s >> n.x;
                s >> n.y;
                s >> n.z;
                normals.push_back(n);
            }
            else if (mode == "f")
            {
                unsigned int v = 0, vn = 0;

                for (int i = 0; i < 3; ++i)
                {
                    s >> v;
                    s.ignore(2);
                    s >> vn;
                    vertices.push_back({ coords[v - 1], normals[vn - 1] });
                }
            }
        }

        return vertices;
    }

    // Generates eight vertices for a unit cube.
    static std::vector<Gs::Vector3f> GenerateCubeVertices()
    {
        return
        {
            { -1, -1, -1 }, { -1,  1, -1 }, {  1,  1, -1 }, {  1, -1, -1 },
            { -1, -1,  1 }, { -1,  1,  1 }, {  1,  1,  1 }, {  1, -1,  1 },
        };
    }

    // Generates 36 indices for a unit cube of 8 vertices
    // (36 = 3 indices per triangle * 2 triangles per cube face * 6 faces).
    static std::vector<std::uint32_t> GenerateCubeTriangleIndices()
    {
        return
        {
            0, 1, 2, 0, 2, 3, // front
            3, 2, 6, 3, 6, 7, // right
            4, 5, 1, 4, 1, 0, // left
            1, 5, 6, 1, 6, 2, // top
            4, 0, 3, 4, 3, 7, // bottom
            7, 6, 5, 7, 5, 4, // back
        };
    }

    // Generates 24 indices for a unit cube of 8 vertices.
    // (24 = 4 indices per quad * 1 quad per cube face * 6 faces)
    static std::vector<std::uint32_t> GenerateCubeQuadlIndices()
    {
        return
        {
            0, 1, 3, 2, // front
            3, 2, 7, 6, // right
            4, 5, 0, 1, // left
            1, 5, 2, 6, // top
            4, 0, 7, 3, // bottom
            7, 6, 4, 5, // back
        };
    }

    // Generates 24 vertices for a unit cube with texture coordinates.
    static std::vector<VertexPositionTexCoord> GenerateTexturedCubeVertices()
    {
        return
        {
            { { -1, -1, -1 }, { 0, 1 } }, { { -1,  1, -1 }, { 0, 0 } }, { {  1,  1, -1 }, { 1, 0 } }, { {  1, -1, -1 }, { 1, 1 } }, // front
            { {  1, -1, -1 }, { 0, 1 } }, { {  1,  1, -1 }, { 0, 0 } }, { {  1,  1,  1 }, { 1, 0 } }, { {  1, -1,  1 }, { 1, 1 } }, // right
            { { -1, -1,  1 }, { 0, 1 } }, { { -1,  1,  1 }, { 0, 0 } }, { { -1,  1, -1 }, { 1, 0 } }, { { -1, -1, -1 }, { 1, 1 } }, // left
            { { -1,  1, -1 }, { 0, 1 } }, { { -1,  1,  1 }, { 0, 0 } }, { {  1,  1,  1 }, { 1, 0 } }, { {  1,  1, -1 }, { 1, 1 } }, // top
            { { -1, -1,  1 }, { 0, 1 } }, { { -1, -1, -1 }, { 0, 0 } }, { {  1, -1, -1 }, { 1, 0 } }, { {  1, -1,  1 }, { 1, 1 } }, // bottom
            { {  1, -1,  1 }, { 0, 1 } }, { {  1,  1,  1 }, { 0, 0 } }, { { -1,  1,  1 }, { 1, 0 } }, { { -1, -1,  1 }, { 1, 1 } }, // back
        };
    }

    // Generates 36 indices for a unit cube of 24 vertices
    static std::vector<std::uint32_t> GenerateTexturedCubeTriangleIndices()
    {
        return
        {
             0,  1,  2,  0,  2,  3, // front
             4,  5,  6,  4,  6,  7, // right
             8,  9, 10,  8, 10, 11, // left
            12, 13, 14, 12, 14, 15, // top
            16, 17, 18, 16, 18, 19, // bottom
            20, 21, 22, 20, 22, 23, // back
        };
    }

protected:

    template <typename VertexType>
    LLGL::Buffer* CreateVertexBuffer(const std::vector<VertexType>& vertices, const LLGL::VertexFormat& vertexFormat)
    {
        return renderer->CreateBuffer(
            LLGL::VertexBufferDesc(static_cast<unsigned int>(vertices.size() * sizeof(VertexType)), vertexFormat),
            vertices.data()
        );
    }

    template <typename IndexType>
    LLGL::Buffer* CreateIndexBuffer(const std::vector<IndexType>& indices, const LLGL::IndexFormat& indexFormat)
    {
        return renderer->CreateBuffer(
            LLGL::IndexBufferDesc(static_cast<unsigned int>(indices.size() * sizeof(IndexType)), indexFormat),
            indices.data()
        );
    }

    template <typename Buffer>
    LLGL::Buffer* CreateConstantBuffer(const Buffer& buffer)
    {
        static_assert(!std::is_pointer<Buffer>::value, "buffer type must not be a pointer");
        return renderer->CreateBuffer(
            LLGL::ConstantBufferDesc(sizeof(buffer)),
            &buffer
        );
    }

    template <typename T>
    void UpdateBuffer(LLGL::Buffer* buffer, const T& data)
    {
        GS_ASSERT(buffer != nullptr);
        renderer->WriteBuffer(*buffer, &data, sizeof(data), 0);
    }

    // Returns the aspect ratio of the render context resolution (X:Y).
    float GetAspectRatio() const
    {
        auto resolution = context->GetVideoMode().resolution;
        return (static_cast<float>(resolution.width) / static_cast<float>(resolution.height));
    }

    // Returns ture if OpenGL is used as rendering API.
    bool IsOpenGL() const
    {
        return (renderer->GetRendererID() == LLGL::RendererID::OpenGL);
    }

    // Used by the window resize handler
    bool IsLoadingDone() const
    {
        return loadingDone_;
    }

    // Returns a projection with the specified parameters for the respective renderer.
    Gs::Matrix4f PerspectiveProjection(float aspectRatio, float near, float far, float fov)
    {
        int flags = (IsOpenGL() ? Gs::ProjectionFlags::UnitCube : 0);
        return Gs::ProjectionMatrix4f::Perspective(aspectRatio, near, far, fov, flags).ToMatrix4();
    }

    // Returns true if the specified shading language is supported.
    bool Supported(const LLGL::ShadingLanguage shadingLanguage) const
    {
        const auto& languages = renderer->GetRenderingCaps().shadingLanguages;
        return (std::find(languages.begin(), languages.end(), shadingLanguage) != languages.end());
    }

};

std::string Tutorial::rendererModule_;


template <typename T>
int RunTutorial(int argc, char* argv[])
{
    try
    {
        /* Run tutorial */
        Tutorial::SelectRendererModule(argc, argv);
        auto tutorial = std::unique_ptr<T>(new T());
        tutorial->Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        #ifdef _WIN32
        system("pause");
        #endif
    }
    return 0;
}

#define LLGL_IMPLEMENT_TUTORIAL(CLASS)          \
    int main(int argc, char* argv[])            \
    {                                           \
        return RunTutorial<CLASS>(argc, argv);  \
    }


#endif

