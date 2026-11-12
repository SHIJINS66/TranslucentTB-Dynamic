#include "taskbarattributeworker.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <member_thunk/member_thunk.hpp>
#include <set>
#include <tlhelp32.h>

#include "constants.hpp"
#include "../localization.hpp"
#include "../uwp/uwp.hpp"
#include "../../ProgramLog/error/win32.hpp"
#include "../../ProgramLog/error/winrt.hpp"
#include "undoc/explorer.hpp"
#include "undoc/user32.hpp"
#include "undoc/winuser.hpp"
#include "win32.hpp"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Foundation.Metadata.h"

class TaskbarAttributeWorker::AttributeRefresher {
private:
	TaskbarAttributeWorker &m_Worker;
	std::set<HMONITOR> m_ToRefresh;
	bool m_Armed;

public:
	AttributeRefresher(TaskbarAttributeWorker &worker, bool refresh = true) noexcept :
		m_Worker(worker), m_Armed(refresh) { }

	AttributeRefresher(const AttributeRefresher &) = delete;
	AttributeRefresher &operator =(const AttributeRefresher &) = delete;

	void refresh(taskbar_iterator it)
	{
		if (m_Armed)
		{
			m_ToRefresh.insert(it->first);
		}
	}

	void disarm() noexcept
	{
		m_Armed = false;
		m_ToRefresh.clear();
	}

	~AttributeRefresher() noexcept(false)
	{
		if (m_Armed)
		{
			for (const auto mon : m_ToRefresh)
			{
				if (const auto it = m_Worker.m_Taskbars.find(mon); it != m_Worker.m_Taskbars.end())
				{
					m_Worker.RefreshAttribute(it);
				}
			}
		}
	}
};

