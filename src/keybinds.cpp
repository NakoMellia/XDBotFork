
#include "hacks/layout_mode.hpp"
#include "hacks/show_trajectory.hpp"
#include "includes.hpp"
#include "ui/clickbot_layer.hpp"
#include "ui/game_ui.hpp"
#include "ui/macro_editor.hpp"
#include "ui/record_layer.hpp"
#include "ui/render_settings_layer.hpp"

#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>

// NOTE: geode.custom-keybinds is incompatible with Geode v5
// (EventFilter/EventListener removed).
// TODO: Migrate to Geode v5 built-in KeybindSettingV3 system.
// #ifdef GEODE_IS_WINDOWS
// #include <geode.custom-keybinds/include/Keybinds.hpp>
// #include <regex>
// #endif

const std::vector<std::string> keybindIDs = {
    "open_menu",        "toggle_recording",     "toggle_playing",
    "toggle_speedhack", "toggle_frame_stepper", "step_frame",
    "toggle_render",    "toggle_noclip",        "show_trajectory"};

class $modify(CCKeyboardDispatcher) {
  bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat,
                           double p3) {

    auto &g = Global::get();

    int keyInt = static_cast<int>(key);
    if (g.allKeybinds.contains(keyInt) && !isKeyRepeat) {
      for (size_t i = 0; i < 6; i++) {
        if (std::find(g.keybinds[i].begin(), g.keybinds[i].end(), keyInt) !=
            g.keybinds[i].end())
          g.heldButtons[i] = isKeyDown;
      }
    }

    // if (key == enumKeyCodes::KEY_L && !isKeyRepeat && isKeyDown) {
    // }

    // if (key == enumKeyCodes::KEY_F && !isKeyRepeat && isKeyDown &&
    // PlayLayer::get()) {
    //   log::debug("POS DEBUG {}", PlayLayer::get()->m_player1->getPosition());
    //   log::debug("POS2 DEBUG {}",
    //   PlayLayer::get()->m_player2->getPosition());
    // }

    // if (key == enumKeyCodes::KEY_J && !isKeyRepeat && isKeyDown &&
    // PlayLayer::get()) {
    //   std::string str =
    //   ZipUtils::decompressString(PlayLayer::get()->m_level->m_levelString.c_str(),
    //   true, 0); log::debug("{}", str);
    // }

    // NakoMod: Swift Click handling
    if (g.swiftClickEnabled && !isKeyRepeat && isKeyDown &&
        keyInt == g.swiftClickKey && PlayLayer::get() &&
        (g.state == state::recording || g.state == state::none)) {
      PlayLayer *pl = PlayLayer::get();
      if (pl && !pl->m_isPaused) {
        for (int i = 0; i < g.swiftClickCount; i++) {
          pl->handleButton(true, 1, false);
          pl->handleButton(false, 1, false);
        }
      }
    }

    return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown,
                                                     isKeyRepeat, p3);
  }
};

// ======================================================================
// custom-keybinds is incompatible with Geode v5 (EventFilter/EventListener
// removed).
// TODO: Migrate to Geode v5 built-in KeybindSettingV3 system.
// All code below is disabled until migration is complete.
// ======================================================================
#if 0

#ifdef GEODE_IS_ANDROID

namespace keybinds {

  struct ActionID {};

};

#endif

using namespace keybinds;

void onKeybind(bool down, ActionID id) {
#ifdef GEODE_IS_WINDOWS

  auto& g = Global::get();

  if (!down || (LevelEditorLayer::get() && !g.mod->getSettingValue<bool>("editor_keybinds")) || g.mod->getSettingValue<bool>("disable_keybinds"))
    return;

  if (g.state != state::recording && g.mod->getSettingValue<bool>("recording_only_keybinds"))
    return;

  if (id == "open_menu"_spr) {
    if (g.layer) {
      static_cast<RecordLayer*>(g.layer)->onClose(nullptr);
      return;
    }

    RecordLayer::openMenu();
  }

  if (id == "toggle_recording"_spr)
    Macro::toggleRecording();

  if (id == "toggle_playing"_spr)
    Macro::togglePlaying();

  if (id == "toggle_frame_stepper"_spr && PlayLayer::get())
    Global::toggleFrameStepper();

  if (id == "step_frame"_spr)
    Global::frameStep();

  if (id == "toggle_speedhack"_spr)
    Global::toggleSpeedhack();

  if (id == "show_trajectory"_spr) {
    g.mod->setSavedValue("macro_show_trajectory", !g.mod->getSavedValue<bool>("macro_show_trajectory"));

    if (g.layer) {
      if (static_cast<RecordLayer*>(g.layer)->trajectoryToggle)
        static_cast<RecordLayer*>(g.layer)->trajectoryToggle->toggle(g.mod->getSavedValue<bool>("macro_show_trajectory"));
    }

    g.showTrajectory = g.mod->getSavedValue<bool>("macro_show_trajectory");
    if (!g.showTrajectory) ShowTrajectory::trajectoryOff();
  }

  if (id == "toggle_render"_spr && PlayLayer::get()) {
    bool result = Renderer::toggle();

    if (result && Global::get().renderer.recording)
      Notification::create("Started Rendering", NotificationIcon::Info)->show();

    if (g.layer) {
      if (static_cast<RecordLayer*>(g.layer)->renderToggle)
        static_cast<RecordLayer*>(g.layer)->renderToggle->toggle(Global::get().renderer.recording);
    }

  }

  if (id == "toggle_noclip"_spr) {
    g.mod->setSavedValue("macro_noclip", !g.mod->getSavedValue<bool>("macro_noclip"));

    if (g.layer) {
      if (static_cast<RecordLayer*>(g.layer)->noclipToggle)
        static_cast<RecordLayer*>(g.layer)->noclipToggle->toggle(g.mod->getSavedValue<bool>("macro_noclip"));
    }
  }

#endif

}

$execute{

#ifdef GEODE_IS_WINDOWS

    BindManager * bm = BindManager::get();

    bm->registerBindable({
        "open_menu"_spr,
        "Open Menu",
        "Open Menu.",
        { keybinds::Keybind::create(KEY_F, Modifier::Alt) },
        "xdBot",
        false
    });

    // ... (all other registerBindable calls) ...

#endif
}

#endif // #if 0
