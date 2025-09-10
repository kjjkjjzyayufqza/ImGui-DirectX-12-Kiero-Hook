#include "UiRenderer.h"
#include <imgui.h>
#include <filesystem>
#include <dev/logger.h>

namespace ui
{
	static bool g_showMainWindow = true;

	void Initialize()
	{
		// Try to load Segoe UI 14pt like ReShade; fallback to ImGui default
		ImGuiIO& io = ImGui::GetIO();
		const char* segouePath = "C:/Windows/Fonts/segoeui.ttf";
		ImFont* font = nullptr;
		if (std::filesystem::exists(segouePath))
		{
			font = io.Fonts->AddFontFromFileTTF(segouePath, 14.0f);
		}
		if (!font)
		{
            // Log error
            LOG_INFO("Failed to load Segoe UI font");
            LOG_INFO("Using default font");
			font = io.Fonts->AddFontDefault();
		}
		io.FontDefault = font;
		io.Fonts->Build();
	}

	void Draw()
	{
		if (g_showMainWindow)
		{
			ImGui::SetNextWindowSize(ImVec2(600.0f, 0.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("ImGui DX12 Hook - Tools");
			ImGui::Text("Hello from UiRenderer!");
			ImGui::Separator();
			ImGui::TextWrapped("Add your controls here. This function is invoked every frame.");
			ImGui::End();
		}
	}

	void Shutdown()
	{
		// Nothing to do currently
	}
}
