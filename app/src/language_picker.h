// Language picker shown at startup. Sets ReXGlue's user_language cvar.
#pragma once
namespace afro {
// Call before Runtime::Setup(). Returns the XLanguage value applied.
unsigned ApplyStartupLanguage();
}
