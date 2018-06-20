/*
 * main.cpp (Tutorial06_MultiContext)
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015-2018 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include <tutorial.h>


int main(int argc, char* argv[])
{
    try
    {
        // Load render system module
        LLGL::RenderingDebugger debugger;
        auto renderer = LLGL::RenderSystem::Load(GetSelectedRendererModule(argc, argv), nullptr, &debugger);

        std::cout << "LLGL Renderer: " << renderer->GetName() << std::endl;

        // Create two render contexts
        LLGL::RenderContextDescriptor contextDesc;
        {
            contextDesc.videoMode.resolution            = { 640, 480 };
            contextDesc.vsync.enabled                   = true;
            contextDesc.multiSampling                   = LLGL::MultiSamplingDescriptor(8);
            contextDesc.profileOpenGL.contextProfile    = LLGL::OpenGLContextProfile::CoreProfile;
        }
        auto context1 = renderer->CreateRenderContext(contextDesc);
        auto context2 = renderer->CreateRenderContext(contextDesc);

        // Create command buffer
        auto commands = renderer->CreateCommandBuffer();

        // Create input handler
        auto input = std::make_shared<LLGL::Input>();

        auto& window1 = static_cast<LLGL::Window&>(context1->GetSurface());
        auto& window2 = static_cast<LLGL::Window&>(context2->GetSurface());

        window1.AddEventListener(input);
        window2.AddEventListener(input);

        // Set window titles
        window1.SetTitle(L"LLGL Tutorial 06: Multi Context (1)");
        window2.SetTitle(L"LLGL Tutorial 06: Multi Context (2)");

        // Set window positions
        LLGL::Extent2D desktopResolution;
        if (auto display = LLGL::Display::QueryPrimary())
            desktopResolution = display->GetDisplayMode().resolution;

        const LLGL::Offset2D desktopCenter
        {
            static_cast<int>(desktopResolution.width)/2,
            static_cast<int>(desktopResolution.height)/2
        };

        window1.SetPosition({ desktopCenter.x - 700, desktopCenter.y - 480/2 });
        window2.SetPosition({ desktopCenter.x + 700 - 640, desktopCenter.y - 480/2 });

        // Show windows
        window1.Show();
        window2.Show();

        // Vertex data structure
        struct Vertex
        {
            Gs::Vector2f    position;
            LLGL::ColorRGBf color;
        };

        // Vertex data
        float objSize = 0.5f;
        Vertex vertices[] =
        {
            // Triangle
            { {        0,  objSize }, { 1, 0, 0 } },
            { {  objSize, -objSize }, { 0, 1, 0 } },
            { { -objSize, -objSize }, { 0, 0, 1 } },

            // Quad
            { { -objSize, -objSize }, { 1, 0, 0 } },
            { { -objSize,  objSize }, { 1, 0, 0 } },
            { {  objSize, -objSize }, { 1, 1, 0 } },
            { {  objSize,  objSize }, { 1, 1, 0 } },
        };

        // Vertex format
        LLGL::VertexFormat vertexFormat;
        vertexFormat.AppendAttribute({ "position", LLGL::Format::RG32Float }); // position has 2 float components
        vertexFormat.AppendAttribute({ "color",    LLGL::Format::RGB32Float }); // color has 3 float components

        // Create vertex buffer
        LLGL::BufferDescriptor vertexBufferDesc;
        {
            vertexBufferDesc.type                   = LLGL::BufferType::Vertex;
            vertexBufferDesc.size                   = sizeof(vertices);         // Size (in bytes) of the vertex buffer
            vertexBufferDesc.vertexBuffer.format    = vertexFormat;             // Vertex format layout
        }
        auto vertexBuffer = renderer->CreateBuffer(vertexBufferDesc, vertices);

        // Create shaders
        auto vertexShader = renderer->CreateShader(LLGL::ShaderType::Vertex);
        auto geometryShader = renderer->CreateShader(LLGL::ShaderType::Geometry);
        auto fragmentShader = renderer->CreateShader(LLGL::ShaderType::Fragment);

        // Define the lambda function to read an entire text file
        auto ReadFileContent = [](const std::string& filename)
        {
            std::ifstream file(filename);

            if (!file.good())
                throw std::runtime_error("failed to read file: \"" + filename + "\"");

            return std::string(
                ( std::istreambuf_iterator<char>(file) ),
                ( std::istreambuf_iterator<char>() )
            );
        };

        auto ReadBinaryFile = [](const std::string& filename)
        {
            // Read file and for failure
            std::ifstream file { filename, std::ios_base::binary | std::ios_base::ate };

            if (!file.good())
                throw std::runtime_error("failed to read file: \"" + filename + "\"");

            const auto fileSize = static_cast<size_t>(file.tellg());
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            return buffer;
        };

        // Load vertex - and fragment shader code from file
        auto CompileShader = [](LLGL::Shader* shader, const std::string& source, std::vector<char> byteCode = {}, const LLGL::ShaderDescriptor& shaderDesc = {})
        {
            // Compile shader
            if (byteCode.empty())
                shader->Compile(source, shaderDesc);
            else
                shader->LoadBinary(std::move(byteCode), shaderDesc);

            // Print info log (warnings and errors)
            std::string log = shader->QueryInfoLog();
            if (!log.empty())
                std::cerr << log << std::endl;
        };

        const auto& languages = renderer->GetRenderingCaps().shadingLanguages;
        if (std::find(languages.begin(), languages.end(), LLGL::ShadingLanguage::GLSL) != languages.end())
        {
            CompileShader(vertexShader, ReadFileContent("vertex.glsl"));
            CompileShader(geometryShader, ReadFileContent("geometry.glsl"));
            CompileShader(fragmentShader, ReadFileContent("fragment.glsl"));
        }
        else if (std::find(languages.begin(), languages.end(), LLGL::ShadingLanguage::SPIRV) != languages.end())
        {
            CompileShader(vertexShader, "", ReadBinaryFile("vertex.450core.spv"));
            CompileShader(geometryShader, "", ReadBinaryFile("geometry.450core.spv"));
            CompileShader(fragmentShader, "", ReadBinaryFile("fragment.450core.spv"));
        }
        else if (std::find(languages.begin(), languages.end(), LLGL::ShadingLanguage::HLSL) != languages.end())
        {
            auto shaderCode = ReadFileContent("shader.hlsl");
            CompileShader(vertexShader, shaderCode, {}, { "VS", "vs_4_0" });
            CompileShader(geometryShader, shaderCode, {}, { "GS", "gs_4_0" });
            CompileShader(fragmentShader, shaderCode, {}, { "PS", "ps_4_0" });
        }

        // Create shader program which is used as composite
        auto shaderProgram = renderer->CreateShaderProgram();

        // Attach vertex- and fragment shader to the shader program
        shaderProgram->AttachShader(*vertexShader);
        shaderProgram->AttachShader(*geometryShader);
        shaderProgram->AttachShader(*fragmentShader);

        // Bind vertex attribute layout (this is not required for a compute shader program)
        shaderProgram->BuildInputLayout(1, &vertexFormat);

        // Link shader program and check for errors
        if (!shaderProgram->LinkShaders())
            throw std::runtime_error(shaderProgram->QueryInfoLog());

        // Create graphics pipeline
        LLGL::GraphicsPipeline* pipeline[2] = {};
        const bool logicOpSupported = renderer->GetRenderingCaps().features.hasLogicOp;

        LLGL::GraphicsPipelineDescriptor pipelineDesc;
        {
            pipelineDesc.primitiveTopology          = LLGL::PrimitiveTopology::TriangleStrip;
            pipelineDesc.shaderProgram              = shaderProgram;
            pipelineDesc.rasterizer.multiSampling   = contextDesc.multiSampling;
        }
        pipeline[0] = renderer->CreateGraphicsPipeline(pipelineDesc);

        {
            // Only enable logic operations if it's supported, otherwise an exception is thrown
            if (logicOpSupported)
                pipelineDesc.blend.logicOp = LLGL::LogicOp::CopyInverted;
        }
        pipeline[1] = renderer->CreateGraphicsPipeline(pipelineDesc);

        // Initialize viewport array
        LLGL::Viewport viewports[2] =
        {
            LLGL::Viewport {   0.0f, 0.0f, 320.0f, 480.0f },
            LLGL::Viewport { 320.0f, 0.0f, 320.0f, 480.0f },
        };

        bool enableLogicOp = false;

        if (logicOpSupported)
            std::cout << "Press SPACE to enabled/disable logic fragment operations" << std::endl;

        // Enter main loop
        while ( ( window1.ProcessEvents() || window2.ProcessEvents() ) && !input->KeyPressed(LLGL::Key::Escape) )
        {
            // Switch between pipeline states
            if (input->KeyDown(LLGL::Key::Space))
            {
                if (logicOpSupported)
                {
                    enableLogicOp = !enableLogicOp;
                    if (enableLogicOp)
                        std::cout << "Logic Fragment Operation Enabled" << std::endl;
                    else
                        std::cout << "Logic Fragment Operation Disabled" << std::endl;
                }
                else
                    std::cout << "Logic Fragment Operation Not Supported" << std::endl;
            }

            // Draw content in 1st render context
            commands->SetRenderTarget(*context1);
            {
                // Set viewport and scissor arrays
                commands->SetViewports(2, viewports);

                // Set graphics pipeline
                commands->SetGraphicsPipeline(*pipeline[enableLogicOp ? 1 : 0]);

                // Set vertex buffer
                commands->SetVertexBuffer(*vertexBuffer);

                // Clear color buffer
                commands->Clear(LLGL::ClearFlags::Color);

                // Draw triangle with 3 vertices
                commands->Draw(3, 0);
            }
            // Present the result on the screen
            context1->Present();

            // Draw content in 2nd render context
            commands->SetRenderTarget(*context2);
            {
                // Set viewport and scissor arrays
                commands->SetViewports(2, viewports);

                // Set graphics pipeline
                commands->SetGraphicsPipeline(*pipeline[enableLogicOp ? 1 : 0]);

                // Set vertex buffer
                commands->SetVertexBuffer(*vertexBuffer);

                // Clear color buffer
                commands->Clear(LLGL::ClearFlags::Color);

                // Draw quad with 4 vertices
                commands->Draw(4, 3);
            }
            // Present the result on the screen
            context2->Present();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
