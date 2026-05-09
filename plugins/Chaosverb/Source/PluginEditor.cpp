#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

//==============================================================================
ChaosverbAudioProcessorEditor::ChaosverbAudioProcessorEditor (ChaosverbAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // =========================================================================
    // STEP 1: Create all relays FIRST (no dependencies)
    // Construction order matches declaration order in .h file.
    // =========================================================================

    // Float relays (16)
    decayRelay            = std::make_unique<juce::WebSliderRelay>       ("decay");
    preDelayRelay         = std::make_unique<juce::WebSliderRelay>       ("preDelay");
    densityRelay          = std::make_unique<juce::WebSliderRelay>       ("density");
    spectralTiltRelay     = std::make_unique<juce::WebSliderRelay>       ("spectralTilt");
    saturationRelay        = std::make_unique<juce::WebSliderRelay>       ("saturation");
    modRateRelay          = std::make_unique<juce::WebSliderRelay>       ("modRate");
    flutterSpeedRelay     = std::make_unique<juce::WebSliderRelay>       ("flutterSpeed");
    mutationIntervalRelay = std::make_unique<juce::WebSliderRelay>       ("mutationInterval");
    crossfadeSpeedRelay   = std::make_unique<juce::WebSliderRelay>       ("crossfadeSpeed");
    widthRelay            = std::make_unique<juce::WebSliderRelay>       ("width");
    mixRelay              = std::make_unique<juce::WebSliderRelay>       ("mix");
    lowCutRelay           = std::make_unique<juce::WebSliderRelay>       ("lowCut");
    highCutRelay          = std::make_unique<juce::WebSliderRelay>       ("highCut");
    tiltRelay             = std::make_unique<juce::WebSliderRelay>       ("tilt");
    wowFlutterAmountRelay = std::make_unique<juce::WebSliderRelay>       ("wowFlutterAmount");
    outputLevelRelay      = std::make_unique<juce::WebSliderRelay>       ("outputLevel");
    duckingAmountRelay    = std::make_unique<juce::WebSliderRelay>       ("duckingAmount");
    duckAttackRelay       = std::make_unique<juce::WebSliderRelay>       ("duckAttack");
    duckReleaseRelay      = std::make_unique<juce::WebSliderRelay>       ("duckRelease");
    gravityRelay          = std::make_unique<juce::WebSliderRelay>       ("gravity");
    mutationAmountRelay   = std::make_unique<juce::WebSliderRelay>       ("mutationAmount");
    intervalModeRelay     = std::make_unique<juce::WebSliderRelay>       ("intervalMode");
    crossfadeModeRelay    = std::make_unique<juce::WebSliderRelay>       ("crossfadeMode");

    // Bool relays (15)
    decayLockRelay        = std::make_unique<juce::WebToggleButtonRelay> ("decayLock");
    preDelayLockRelay     = std::make_unique<juce::WebToggleButtonRelay> ("preDelayLock");
    densityLockRelay      = std::make_unique<juce::WebToggleButtonRelay> ("densityLock");
    spectralTiltLockRelay = std::make_unique<juce::WebToggleButtonRelay> ("spectralTiltLock");
    saturationLockRelay   = std::make_unique<juce::WebToggleButtonRelay> ("saturationLock");
    modRateLockRelay      = std::make_unique<juce::WebToggleButtonRelay> ("modRateLock");
    flutterSpeedLockRelay = std::make_unique<juce::WebToggleButtonRelay> ("flutterSpeedLock");
    widthLockRelay        = std::make_unique<juce::WebToggleButtonRelay> ("widthLock");
    mixLockRelay          = std::make_unique<juce::WebToggleButtonRelay> ("mixLock");
    lowCutLockRelay       = std::make_unique<juce::WebToggleButtonRelay> ("lowCutLock");
    highCutLockRelay      = std::make_unique<juce::WebToggleButtonRelay> ("highCutLock");
    tiltLockRelay         = std::make_unique<juce::WebToggleButtonRelay> ("tiltLock");
    wowFlutterAmountLockRelay = std::make_unique<juce::WebToggleButtonRelay>("wowFlutterAmountLock");
    wowFlutterEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("wowFlutterEnabled");
    outputLevelLockRelay   = std::make_unique<juce::WebToggleButtonRelay>("outputLevelLock");
    duckingAmountLockRelay = std::make_unique<juce::WebToggleButtonRelay>("duckingAmountLock");
    duckAttackLockRelay    = std::make_unique<juce::WebToggleButtonRelay>("duckAttackLock");
    duckReleaseLockRelay   = std::make_unique<juce::WebToggleButtonRelay>("duckReleaseLock");
    gravityLockRelay       = std::make_unique<juce::WebToggleButtonRelay>("gravityLock");
    bypassRelay            = std::make_unique<juce::WebToggleButtonRelay>("bypass");

    // =========================================================================
    // STEP 2: Create WebView SECOND (relays must exist first)
    // ALL 31 relays registered via .withOptionsFrom() in the Options chain.
    // =========================================================================
    webView = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider ([this] (const auto& url) { return getResource (url); })
            // Float relays
            .withOptionsFrom (*decayRelay)
            .withOptionsFrom (*preDelayRelay)
            .withOptionsFrom (*densityRelay)
            .withOptionsFrom (*spectralTiltRelay)
            .withOptionsFrom (*saturationRelay)
            .withOptionsFrom (*modRateRelay)
            .withOptionsFrom (*flutterSpeedRelay)
            .withOptionsFrom (*mutationIntervalRelay)
            .withOptionsFrom (*crossfadeSpeedRelay)
            .withOptionsFrom (*widthRelay)
            .withOptionsFrom (*mixRelay)
            .withOptionsFrom (*lowCutRelay)
            .withOptionsFrom (*highCutRelay)
            .withOptionsFrom (*tiltRelay)
            .withOptionsFrom (*wowFlutterAmountRelay)
            .withOptionsFrom (*outputLevelRelay)
            .withOptionsFrom (*duckingAmountRelay)
            .withOptionsFrom (*duckAttackRelay)
            .withOptionsFrom (*duckReleaseRelay)
            .withOptionsFrom (*gravityRelay)
            .withOptionsFrom (*mutationAmountRelay)
            .withOptionsFrom (*intervalModeRelay)
            .withOptionsFrom (*crossfadeModeRelay)
            // Bool relays (16)
            .withOptionsFrom (*decayLockRelay)
            .withOptionsFrom (*preDelayLockRelay)
            .withOptionsFrom (*densityLockRelay)
            .withOptionsFrom (*spectralTiltLockRelay)
            .withOptionsFrom (*saturationLockRelay)
            .withOptionsFrom (*modRateLockRelay)
            .withOptionsFrom (*flutterSpeedLockRelay)
            .withOptionsFrom (*widthLockRelay)
            .withOptionsFrom (*mixLockRelay)
            .withOptionsFrom (*lowCutLockRelay)
            .withOptionsFrom (*highCutLockRelay)
            .withOptionsFrom (*tiltLockRelay)
            .withOptionsFrom (*wowFlutterAmountLockRelay)
            .withOptionsFrom (*wowFlutterEnabledRelay)
            .withOptionsFrom (*outputLevelLockRelay)
            .withOptionsFrom (*duckingAmountLockRelay)
            .withOptionsFrom (*duckAttackLockRelay)
            .withOptionsFrom (*duckReleaseLockRelay)
            .withOptionsFrom (*gravityLockRelay)
            .withOptionsFrom (*bypassRelay)
            // Native functions: JS → C++ calls
            .withNativeFunction ("mutateNow", [this] (const juce::Array<juce::var>&,
                                                       std::function<void (juce::var)> complete)
            {
                processorRef.triggerMutation();
                complete ({});
            })
            .withNativeFunction ("toggleMutationTimer", [this] (const juce::Array<juce::var>&,
                                                                 std::function<void (juce::var)> complete)
            {
                bool newState = !processorRef.isMutationTimerRunning();
                processorRef.setMutationTimerRunning (newState);
                complete (juce::var (newState));
            })
            .withNativeFunction ("getMutationState", [this] (const juce::Array<juce::var>&,
                                                              std::function<void (juce::var)> complete)
            {
                auto* obj = new juce::DynamicObject();
                obj->setProperty ("remainingMs",  processorRef.getRemainingTimeMs());
                obj->setProperty ("running",      processorRef.isMutationTimerRunning());
                obj->setProperty ("intervalMs",   processorRef.computeMutationIntervalMs());
                complete (juce::var (obj));
            })
            .withNativeFunction ("resetToDefaults", [this] (const juce::Array<juce::var>&,
                                                             std::function<void (juce::var)> complete)
            {
                // Load user-saved defaults if present, else fall back to factory.
                juce::PropertiesFile::Options opts;
                opts.applicationName     = "Chaosverb";
                opts.filenameSuffix      = ".defaults";
                opts.osxLibrarySubFolder = "Application Support";
                juce::ApplicationProperties props;
                props.setStorageParameters (opts);
                auto* user = props.getUserSettings();
                const bool haveUser = (user->getAllProperties().size() > 0);

                auto& apvts = processorRef.parameters;
                for (auto* param : apvts.processor.getParameters())
                {
                    if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                    {
                        const auto id = rangedParam->getParameterID();
                        float norm;
                        if (haveUser && user->containsKey (id))
                            norm = (float) user->getDoubleValue (id);
                        else
                            norm = rangedParam->getDefaultValue();
                        rangedParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
                    }
                }
                complete ({});
            })
            .withNativeFunction ("saveCurrentAsDefaults", [this] (const juce::Array<juce::var>&,
                                                                    std::function<void (juce::var)> complete)
            {
                juce::PropertiesFile::Options opts;
                opts.applicationName     = "Chaosverb";
                opts.filenameSuffix      = ".defaults";
                opts.osxLibrarySubFolder = "Application Support";
                juce::ApplicationProperties props;
                props.setStorageParameters (opts);
                auto* user = props.getUserSettings();
                user->clear();

                auto& apvts = processorRef.parameters;
                for (auto* param : apvts.processor.getParameters())
                {
                    if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                    {
                        const auto id = rangedParam->getParameterID();
                        const float norm = rangedParam->getValue(); // normalized 0–1
                        user->setValue (id, (double) norm);
                    }
                }
                props.saveIfNeeded();
                complete (juce::var (true));
            })
            .withNativeFunction ("clearUserDefaults", [] (const juce::Array<juce::var>&,
                                                           std::function<void (juce::var)> complete)
            {
                juce::PropertiesFile::Options opts;
                opts.applicationName     = "Chaosverb";
                opts.filenameSuffix      = ".defaults";
                opts.osxLibrarySubFolder = "Application Support";
                juce::ApplicationProperties props;
                props.setStorageParameters (opts);
                props.getUserSettings()->clear();
                props.saveIfNeeded();
                complete (juce::var (true));
            })
            //==================================================================
            // Licensing native functions — JS UI <-> LicenseManager bridge
            //==================================================================
            .withNativeFunction ("getLicenseStatus", [this] (const juce::Array<juce::var>&,
                                                               std::function<void (juce::var)> complete)
            {
                auto& lm = processorRef.getLicenseManager();
                auto* obj = new juce::DynamicObject();
                const auto state = lm.getState();
                juce::String stateStr;
                switch (state) {
                    case LicenseManager::State::Licensed:     stateStr = "licensed";      break;
                    case LicenseManager::State::TrialActive:  stateStr = "trial_active";  break;
                    case LicenseManager::State::TrialExpired: stateStr = "trial_expired"; break;
                    default:                                  stateStr = "unlicensed";    break;
                }
                obj->setProperty ("state",            stateStr);
                obj->setProperty ("active",           lm.isActive());
                obj->setProperty ("key",              lm.getLicenseKey());
                obj->setProperty ("trialDaysLeft",    lm.getTrialDaysRemaining());
                obj->setProperty ("trialEverStarted", lm.trialEverStarted());
                complete (juce::var (obj));
            })
            .withNativeFunction ("activateLicense", [this] (const juce::Array<juce::var>& args,
                                                              std::function<void (juce::var)> complete)
            {
                if (args.size() < 1) {
                    auto* obj = new juce::DynamicObject();
                    obj->setProperty ("success", false);
                    obj->setProperty ("message", "No key provided.");
                    complete (juce::var (obj));
                    return;
                }
                const auto key = args[0].toString().trim();
                processorRef.getLicenseManager().verifyAsync (key,
                    [complete] (bool ok, juce::String msg) {
                        auto* obj = new juce::DynamicObject();
                        obj->setProperty ("success", ok);
                        obj->setProperty ("message", msg);
                        complete (juce::var (obj));
                    });
            })
            .withNativeFunction ("startTrial", [this] (const juce::Array<juce::var>&,
                                                         std::function<void (juce::var)> complete)
            {
                const bool started = processorRef.getLicenseManager().startTrial();
                auto* obj = new juce::DynamicObject();
                obj->setProperty ("success", started);
                obj->setProperty ("message", started ? "Trial started." : "Trial already used.");
                complete (juce::var (obj));
            })
            .withNativeFunction ("deactivateLicense", [this] (const juce::Array<juce::var>&,
                                                                std::function<void (juce::var)> complete)
            {
                processorRef.getLicenseManager().deactivateAsync (
                    [complete] (bool ok, juce::String msg) {
                        auto* obj = new juce::DynamicObject();
                        obj->setProperty ("success", ok);
                        obj->setProperty ("message", msg);
                        complete (juce::var (obj));
                    });
            })
            .withNativeFunction ("openExternalURL", [] (const juce::Array<juce::var>& args,
                                                          std::function<void (juce::var)> complete)
            {
                if (args.size() >= 1) {
                    const auto urlStr = args[0].toString();
                    juce::URL(urlStr).launchInDefaultBrowser();
                }
                complete (juce::var (true));
            })
            .withNativeFunction ("setEditorSize", [this] (const juce::Array<juce::var>& args,
                                                           std::function<void (juce::var)> complete)
            {
                if (args.size() >= 2) {
                    const int w = (int) args[0];
                    const int h = (int) args[1];
                    juce::MessageManager::callAsync ([this, w, h]() { setSize (w, h); });
                }
                complete (juce::var (true));
            })
    );

    // =========================================================================
    // STEP 3: Create attachments LAST (relays and webView must exist first)
    // Pattern 12: 3-argument constructor (parameter, relay, undoManager=nullptr)
    // =========================================================================
    auto& apvts = processorRef.parameters;

    // Float attachments
    decayAttachment        = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("decay"),            *decayRelay,            nullptr);
    preDelayAttachment     = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("preDelay"),         *preDelayRelay,         nullptr);
    densityAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("density"),          *densityRelay,          nullptr);
    spectralTiltAttachment = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("spectralTilt"),     *spectralTiltRelay,     nullptr);
    saturationAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("saturation"),       *saturationRelay,       nullptr);
    modRateAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("modRate"),          *modRateRelay,          nullptr);
    flutterSpeedAttachment = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("flutterSpeed"),     *flutterSpeedRelay,     nullptr);
    mutationIntervalAttachment = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("mutationInterval"), *mutationIntervalRelay, nullptr);
    crossfadeSpeedAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("crossfadeSpeed"),   *crossfadeSpeedRelay,   nullptr);
    widthAttachment        = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("width"),            *widthRelay,            nullptr);
    mixAttachment          = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("mix"),              *mixRelay,              nullptr);
    lowCutAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("lowCut"),           *lowCutRelay,           nullptr);
    highCutAttachment      = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("highCut"),          *highCutRelay,          nullptr);
    tiltAttachment         = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("tilt"),             *tiltRelay,             nullptr);
    wowFlutterAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("wowFlutterAmount"), *wowFlutterAmountRelay, nullptr);
    outputLevelAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("outputLevel"),      *outputLevelRelay,      nullptr);
    duckingAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("duckingAmount"),    *duckingAmountRelay,    nullptr);
    duckAttackAttachment    = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("duckAttack"),       *duckAttackRelay,       nullptr);
    duckReleaseAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("duckRelease"),       *duckReleaseRelay,      nullptr);
    gravityAttachment       = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("gravity"),          *gravityRelay,          nullptr);
    mutationAmountAttachment = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("mutationAmount"),   *mutationAmountRelay,   nullptr);
    intervalModeAttachment   = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("intervalMode"),     *intervalModeRelay,     nullptr);
    crossfadeModeAttachment  = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter ("crossfadeMode"),    *crossfadeModeRelay,    nullptr);

    // Bool attachments
    decayLockAttachment        = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("decayLock"),        *decayLockRelay,        nullptr);
    preDelayLockAttachment     = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("preDelayLock"),     *preDelayLockRelay,     nullptr);
    densityLockAttachment      = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("densityLock"),      *densityLockRelay,      nullptr);
    spectralTiltLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("spectralTiltLock"), *spectralTiltLockRelay, nullptr);
    saturationLockAttachment   = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("saturationLock"),   *saturationLockRelay,   nullptr);
    modRateLockAttachment      = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("modRateLock"),      *modRateLockRelay,      nullptr);
    flutterSpeedLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("flutterSpeedLock"), *flutterSpeedLockRelay, nullptr);
    widthLockAttachment        = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("widthLock"),        *widthLockRelay,        nullptr);
    mixLockAttachment          = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("mixLock"),          *mixLockRelay,          nullptr);
    lowCutLockAttachment       = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("lowCutLock"),       *lowCutLockRelay,       nullptr);
    highCutLockAttachment      = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("highCutLock"),      *highCutLockRelay,      nullptr);
    tiltLockAttachment         = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("tiltLock"),         *tiltLockRelay,         nullptr);
    wowFlutterAmountLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("wowFlutterAmountLock"), *wowFlutterAmountLockRelay, nullptr);
    wowFlutterEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("wowFlutterEnabled"), *wowFlutterEnabledRelay, nullptr);
    outputLevelLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("outputLevelLock"), *outputLevelLockRelay, nullptr);
    duckingAmountLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("duckingAmountLock"), *duckingAmountLockRelay, nullptr);
    duckAttackLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("duckAttackLock"),    *duckAttackLockRelay,    nullptr);
    duckReleaseLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("duckReleaseLock"),   *duckReleaseLockRelay,   nullptr);
    gravityLockAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("gravityLock"), *gravityLockRelay, nullptr);
    bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter ("bypass"), *bypassRelay, nullptr);

    // =========================================================================
    // STEP 4: Make WebView visible and load UI
    // =========================================================================
    addAndMakeVisible (*webView);
    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    // =========================================================================
    // STEP 5: Set plugin window size + resizable with fixed aspect ratio
    // =========================================================================

    // Read saved size BEFORE setResizeLimits — that call clamps the initial
    // 0×0 component to the minimum, triggering resized(), which would
    // overwrite these properties with the clamped values.
    int savedW = processorRef.parameters.state.getProperty ("editorWidth",  900);
    int savedH = processorRef.parameters.state.getProperty ("editorHeight", 400);

    setResizable (true, true);
    setResizeLimits (630, 280, 1800, 800);
    getConstrainer()->setFixedAspectRatio (900.0 / 400.0);
    setSize (savedW, savedH);
}

