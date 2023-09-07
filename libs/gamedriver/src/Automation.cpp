

#include "gamedriver/Automation.h"
#include "gamedriver/Configure.h"

Automation::Automation()
{

}

Automation::~Automation()
{

}

void Automation::setup()
{
    const bool batchMode = !mBatchFile.empty();

    // First check if a custom automation spec has been provided. If it fails to load, the app
    // must be closed since it could be invoked from a script.
    if (batchMode && mBatchFile != "default")
    {
        auto size = getFileSize(mBatchFile.c_str());
        if (size > 0)
        {
            std::ifstream     in(mBatchFile, std::ifstream::binary | std::ifstream::in);
            std::vector<char> json(static_cast<unsigned long>(size));
            in.read(json.data(), size);
            mAutomationSpec = filament::viewer::AutomationSpec::generate(json.data(), size);
            if (!mAutomationSpec)
            {
                std::cerr << "Unable to parse automation spec: " << mBatchFile << std::endl;
                exit(1);
            }
        }
        else
        {
            std::cerr << "Unable to load automation spec: " << mBatchFile << std::endl;
            exit(1);
        }
    }

    // If no custom spec has been provided, or if in interactive mode, load the default spec.
    if (!mAutomationSpec)
    {
        mAutomationSpec = filament::viewer::AutomationSpec::generateDefaultTestCases();
    }

    mAutomationEngine = new filament::viewer::AutomationEngine(mAutomationSpec, &gSettings);

    if (batchMode)
    {
        mAutomationEngine->startBatchMode();
        auto options              = mAutomationEngine->getOptions();
        options.sleepDuration     = 0.0;
        options.exportScreenshots = true;
        options.exportSettings    = true;
        mAutomationEngine->setOptions(options);
        //stopAnimation();
    }
}

void Automation::tick(filament::View* view, filament::gltfio::FilamentInstance* instance, filament::Renderer* renderer, long long dt)
{

    if (mAutomationEngine->shouldClose())
    {
        return;
    }
    
    filament::viewer::AutomationEngine::ViewerContent content = {
        .view          = view,
        .renderer      = renderer,
        .materials     = instance->getMaterialInstances(),
        .materialCount = instance->getMaterialInstanceCount(),
    };
    mAutomationEngine->tick(renderer->getEngine(), content, ImGui::GetIO().DeltaTime);
}