template<DWORD insert, DWORD remove>
void TaskbarAttributeWorker::WindowInsertRemove(DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
	if (event == insert)
	{
		RefreshDynamicTintAfterContentChange(hwnd);
	}
	if (const Window window(hwnd); idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
	{
		if (event == insert && window.valid())
		{
			InsertWindow(window, true);
		}
		else if (event == remove)
		{
			AttributeRefresher refresher(*this);
			for (auto it = m_Taskbars.begin(); it != m_Taskbars.end(); ++it)
			{
				RemoveWindow(window, it, refresher);
			}
		}
	}
}

void TaskbarAttributeWorker::OnAeroPeekEnterExit(DWORD event, HWND, LONG, LONG, DWORD, DWORD)
{
	m_PeekActive = event == EVENT_SYSTEM_PEEKSTART;
	MessagePrint(spdlog::level::debug, m_PeekActive ? L"Aero Peek entered" : L"Aero Peek exited");

	RefreshAllAttributes();
}

void TaskbarAttributeWorker::OnWindowStateChange(DWORD, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
	RefreshDynamicTintAfterContentChange(hwnd);
	if (const Window window(hwnd); idObject == OBJID_WINDOW && idChild == CHILDID_SELF && window.valid())
	{
		InsertWindow(window, true);
	}
}

void TaskbarAttributeWorker::OnWindowCreateDestroy(DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
	if (event == EVENT_OBJECT_CREATE)
	{
		RefreshDynamicTintAfterContentChange(hwnd);
	}
	if (const Window window(hwnd); idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
	{
		if (event == EVENT_OBJECT_CREATE && window.valid())
		{
			if (const auto className = window.classname(); className && (*className == TASKBAR || *className == SECONDARY_TASKBAR))
			{
				MessagePrint(spdlog::level::debug, L"A taskbar got created, refreshing...");
				ResetState();
			}
			else
			{
				InsertWindow(window, true);
			}
		}
		else if (event == EVENT_OBJECT_DESTROY)
		{
			// events are asynchronous, the window might be invalid already
			// important to not try to query its info here, just go off the handle
			AttributeRefresher refresher(*this);
			for (auto it = m_Taskbars.begin(); it != m_Taskbars.end(); ++it)
			{
				if (it->second.Taskbar.TaskbarWindow == window)
				{
					MessagePrint(spdlog::level::debug, L"A taskbar got destroyed, refreshing...");
					ResetState();

					// iterators invalid
					refresher.disarm();
					return;
				}

				RemoveWindow<LogWindowRemovalDestroyed>(window, it, refresher);
			}
		}
	}
}

void TaskbarAttributeWorker::OnForegroundWindowChange(DWORD, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
	if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
	{
		UpdateDynamicTint();
		const Window oldForegroundWindow = std::exchange(m_ForegroundWindow, Window(hwnd).valid() ? hwnd : Window::NullWindow);

		if (Error::ShouldLog<spdlog::level::debug>())
		{
			MessagePrint(spdlog::level::debug, std::format(L"Changed foreground window to {}", DumpWindow(m_ForegroundWindow)));
		}

		AttributeRefresher refresher(*this);
		HMONITOR oldMonitor = nullptr;
		if (oldForegroundWindow)
		{
			oldMonitor = oldForegroundWindow.monitor();
			if (const auto it = m_Taskbars.find(oldMonitor); it != m_Taskbars.end())
			{
				refresher.refresh(it);
			}
		}

		if (m_ForegroundWindow)
		{
			if (auto newMonitor = m_ForegroundWindow.monitor(); newMonitor != oldMonitor)
			{
				if (const auto it = m_Taskbars.find(newMonitor); it != m_Taskbars.end())
				{
					refresher.refresh(it);
				}
			}
		}
	}
}

void TaskbarAttributeWorker::OnWindowOrderChange(DWORD, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
	RefreshDynamicTintAfterContentChange(hwnd);
	if (const Window window(hwnd); idObject == OBJID_WINDOW && idChild == CHILDID_SELF && window.valid())
	{
		if (const auto iter = m_Taskbars.find(window.monitor()); iter != m_Taskbars.end())
		{
			RefreshAttribute(iter);
		}
	}
}

void TaskbarAttributeWorker::OnStartVisibilityChange(bool state)
{
	HMONITOR mon = nullptr;
	if (state)
	{
		mon = m_CurrentStartMonitor = GetStartMenuMonitor();

		if (Error::ShouldLog<spdlog::level::debug>())
		{
			MessagePrint(spdlog::level::debug, std::format(L"Start menu opened on monitor {}", static_cast<void *>(mon)));
		}
	}
	else
	{
		mon = std::exchange(m_CurrentStartMonitor, nullptr);

		MessagePrint(spdlog::level::debug, L"Start menu closed");
	}

	if (const auto iter = m_Taskbars.find(mon); iter != m_Taskbars.end())
	{
		RefreshAttribute(iter);
	}
}

void TaskbarAttributeWorker::OnTaskViewVisibilityChange(bool state)
{
	m_TaskViewActive = state;
	MessagePrint(spdlog::level::debug, m_TaskViewActive ? L"Task View opened" : L"Task View closed");

	RefreshAllAttributes();
}

void TaskbarAttributeWorker::OnSearchVisibilityChange(bool state)
{
	HMONITOR mon = nullptr;
	if (state)
	{
		mon = m_CurrentSearchMonitor = GetSearchMonitor();

		if (Error::ShouldLog<spdlog::level::debug>())
		{
			MessagePrint(spdlog::level::debug, std::format(L"Search opened on monitor {}", static_cast<void *>(mon)));
		}
	}
	else
	{
		mon = std::exchange(m_CurrentSearchMonitor, nullptr);

		MessagePrint(spdlog::level::debug, L"Search closed");
	}

	if (const auto iter = m_Taskbars.find(mon); iter != m_Taskbars.end())
	{
		RefreshAttribute(iter);
	}
}

void TaskbarAttributeWorker::OnFindInStartVisibilityChange(bool state)
{
	HMONITOR mon = nullptr;
	if (state)
	{
		mon = m_CurrentFindInStartMonitor = GetFindInStartMonitor();

		if (Error::ShouldLog<spdlog::level::debug>())
		{
			MessagePrint(spdlog::level::debug, std::format(L"Find in Start opened on monitor {}", static_cast<void*>(mon)));
		}
	}
	else
	{
		mon = std::exchange(m_CurrentFindInStartMonitor, nullptr);

		MessagePrint(spdlog::level::debug, L"Find in Start closed");
	}

	if (const auto iter = m_Taskbars.find(mon); iter != m_Taskbars.end())
	{
		RefreshAttribute(iter);
	}
}

void TaskbarAttributeWorker::OnForceRefreshTaskbar(Window taskbar)
{
	if (!m_TaskbarService)
	{
		m_disableAttributeRefreshReply = true;
		auto guard = wil::scope_exit([this]
		{
			m_disableAttributeRefreshReply = false;
		});

		taskbar.send_message(WM_DWMCOMPOSITIONCHANGED);
		guard.reset();
		taskbar.send_message(WM_DWMCOMPOSITIONCHANGED);
	}
}

LRESULT TaskbarAttributeWorker::OnSystemSettingsChange(UINT uiAction)
{
	if (uiAction == SPI_SETWORKAREA)
	{
		MessagePrint(spdlog::level::debug, L"Work area change detected, refreshing...");
		ResetState();
	}

	return 0;
}

LRESULT TaskbarAttributeWorker::OnPowerBroadcast(const POWERBROADCAST_SETTING *settings)
{
	if (settings && settings->PowerSetting == GUID_POWER_SAVING_STATUS && settings->DataLength == sizeof(DWORD))
	{
		m_PowerSaver = *reinterpret_cast<const DWORD *>(&settings->Data);
		RefreshAllAttributes();
	}

	return TRUE;
}

LRESULT TaskbarAttributeWorker::OnRequestAttributeRefresh(LPARAM lParam)
{
	if (!m_disableAttributeRefreshReply && !m_TaskbarService)
	{
		const Window window = reinterpret_cast<HWND>(lParam);
		if (const auto iter = m_Taskbars.find(window.monitor()); iter != m_Taskbars.end() && iter->second.Taskbar.TaskbarWindow == window)
		{
			if (const auto config = GetConfig(iter); config.Accent != ACCENT_NORMAL)
			{
				SetAttribute(iter, config);
				return 1;
			}
		}
	}

	return 0;
}

LRESULT TaskbarAttributeWorker::MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (m_DynamicStatsMessage && uMsg == m_DynamicStatsMessage)
	{
		switch (wParam)
		{
		case 0: return static_cast<LRESULT>(m_DynamicTicks);
		case 1: return static_cast<LRESULT>(m_DynamicCaptures);
		case 2: return static_cast<LRESULT>(m_DynamicWrites);
		case 3: return m_DynamicTickMs.load();
		case 4: return m_ConfigManager.GetConfig().DynamicTint.AdaptiveSampling;
		case 5: return static_cast<LRESULT>(m_DynamicInputs);
		case 6: return GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
		case 7: return static_cast<LRESULT>(DynamicTint::CaptureAllocationCount());
		case 8: return static_cast<LRESULT>(m_DynamicStops);
		default: return 0;
		}
	}
	if (uMsg == WM_DYNAMIC_INPUT)
	{
		m_InputPosted.store(false);
		if (m_InputContent.exchange(false))
		{
			++m_DynamicInputs;
			m_DynamicSchedule.Activity(GetTickCount64());
		}
		UpdateDynamicTint();
		return 0;
	}
	if (uMsg == WM_SETTINGCHANGE)
	{
		if (InSendMessage())
		{
			// post the message back to ourself to process outside of SendMessage
			post_message(uMsg, wParam, lParam);
			return 0;
		}
		else
		{
			return OnSystemSettingsChange(static_cast<UINT>(wParam));
		}
	}
	else if (uMsg == WM_DISPLAYCHANGE)
	{
		MessagePrint(spdlog::level::debug, L"Monitor configuration change detected, refreshing...");
		ResetState();
		return 0;
	}
	else if (uMsg == WM_POWERBROADCAST && wParam == PBT_POWERSETTINGCHANGE)
	{
		return OnPowerBroadcast(reinterpret_cast<const POWERBROADCAST_SETTING *>(lParam));
	}
	else if (uMsg == m_TaskbarCreatedMessage)
	{
		MessagePrint(spdlog::level::debug, L"Main taskbar got created, refreshing...");
		ResetState();
		return 0;
	}
	else if (uMsg == m_RefreshRequestedMessage)
	{
		return OnRequestAttributeRefresh(lParam);
	}
	else if (uMsg == m_TaskViewVisibilityChangeMessage)
	{
		OnTaskViewVisibilityChange(wParam);
		return 0;
	}
	else if (uMsg == m_StartVisibilityChangeMessage)
	{
		OnStartVisibilityChange(wParam);
		return 0;
	}
	else if (uMsg == m_SearchVisibilityChangeMessage)
	{
		OnSearchVisibilityChange(wParam);
		return 0;
	}
	else if (uMsg == m_FindInStartVisibilityChangeMessage)
	{
		OnFindInStartVisibilityChange(wParam);
		return 0;
	}
	else if (uMsg == m_ForceRefreshTaskbar)
	{
		OnForceRefreshTaskbar(reinterpret_cast<HWND>(lParam));
		return 0;
	}
	else if (uMsg == m_ApplyColorPreview)
	{
		txmp::TaskbarState state = static_cast<txmp::TaskbarState>(wParam);
		uint32_t rgba = static_cast<uint32_t>(lParam);
		Util::Color color = Util::Color::FromRGBA(rgba);
		try {
			ApplyColorPreview(state, color);
		} catch (const std::out_of_range&) {
			return 1;
		}
		return 0;
	}
	else if (uMsg == WM_TIMER && wParam == m_DynamicTimerId)
	{
		m_DynamicTickPosted.store(false);
		UpdateDynamicTint();
		return 0;
	}

	return MessageWindow::MessageHandler(uMsg, wParam, lParam);
}

TaskbarAppearance TaskbarAttributeWorker::GetConfig(taskbar_iterator taskbar) const
{
	const auto& config = m_ConfigManager.GetConfig();

	if (config.BatterySaverAppearance.Enabled && m_PowerSaver)
	{
		return WithPreview(txmp::TaskbarState::BatterySaver, config.BatterySaverAppearance);
	}

	if (config.TaskViewOpenedAppearance.Enabled && m_TaskViewActive)
	{
		return WithPreview(txmp::TaskbarState::TaskViewOpened, config.TaskViewOpenedAppearance);
	}

	// Task View is ignored by peek, so shall we
	if (m_PeekActive)
	{
		return WithPreview(txmp::TaskbarState::Desktop, config.DesktopAppearance);
	}

	// on windows 11, search is considered open when start is, so we need to check for start first.
	bool startOpened;
	if (m_IsWindows11 && (m_CurrentSearchMonitor != nullptr || m_CurrentFindInStartMonitor != nullptr))
	{
		// checking the search monitor is more reliable on windows 11 (if available)
		// so check the start monitor to see if it's open and then use the search monitor
		// to check *where* it's open.
		startOpened = m_CurrentStartMonitor != nullptr && (m_CurrentSearchMonitor == taskbar->first || m_CurrentFindInStartMonitor == taskbar->first);
	}
	else
	{
		startOpened = m_CurrentStartMonitor == taskbar->first;
	}

	if (config.StartOpenedAppearance.Enabled && startOpened)
	{
		return WithPreview(txmp::TaskbarState::StartOpened, config.StartOpenedAppearance);
	}

	if (config.SearchOpenedAppearance.Enabled && !startOpened && (m_CurrentSearchMonitor == taskbar->first || m_CurrentFindInStartMonitor == taskbar->first))
	{
		return WithPreview(txmp::TaskbarState::SearchOpened, config.SearchOpenedAppearance);
	}

	auto &maximisedWindows = taskbar->second.MaximisedWindows;
	if (config.MaximisedWindowAppearance.Enabled && !maximisedWindows.empty())
	{
		if (config.MaximisedWindowAppearance.HasRules())
		{
			for (const Window wnd : Window::DesktopWindow().get_ordered_childrens())
			{
				// find the highest maximized window in the z-order.
				if (maximisedWindows.contains(wnd))
				{
					if (const auto rule = config.MaximisedWindowAppearance.FindRule(wnd))
					{
						// if it has a rule, use that rule
						return *rule;
					}
					else
					{
						// we only consider the highest z-order maximized window for rules
						// so stop looking through the z-order
						break;
					}
				}
			}
		}

		// otherwise, use the normal maximized state
		return WithPreview(txmp::TaskbarState::MaximisedWindow, config.MaximisedWindowAppearance);
	}

	if (config.VisibleWindowAppearance.Enabled && (!maximisedWindows.empty() || !taskbar->second.NormalWindows.empty()))
	{
		// if there is no maximized window, and the foreground window is on the current monitor
		if (config.VisibleWindowAppearance.HasRules() && maximisedWindows.empty() && m_ForegroundWindow.monitor() == taskbar->first)
		{
			// find a rule for the foreground window
			if (const auto rule = config.VisibleWindowAppearance.FindRule(m_ForegroundWindow))
			{
				// if it has a rule, use that rule
				return *rule;
			}
		}

		// otherwise use normal visible state
		return WithPreview(txmp::TaskbarState::VisibleWindow, config.VisibleWindowAppearance);
	}

	return WithPreview(txmp::TaskbarState::Desktop, config.DesktopAppearance);
}

void TaskbarAttributeWorker::ShowAeroPeekButton(const TaskbarInfo &taskbar, bool show)
{
	if (const auto style = taskbar.PeekWindow.get_long_ptr(GWL_EXSTYLE))
	{
		const bool success = SetNewWindowExStyle(taskbar.PeekWindow, *style, show
			? (*style & ~WS_EX_LAYERED)
			: (*style | WS_EX_LAYERED));

		if (!show && success)
		{
			// Non-zero alpha makes the button still interactible, even if practically invisible.
			if (!SetLayeredWindowAttributes(taskbar.PeekWindow, 0, 1, LWA_ALPHA))
			{
				LastErrorHandle(spdlog::level::warn, L"Failed to set peek button layered attributes");
			}
		}
	}
}

void TaskbarAttributeWorker::ShowTaskbarLine(const TaskbarInfo &taskbar, bool show)
{
	if (auto workerW = taskbar.WorkerWWindow)
	{
		workerW.show(show ? SW_SHOWNA : SW_HIDE);
	}
	else if (taskbar.InnerXamlContent)
	{
		if (show)
		{
			if (auto rect = taskbar.InnerXamlContent.client_rect())
			{
				const float scaleFactor = static_cast<float>(GetDpiForWindow(taskbar.InnerXamlContent)) / USER_DEFAULT_SCREEN_DPI;

				// bottom taskbar is the only available option in Windows 11 22000.
				rect->top += std::lround(1.0f * scaleFactor);
				if (wil::unique_hrgn rgn { CreateRectRgnIndirect(&*rect) })
				{
					if (SetWindowRgn(taskbar.InnerXamlContent, rgn.get(), true))
					{
						rgn.release();
					}
					else
					{
						LastErrorHandle(spdlog::level::warn, L"Failed to set region of inner XAML window");
					}
				}
			}
		}
		else
		{
			if (!SetWindowRgn(taskbar.InnerXamlContent, nullptr, true)) [[unlikely]]
			{
				LastErrorHandle(spdlog::level::info, L"Failed to clear window region of inner taskbar XAML");
			}
		}
	}
}

void TaskbarAttributeWorker::SetAttribute(taskbar_iterator taskbar, TaskbarAppearance config)
{
	if (m_DynamicRunning)
	{
		const auto monitor = GetTaskbarMonitor(taskbar->second.Taskbar.TaskbarWindow);
		if (const auto it = m_DynamicColors.find(monitor); it != m_DynamicColors.end())
		{
			if (it->second.size() >= 2 && m_TaskbarService)
			{
				// Any state refresh (Start menu, a taskbar click, Win key...) would
				// otherwise repaint the configured colour for a frame, which shows
				// as the bar flashing back to its old look. Re-push the current
				// gradient instead so the tint never drops out.
				const UINT count = static_cast<UINT>(it->second.size());
				const auto alpha = m_ConfigManager.GetConfig().DynamicTint.Alpha;
				std::vector<UINT> abgr(count);
				std::vector<FLOAT> offsets(count);
				for (UINT i = 0; i < count; ++i)
				{
					abgr[i] = Util::Color { it->second[i].R, it->second[i].G, it->second[i].B, alpha }.ToABGR();
					offsets[i] = static_cast<FLOAT>(i) / static_cast<FLOAT>(count - 1);
				}
				m_TaskbarService->SetTaskbarGradient(taskbar->second.Taskbar.TaskbarWindow, count, abgr.data(), offsets.data());
				return;
			}

			if (it->second.size() == 1)
			{
				const auto alpha = m_ConfigManager.GetConfig().DynamicTint.Alpha;
				config.Accent = ACCENT_ENABLE_ACRYLICBLURBEHIND;
				config.Color = Util::Color { it->second[0].R, it->second[0].G, it->second[0].B, alpha };
			}
		}
	}

	if (m_TaskbarService)
	{
		if (config.Accent == ACCENT_NORMAL)
		{
			HresultVerify(m_TaskbarService->ReturnTaskbarToDefaultAppearance(taskbar->second.Taskbar.TaskbarWindow), spdlog::level::info, L"Failed to restore taskbar to normal");
		}
		else if (config.Accent == ACCENT_ENABLE_BLURBEHIND)
		{
			HresultVerify(m_TaskbarService->SetTaskbarBlur(taskbar->second.Taskbar.TaskbarWindow, config.Color.ToABGR(), config.BlurRadius / 3), spdlog::level::info, L"Failed to set taskbar brush");
		}
		else
		{
			auto color = config.Color;
			TaskbarBrush brush = SolidColor;

			if (config.Accent == ACCENT_ENABLE_ACRYLICBLURBEHIND)
			{
				brush = Acrylic;
			}
			else if (config.Accent == ACCENT_ENABLE_GRADIENT)
			{
				color.A = 0xFF;
			}

			HresultVerify(m_TaskbarService->SetTaskbarAppearance(taskbar->second.Taskbar.TaskbarWindow, brush, color.ToABGR()), spdlog::level::info, L"Failed to set taskbar brush");
		}
	}
	else
	{
		const auto window = taskbar->second.Taskbar.TaskbarWindow;

		if (config.Accent != ACCENT_NORMAL)
		{
			m_NormalTaskbars.erase(window);

			const bool isAcrylic = config.Accent == ACCENT_ENABLE_ACRYLICBLURBEHIND;
			if (isAcrylic && config.Color.A == 0)
			{
				// Acrylic mode doesn't likes a completely 0 opacity
				config.Color.A = 1;
			}

			ACCENT_POLICY policy = {
				config.Accent,
				static_cast<UINT>(isAcrylic ? 0 : 2),
				config.Color.ToABGR(),
				0
			};

			const WINDOWCOMPOSITIONATTRIBDATA data = {
				WCA_ACCENT_POLICY,
				&policy,
				sizeof(policy)
			};

			if (!SetWindowCompositionAttribute(window, &data)) [[unlikely]]
			{
				LastErrorHandle(spdlog::level::info, L"Failed to set window composition attribute");
			}
		}
		else if (const auto [it, inserted] = m_NormalTaskbars.insert(window); inserted)
		{
			// If this is in response to a window being moved, we send the message way too often
			// and Explorer doesn't like that too much.
			window.send_message(WM_DWMCOMPOSITIONCHANGED, 1, 0);
		}
	}
}

void TaskbarAttributeWorker::RefreshAttribute(taskbar_iterator taskbar)
{
	// These functions may trigger Windows internal message loops,
	// do not pass any member of taskbar map by reference.
	// See comment in InsertWindow.
	const auto taskbarInfo = taskbar->second.Taskbar;

	const auto &cfg = GetConfig(taskbar);
	SetAttribute(taskbar, cfg);

	if (m_TaskbarService)
	{
		HresultVerify(m_TaskbarService->SetTaskbarBorderVisibility(taskbarInfo.TaskbarWindow, cfg.ShowLine), spdlog::level::info, L"Failed to set taskbar border visibility");
	}
	else if (taskbarInfo.InnerXamlContent || taskbarInfo.WorkerWWindow)
	{
		ShowTaskbarLine(taskbarInfo, cfg.ShowLine);
	}
	else if (taskbarInfo.PeekWindow)
	{
		// Ignore changes when peek is active
		if (!m_PeekActive)
		{
			ShowAeroPeekButton(taskbarInfo, cfg.ShowPeek);
		}
	}
}

void TaskbarAttributeWorker::RefreshAllAttributes()
{
	AttributeRefresher refresher(*this);
	for (auto it = m_Taskbars.begin(); it != m_Taskbars.end(); ++it)
	{
		refresher.refresh(it);
	}
}

bool TaskbarAttributeWorker::IsFullscreenOnMonitor(HMONITOR monitor) const
{
	MONITORINFO monitorInfo { sizeof(monitorInfo) };
	if (!GetMonitorInfo(monitor, &monitorInfo)) [[unlikely]]
	{
		return false;
	}

	const Window foreground = Window::ForegroundWindow();
	if (!foreground.valid() || foreground.handle() == DesktopWindow() || foreground.handle() == ShellWindow() || foreground.handle() == m_WindowHandle)
	{
		return false;
	}

	if (!foreground.visible() || foreground.cloaked())
	{
		return false;
	}

	const auto rect = foreground.rect();
	if (!rect) [[unlikely]]
	{
		return false;
	}

	static constexpr LONG TOLERANCE = 2;
	const RECT &screen = monitorInfo.rcMonitor;
	return rect->left <= screen.left + TOLERANCE && rect->top <= screen.top + TOLERANCE &&
		rect->right >= screen.right - TOLERANCE && rect->bottom >= screen.bottom - TOLERANCE;
}

namespace {
	// True while the cursor sits on the taskbar itself or on one of its flyouts
	// (thumbnail previews, jump lists...). Those surfaces overlap the sampled
	// band, so sampling them would feed window thumbnails into the tint.
	bool IsShellUiUnderCursor() noexcept
	{
		POINT point { };
		if (!GetCursorPos(&point))
		{
			return false;
		}

		const HWND under = WindowFromPoint(point);
		if (!under)
		{
			return false;
		}

		wchar_t class_name[64] { };
		const HWND root = GetAncestor(under, GA_ROOT);
		if (!GetClassNameW(root ? root : under, class_name, static_cast<int>(sizeof(class_name) / sizeof(class_name[0]))))
		{
			return false;
		}

		// The taskbar itself (primary or secondary), or anything inside it.
		if (wcscmp(class_name, L"Shell_TrayWnd") == 0 || wcscmp(class_name, L"Shell_SecondaryTrayWnd") == 0)
		{
			return true;
		}

		// Flyouts such as thumbnail previews and jump lists are topmost tool
		// windows owned by explorer. A normal Explorer window (CabinetWClass)
		// is not a tool window and must never freeze sampling - hovering the
		// file manager is ordinary usage, not shell UI.
		if ((GetWindowLongPtrW(under, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0)
		{
			DWORD pid = 0;
			GetWindowThreadProcessId(under, &pid);

			const HWND shell = GetShellWindow();
			DWORD shellPid = 0;
			if (shell)
			{
				GetWindowThreadProcessId(shell, &shellPid);
			}

			return shellPid != 0 && pid == shellPid;
		}

		return false;
	}

	Util::Color MedianColor(const std::deque<std::vector<Util::Color>> &history, std::size_t index) noexcept
	{
		std::array<BYTE, 9> reds { }, greens { }, blues { };
		const std::size_t count = std::min<std::size_t>(history.size(), reds.size());
		if (count == 0)
		{
			return { 0, 0, 0, 255 };
		}

		for (std::size_t i = 0; i < count; ++i)
		{
			const auto &color = history[i][index];
			reds[i] = color.R;
			greens[i] = color.G;
			blues[i] = color.B;
		}

		auto medianOf = [count](std::array<BYTE, 9> &values) noexcept
		{
			std::sort(values.begin(), values.begin() + count);
			return values[count / 2];
		};

		return { medianOf(reds), medianOf(greens), medianOf(blues), 255 };
	}
}

void TaskbarAttributeWorker::RefreshDynamicTintAfterContentChange(HWND hwnd)
{
	if (!hwnd || !m_ConfigManager.GetConfig().DynamicTint.Enabled) return;
	const HWND foreground = GetForegroundWindow();
	if (GetAncestor(hwnd, GA_ROOT) != foreground) return;
	const auto name = Window(foreground).classname();
	if (!name || *name == TASKBAR || *name == SECONDARY_TASKBAR || *name == L"TaskListThumbnailWnd") return;
	const auto now = GetTickCount64();
	m_DynamicSchedule.Activity(now);
	if (foreground == m_HoverStartupWindow && now <= m_HoverStartupUntil)
	{
		m_HoverRefreshPending = true;
		m_HoverRefreshAfter = std::min(now + 120, m_HoverStartupUntil);
	}
	ScheduleDynamicTick(1);
}

void TaskbarAttributeWorker::UpdateDynamicTint()
{
	const DynamicTintConfig &tint = m_ConfigManager.GetConfig().DynamicTint;
	static constexpr UINT IDLE_INTERVAL = 500;
	++m_DynamicTicks;
	const bool monitorInput = tint.Enabled && tint.AdaptiveSampling;
	if (m_InputMonitoring.exchange(monitorInput) != monitorInput && m_DynamicTickWakeEvent) SetEvent(m_DynamicTickWakeEvent);

	if (!tint.Enabled || m_Taskbars.empty())
	{
		if (m_DynamicRunning || m_ShellFrozen || m_DynamicPreviewMonitor)
		{
			StopDynamicTint(L"dynamic tint disabled");
		}
		m_HoverTracking.store(false);
		ScheduleDynamicTick(INFINITE);
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	const ULONGLONG tickNow = GetTickCount64();
	const UINT sampleMs = tint.SampleInterval > 0 ? static_cast<UINT>(tint.SampleInterval)
		: std::max<UINT>(1, 1000u / static_cast<UINT>(std::clamp(tint.SamplesPerSecond, 1, 60)));
	const UINT frameMs = std::max<UINT>(1, 1000u / static_cast<UINT>(std::clamp(tint.Fps, 20, 120)));
	bool animationNeeded = false, samplingBlocked = false;
	auto schedule = wil::scope_exit([&]
	{
		unsigned delay = frameMs;
		if (tint.AdaptiveSampling)
		{
			delay = m_DynamicSchedule.Delay(GetTickCount64(), sampleMs, true);
			if (animationNeeded) delay = std::min(delay, frameMs);
			if (samplingBlocked) delay = animationNeeded ? frameMs : 80;
		}
		ScheduleDynamicTick(std::max(1u, delay));
	});
	const HWND foreground = GetForegroundWindow();
	const bool foregroundChanged = foreground && foreground != m_LastDynamicForeground;
	if (foreground)
	{
		m_LastDynamicForeground = foreground;
	}
	if (foregroundChanged)
	{
		// A selected application is not a thumbnail hover, even if the cursor
		// is still in the first 320 pixels of its content. Do not rearm until
		// the cursor leaves the bar and starts another hover gesture.
		m_DynamicPreviewMonitor = nullptr;
		m_DynamicPreviewCancelled = true;
		m_ShellFastResume = true;
		m_ShellDismissUntil = std::min(m_ShellDismissUntil, tickNow + 80);
		// Keep the displayed colours as the new fade's origin, but discard
		// the previous application's targets, median history and transition.
		m_DynamicTargets.clear();
		m_DynamicHistory.clear();
		m_DynamicTransitions.clear();
		m_LastDynamicSample = { };
		m_HoverRefreshPending = true;
		m_HoverRefreshAfter = tickNow + 250;
		m_HoverAnimating = false;
		m_HoverStartupWindow = nullptr;
		m_HoverStartupUntil = 0;
		m_DynamicSchedule.Startup(tickNow);
		if (const auto className = Window(foreground).classname(); className && *className != TASKBAR && *className != SECONDARY_TASKBAR && *className != L"TaskListThumbnailWnd")
		{
			m_HoverStartupWindow = foreground;
			m_HoverStartupUntil = tickNow + 6000;
		}
	}
	// Search/task-view buttons can belong to other processes, and thumbnail
	// islands may be hit-test transparent. Protect the taskbar rectangle and
	// its preview neighbourhood without relying exclusively on window classes.
	bool cursorOnShell = false;
	bool cursorOnBar = false;
	POINT cursorPoint { };
	if (GetCursorPos(&cursorPoint))
	{
		static constexpr LONG PREVIEW_REACH = 320;
		bool nearArmedBar = false;
		for (const auto &[monitor, info] : m_Taskbars)
		{
			const auto rect = info.Taskbar.TaskbarWindow.rect();
			if (!rect)
			{
				continue;
			}

			if (cursorPoint.x >= rect->left && cursorPoint.x < rect->right &&
				cursorPoint.y >= rect->top && cursorPoint.y < rect->bottom)
			{
				cursorOnShell = true;
				cursorOnBar = true;
				if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
					(GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0)
				{
					m_DynamicPreviewCancelled = true;
					m_ShellFastResume = true;
				}
				m_DynamicPreviewMonitor = m_DynamicPreviewCancelled ? nullptr : monitor;
				break;
			}

			if (m_DynamicPreviewMonitor == monitor &&
				cursorPoint.x >= rect->left - PREVIEW_REACH && cursorPoint.x < rect->right + PREVIEW_REACH &&
				cursorPoint.y >= rect->top - PREVIEW_REACH && cursorPoint.y < rect->bottom + PREVIEW_REACH)
			{
				nearArmedBar = true;
			}
		}
		if (!cursorOnShell)
		{
			m_HoverRefreshPending = false;
			cursorOnShell = nearArmedBar;
			m_DynamicPreviewCancelled = false;
			if (!nearArmedBar)
			{
				m_DynamicPreviewMonitor = nullptr;
			}
		}
	}
	else
	{
		// A failed cursor query must not release an existing freeze.
		cursorOnShell = m_ShellFrozen;
	}

	const bool shellHover = cursorOnShell || IsShellUiUnderCursor();
	m_HoverTracking.store(cursorOnShell);
	const bool refreshHovered = cursorOnBar && m_HoverRefreshPending && tickNow >= m_HoverRefreshAfter;
	samplingBlocked = shellHover;
	const bool applyHovered = cursorOnBar && (refreshHovered || m_HoverAnimating);
	if (!shellHover)
	{
		m_HoverAnimating = false;
	}
	if (shellHover && !applyHovered)
	{
		// Let the flyout's closing animation finish before sampling its pixels.
		// Application selection needs only a short compositor settling time;
		// an ordinary thumbnail dismissal keeps the existing closing grace.
		m_ShellDismissUntil = tickNow + (m_ShellFastResume ? 80 : 250);
	}
	if (!applyHovered && (shellHover || tickNow < m_ShellDismissUntil))
	{
		if (!m_ShellFrozen)
		{
			m_ShellFrozen = true;
				m_ShellFreezeStart = now;
		}
		// Pause the entire pipeline: sampling, transitions and gradient pushes.
		return;
	}
	if (!shellHover) samplingBlocked = false;
	if (m_ShellFrozen)
	{
		const auto frozenFor = now - m_ShellFreezeStart;
		for (auto &[monitor, transitions] : m_DynamicTransitions)
		{
			for (auto &transition : transitions)
			{
				if (transition.Active)
				{
					transition.Start += frozenFor;
				}
			}
		}
		m_ShellFrozen = false;
	}
	m_ShellFastResume = false;
	const bool sampleDue = (!shellHover || refreshHovered) &&
		(refreshHovered || !m_DynamicRunning || m_DynamicSchedule.Due(tickNow, sampleMs, tint.AdaptiveSampling));

	if (sampleDue)
	{
		m_LastDynamicSample = now;
		bool capturedChange = false;

		std::unordered_map<HMONITOR, std::vector<Util::Color>> targets;

		for (const auto &[monitor, info] : m_Taskbars)
		{
			if (tint.OnlyFullscreen && !IsFullscreenOnMonitor(monitor))
			{
				continue;
			}

			MONITORINFO monitorInfo { sizeof(monitorInfo) };
			if (!GetMonitorInfo(monitor, &monitorInfo)) [[unlikely]]
			{
				continue;
			}

			const auto taskbarRect = info.Taskbar.TaskbarWindow.rect();
			if (!taskbarRect) [[unlikely]]
			{
				continue;
			}

			const auto band = DynamicTint::ComputeSampleBand(*taskbarRect, monitorInfo.rcMonitor, tint.SampleDepth);
			if (!band)
			{
				continue;
			}


			std::vector<Util::Color> sampled;
			if (tint.Zones > 1 && m_TaskbarService)
			{
				sampled = DynamicTint::SampleBandZones(*band, tint.Zones, 12, tint.NearBias);
			}
			else if (const auto single = DynamicTint::SampleBand(*band, tint.SampleWidth, 12, tint.NearBias); single)
			{
				sampled.push_back(*single);
			}

			if (sampled.empty())
			{
				continue;
			}

			for (auto &color : sampled)
			{
				color = DynamicTint::Adjust(color, tint);
			}

			++m_DynamicCaptures;
			const auto oldTarget = m_DynamicTargets.find(monitor);
			if (oldTarget == m_DynamicTargets.end() || oldTarget->second.size() != sampled.size()) capturedChange = true;
			else for (std::size_t i = 0; i < sampled.size(); ++i)
				capturedChange = capturedChange || DynamicTint::ColorDelta(oldTarget->second[i], sampled[i]) > tint.Deadband;

			// A large jump against the previous target means the scene changed:
			// keeping the old window would smear the previous scene over the new
			// one, so restart it with this sample (no median lag on cuts).
			bool cut = false;
			if (const auto previous = m_DynamicTargets.find(monitor); previous != m_DynamicTargets.end() && previous->second.size() == sampled.size())
			{
				for (std::size_t i = 0; i < sampled.size(); ++i)
				{
					if (DynamicTint::ColorDelta(previous->second[i], sampled[i]) >= tint.CutThreshold)
					{
						cut = true;
						break;
					}
				}
			}

			auto &history = m_DynamicHistory[monitor];
			if (cut || refreshHovered)
			{
				history.clear();
			}
			history.push_back(sampled);
			while (history.size() > static_cast<std::size_t>(tint.MedianWindow))
			{
				history.pop_front();
			}

			std::vector<Util::Color> filtered(sampled.size());
			for (std::size_t i = 0; i < filtered.size(); ++i)
			{
				filtered[i] = MedianColor(history, i);
			}


			targets[monitor] = std::move(filtered);
		}

		m_DynamicSchedule.Sampled(tickNow, capturedChange);
		if (refreshHovered)
		{
			m_HoverRefreshPending = false;
			m_HoverAnimating = !targets.empty();
		}

		for (auto it = m_DynamicTargets.begin(); it != m_DynamicTargets.end();)
		{
			if (!targets.contains(it->first))
			{
				m_DynamicHistory.erase(it->first);
				it = m_DynamicTargets.erase(it);
			}
			else
			{
				++it;
			}
		}

		for (auto &[monitor, colors] : targets)
		{
			m_DynamicTargets[monitor] = colors;
		}
	}

	if (m_DynamicTargets.empty())
	{
		// A foreground change invalidates targets before the adaptive deadline
		// permits another capture. Keep the displayed colours and cached brush
		// through that gap; an empty pending target is not a request to restore
		// every taskbar to its configured (often transparent) appearance.
		if (!sampleDue)
		{
			return;
		}
		if (m_DynamicRunning)
		{
			StopDynamicTint(L"no fullscreen content to sample");
		}
		else
		{
			m_DynamicTickMs.store(IDLE_INTERVAL, std::memory_order_relaxed);
		}
		return;
	}

	// Animate every tick towards the (median-filtered) target.
	bool changed = !m_DynamicRunning;
	bool hoverTransitionActive = false;
	for (auto &[monitor, target] : m_DynamicTargets)
	{
		auto &current = m_DynamicColors[monitor];
		if (current.size() != target.size())
		{
			current = target;
			m_DynamicTransitions.erase(monitor);
			changed = true;
			continue;
		}

		auto &transitions = m_DynamicTransitions[monitor];
		transitions.resize(target.size());

		// If any zone makes a scene-sized jump, the whole bar eases together:
		// zones whose colours barely move would otherwise crawl there on the
		// slow exponential path while their neighbours finish in 240 ms, which
		// reads as the bar tearing apart mid-transition.
		int maxDelta = 0;
		for (std::size_t i = 0; i < target.size(); ++i)
		{
			maxDelta = std::max(maxDelta, DynamicTint::ColorDelta(current[i], target[i]));
		}
		const bool cut = maxDelta >= tint.CutThreshold;

		for (std::size_t i = 0; i < target.size(); ++i)
		{
			ZoneTransition &transition = transitions[i];

			// A large jump starts a fixed-duration eased transition: it advances
			// evenly, finishes in cut_duration_ms, and lands exactly on target.
			// Small in-scene changes keep the slow exponential damping instead.
			if (!transition.Active && (cut || DynamicTint::ColorDelta(current[i], target[i]) >= tint.CutThreshold))
			{
				transition.From = current[i];
				transition.To = target[i];
				transition.Start = now;
				transition.DurationMs = tint.CutDurationMs;
				transition.Active = true;
			}

			Util::Color next{ };
			if (transition.Active)
			{
				// Follow the latest sample while the cut fades in.
				transition.To = target[i];
				const double elapsedMs = std::chrono::duration<double, std::milli>(now - transition.Start).count();
				const double t = std::clamp(elapsedMs / transition.DurationMs, 0.0, 1.0);
				const double eased = t * t * (3.0 - 2.0 * t);
				next = (t >= 1.0) ? transition.To : DynamicTint::Blend(transition.From, transition.To, static_cast<float>(eased));
				if (t >= 1.0)
				{
					transition.Active = false;
				}
			}
			else
			{
				next = DynamicTint::Blend(current[i], target[i], tint.Smoothing);
				if (DynamicTint::ColorDelta(next, target[i]) <= tint.Deadband)
				{
					// Snap instead of dragging an exponential tail.
					next = target[i];
				}
			}

			if (DynamicTint::ColorDelta(current[i], next) > 0)
			{
				changed = true;
			}
			current[i] = next;
			hoverTransitionActive = hoverTransitionActive || transition.Active ||
				DynamicTint::ColorDelta(current[i], target[i]) > 0;
		}
	}

	for (auto it = m_DynamicColors.begin(); it != m_DynamicColors.end();)
	{
		if (!m_DynamicTargets.contains(it->first))
		{
			it = m_DynamicColors.erase(it);
			changed = true;
		}
		else
		{
			++it;
		}
	}

	animationNeeded = hoverTransitionActive;
	m_DynamicRunning = true;
	if (!hoverTransitionActive)
	{
		m_HoverAnimating = false;
	}

	if (!changed)
	{
		return;
	}

	const bool gradient = tint.Zones > 1 && m_TaskbarService;
	if (gradient)
	{
		for (const auto &[monitor, colors] : m_DynamicColors)
		{
			if (colors.size() < 2)
			{
				continue;
			}

			const auto it = m_Taskbars.find(monitor);
			if (it == m_Taskbars.end())
			{
				continue;
			}

			const UINT count = static_cast<UINT>(colors.size());
			std::vector<UINT> abgr(count);
			std::vector<FLOAT> offsets(count);
			for (UINT i = 0; i < count; ++i)
			{
				abgr[i] = Util::Color { colors[i].R, colors[i].G, colors[i].B, tint.Alpha }.ToABGR();
				offsets[i] = static_cast<FLOAT>(i) / static_cast<FLOAT>(count - 1);
			}


			++m_DynamicWrites;
			HresultVerify(m_TaskbarService->SetTaskbarGradient(it->second.Taskbar.TaskbarWindow, count, abgr.data(), offsets.data()), spdlog::level::info, L"Failed to set taskbar gradient");
		}
	}
	else
	{
		for (const auto &[monitor, colors] : m_DynamicColors)
		{
			if (colors.empty())
			{
				continue;
			}

			if (const auto it = m_Taskbars.find(monitor); it != m_Taskbars.end())
			{
				SetAttribute(it, GetConfig(it));
			}
		}
	}
}

void TaskbarAttributeWorker::StopDynamicTint(std::wstring_view reason)
{
	if (Error::ShouldLog<spdlog::level::debug>())
	{
		MessagePrint(spdlog::level::debug, std::format(L"Stopping dynamic tint: {}", reason));
	}

	const bool wasRunning = m_DynamicRunning;
	if (wasRunning) ++m_DynamicStops;
	m_DynamicRunning = false;
	m_ShellFrozen = false;
	m_ShellFreezeStart = { };
	m_ShellDismissUntil = 0;
	m_DynamicPreviewMonitor = nullptr;
	m_DynamicPreviewCancelled = false;
	m_ShellFastResume = false;
	m_LastDynamicForeground = nullptr;
	m_HoverRefreshPending = false;
	m_HoverRefreshAfter = 0;
	m_HoverAnimating = false;
	m_HoverStartupWindow = nullptr;
	m_HoverStartupUntil = 0;
	m_DynamicColors.clear();
	m_DynamicTargets.clear();
	m_DynamicHistory.clear();
	m_DynamicTransitions.clear();

	m_DynamicTickMs.store(500, std::memory_order_relaxed);

	if (wasRunning)
	{
		RefreshAllAttributes();
	}
}

void TaskbarAttributeWorker::LogWindowInsertion(const std::pair<std::unordered_set<Window>::iterator, bool> &result, std::wstring_view state, HMONITOR mon)
{
	if (result.second && Error::ShouldLog<spdlog::level::debug>())
	{
		MessagePrint(spdlog::level::debug, std::format(L"Inserting {} window {} to monitor {}", state, DumpWindow(*result.first), static_cast<void *>(mon)));
	}
}

void TaskbarAttributeWorker::LogWindowRemoval(std::wstring_view state, Window window, HMONITOR mon)
{
	if (Error::ShouldLog<spdlog::level::debug>())
	{
		MessagePrint(spdlog::level::debug, std::format(L"Removing {} window {} from monitor {}", state, DumpWindow(window), static_cast<void *>(mon)));
	}
}

void TaskbarAttributeWorker::LogWindowRemovalDestroyed(std::wstring_view state, Window window, HMONITOR mon)
{
	if (Error::ShouldLog<spdlog::level::debug>())
	{
		MessagePrint(spdlog::level::debug, std::format(L"Removing {} window {} [window destroyed] from monitor {}", state, static_cast<void *>(window.handle()), static_cast<void *>(mon)));
	}
}

void TaskbarAttributeWorker::InsertWindow(Window window, bool refresh)
{
	if (window.classname() == CORE_WINDOW) [[unlikely]]
	{
		// Windows.UI.Core.CoreWindow is always shell UI stuff
		// that we either have a dynamic mode for or should ignore.
		// so just skip it.
		return;
	}

	AttributeRefresher refresher(*this, refresh);

	// Note: The checks are done before iterating because
	// some methods (most notably Window::on_current_desktop)
	// will trigger a Windows internal message loop,
	// which pumps messages to the worker. When the DPI is
	// changing, it means m_Taskbars is cleared while we still
	// have an iterator to it. Acquiring the iterator after the
	// call to on_current_desktop resolves this issue.
	const bool windowMatches = window.is_user_window() && !m_ConfigManager.GetConfig().IgnoredWindows.IsFiltered(window);
	const bool isMaximised = window.maximised();
	const bool isMinimised = window.minimised();
	const HMONITOR mon = window.monitor();

	for (auto it = m_Taskbars.begin(); it != m_Taskbars.end(); ++it)
	{
		auto &maximised = it->second.MaximisedWindows;
		auto &normal = it->second.NormalWindows;

		if (it->first == mon)
		{
			if (windowMatches && isMaximised)
			{
				if (normal.erase(window) > 0)
				{
					LogWindowRemoval(L"normal", window, mon);
				}

				LogWindowInsertion(maximised.insert(window), L"maximised", mon);

				refresher.refresh(it);
				continue;
			}
			else if (windowMatches && !isMinimised)
			{
				if (maximised.erase(window) > 0)
				{
					LogWindowRemoval(L"maximised", window, mon);
				}

				LogWindowInsertion(normal.insert(window), L"normal", mon);

				refresher.refresh(it);
				continue;
			}

			// fall out the if if the window is minimized
		}

		RemoveWindow(window, it, refresher);
	}
}

template<void(*logger)(std::wstring_view, Window, HMONITOR)>
void TaskbarAttributeWorker::RemoveWindow(Window window, taskbar_iterator it, AttributeRefresher& refresher)
{
	bool erased = false;
	if (it->second.MaximisedWindows.erase(window) > 0)
	{
		logger(L"maximised", window, it->first);
		erased = true;
	}

	if (it->second.NormalWindows.erase(window) > 0)
	{
		logger(L"normal", window, it->first);
		erased = true;
	}

	// only refresh the taskbar once in the case the window is in both
	if (erased)
	{
		refresher.refresh(it);
	}
}

bool TaskbarAttributeWorker::SetNewWindowExStyle(Window wnd, LONG_PTR oldStyle, LONG_PTR newStyle)
{
	if (oldStyle != newStyle)
	{
		return wnd.set_long_ptr(GWL_EXSTYLE, newStyle).has_value();
	}
	else
	{
		return true;
	}
}

void TaskbarAttributeWorker::DumpWindowSet(std::wstring_view prefix, const std::unordered_set<Window> &set, bool showInfo)
{
	if (!set.empty())
	{
		std::wstring buf;
		for (const Window window : set)
		{
			buf.clear();
			if (showInfo)
			{
				buf += prefix;
				buf += DumpWindow(window);
			}
			else
			{
				std::format_to(std::back_inserter(buf), L"{}{}", prefix, static_cast<void *>(window.handle()));
			}
			MessagePrint(spdlog::level::off, buf);
		}
	}
	else
	{
		MessagePrint(spdlog::level::off, std::format(L"{}[none]", prefix));
	}
}

std::wstring TaskbarAttributeWorker::DumpWindow(Window window)
{
	if (window)
	{
		std::wstring title, className, fileName;
		if (auto titleOpt = window.title())
		{
			title = std::move(*titleOpt);
			if (auto classNameOpt = window.classname())
			{
				className = std::move(*classNameOpt);
				if (const auto fileOpt = window.file())
				{
					fileName = fileOpt->filename().native();
				}
			}
		}

		return std::format(L"{} [{}] [{}] [{}]", static_cast<void *>(window.handle()), title, className, fileName);
	}
	else
	{
		return L"0x0";
	}
}

void TaskbarAttributeWorker::CreateAppVisibility()
{
	if (m_StartVisibilityChangeMessage)
	{
		try
		{
			m_IAV = wil::CoCreateInstance<IAppVisibility>(CLSID_AppVisibility);
		}
		catch (const wil::ResultException &err)
		{
			ResultExceptionHandle(err, spdlog::level::warn, L"Failed to create app visibility instance.");
			return;
		}

		const auto av_sink = winrt::make_self<LauncherVisibilitySink>(*this, *m_StartVisibilityChangeMessage);
		m_IAVECookie.associate(m_IAV.get());
		HresultVerify(m_IAV->Advise(av_sink.get(), m_IAVECookie.put()), spdlog::level::warn, L"Failed to register app visibility sink.");
	}
}

void TaskbarAttributeWorker::CreateSearchManager()
{
	UnregisterSearchCallbacks();

	m_SearchManager = nullptr;
	m_SearchViewCoordinator = nullptr;
	m_FindInStartViewCoordinator = nullptr;

	if (m_SearchVisibilityChangeMessage)
	{
		if (m_IsWindows11)
		{
			try
			{
				using winrt::WindowsUdk::UI::Shell::ShellView;
				using winrt::WindowsUdk::UI::Shell::ShellViewCoordinator;

				m_SearchViewCoordinator = ShellViewCoordinator { winrt::WindowsUdk::UI::Shell::ShellView::Search };

				m_SearchViewVisibilityChangedToken = m_SearchViewCoordinator.VisibilityChanged([this](const ShellViewCoordinator &coordinator, const wf::IInspectable &)
				{
					post_message(*m_SearchVisibilityChangeMessage, coordinator.Visibility() == winrt::WindowsUdk::UI::Shell::ViewVisibility::Visible);
				});

				m_FindInStartViewCoordinator = ShellViewCoordinator { winrt::WindowsUdk::UI::Shell::ShellView::FindInStart };

				m_FindInStartVisibilityChangedToken = m_FindInStartViewCoordinator.VisibilityChanged([this](const ShellViewCoordinator& coordinator, const wf::IInspectable&)
				{
					post_message(*m_FindInStartVisibilityChangeMessage, coordinator.Visibility() == winrt::WindowsUdk::UI::Shell::ViewVisibility::Visible);
				});
			}
			HresultErrorCatch(spdlog::level::warn, L"Failed to create ShellViewCoordinator");
		}
		else
		{
			try
			{
				using winrt::Windows::Internal::Shell::Experience::IShellExperienceManagerFactory;
				using winrt::Windows::Internal::Shell::Experience::ICortanaExperienceManager;

				auto serviceManager = wil::CoCreateInstance<IServiceProvider>(CLSID_ImmersiveShell, CLSCTX_LOCAL_SERVER);

				IShellExperienceManagerFactory factory(nullptr);
				winrt::check_hresult(serviceManager->QueryService(winrt::guid_of<IShellExperienceManagerFactory>(), winrt::guid_of<IShellExperienceManagerFactory>(), winrt::put_abi(factory)));

				m_SearchManager = factory.GetExperienceManager(win32::IsAtLeastBuild(19041) ? SEH_SearchApp : SEH_Cortana).as<ICortanaExperienceManager>();

				m_SuggestionsShownToken = m_SearchManager.SuggestionsShown([this](const ICortanaExperienceManager &, const wf::IInspectable &)
				{
					post_message(*m_SearchVisibilityChangeMessage, true, 0);
				});

				m_SuggestionsHiddenToken = m_SearchManager.SuggestionsHidden([this](const ICortanaExperienceManager &, const wf::IInspectable &)
				{
					post_message(*m_SearchVisibilityChangeMessage, false, 0);
				});
			}
			ResultExceptionCatch(spdlog::level::warn, L"Failed to create immersive shell service provider")
			HresultErrorCatch(spdlog::level::warn, L"Failed to query for ICortanaExperienceManager");
		}
	}
}

void TaskbarAttributeWorker::UnregisterSearchCallbacks() noexcept
{
	if (m_SearchManager)
	{
		if (m_SuggestionsShownToken)
		{
			m_SearchManager.SuggestionsShown(m_SuggestionsShownToken);
			m_SuggestionsShownToken = { };
		}

		if (m_SuggestionsHiddenToken)
		{
			m_SearchManager.SuggestionsHidden(m_SuggestionsHiddenToken);
			m_SuggestionsHiddenToken = { };
		}
	}

	if (m_FindInStartViewCoordinator)
	{
		if (m_FindInStartVisibilityChangedToken)
		{
			m_FindInStartViewCoordinator.VisibilityChanged(m_FindInStartVisibilityChangedToken);
			m_FindInStartVisibilityChangedToken = { };
		}
	}

	if (m_SearchViewCoordinator)
	{
		if (m_SearchViewVisibilityChangedToken)
		{
			m_SearchViewCoordinator.VisibilityChanged(m_SearchViewVisibilityChangedToken);
			m_SearchViewVisibilityChangedToken = { };
		}
	}
}

void TaskbarAttributeWorker::CreateTaskViewManager()
{
	UnregisterTaskViewCallbacks();

	m_TaskViewViewCoordinator = nullptr;

	if (m_TaskViewVisibilityChangeMessage && m_IsWindows11)
	{
		try
		{
			using winrt::WindowsUdk::UI::Shell::ShellView;
			using winrt::WindowsUdk::UI::Shell::ShellViewCoordinator;

			m_TaskViewViewCoordinator = ShellViewCoordinator { winrt::WindowsUdk::UI::Shell::ShellView::TaskView };

			m_TaskViewVisibilityChangedToken = m_TaskViewViewCoordinator.VisibilityChanged([this](const ShellViewCoordinator& coordinator, const wf::IInspectable&)
			{
				post_message(*m_TaskViewVisibilityChangeMessage, coordinator.Visibility() == winrt::WindowsUdk::UI::Shell::ViewVisibility::Visible);
			});
		}
		HresultErrorCatch(spdlog::level::warn, L"Failed to create ShellViewCoordinator");
	}
}

void TaskbarAttributeWorker::UnregisterTaskViewCallbacks() noexcept
{
	if (m_TaskViewViewCoordinator)
	{
		if (m_TaskViewVisibilityChangedToken)
		{
			m_TaskViewViewCoordinator.VisibilityChanged(m_TaskViewVisibilityChangedToken);
			m_TaskViewVisibilityChangedToken = { };
		}
	}
}

WINEVENTPROC TaskbarAttributeWorker::CreateThunk(void(CALLBACK TaskbarAttributeWorker:: *proc)(DWORD, HWND, LONG, LONG, DWORD, DWORD))
{
	return m_ThunkPage.make_thunk<WINEVENTPROC>(this, proc);
}

wil::unique_hwineventhook TaskbarAttributeWorker::CreateHook(DWORD eventMin, DWORD eventMax, WINEVENTPROC proc)
{
	if (wil::unique_hwineventhook hook { SetWinEventHook(eventMin, eventMax, nullptr, proc, 0, 0, WINEVENT_OUTOFCONTEXT) })
	{
		return hook;
	}
	else
	{
		MessagePrint(spdlog::level::critical, L"Failed to create a Windows event hook.");
	}
}

void TaskbarAttributeWorker::ReturnToStock()
{
	if (m_TaskbarService)
	{
		HresultVerify(m_TaskbarService->RestoreAllTaskbarsToDefault(), spdlog::level::info, L"Failed to restore taskbars");
	}
	else
	{
		// Copy taskbars to a vector because some functions
		// trigger Windows internal message loop.
		std::vector<TaskbarInfo> taskbarInfos;
		for (auto it = m_Taskbars.begin(); it != m_Taskbars.end(); ++it)
		{
			SetAttribute(it, { ACCENT_NORMAL, { 0, 0, 0, 0 }, true, true, 0.0f });

			taskbarInfos.push_back(it->second.Taskbar);
		}

		for (const auto& taskbarInfo : taskbarInfos)
		{
			if (taskbarInfo.InnerXamlContent || taskbarInfo.WorkerWWindow)
			{
				ShowTaskbarLine(taskbarInfo, true);
			}
			else if (taskbarInfo.PeekWindow)
			{
				ShowAeroPeekButton(taskbarInfo, true);
			}
		}
	}
}

bool TaskbarAttributeWorker::IsStartMenuOpened() const
{
	if (m_IAV)
	{
		BOOL start_visible;
		const HRESULT hr = m_IAV->IsLauncherVisible(&start_visible);
		if (SUCCEEDED(hr))
		{
			return start_visible;
		}
		else
		{
			HresultHandle(hr, spdlog::level::info, L"Failed to query launcher visibility state.");
		}
	}

	return false;
}

bool TaskbarAttributeWorker::IsSearchOpened() const try
{
	if (m_SearchViewCoordinator)
	{
		return m_SearchViewCoordinator.Visibility() == winrt::WindowsUdk::UI::Shell::ViewVisibility::Visible;
	}
	else if (m_SearchManager)
	{
		return m_SearchManager.SuggestionsShowing();
	}
	else
	{
		return false;
	}
}
catch (const winrt::hresult_error &err)
{
	HresultErrorHandle(err, spdlog::level::info, L"Failed to check if search is opened");
	return false;
}

bool TaskbarAttributeWorker::IsFindInStartOpened() const try
{
	if (m_FindInStartViewCoordinator)
	{
		return m_FindInStartViewCoordinator.Visibility() == winrt::WindowsUdk::UI::Shell::ViewVisibility::Visible;
	}
	else
	{
		return false;
	}
}
catch (const winrt::hresult_error &err)
{
	HresultErrorHandle(err, spdlog::level::info, L"Failed to check if find in start is opened");
	return false;
}

void TaskbarAttributeWorker::InsertTaskbar(HMONITOR mon, Window window)
{
	TaskbarInfo taskbarInfo = { .TaskbarWindow = window };
	if (m_TaskbarType == TaskbarType::Mixed)
	{
		taskbarInfo.WorkerWWindow = window.find_child(L"WorkerW"); // early 22621
		taskbarInfo.InnerXamlContent = window.find_child(L"Windows.UI.Composition.DesktopWindowContentBridge", L"DesktopWindowXamlSource"); // 22000
	}
	else if (m_TaskbarType == TaskbarType::Classic)
	{
		taskbarInfo.PeekWindow = window.find_child(L"TrayNotifyWnd").find_child(L"TrayShowDesktopButtonWClass");
	}

	m_NormalTaskbars.insert(window);
	m_Taskbars.insert_or_assign(mon, MonitorInfo { taskbarInfo });

	if (m_TaskbarType != TaskbarType::XAML)
	{
		if (wil::unique_hhook hook { m_InjectExplorerHook(window) })
		{
			m_Hooks.push_back(std::move(hook));
		}
		else
		{
			LastErrorHandle(spdlog::level::critical, L"Failed to set hook.");
		}
	}
}

BOOL TaskbarAttributeWorker::MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT lprcMonitor, LPARAM dwData)
{
	const auto info = reinterpret_cast<MonitorEnumInfo*>(&dwData);
	for (const UINT edge : { ABE_BOTTOM, ABE_TOP, ABE_LEFT, ABE_RIGHT })
	{
		APPBARDATA data = { sizeof(data) };
		data.uEdge = edge;
		data.rc = *lprcMonitor;

		HWND hwndAutoHide = reinterpret_cast<HWND>(SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &data));
		if (hwndAutoHide == info->window)
		{
			info->monitor = hMonitor;

			return false;
		}
	}

	return true;
}

BOOL TaskbarAttributeWorker::WindowEnumProc(HWND hwnd, LPARAM lParam)
{
	Window window(hwnd);
	if (window.classname() == L"Windows.UI.Composition.DesktopWindowContentBridge" && window.title() == L"DesktopWindowXamlSource")
	{
		auto islandsCount = reinterpret_cast<uint32_t*>(lParam);
		++(*islandsCount);
	}

	return true;
}

HMONITOR TaskbarAttributeWorker::GetTaskbarMonitor(Window taskbar)
{
	MonitorEnumInfo info = { taskbar };
	if (EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&info)) && info.monitor)
	{
		return info.monitor;
	}
	else
	{
		return taskbar.monitor();
	}
}