ChaosverbAudioProcessorEditor::~ChaosverbAudioProcessorEditor()
{
    // Destruction order is automatic — reverse of declaration order in .h file:
    // 1. Attachments destroyed first (call evaluateJavascript — webView still alive)
    // 2. WebView destroyed second (attachments already gone)
    // 3. Relays destroyed last (nothing using them)
    //
    // No explicit cleanup needed. std::unique_ptr handles everything correctly.
}

//==============================================================================
void ChaosverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background — WebView covers the entire editor, so this is a fallback
    // color shown briefly during load or if WebView fails.
    g.fillAll (juce::Colour (0xff0e0e0e));
}

void ChaosverbAudioProcessorEditor::resized()
{
    // WebView fills the entire plugin window
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());

    // Persist size in APVTS state tree — survives editor close/reopen
    // and DAW save/load (copyState includes these properties)
    processorRef.parameters.state.setProperty ("editorWidth",  getWidth(),  nullptr);
    processorRef.parameters.state.setProperty ("editorHeight", getHeight(), nullptr);
}

//==============================================================================
// Resource provider — Pattern 8: explicit URL mapping
//
// BinaryData symbol names are derived from file names (not paths):
//   Source/ui/public/index.html               -> BinaryData::index_html
//   Source/ui/public/js/juce/index.js         -> BinaryData::index_js
//   Source/ui/public/js/juce/check_native_interop.js -> BinaryData::check_native_interop_js
//==============================================================================
std::optional<juce::WebBrowserComponent::Resource>
ChaosverbAudioProcessorEditor::getResource (const juce::String& url)
{
    auto makeVector = [] (const char* data, int size)
    {
        return std::vector<std::byte> (
            reinterpret_cast<const std::byte*> (data),
            reinterpret_cast<const std::byte*> (data) + size);
    };

    // Main HTML document
    if (url == "/" || url == "/index.html")
    {
        return juce::WebBrowserComponent::Resource {
            makeVector (BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String ("text/html")
        };
    }

    // JUCE WebView JavaScript bridge (Pattern 21: ES6 module)
    if (url == "/js/juce/index.js")
    {
        return juce::WebBrowserComponent::Resource {
            makeVector (BinaryData::index_js, BinaryData::index_jsSize),
            juce::String ("application/javascript")
        };
    }

    // Native interop check script (Pattern 13: REQUIRED)
    if (url == "/js/juce/check_native_interop.js")
    {
        return juce::WebBrowserComponent::Resource {
            makeVector (BinaryData::check_native_interop_js,
                        BinaryData::check_native_interop_jsSize),
            juce::String ("application/javascript")
        };
    }

    // Resource not found
    return std::nullopt;
}
