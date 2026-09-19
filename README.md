# DR2C Internal ImGui Tool

This project is an in-process x86 overlay for `prog.exe`.

The first renderer path hooks `opengl32!wglSwapBuffers`, obtains the game window from the swap HDC, and renders Dear ImGui with the Win32/OpenGL3 backends.

Build with the 32-bit MinGW toolchain and place `DR2CInternalOverlay.dll` next to the existing injector's DLL path. The existing Qt project can inject this DLL during the experiment, but all future game data and commands should move into this project.