TaskbarType TaskbarAttributeWorker::GetTaskbarType(Window taskbar)
{
	wil::unique_cotaskmem_string system32;
	const HRESULT hr = SHGetKnownFolderPath(FOLDERID_System, KF_FLAG_DEFAULT, nullptr, system32.put());
	if (FAILED(hr)) [[unlikely]]
	{
		HresultHandle(hr, spdlog::level::critical, L"Failed to get System32 path");
	}

	std::filesystem::path taskbarDll = system32.get();
	taskbarDll /= L"Taskbar.dll";

	bool hasTaskbarDll = false;
	wil::unique_tool_help_snapshot snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, taskbar.process_id()));

	MODULEENTRY32 me = { .dwSize = sizeof(me) };

	if (!Module32First(snapshot.get(), &me))
	{
		// this can happen if explorer dies
		return TaskbarType::Unknown;
	}

	do
	{
		if (win32::IsSameFilename(me.szExePath, taskbarDll.native()))
		{
			hasTaskbarDll = true;
			break;
		}
	} while (Module32Next(snapshot.get(), &me));

	snapshot.reset();

	if (hasTaskbarDll)
	{
		uint32_t islandsCount = 0;
		EnumChildWindows(taskbar, WindowEnumProc, reinterpret_cast<LPARAM>(&islandsCount));
		if (islandsCount == 2)
		{
			return TaskbarType::Mixed;
		}
		else if (islandsCount == 1)
		{
			return TaskbarType::XAML;
		}
		else
		{
			return TaskbarType::Unknown;
		}
	}
	else
	{
		return TaskbarType::Classic;
	}
}

