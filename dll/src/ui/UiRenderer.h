#pragma once

#include <imgui.h>
#include <string>
#include <vector>

namespace ui
{
	// Initializes UI module state (if any). Safe to call multiple times.
	void Initialize();

	// Per-frame draw. Called between ImGui::NewFrame() and ImGui::Render().
	void Draw();

	// Optional teardown if needed in the future.
	void Shutdown();
}
