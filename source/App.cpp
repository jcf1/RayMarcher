/**
 C++ framework for GPU ray marching.
 Copyright 2014-2016 Morgan McGuire
 mcguire@cs.williams.edu
*/
#include <G3D/G3DAll.h>

/** Application framework. */
class App : public GApp {
protected:

    shared_ptr<Texture>         m_environmentMap;
    Array<shared_ptr<Texture> > m_veniceTexture;

public:
    
    App(const GApp::Settings& settings = GApp::Settings());

    virtual void onInit() override;

    virtual void onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& surfaceArray) override;
};

//////////////////////////////////////////////////////////////////////////////

App::App(const GApp::Settings& settings) : GApp(settings) {
}


void App::onInit() {
    GApp::onInit();

    setFrameDuration(1.0f / 60.0f);
    showRenderingStats  = false;

    createDeveloperHUD();
    developerWindow->sceneEditorWindow->setVisible(false);
    developerWindow->cameraControlWindow->setVisible(false);
    developerWindow->setVisible(false);
    developerWindow->videoRecordDialog->setEnabled(true);

    Texture::Specification e;
    e.filename = System::findDataFile("cubemap/islands/islands_*.jpg");
    e.encoding.format = ImageFormat::SRGB8();
    m_environmentMap = Texture::create(e);

    for (int i = 0; i < 4; ++i) {
        m_veniceTexture.append(Texture::fromFile(format("image/iChannel%d.jpg", i)));
    }

    debugCamera()->setFieldOfViewAngle(40 * units::degrees());
}


void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {
    // Bind the main framebuffer
    rd->push2D(m_framebuffer); {
        rd->clear();

        Args args;

        // Prepare the arguments for the shader function invoked below
        args.setUniform("cameraToWorldMatrix",    activeCamera()->frame());

        m_environmentMap->setShaderArgs(args, "environmentMap.", Sampler::cubeMap());
        args.setUniform("environmentMap_MIPConstant", std::log2(float(m_environmentMap->width() * sqrt(3.0f))));

        args.setUniform("tanHalfFieldOfViewY",  float(tan(activeCamera()->projection().fieldOfViewAngle() / 2.0f)));

        // Projection matrix, for writing to the depth buffer. This
        // creates the input that allows us to use the depth of field
        // effect below.
        Matrix4 projectionMatrix;
        activeCamera()->getProjectUnitMatrix(rd->viewport(), projectionMatrix);
        args.setUniform("projectionMatrix22", projectionMatrix[2][2]);
        args.setUniform("projectionMatrix23", projectionMatrix[2][3]);

        // Textures for the Venice example
        for (int i = 0; i < m_veniceTexture.size(); ++i) {
            args.setUniform(format("iChannel%d", i), m_veniceTexture[i], Sampler::defaults());
        }

        // Set the domain of the shader to the viewport rectangle
        args.setRect(rd->viewport());

        // Call the program in trace.pix for every pixel within the
        // domain, using many threads on the GPU. This call returns
        // immediately on the CPU and the code executes asynchronously
        // on the GPU.
        LAUNCH_SHADER(
            //"shader/trace-minimal.pix"
            //"shader/trace-analytic.pix"
            "shader/trace-raymarch.pix"
            //"shader/trace-venice.pix"
            , args);

        // Post-process special effects
        m_depthOfField->apply(rd, m_framebuffer->texture(0), 
                              m_framebuffer->texture(Framebuffer::DEPTH), 
                              activeCamera(), Vector2int16());
        
    } rd->pop2D();

    swapBuffers();

    rd->clear();

    // Perform gamma correction, bloom, and SSAA, and write to the native window frame buffer
    m_film->exposeAndRender(rd, activeCamera()->filmSettings(), m_framebuffer->texture(0), 0, 0);
}


//////////////////////////////////////////////////////////////////////


G3D_START_AT_MAIN();

int main(int argc, const char* argv[]) {
    GApp::Settings settings(argc, argv);

    settings.window.caption = "Simple GPU Ray Tracer";
    settings.window.width   = 780;
    settings.window.height  = 500;
    settings.hdrFramebuffer.depthGuardBandThickness = Vector2int16(0, 0);
    settings.hdrFramebuffer.colorGuardBandThickness = Vector2int16(0, 0);
    settings.screenshotDirectory = "../journal/";

    return App(settings).run();
}