TaskbarAttributeWorker::TaskbarAttributeWorker(ConfigManager &cfgManager, HINSTANCE hInstance, DynamicLoader &loader, const std::optional<std::filesystem::path> &storageFolder) :
	MessageWindow(TTB_WORKERWINDOW, TTB_WORKERWINDOW, hInstance, WS_POPUP, WS_EX_NOREDIRECTIONBITMAP),
	SetWindowCompositionAttribute(loader.SetWindowCompositionAttribute()),
	ShouldSystemUseDarkMode(loader.ShouldSystemUseDarkMode()),
	m_PowerSaver(false),
	m_TaskViewActive(false),
	m_PeekActive(false),
	m_disableAttributeRefreshReply(false),
	m_ResettingState(false),
	m_ResetStateReentered(false),
	m_TaskbarType(TaskbarType::Unknown),
	m_CurrentStartMonitor(nullptr),
	m_CurrentSearchMonitor(nullptr),
	m_CurrentFindInStartMonitor(nullptr),
	m_ConfigManager(cfgManager),
	m_ThunkPage(member_thunk::allocate_page()),
	m_PeekUnpeekHook(CreateHook(EVENT_SYSTEM_PEEKSTART, EVENT_SYSTEM_PEEKEND, CreateThunk(&TaskbarAttributeWorker::OnAeroPeekEnterExit))),
	m_CloakUncloakHook(CreateHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED, CreateThunk(&TaskbarAttributeWorker::WindowInsertRemove<EVENT_OBJECT_UNCLOAKED, EVENT_OBJECT_CLOAKED>))),
	m_MinimizeRestoreHook(CreateHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, CreateThunk(&TaskbarAttributeWorker::WindowInsertRemove<EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZESTART>))),
	m_ShowHideHook(CreateHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE, CreateThunk(&TaskbarAttributeWorker::WindowInsertRemove<EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE>))),
	m_CreateDestroyHook(CreateHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, CreateThunk(&TaskbarAttributeWorker::OnWindowCreateDestroy))),
	m_ForegroundChangeHook(CreateHook(EVENT_SYSTEM_FOREGROUND, CreateThunk(&TaskbarAttributeWorker::OnForegroundWindowChange))),
	m_OrderChangeHook(CreateHook(EVENT_OBJECT_REORDER, CreateThunk(&TaskbarAttributeWorker::OnWindowOrderChange))),
	m_SearchManager(nullptr),
	m_SearchViewCoordinator(nullptr),
	m_FindInStartViewCoordinator(nullptr),
	m_TaskViewViewCoordinator(nullptr),
	m_TaskbarCreatedMessage(Window::RegisterMessage(WM_TASKBARCREATED)),
	m_RefreshRequestedMessage(Window::RegisterMessage(WM_TTBHOOKREQUESTREFRESH)),
	m_TaskViewVisibilityChangeMessage(Window::RegisterMessage(WM_TTBHOOKTASKVIEWVISIBILITYCHANGE)),
	m_IsTaskViewOpenedMessage(Window::RegisterMessage(WM_TTBHOOKISTASKVIEWOPENED)),
	m_StartVisibilityChangeMessage(Window::RegisterMessage(WM_TTBSTARTVISIBILITYCHANGE)),
	m_SearchVisibilityChangeMessage(Window::RegisterMessage(WM_TTBSEARCHVISIBILITYCHANGE)),
	m_FindInStartVisibilityChangeMessage(Window::RegisterMessage(WM_TTBFINDINSTARTVISIBILITYCHANGE)),
	m_ForceRefreshTaskbar(Window::RegisterMessage(WM_TTBFORCEREFRESHTASKBAR)),
	m_ApplyColorPreview(Window::RegisterMessage(WM_TTBAPPLYCOLORPREVIEW)),
	m_LastExplorerPid(0),
	m_HookDll(storageFolder, cfgManager.GetConfig().CopyDlls.value_or(true), L"ExplorerHooks.dll"),
	m_InjectExplorerHook(m_HookDll.GetProc<PFN_INJECT_EXPLORER_HOOK>("InjectExplorerHook")),
	m_TAPDll(storageFolder, cfgManager.GetConfig().CopyDlls.value_or(true), L"ExplorerTAP.dll"),
	m_InjectExplorerTAP(m_TAPDll.GetProc<PFN_INJECT_EXPLORER_TAP>("InjectExplorerTAP")),
	m_IsWindows11(win32::IsAtLeastBuild(22000)),
	m_IsBlurAccentStateSupported(!m_IsWindows11)
{
	const auto stateThunk = CreateThunk(&TaskbarAttributeWorker::OnWindowStateChange);
	m_ResizeMoveHook = CreateHook(EVENT_OBJECT_LOCATIONCHANGE, stateThunk);
	m_TitleChangeHook = CreateHook(EVENT_OBJECT_NAMECHANGE, stateThunk);
	m_ParentChangeHook = CreateHook(EVENT_OBJECT_PARENTCHANGE, stateThunk);
	m_ThunkPage.mark_executable();

	m_PowerSaverHook.reset(RegisterPowerSettingNotification(m_WindowHandle, &GUID_POWER_SAVING_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE));
	if (!m_PowerSaverHook)
	{
		LastErrorHandle(spdlog::level::warn, L"Failed to create battery saver notification handle");
	}

	CreateAppVisibility();

	if (win32::IsExactBuild(22000))
	{
		// Windows 11 RTM. sometimes very laggy at release, fixed in KB5006746 (22000.282)
		if (const auto [version, hr] = win32::GetWindowsBuild(); SUCCEEDED(hr))
		{
			m_IsBlurAccentStateSupported = version.Revision >= 282;
		}
	}

	// The tint is driven by a high-resolution waitable timer on its own thread:
	// WM_TIMER alone is limited to the ~15.6 ms system tick, which caps the
	// animation at ~64 fps. The thread only wakes and posts a tick.
	m_DynamicTimerId = 0x5454;
	StartDynamicTickThread();

	// we don't want to consider the first state reset as an Explorer restart.
	ResetState(true);
}

