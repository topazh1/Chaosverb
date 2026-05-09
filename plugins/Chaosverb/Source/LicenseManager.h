#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <utility>

//==============================================================================
//  LicenseManager — Polar.sh license verification + 7-day trial.
//
//  States (computed on demand):
//    LICENSED       — verified license key + activation on file, plugin active
//    TRIAL_ACTIVE   — within 7-day trial window since trial start
//    TRIAL_EXPIRED  — trial was started but more than 7 days ago
//    UNLICENSED     — no license + trial never started, plugin silent
//
//  Activation: user enters Polar license key → POST /activate creates
//  per-machine activation_id (counts against product's activation limit) →
//  stored locally for future /validate calls.
//
//  Validation: stored key + activation_id POSTed to /validate on each
//  plugin load. 30-day offline grace period from last successful validate.
//
//  Persistence: ~/Library/Application Support/Chaosverb.license
//==============================================================================
class LicenseManager
{
public:
    enum class State
    {
        Unlicensed,
        TrialActive,
        TrialExpired,
        Licensed
    };

    LicenseManager()
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName     = "Chaosverb";
        opts.filenameSuffix      = ".license";
        opts.osxLibrarySubFolder = "Application Support";
        opts.commonToAllUsers    = false;
        opts.doNotSave           = false;
        opts.ignoreCaseOfKeyNames = true;
        props_.setStorageParameters(opts);
    }

    //==========================================================================
    void initialize()
    {
        const auto key   = getLicenseKey();
        const auto actId = getActivationId();
        if (key.isNotEmpty() && actId.isNotEmpty())
        {
            // Trust cache during 30-day grace, then re-verify online.
            const auto lastVerifyMs = (juce::int64) props_.getUserSettings()
                ->getDoubleValue("lastVerifiedMs", 0.0);
            const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();
            const auto graceMs = (juce::int64) 30 * 24 * 3600 * 1000;
            if (nowMs - lastVerifyMs < graceMs)
                licensed_.store(true);

            validateAsync([](bool, juce::String) { /* silent refresh */ });
        }
        recomputeState();
    }

    //==========================================================================
    State getState() const noexcept { return state_.load(); }

    bool isActive() const noexcept
    {
        const auto s = state_.load();
        return s == State::Licensed || s == State::TrialActive;
    }

    juce::String getLicenseKey()    const { return props_.getUserSettings()->getValue("licenseKey", ""); }
    juce::String getActivationId()  const { return props_.getUserSettings()->getValue("activationId", ""); }

    int getTrialDaysRemaining() const noexcept
    {
        const auto trialStartMs = (juce::int64) props_.getUserSettings()
            ->getDoubleValue("trialStartMs", 0.0);
        if (trialStartMs == 0) return 0;
        const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();
        const auto elapsedMs = nowMs - trialStartMs;
        const auto remainMs  = ((juce::int64) kTrialDays * 24 * 3600 * 1000) - elapsedMs;
        if (remainMs <= 0) return 0;
        return (int)((remainMs / (24LL * 3600LL * 1000LL)) + 1);
    }

    bool trialEverStarted() const noexcept
    {
        return props_.getUserSettings()->getDoubleValue("trialStartMs", 0.0) > 0.0;
    }

    //==========================================================================
    bool startTrial()
    {
        if (trialEverStarted()) return false;
        const auto nowMs = juce::Time::getCurrentTime().toMilliseconds();
        props_.getUserSettings()->setValue("trialStartMs", (double) nowMs);
        props_.saveIfNeeded();
        recomputeState();
        return true;
    }

    // Activate license on this machine. Hits /activate endpoint, stores
    // activation_id locally. Counts against per-license activation cap.
    void verifyAsync(const juce::String& key,
                     std::function<void(bool, juce::String)> callback)
    {
        juce::Thread::launch([this, key, callback = std::move(callback)]() mutable
        {
            const auto result = activateOnline(key);

            juce::MessageManager::callAsync([this, key, result, callback = std::move(callback)]()
            {
                if (result.success)
                {
                    props_.getUserSettings()->setValue("licenseKey",   key);
                    props_.getUserSettings()->setValue("activationId", result.activationId);
                    props_.getUserSettings()->setValue("lastVerifiedMs",
                        (double) juce::Time::getCurrentTime().toMilliseconds());
                    props_.saveIfNeeded();
                    licensed_.store(true);
                }
                recomputeState();
                callback(result.success, result.message);
            });
        });
    }

    // Deactivate runs the network call SYNCHRONOUSLY on background thread,
    // then clears local state on success.  Caller awaits via callback so UI
    // can report failures (e.g. server still holding the activation slot).
    void deactivateAsync(std::function<void(bool, juce::String)> callback)
    {
        const auto key   = getLicenseKey();
        const auto actId = getActivationId();
        if (key.isEmpty() || actId.isEmpty())
        {
            // Nothing on file — just clear local state.
            clearLocalLicense();
            callback(true, "Deactivated locally.");
            return;
        }

        juce::Thread::launch([this, key, actId, callback = std::move(callback)]() mutable
        {
            const auto result = deactivateOnline(key, actId);

            juce::MessageManager::callAsync([this, result, callback = std::move(callback)]()
            {
                if (result.first)
                {
                    clearLocalLicense();
                    callback(true, "Deactivated. Slot freed on server.");
                }
                else
                {
                    // Server failed — don't wipe local state, user can retry.
                    callback(false, "Server deactivation failed: " + result.second
                                  + ". Try again in 60 seconds, or remove the activation"
                                    " manually from your Polar dashboard.");
                }
            });
        });
    }

    // Local-only wipe (used after successful server deactivate, or as fallback).
    void clearLocalLicense()
    {
        props_.getUserSettings()->removeValue("licenseKey");
        props_.getUserSettings()->removeValue("activationId");
        props_.getUserSettings()->removeValue("lastVerifiedMs");
        props_.saveIfNeeded();
        licensed_.store(false);
        recomputeState();
    }

    // Backward-compat: sync local-only wipe (no server call).
    void deactivate()
    {
        deactivateAsync([](bool, juce::String) {});
    }

