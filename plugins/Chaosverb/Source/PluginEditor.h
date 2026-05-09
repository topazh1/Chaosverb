#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Forward declaration
class ChaosverbAudioProcessor;

//==============================================================================
/**
 * ChaosverbAudioProcessorEditor — WebView-based plugin editor
 *
 * Stage 3 Phase 5.1: WebView setup with all 31 parameter bindings.
 * - 16 WebSliderRelay / WebSliderParameterAttachment (float params)
 * - 15 WebToggleButtonRelay / WebToggleButtonParameterAttachment (bool params)
 *
 * Window size: 900 x 400 (resizable, CSS transform scaling)
 */
class ChaosverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChaosverbAudioProcessorEditor (ChaosverbAudioProcessor&);
    ~ChaosverbAudioProcessorEditor() override;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    // Reference to processor
    ChaosverbAudioProcessor& processorRef;

    //==========================================================================
    // CRITICAL MEMBER DECLARATION ORDER
    // Members are destroyed in REVERSE order of declaration.
    // Order MUST be: Relays -> WebView -> Attachments
    //
    // Why: Attachments call evaluateJavascript() on destruction.
    //      WebView must still exist when attachments are destroyed.
    //      Relays must still exist when WebView is destroyed.
    //
    // Violation causes: undefined behaviour in release builds only
    // (debug builds may hide this with different destruction timing).
    //==========================================================================

    // 1. RELAYS FIRST — 12 Float relays + 10 Bool relays = 22 total
    // ─────────────────────────────────────────────────────────────
    // Float relays (WebSliderRelay)
    std::unique_ptr<juce::WebSliderRelay>         decayRelay;
    std::unique_ptr<juce::WebSliderRelay>         preDelayRelay;
    std::unique_ptr<juce::WebSliderRelay>         densityRelay;
    std::unique_ptr<juce::WebSliderRelay>         spectralTiltRelay;
    std::unique_ptr<juce::WebSliderRelay>         saturationRelay;
    std::unique_ptr<juce::WebSliderRelay>         modRateRelay;
    std::unique_ptr<juce::WebSliderRelay>         flutterSpeedRelay;
    std::unique_ptr<juce::WebSliderRelay>         mutationIntervalRelay;
    std::unique_ptr<juce::WebSliderRelay>         crossfadeSpeedRelay;
    std::unique_ptr<juce::WebSliderRelay>         widthRelay;
    std::unique_ptr<juce::WebSliderRelay>         mixRelay;
    std::unique_ptr<juce::WebSliderRelay>         lowCutRelay;
    std::unique_ptr<juce::WebSliderRelay>         highCutRelay;
    std::unique_ptr<juce::WebSliderRelay>         tiltRelay;
    std::unique_ptr<juce::WebSliderRelay>         wowFlutterAmountRelay;
    std::unique_ptr<juce::WebSliderRelay>         outputLevelRelay;
    std::unique_ptr<juce::WebSliderRelay>         duckingAmountRelay;
    std::unique_ptr<juce::WebSliderRelay>         duckAttackRelay;
    std::unique_ptr<juce::WebSliderRelay>         duckReleaseRelay;
    std::unique_ptr<juce::WebSliderRelay>         gravityRelay;
    std::unique_ptr<juce::WebSliderRelay>         mutationAmountRelay;
    std::unique_ptr<juce::WebSliderRelay>         intervalModeRelay;
    std::unique_ptr<juce::WebSliderRelay>         crossfadeModeRelay;

    // Bool relays (WebToggleButtonRelay)
    std::unique_ptr<juce::WebToggleButtonRelay>   decayLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   preDelayLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   densityLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   spectralTiltLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   saturationLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   modRateLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   flutterSpeedLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   widthLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   mixLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   lowCutLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   highCutLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   tiltLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   wowFlutterAmountLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   wowFlutterEnabledRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   outputLevelLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   duckingAmountLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   duckAttackLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   duckReleaseLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   gravityLockRelay;
    std::unique_ptr<juce::WebToggleButtonRelay>   bypassRelay;

    // 2. WEBVIEW SECOND — depends on relays being alive
    // ─────────────────────────────────────────────────────────────
    std::unique_ptr<juce::WebBrowserComponent>    webView;

    // 3. ATTACHMENTS LAST — depend on both relays and webView
    // ─────────────────────────────────────────────────────────────
    // Float attachments (WebSliderParameterAttachment)
    std::unique_ptr<juce::WebSliderParameterAttachment>       decayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       preDelayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       densityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       spectralTiltAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       saturationAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       modRateAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       flutterSpeedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       mutationIntervalAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       crossfadeSpeedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       widthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       mixAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       lowCutAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       highCutAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       tiltAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       wowFlutterAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       outputLevelAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       duckingAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       duckAttackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       duckReleaseAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       gravityAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       mutationAmountAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       intervalModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       crossfadeModeAttachment;

    // Bool attachments (WebToggleButtonParameterAttachment)
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> decayLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> preDelayLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> densityLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> spectralTiltLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> saturationLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> modRateLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> flutterSpeedLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> widthLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> mixLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lowCutLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> highCutLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> tiltLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> wowFlutterAmountLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> wowFlutterEnabledAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> outputLevelLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> duckingAmountLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> duckAttackLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> duckReleaseLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> gravityLockAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    //==========================================================================
    // Resource provider (Pattern 8: explicit URL mapping)
    std::optional<juce::WebBrowserComponent::Resource>
    getResource (const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChaosverbAudioProcessorEditor)
};