thread_local TaskbarAttributeWorker *TaskbarAttributeWorker::s_InputWorker = nullptr;

void TaskbarAttributeWorker::ScheduleDynamicTick(unsigned milliseconds)
{
    if (m_DynamicTickMs.exchange(milliseconds) != milliseconds && m_DynamicTickWakeEvent)
        SetEvent(m_DynamicTickWakeEvent);
}

void TaskbarAttributeWorker::SignalDynamicInput(bool content)
{
    if (!m_InputMonitoring.load() || m_DynamicTickStop.load()) return;
    if (content) m_InputContent.store(true);
    if (!m_InputPosted.exchange(true))
    {
        if (!PostMessageW(m_WindowHandle, WM_DYNAMIC_INPUT, 0, 0)) m_InputPosted.store(false);
    }
}

LRESULT CALLBACK TaskbarAttributeWorker::DynamicKeyboardHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && s_InputWorker && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN))
        s_InputWorker->SignalDynamicInput(true);
    // No key code, text or input payload is inspected, stored or suppressed.
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK TaskbarAttributeWorker::DynamicMouseHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && s_InputWorker)
    {
        if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN ||
            wParam == WM_XBUTTONDOWN || wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL)
            s_InputWorker->SignalDynamicInput(true);
        else if (wParam == WM_MOUSEMOVE && s_InputWorker->m_HoverTracking.load())
        {
            static thread_local ULONGLONG lastMove = 0;
            const auto now = GetTickCount64();
            if (now - lastMove >= 50) { lastMove = now; s_InputWorker->SignalDynamicInput(false); }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void TaskbarAttributeWorker::StartDynamicTickThread()
{
    m_DynamicTickStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_DynamicTickWakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_DynamicTickStopEvent || !m_DynamicTickWakeEvent)
        LastErrorHandle(spdlog::level::critical, L"Failed to create tint scheduler events");
    m_DynamicTickThread = std::thread([this]
    {
        s_InputWorker = this;
        wil::unique_hhook keyboard, mouse;
        const HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        const HANDLE handles[] {m_DynamicTickStopEvent, m_DynamicTickWakeEvent, timer};
        // Establish this thread's queue before installing low-level input hooks.
        MSG message;
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
        bool hadMonitoring = false;
        while (!m_DynamicTickStop.load())
        {
            const bool monitoring = m_InputMonitoring.load();
            if (monitoring != hadMonitoring)
            {
                keyboard.reset(); mouse.reset();
                if (monitoring)
                {
                    keyboard.reset(SetWindowsHookExW(WH_KEYBOARD_LL, DynamicKeyboardHook, GetModuleHandleW(nullptr), 0));
                    mouse.reset(SetWindowsHookExW(WH_MOUSE_LL, DynamicMouseHook, GetModuleHandleW(nullptr), 0));
                    if (!keyboard || !mouse) MessagePrint(spdlog::level::warn, L"Input hint hook unavailable; low-rate fallback remains active");
                }
                hadMonitoring = monitoring;
            }
            const unsigned ms = m_DynamicTickMs.load();
            LARGE_INTEGER due { };
            due.QuadPart = -static_cast<LONGLONG>(std::max(1u, ms)) * 10000LL;
            const bool armed = timer && ms != INFINITE && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            if (timer && ms == INFINITE) CancelWaitableTimer(timer);
            const DWORD count = armed ? 3 : 2;
            // Pumping hook messages must not restart the capture deadline. Keep
            // waiting on the same timer until it fires or the UI reschedules it.
            for (;;)
            {
                const DWORD result = MsgWaitForMultipleObjects(count, handles, FALSE, armed ? INFINITE : ms, QS_ALLINPUT);
                if (result == WAIT_OBJECT_0 || m_DynamicTickStop.load()) break;
                if (result == WAIT_OBJECT_0 + 1) break;
                if (result == WAIT_OBJECT_0 + count)
                {
                    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&message);
                        DispatchMessageW(&message);
                    }
                    continue;
                }
                if (result == WAIT_TIMEOUT || (armed && result == WAIT_OBJECT_0 + 2))
                {
                    if (!m_DynamicTickPosted.exchange(true))
                        if (!PostMessageW(m_WindowHandle, WM_TIMER, static_cast<WPARAM>(m_DynamicTimerId), 0)) m_DynamicTickPosted.store(false);
                    break;
                }
                WaitForSingleObject(m_DynamicTickStopEvent, 50);
                break;
            }
        }
        keyboard.reset(); mouse.reset();
        s_InputWorker = nullptr;
        if (timer) CloseHandle(timer);
    });
}