private:
    //==========================================================================
    // Polar.sh — Entropia Audio organization
    static constexpr const char* kOrganizationId = "95b4b796-ea68-4674-a5ce-78f0244fef58";

    static constexpr const char* kActivateURL =
        "https://api.polar.sh/v1/customer-portal/license-keys/activate";
    static constexpr const char* kValidateURL =
        "https://api.polar.sh/v1/customer-portal/license-keys/validate";
    static constexpr const char* kDeactivateURL =
        "https://api.polar.sh/v1/customer-portal/license-keys/deactivate";

    static constexpr int kTrialDays = 7;

    struct ActivateResult { bool success; juce::String message; juce::String activationId; };

    juce::String makeMachineLabel() const
    {
        // Human-readable label shown in Polar dashboard for each activation.
        return juce::SystemStats::getComputerName() + " ("
             + juce::SystemStats::getOperatingSystemName() + ")";
    }

    std::pair<bool, juce::String> postJson(const juce::String& url,
                                           const juce::var&    body,
                                           juce::var&          parsedOut)
    {
        const auto bodyStr = juce::JSON::toString(body);

        juce::URL u(url);
        u = u.withPOSTData(bodyStr);

        auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                        .withConnectionTimeoutMs(8000)
                        .withExtraHeaders("Content-Type: application/json\r\n"
                                          "Accept: application/json\r\n");

        int statusCode = 0;
        auto stream = u.createInputStream(opts.withStatusCode(&statusCode));
        if (stream == nullptr)
            return {false, "No connection — check your internet."};

        const auto response = stream->readEntireStreamAsString();
        parsedOut = juce::JSON::parse(response);

        if (statusCode >= 400)
        {
            const auto detail = parsedOut.getProperty("detail", juce::var());
            // Polar may return detail as string OR array of validation errors
            if (detail.isString())  return {false, detail.toString()};
            if (detail.isArray() && detail.size() > 0)
                return {false, detail[0].getProperty("msg", "Invalid license key").toString()};
            return {false, "Invalid license key (HTTP " + juce::String(statusCode) + ")"};
        }

        return {true, ""};
    }

    ActivateResult activateOnline(const juce::String& key)
    {
        // First-time: POST /activate to claim an activation slot.
        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("key",             key);
        body->setProperty("organization_id", juce::String(kOrganizationId));
        body->setProperty("label",           makeMachineLabel());

        juce::var parsed;
        const auto result = postJson(kActivateURL, juce::var(body.get()), parsed);
        if (!result.first)
            return {false, result.second, {}};

        // Polar returns: { id, license_key: { status, ... }, label, ... }
        const auto activationId = parsed.getProperty("id", "").toString();
        const auto lkey         = parsed.getProperty("license_key", juce::var());
        const auto status       = lkey.getProperty("status", "").toString();

        if (activationId.isEmpty())
            return {false, "Activation failed — no activation ID returned.", {}};

        if (status != "granted")
        {
            if (status == "revoked")  return {false, "License revoked.", {}};
            if (status == "disabled") return {false, "License disabled.", {}};
            if (status == "expired")  return {false, "License expired.", {}};
            return {false, "License not active (" + status + ").", {}};
        }

        return {true, "License activated.", activationId};
    }

    void validateAsync(std::function<void(bool, juce::String)> callback)
    {
        const auto key   = getLicenseKey();
        const auto actId = getActivationId();
        if (key.isEmpty() || actId.isEmpty()) {
            callback(false, "No stored license.");
            return;
        }

        juce::Thread::launch([this, key, actId, callback = std::move(callback)]() mutable
        {
            juce::DynamicObject::Ptr body = new juce::DynamicObject();
            body->setProperty("key",             key);
            body->setProperty("organization_id", juce::String(kOrganizationId));
            body->setProperty("activation_id",   actId);

            juce::var parsed;
            const auto result = postJson(kValidateURL, juce::var(body.get()), parsed);

            juce::MessageManager::callAsync([this, result, parsed, callback = std::move(callback)]()
            {
                if (!result.first)
                {
                    // Network failure → don't lock immediately, rely on grace period.
                    callback(false, result.second);
                    return;
                }
                const auto status = parsed.getProperty("status", "").toString();
                const bool ok = (status == "granted");
                if (ok)
                {
                    props_.getUserSettings()->setValue("lastVerifiedMs",
                        (double) juce::Time::getCurrentTime().toMilliseconds());
                    props_.saveIfNeeded();
                    licensed_.store(true);
                }
                else
                {
                    licensed_.store(false);
                }
                recomputeState();
                callback(ok, ok ? "License valid." : ("License " + status));
            });
        });
    }

    std::pair<bool, juce::String> deactivateOnline(const juce::String& key, const juce::String& actId)
    {
        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty("key",             key);
        body->setProperty("organization_id", juce::String(kOrganizationId));
        body->setProperty("activation_id",   actId);

        juce::var parsed;
        return postJson(kDeactivateURL, juce::var(body.get()), parsed);
    }

    void recomputeState() noexcept
    {
        if (licensed_.load())
        {
            state_.store(State::Licensed);
            return;
        }
        if (trialEverStarted())
        {
            state_.store(getTrialDaysRemaining() > 0 ? State::TrialActive
                                                    : State::TrialExpired);
            return;
        }
        state_.store(State::Unlicensed);
    }

    mutable juce::ApplicationProperties props_;  // const queries call non-const getUserSettings()
    std::atomic<bool>                   licensed_{false};
    std::atomic<State>                  state_{State::Unlicensed};
};
