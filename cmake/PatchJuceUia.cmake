# Periscope: backport of JUCE commit "Accessibility (Windows): Guard UiaDisconnectAllProviders so that it is only
# called in standalone apps" (develop, 2026-07-24), which JUCE 8.0.13 does not have.
#
# Why: when the last plugin instance unloads, JUCE's ~WindowsUIAWrapper calls UiaDisconnectAllProviders. That cuts
# off EVERY UI Automation provider in the host process (REAPER's and other plugins'), and with NVDA running it can
# block REAPER's main thread for minutes: found by minidump on 2026-10-25 after removing Periscope from the master.
# Each of our providers is still disconnected one by one when its component goes away.
#
# The edit is made in the fetched JUCE sources at configure time, is idempotent (a cached, already patched checkout
# is left alone) and stops the configure if the code no longer looks like 8.0.13, so it can never silently not apply.
function (ovni_patch_juce_uia juce_dir)
    set(native "${juce_dir}/modules/juce_gui_basics/native/accessibility")

    # 1. The guard inside disconnectAllProviders itself, as upstream.
    set(wrapper "${native}/juce_WindowsUIAWrapper_windows.h")
    file(READ "${wrapper}" text)
    string(FIND "${text}" "Periscope: UIA guard" done)
    if (done EQUAL -1)
        set(anchor "JUCE_COMRESULT disconnectAllProviders()")
        string(FIND "${text}" "${anchor}" at)
        if (at EQUAL -1)
            message(FATAL_ERROR "[ovni] JUCE UIA patch: disconnectAllProviders not found in ${wrapper}")
        endif ()
        string(SUBSTRING "${text}" ${at} -1 tail)
        string(FIND "${tail}" "{" brace)
        math(EXPR cut "${at} + ${brace} + 1")
        string(SUBSTRING "${text}" 0 ${cut} head)
        string(SUBSTRING "${text}" ${cut} -1 rest)
        set(guard "\n        // Periscope: UIA guard (JUCE develop 2026-07-24): never in a plugin, the host still uses its providers.\n        if (! JUCEApplicationBase::isStandaloneApp())\n            return (HRESULT) UIA_E_NOTSUPPORTED;\n")
        file(WRITE "${wrapper}" "${head}${guard}${rest}")
        message(STATUS "[ovni] JUCE UIA patch applied to juce_WindowsUIAWrapper_windows.h")
    endif ()

    # 2. The caller's own check becomes redundant; upstream removed it, so do we.
    set(impl "${native}/juce_Accessibility_windows.cpp")
    file(READ "${impl}" text)
    set(old "if (providerCount == 0 && JUCEApplicationBase::isStandaloneApp())")
    string(FIND "${text}" "${old}" at)
    if (NOT at EQUAL -1)
        string(REPLACE "${old}" "if (providerCount == 0)" text "${text}")
        file(WRITE "${impl}" "${text}")
        message(STATUS "[ovni] JUCE UIA patch applied to juce_Accessibility_windows.cpp")
    endif ()
endfunction ()