void TaskbarAttributeWorker::DumpState()
{
	MessagePrint(spdlog::level::off, L"===== Begin TaskbarAttributeWorker state dump =====");

	std::wstring buf;
	for (const auto &[monitor, info] : m_Taskbars)
	{
		buf.clear();
		std::format_to(std::back_inserter(buf), L"Monitor {}:", static_cast<void *>(monitor));
		MessagePrint(spdlog::level::off, buf);

		buf.clear();
		std::format_to(std::back_inserter(buf), L"\tTaskbar handle: {}", DumpWindow(info.Taskbar.TaskbarWindow));
		MessagePrint(spdlog::level::off, buf);

		buf.clear();
		std::format_to(std::back_inserter(buf), L"\tPeek button handle: {}", DumpWindow(info.Taskbar.PeekWindow));
		MessagePrint(spdlog::level::off, buf);

		buf.clear();
		std::format_to(std::back_inserter(buf), L"\tInner XAML handle: {}", DumpWindow(info.Taskbar.InnerXamlContent));
		MessagePrint(spdlog::level::off, buf);

		buf.clear();
		std::format_to(std::back_inserter(buf), L"\tWorkerW handle: {}", DumpWindow(info.Taskbar.WorkerWWindow));
		MessagePrint(spdlog::level::off, buf);

		MessagePrint(spdlog::level::off, L"\tMaximised windows:");
		DumpWindowSet(L"\t\t\t", info.MaximisedWindows);

		MessagePrint(spdlog::level::off, L"\tNormal windows:");
		DumpWindowSet(L"\t\t\t", info.NormalWindows);
	}

	buf.clear();
	std::format_to(std::back_inserter(buf), L"User is using Aero Peek: {}", m_PeekActive);
	MessagePrint(spdlog::level::off, buf);

	buf.clear();
	std::format_to(std::back_inserter(buf), L"User is using Task View: {}", m_TaskViewActive);
	MessagePrint(spdlog::level::off, buf);

	if (m_CurrentStartMonitor != nullptr)
	{
		buf.clear();
		std::format_to(std::back_inserter(buf), L"Start menu is opened: true [monitor {}]", static_cast<void *>(m_CurrentStartMonitor));
		MessagePrint(spdlog::level::off, buf);
	}
	else
	{
		MessagePrint(spdlog::level::off, L"Start menu is opened: false");
	}

	if (m_CurrentSearchMonitor != nullptr)
	{
		buf.clear();
		std::format_to(std::back_inserter(buf), L"Search is opened: true [monitor {}]", static_cast<void *>(m_CurrentSearchMonitor));
		MessagePrint(spdlog::level::off, buf);
	}
	else
	{
		MessagePrint(spdlog::level::off, L"Search is opened: false");
	}

	if (m_CurrentFindInStartMonitor != nullptr)
	{
		buf.clear();
		std::format_to(std::back_inserter(buf), L"Find in Start is opened: true [monitor {}]", static_cast<void*>(m_CurrentFindInStartMonitor));
		MessagePrint(spdlog::level::off, buf);
	}
	else
	{
		MessagePrint(spdlog::level::off, L"Find in Start is opened: false");
	}

	buf.clear();
	std::format_to(std::back_inserter(buf), L"Current foreground window: {}", DumpWindow(m_ForegroundWindow));
	MessagePrint(spdlog::level::off, buf);

	switch (m_TaskbarType)
	{
	case TaskbarType::Unknown:
		MessagePrint(spdlog::level::off, L"Current taskbar type: Unknown");
		break;

	case TaskbarType::Classic:
		MessagePrint(spdlog::level::off, L"Current taskbar type: Classic");
		break;

	case TaskbarType::Mixed:
		MessagePrint(spdlog::level::off, L"Current taskbar type: Mixed");
		break;

	case TaskbarType::XAML:
		MessagePrint(spdlog::level::off, L"Current taskbar type: XAML");
		break;
	}

	buf.clear();
	std::format_to(std::back_inserter(buf), L"Worker handles attribute refresh requests from hooks: {}", !m_disableAttributeRefreshReply);
	MessagePrint(spdlog::level::off, buf);

	buf.clear();
	std::format_to(std::back_inserter(buf), L"Battery saver is active: {}", m_PowerSaver);
	MessagePrint(spdlog::level::off, buf);

	MessagePrint(spdlog::level::off, L"Taskbars currently using normal appearance:");
	DumpWindowSet(L"\t\t", m_NormalTaskbars, false);

	MessagePrint(spdlog::level::off, L"===== End TaskbarAttributeWorker state dump =====");
}

