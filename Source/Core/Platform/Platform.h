//
// Created by Orgest on 6/8/2025.
//
#pragma once

namespace Platform
{
	enum class DisplayMode : u8
	{
		Windowed,
		BorderlessFullscreen,
		Fullscreen
	};

	struct DisplayState
	{
		u8 isBorderless : 1;
		u8 isExclusiveFullscreen : 1;
		u8 isFocused : 1;
		u8 isResized : 1;
		u8 isMinimized : 1;
		u8 isAudioPlaying : 1;
		u8 padding : 2;
	};

	struct OSVersion
	{
		u32 Major;
		u32 Minor;
		u32 Build;
	};

	using WindowHandle = void*;
	struct WindowContext
	{
		const char* platformName = nullptr;
		WindowHandle handle = nullptr; // Can be HWND or SDL_Window who knows
		OSVersion version = {};

		// Timing and perf counters
		u32 frameCount = 0;
		i64 perfCountFrequency = 0;
		f64 deltaTime = 0.0;
		f64 lastFrameTime = 0.0;
		f64 elapsedTime = 0.0;
		f64 accumulatedTime = 0.0;

		// FPS and frametime
		f32 fps = 0;
		f32 displayedFPS = 0.0f;
		f32 frameTime = 0.0f;
		f32 displayUpdateRate = 0.4f;

		// Window dimensions and positioning
		i32 windowWidth = 0;
		i32 windowHeight = 0;
		i32 monitorWidth = 0;
		i32 monitorHeight = 0;
		i32 positionX = 0;
		i32 positionY = 0;
		f32 dpiScale = 1.0f;

		// display state
		f32 outOfFocusFPS = 15.0f;
		DisplayState displayState{};
		DisplayMode displayMode{};

		template <typename T = f32>
		[[nodiscard]] T GetDeltaTime() const { return static_cast<T>(deltaTime); }

		template <typename T = f32>
		[[nodiscard]] T GetElapsedTime() const { return static_cast<T>(elapsedTime); }
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

	ORGAPI void IsMinPlatformRequirement();
	ORGAPI void Init(WindowContext* window, i32 width = 0, i32 height = 0, DisplayMode mode = DisplayMode::Windowed);
	ORGAPI void InitKeyMappings();
	ORGAPI void InitRawInput();
	ORGAPI std::string GetCPUName();
	ORGAPI bool GetWindowSize(const WindowHandle& handle, u32& width, u32& height);
	ORGAPI void SetDisplayMode(WindowContext& window, DisplayMode mode);
	ORGAPI void UpdateScreenDimensions(WindowContext& window);
	ORGAPI std::wstring ConvertToWideString(const std::string_view& str);
	ORGAPI std::string ConvertToString(const std::wstring_view& wstr);
	ORGAPI void* Allocate(size_t size);
	ORGAPI void* AllocateFromArena(void* arena, std::size_t size);
	ORGAPI void Free(void* ptr);
	ORGAPI WindowHandle GetNativeWindowHandle(const WindowContext& window);
	ORGAPI void StartFrame(WindowContext& window);
	ORGAPI void ShowWindow(const WindowContext& window);
	ORGAPI bool ProcessMessages(WindowContext* window = nullptr);
	ORGAPI bool ShowMessageBox(std::string_view message, std::string_view title = "Message", MessageBoxType type = MessageBoxType::Info);

	// Gamepad
	ORGAPI void UpdateGamepads();
	// Some utils that might work lmao
	ORGAPI bool IsMusicPlayerPlaying();
	ORGAPI BatteryState GetBatteryState();
	ORGAPI void CenterMouse(const WindowContext* window);
	ORGAPI void HideCursor(bool show);
	ORGAPI void LockCursor(WindowContext& wc, bool enable);
	ORGAPI bool SetMouseVisibility(bool show);
};


