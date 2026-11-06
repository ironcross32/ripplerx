// Screen reader announcements.
//
// On Windows JUCE's AccessibilityHandler::postAnnouncement speaks through SAPI,
// which talks over the user's screen reader with a different voice. Instead we
// raise a UI Automation notification event (like Surge XT does), so the screen
// reader speaks the text itself, with its own voice, speech rate and
// interruption rules. Other platforms keep using JUCE's implementation.

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <oleauto.h>
 #include <UIAutomation.h>
 #pragma comment(lib, "oleaut32.lib")
#endif

namespace a11y
{

#if JUCE_WINDOWS
namespace
{
    using RaiseNotificationFn = HRESULT (WINAPI*)(IRawElementProviderSimple*, NotificationKind,
                                                  NotificationProcessing, BSTR, BSTR);
    using ClientsListeningFn = BOOL (WINAPI*)();

    struct UiaFunctions
    {
        RaiseNotificationFn raiseNotification = nullptr;
        ClientsListeningFn clientsAreListening = nullptr;

        UiaFunctions()
        {
            // UiaRaiseNotificationEvent exists from Windows 10 1709 on, so load it dynamically
            HMODULE dll = GetModuleHandleW(L"UIAutomationCore.dll");
            if (dll == nullptr)
                dll = LoadLibraryW(L"UIAutomationCore.dll");
            if (dll == nullptr)
                return;

            raiseNotification = reinterpret_cast<RaiseNotificationFn>(GetProcAddress(dll, "UiaRaiseNotificationEvent"));
            clientsAreListening = reinterpret_cast<ClientsListeningFn>(GetProcAddress(dll, "UiaClientsAreListening"));
        }

        static const UiaFunctions& get()
        {
            static const UiaFunctions instance;
            return instance;
        }
    };

    // Finds an accessible component to raise the event from: the source if given,
    // else whatever has keyboard focus, else any visible top-level window of ours.
    juce::AccessibilityHandler* findHandler(juce::Component* source)
    {
        auto fromComponent = [](juce::Component* c) -> juce::AccessibilityHandler* {
            for (; c != nullptr; c = c->getParentComponent())
                if (c->isShowing())
                    if (auto* h = c->getAccessibilityHandler())
                        return h;
            return nullptr;
        };

        if (auto* h = fromComponent(source))
            return h;
        if (auto* h = fromComponent(juce::Component::getCurrentlyFocusedComponent()))
            return h;

        for (int i = 0; i < juce::ComponentPeer::getNumPeers(); ++i)
            if (auto* peer = juce::ComponentPeer::getPeer(i))
                if (auto* h = fromComponent(&peer->getComponent()))
                    return h;

        return nullptr;
    }

    // Returns false if the UIA path is unavailable, so the caller can fall back.
    bool raiseUiaNotification(const juce::String& text, juce::Component* source)
    {
        const auto& uia = UiaFunctions::get();
        if (uia.raiseNotification == nullptr)
            return false;

        if (uia.clientsAreListening != nullptr && !uia.clientsAreListening())
            return true; // nobody to hear it; don't fall back to SAPI either

        auto* handler = findHandler(source);
        if (handler == nullptr)
            return false;

        // JUCE's native handle is a COM object whose first base is IRawElementProviderSimple,
        // so it can be used as an IUnknown. The type is only forward-declared outside JUCE.
        auto* unknown = reinterpret_cast<IUnknown*>(handler->getNativeImplementation());
        if (unknown == nullptr)
            return false;

        IRawElementProviderSimple* provider = nullptr;
        if (FAILED(unknown->QueryInterface(__uuidof(IRawElementProviderSimple), reinterpret_cast<void**>(&provider)))
            || provider == nullptr)
            return false;

        BSTR displayString = SysAllocString(text.toWideCharPointer());
        BSTR activityId = SysAllocString(L"RipplerX.Announcement");

        const auto hr = uia.raiseNotification(provider,
                                              NotificationKind_ActionCompleted,
                                              NotificationProcessing_ImportantMostRecent,
                                              displayString,
                                              activityId);

        SysFreeString(displayString);
        SysFreeString(activityId);
        provider->Release();

        return SUCCEEDED(hr);
    }
}
#endif

void announce(const juce::String& text, juce::Component* source)
{
    if (text.isEmpty())
        return;

    if (!juce::MessageManager::existsAndIsCurrentThread()) {
        juce::Component::SafePointer<juce::Component> safeSource(source);
        juce::MessageManager::callAsync([text, safeSource] { announce(text, safeSource.getComponent()); });
        return;
    }

   #if JUCE_WINDOWS
    if (raiseUiaNotification(text, source))
        return;
   #endif

    juce::AccessibilityHandler::postAnnouncement(text, juce::AccessibilityHandler::AnnouncementPriority::high);
}

} // namespace a11y