void TaskbarAttributeWorker::ResetState(bool manual)
{
	if (!m_ResettingState)
	{
		MessagePrint(spdlog::level::debug, L"Resetting worker state");

		m_ResettingState = true;
		m_ResetStateReentered = false;
		auto guard = wil::scope_exit([this]
		{
			m_ResettingState = false;
			m_ResetStateReentered = false;
		});

		// Clear state
		m_PowerSaver = false;
		m_PeekActive = false;
		m_TaskViewActive = false;
		m_CurrentStartMonitor = nullptr;
		m_CurrentSearchMonitor = nullptr;
		m_CurrentFindInStartMonitor = nullptr;
		m_ForegroundWindow = Window::NullWindow;

		m_Taskbars.clear();
		m_NormalTaskbars.clear();

		m_TaskbarService = nullptr;

		// Keep old hooks alive while we rehook to avoid DLL unload.
		auto oldHooks = std::move(m_Hooks);
		m_Hooks.clear();

		for (const Window main_taskbar : Window::FindEnum(TASKBAR))
		{
			if (main_taskbar.file().value_or({}).filename() == L"explorer.exe")
			{
				const auto pid = main_taskbar.process_id();
				if (!manual)
				{
					if (m_LastExplorerPid != 0 && pid != m_LastExplorerPid)
					{
						const auto now = std::chrono::steady_clock::now();
						if (now < m_LastExplorerRestart + std::chrono::seconds(30)) [[unlikely]]
						{
							Localization::ShowLocalizedMessageBox(IDS_EXPLORER_RESTARTED_TOO_MUCH, MB_OK | MB_ICONWARNING | MB_SETFOREGROUND, hinstance()).join();
							ExitProcess(1);
						}

						m_LastExplorerRestart = now;
					}
				}

				m_LastExplorerPid = pid;

				m_TaskbarType = GetTaskbarType(main_taskbar);

				if (m_TaskbarType == TaskbarType::XAML)
				{
					const HRESULT hr = m_InjectExplorerTAP(main_taskbar, IID_PPV_ARGS(m_TaskbarService.put()));
					if (hr == HRESULT_FROM_WIN32(ERROR_PRODUCT_VERSION))
					{
						Localization::ShowLocalizedMessageBox(IDS_RESTART_REQUIRED, MB_OK | MB_ICONWARNING | MB_SETFOREGROUND, hinstance()).join();
						ExitProcess(1);
					}
					else
					{
						HresultVerify(hr, spdlog::level::critical, L"Failed to initialize XAML Diagnostics.");
					}

					HresultVerify(m_TaskbarService->RestoreAllTaskbarsToDefaultWhenProcessDies(GetCurrentProcessId()), spdlog::level::warn, L"Couldn't configure TAP to restore taskbar appearance once " APP_NAME L" dies.");

					if (const auto fullName = UWP::GetPackageFullName())
					{
						HresultVerify(m_TaskbarService->KillExplorerWhenPackageUninstalls(fullName->c_str()), spdlog::level::warn, L"Couldn't configure TAP to kill Explorer once " APP_NAME L" is uninstalled.");
					}
				}
				else if (m_TaskbarType != TaskbarType::Unknown)
				{
					if (!m_IsBlurAccentStateSupported)
					{
						m_ConfigManager.UpgradeBlur();
					}
				}
				else
				{
					// unknown taskbar type - we might've detected explorer too early. do nothing for now. we should get a refresh later and be able to detect it.
					return;
				}

				InsertTaskbar(GetTaskbarMonitor(main_taskbar), main_taskbar);
				break; // only one main taskbar.
			}
		}

		for (const Window secondtaskbar : Window::FindEnum(SECONDARY_TASKBAR))
		{
			InsertTaskbar(GetTaskbarMonitor(secondtaskbar), secondtaskbar);
		}

		// drop the old hooks.
		oldHooks.clear();

		CreateSearchManager();
		CreateTaskViewManager();

		// This might race but it's not an issue because
		// it'll race on a single thread and only ends up
		// doing something twice.

		SYSTEM_POWER_STATUS powerStatus;
		if (GetSystemPowerStatus(&powerStatus))
		{
			m_PowerSaver = powerStatus.SystemStatusFlag;
		}
		else
		{
			LastErrorHandle(spdlog::level::warn, L"Failed to verify system power status");
		}

		// TODO: check if aero peek is active

		if (m_TaskViewViewCoordinator)
		{
			m_TaskViewActive = m_TaskViewViewCoordinator.Visibility() == winrt::WindowsUdk::UI::Shell::ViewVisibility::Visible;
		}
		else if (const auto hookWnd = Window::Find(TTBHOOK_TASKVIEWMONITOR, TTBHOOK_TASKVIEWMONITOR); hookWnd && m_IsTaskViewOpenedMessage)
		{
			m_TaskViewActive = hookWnd.send_message(*m_IsTaskViewOpenedMessage);
		}

		m_ForegroundWindow = Window::ForegroundWindow();
		if (IsStartMenuOpened())
		{
			m_CurrentStartMonitor = GetStartMenuMonitor();
		}

		if (IsSearchOpened())
		{
			m_CurrentSearchMonitor = GetSearchMonitor();
		}

		if (IsFindInStartOpened())
		{
			m_CurrentFindInStartMonitor = GetFindInStartMonitor();
		}

		for (const Window window : Window::FindEnum())
		{
			InsertWindow(window, false);
		}

		if (!m_ResetStateReentered)
		{
			// Apply the calculated effects
			RefreshAllAttributes();
		}
		else
		{
			// we got re-entrancy, all this might not even be valid anymore, try it again.
			guard.reset();
			ResetState(manual);
		}
	}
	else
	{
		MessagePrint(spdlog::level::debug, L"ResetState re-entrancy detected");

		m_ResetStateReentered = true;
	}
}

TaskbarAttributeWorker::~TaskbarAttributeWorker() noexcept(false)
{
	m_DynamicTickStop = true;
	if (m_DynamicTickStopEvent)
	{
		SetEvent(m_DynamicTickStopEvent);
	}
	if (m_DynamicTickThread.joinable())
	{
		m_DynamicTickThread.join();
	}
	if (m_DynamicTickStopEvent)
	{
		CloseHandle(m_DynamicTickStopEvent);
		m_DynamicTickStopEvent = nullptr;
	}

	if (m_DynamicTickWakeEvent)
	{
		CloseHandle(m_DynamicTickWakeEvent);
		m_DynamicTickWakeEvent = nullptr;
	}
	m_disableAttributeRefreshReply = true;
	UnregisterTaskViewCallbacks();
	UnregisterSearchCallbacks();
	ReturnToStock();
}
