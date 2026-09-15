#include "runtime_settings_imgui.h"

#include "recomp_runtime_ui.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include <SDL3/SDL.h>

static bool s_initialized;

static void ApplyStyle() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImGui::StyleColorsDark(&style);
  style.WindowRounding = 10.0f;
  style.ChildRounding = 8.0f;
  style.FrameRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.WindowPadding = ImVec2(20.0f, 18.0f);
  style.ItemSpacing = ImVec2(10.0f, 10.0f);
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.045f, 0.085f, 0.98f);
  style.Colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.070f, 0.120f, 0.96f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.34f, 0.16f, 0.68f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.48f, 0.24f, 0.88f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.75f, 0.72f, 1.0f);
  style.Colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.95f, 0.78f, 1.0f);
  style.Colors[ImGuiCol_TabSelected] = ImVec4(0.34f, 0.16f, 0.68f, 1.0f);
  style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.13f, 0.52f, 1.0f);
  style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.48f, 0.24f, 0.88f, 1.0f);
  style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.75f, 0.72f, 1.0f);
}

extern "C" bool Cv4RuntimeSettingsImGuiInit(void *window, void *gl_context) {
  if (s_initialized) return true;
  if (!window || !gl_context) return false;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  ApplyStyle();
  if (!ImGui_ImplSDL3_InitForOpenGL(static_cast<SDL_Window *>(window), gl_context) ||
      !ImGui_ImplOpenGL3_Init("#version 330 core")) {
    ImGui::DestroyContext();
    return false;
  }
  s_initialized = true;
  return true;
}

extern "C" void Cv4RuntimeSettingsImGuiShutdown(void) {
  if (!s_initialized) return;
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  s_initialized = false;
}

extern "C" void Cv4RuntimeSettingsImGuiProcessEvent(const void *event_ptr) {
  if (!s_initialized || !event_ptr) return;
  const SDL_Event *event = static_cast<const SDL_Event *>(event_ptr);
  /* Keyboard and controller navigation are handled by the runtime model in
     host_main.c. Feed pointer input here so ImGui rows remain clickable. */
  switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
      ImGui_ImplSDL3_ProcessEvent(event);
      break;
    default:
      break;
  }
}

extern "C" void Cv4RuntimeSettingsImGuiRender(void *ui_ptr, int width,
                                                int height) {
  if (!s_initialized || !ui_ptr) return;
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  (void)width;
  (void)height;
  recomp_runtime_ui_render_imgui(static_cast<RecompRuntimeUi *>(ui_ptr));
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
