//
// Created by Orgest on 6/8/2025.
//
#pragma once
#include <map>
#include <numeric>
#include <string>
#include "../PrimTypes.h"
#include "Tools/Vector.h"

namespace Platform
{
	enum class DisplayMode : u8
	{
		Windowed,
		BorderlessFullscreen,
		Fullscreen
	};

    struct VideoMode
    {
        u32 width;
        u32 height;
        u32 refreshRate;

        // For easy sorting in UI
        auto operator<=>(const VideoMode&) const = default;
    };

    struct AspectRatio
    {
        u32 x, y;

        // Custom comparator to sort 4:3 < 16:9 < 21:9
        bool operator<(const AspectRatio& other) const
        {
            return (x * other.y) < (other.x * y);
        }
    };

    struct MonitorInfo
    {
        std::string name;
        void* nativeHandle;
        bool isPrimary;
        VideoMode desktopMode;

        std::map<AspectRatio, Vector<VideoMode>> supportedModes;
    };

    struct DisplaySettings
    {
        Vector<MonitorInfo> monitors;
        u32 selectedMonitorIndex = 0;
        VideoMode activeMode;
    };

    struct DisplayState
	{
		u8 isBorderless : 1;
		u8 isExclusiveFullscreen : 1;
		u8 isFocused : 1;
        u8 isResized : 1;
        u8 isMinimized : 1;
        u8 isResizing : 1;
        u8 isMaximized : 1;
    };

    using WindowHandle = void*;
	struct WindowContext
	{
        const char* platformName = nullptr;
        WindowHandle handle = nullptr;
        DisplaySettings displaySettings;

        // Timing and perf counters (QueryPerformanceCounter-based)
		i64 perfCountFrequency = 0;
		f64 deltaTime = 0.0;
        f64 lastFrameTime = 0.0;
		f64 elapsedTime = 0.0;
        u64 totalFrames = 0;

        // FPS and frametime
		f32 fps = 0;
        i32 frameBufferIndex = 0;
        f32 frameTime = 0.0f;
        f32 frameTimeSum = 0.0f;
		f32 displayUpdateRate = 0.4f;
        f32 outOfFocusFPS = BACKGROUND_FPS;

        // Window dimensions and positioning
        i32 windowWidth = 0;
		i32 windowHeight = 0;
	    i32 activeMonitorIndex = 0;
		f32 dpiScale = 1.0f;

	    f32 frameTimeBuffer[60] = {};

		DisplayState displayState{};
		DisplayMode displayMode = DisplayMode::Windowed;

		template <typename T = f32>
		[[nodiscard]] T GetDeltaTime() const { return static_cast<T>(deltaTime); }

		template <typename T = f32>
		[[nodiscard]] T GetElapsedTime() const { return static_cast<T>(elapsedTime); }

		// we are prob not going to use reinterpret_cast here since the platform will be set on compile time
		template <typename T>
		T GetHandle()
		{
			return static_cast<T>(handle);
		}

		template <typename T>
		T GetHandle() const
		{
			return static_cast<const T>(handle);
        }
    };

    enum class MessageBoxType : u32
    {
        Info,
        Warning,
        Error,
        YesNo
    };

    struct BatteryState
    {
        i32 batteryPercentage = 0;
        u8 ACConnected : 1 = 0;
        u8 Charging : 1 = 0;
        u8 HasBattery : 1 = 0;
    };

    inline AspectRatio GetAspectRatio(const u32 width, const u32 height)
    {
        const double ratio = static_cast<double>(width) / height;

        static const std::pair<u32, u32> commonRatios[] = {
            {16, 9}, {9, 16}, {21, 9}, {9, 21}, {16, 10}, {10, 16}
        };

        for (const auto& [rw, rh] : commonRatios)
        {
            if (std::abs(ratio - static_cast<double>(rw) / rh) < 0.05)
                return { rw, rh };
        }

        // If it's truly weird, fall back to the raw math
        const u32 common = std::gcd(width, height);
        return {width / common, height / common };
    }

	ORGAPI void Init(WindowContext* window, i32 width = 0, i32 height = 0, DisplayMode mode = DisplayMode::Windowed);
    ORGAPI bool GetWindowSize(const WindowHandle& handle, u32& width, u32& height);
    ORGAPI void SetTitleBarText(const WindowContext& window, std::string_view text);
    ORGAPI void SetDisplayMode(WindowContext& window, DisplayMode mode);
	ORGAPI void RefreshMonitorData(WindowContext* window);
    ORGAPI void SetWindowResolution(WindowContext& window, u32 width, u32 height);
    ORGAPI void SetMonitorRefreshRate(WindowContext& window, u32 refreshRate);
    ORGAPI void MoveWindowToMonitor(WindowContext& window, i32 monitorIndex);
    ORGAPI void UpdateScreenDimensions(WindowContext& window);
    ORGAPI void StartFrame(WindowContext& window);
    ORGAPI void ShowWindow(const WindowContext& window);
    ORGAPI void MaximizeWindow(WindowContext& window, bool maximize);
    ORGAPI bool ProcessMessages(WindowContext* window = nullptr);

    ORGAPI BatteryState GetBatteryState();
    ORGAPI std::string GetCPUName();

    ORGAPI void* Allocate(size_t size);
    ORGAPI void* AllocateFromArena(void* arena, std::size_t size);
    ORGAPI void Free(void* ptr);
    ORGAPI bool ShowMessageBox(std::string_view message, std::string_view title = "Message",
                               MessageBoxType type = MessageBoxType::Info);

    ORGAPI std::wstring ConvertToWideString(std::string_view str);
    ORGAPI std::string ConvertToString(std::wstring_view wstr);

    ORGAPI void CenterMouse(const WindowContext* window);
    ORGAPI void SetCursorVisible(const WindowContext* window, bool show);
    ORGAPI void SetCursorLocked(const WindowContext* window, bool locked);
    ORGAPI bool GetCursorClientPos(const WindowContext* window, i32& outX, i32& outY);
    ORGAPI void SetCursorClientPos(const WindowContext* window, i32 x, i32 y);
    ORGAPI bool WrapCursorToOppositeEdge(const WindowContext* window, i32 margin = 10);
}
