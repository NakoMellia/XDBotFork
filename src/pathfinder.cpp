#include "pathfinder.hpp"

#include "ui/game_ui.hpp"

#include <Geode/loader/Loader.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#include <cstdio>
#endif

namespace {
std::string getTimeString() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm localTime{};
#ifdef GEODE_IS_WINDOWS
  localtime_s(&localTime, &time);
#else
  localtime_r(&time, &localTime);
#endif
  std::ostringstream stream;
  stream << std::put_time(&localTime, "%H:%M:%S");
  return stream.str();
}

void writeLog(std::string const &level, std::string const &message) {
  auto line =
      fmt::format("{} {} [Main] [megacrack]: {}", getTimeString(), level, message);
  log::info("{}", line);
  std::cout << line << std::endl;
}

std::string actionKey(input const &action) {
  return fmt::format("{}:{}:{}:{}", action.frame, action.button, action.player2,
                     action.down);
}

void insertSorted(std::vector<input> &inputs, input const &action) {
  auto it = std::upper_bound(inputs.begin(), inputs.end(), action,
                             [](input const &left, input const &right) {
                               return left.frame < right.frame;
                             });
  inputs.insert(it, action);
}

bool hasExactAction(std::vector<input> const &inputs, input const &action) {
  return std::any_of(inputs.begin(), inputs.end(), [&](input const &existing) {
    return existing.frame == action.frame && existing.button == action.button &&
           existing.player2 == action.player2 && existing.down == action.down;
  });
}

bool isPrimaryCandidate(input const &action) {
  return action.button == 1 && action.down;
}

bool isSecondaryCandidate(input const &action) {
  return action.down;
}
} // namespace

void PathFinder::loadSettings() {
  auto &finder = get();
  auto *mod = Mod::get();

  finder.backtrackStep = static_cast<int>(
      std::clamp(mod->getSavedValue<int64_t>("pathfinder_backtrack_step", 4),
                 int64_t(1), int64_t(60)));
  finder.maxShiftPerAction = static_cast<int>(
      std::clamp(mod->getSavedValue<int64_t>("pathfinder_max_shifts", 6),
                 int64_t(1), int64_t(32)));
  finder.maxActionsBack = static_cast<int>(
      std::clamp(mod->getSavedValue<int64_t>("pathfinder_actions_back", 8),
                 int64_t(1), int64_t(64)));
  finder.warmupFrames = static_cast<int>(
      std::clamp(mod->getSavedValue<int64_t>("pathfinder_warmup_frames", 12),
                 int64_t(0), int64_t(120)));
  finder.minActionFrame = static_cast<int>(
      std::clamp(mod->getSavedValue<int64_t>("pathfinder_min_action_frame", 6),
                 int64_t(1), int64_t(120)));
  finder.minProgressForRecord = static_cast<int>(std::clamp(
      mod->getSavedValue<int64_t>("pathfinder_min_progress_record", 2),
      int64_t(1), int64_t(240)));
#ifdef GEODE_IS_WINDOWS
  finder.consoleEnabled =
      mod->getSavedValue<bool>("pathfinder_console_logs", true);
#endif
}

bool PathFinder::isRunning() { return get().running; }

void PathFinder::start() {
  auto &finder = get();
  auto &g = Global::get();

  if (finder.running)
    return;

  PlayLayer *playLayer = PlayLayer::get();
  if (!playLayer) {
    FLAlertLayer::create("PathFinder", "Open a level first.", "Ok")->show();
    g.mod->setSavedValue("macro_pathfinder_enabled", false);
    return;
  }

  loadSettings();

#ifdef GEODE_IS_WINDOWS
  if (finder.consoleEnabled && !finder.consoleOpen) {
    if (AllocConsole()) {
      FILE *dummy = nullptr;
      freopen_s(&dummy, "CONOUT$", "w", stdout);
      freopen_s(&dummy, "CONOUT$", "w", stderr);
      freopen_s(&dummy, "CONIN$", "r", stdin);
      finder.consoleOpen = true;
    }
  }
#endif

  finder.running = true;
  finder.handlingDeath = false;
  finder.bestFrame = 0;
  finder.currentAttemptBest = 0;
  finder.addAttempts = 0;
  finder.shiftAttempts.clear();
  finder.restartSerial = 0;
  finder.lastHandledDeathSerial = -1;
  finder.previousStopPlaying = g.stopPlaying;
  g.stopPlaying = false;

  writeLog("INFO ", "PathFinder started.");
  Notification::create("PathFinder started", NotificationIcon::Info)->show();

  if (playLayer->m_isPaused)
    if (PauseLayer *pauseLayer = Global::getPauseLayer())
      pauseLayer->onResume(nullptr);

  if (g.state != state::playing)
    Macro::togglePlaying();

  if (PlayLayer *current = PlayLayer::get()) {
    current->m_levelSettings->m_platformerMode ? current->resetLevelFromStart()
                                               : current->resetLevel();
  }

  Interface::updateLabels();
}

void PathFinder::stop(std::string const &message, bool error) {
  auto &finder = get();
  auto &g = Global::get();

  if (!finder.running && message.empty())
    return;

  finder.running = false;
  finder.handlingDeath = false;
  g.stopPlaying = finder.previousStopPlaying;
  g.mod->setSavedValue("macro_pathfinder_enabled", false);

  if (!message.empty())
    writeLog(error ? "ERROR" : "INFO ", message);

  if (!message.empty()) {
    Notification::create(message, error ? NotificationIcon::Error
                                        : NotificationIcon::Success)
        ->show();
  }

  Interface::updateLabels();
}

void PathFinder::onFrame(int frame) {
  auto &finder = get();
  if (!finder.running)
    return;

  if (frame > finder.currentAttemptBest)
    finder.currentAttemptBest = frame;

  if (frame > finder.warmupFrames)
    finder.lastHandledDeathSerial = -1;
}

void PathFinder::onLevelComplete(int frame) {
  auto &finder = get();
  if (!finder.running)
    return;

  finder.currentAttemptBest = std::max(finder.currentAttemptBest, frame);
  if (finder.currentAttemptBest >= finder.minProgressForRecord &&
      finder.currentAttemptBest > finder.bestFrame) {
    writeLog("INFO ", fmt::format("New Record! {} -> {}", finder.bestFrame,
                                   finder.currentAttemptBest));
    finder.bestFrame = finder.currentAttemptBest;
  }

  stop(fmt::format("PathFinder completed at frame {}.", finder.bestFrame));
}

static bool pathfinderShiftCandidate(std::vector<input> &inputs,
                                     std::unordered_map<std::string, int> &tries,
                                     int frameLimit, int minActionFrame,
                                     int backtrackStep,
                                     int maxShiftPerAction, int maxActionsBack,
                                     bool (*predicate)(input const &)) {
  int considered = 0;

  for (int index = static_cast<int>(inputs.size()) - 1; index >= 0; --index) {
    auto action = inputs[index];
    if (action.frame > frameLimit || !predicate(action))
      continue;

    considered++;
    if (considered > maxActionsBack)
      break;

    auto key = actionKey(action);
    int tryCount = tries[key];
    if (tryCount >= maxShiftPerAction)
      continue;

    int oldFrame = static_cast<int>(action.frame);
    int newFrame =
        std::max(minActionFrame, oldFrame - backtrackStep * (tryCount + 1));
    if (index > 0)
      newFrame = std::max(newFrame, static_cast<int>(inputs[index - 1].frame) + 1);

    if (newFrame >= oldFrame)
      continue;

    action.frame = newFrame;
    inputs.erase(inputs.begin() + index);
    insertSorted(inputs, action);

    tries.erase(key);
    tries[actionKey(action)] = tryCount + 1;

    writeLog("INFO ",
             fmt::format("BACKTRACK: Shifted action {} -> {}", oldFrame, newFrame));
    return true;
  }

  return false;
}

void PathFinder::onDeath(int frame) {
  auto &finder = get();
  auto &g = Global::get();

  if (!finder.running || finder.handlingDeath)
    return;
  if (finder.lastHandledDeathSerial == finder.restartSerial)
    return;

  finder.handlingDeath = true;
  finder.lastHandledDeathSerial = finder.restartSerial;
  finder.currentAttemptBest = std::max(finder.currentAttemptBest, frame);

  if (frame <= 1 && finder.currentAttemptBest <= 1) {
    writeLog("INFO ", "BACKTRACK: Ignored startup reset at frame 1");
    finder.handlingDeath = false;
    return;
  }

  if (finder.currentAttemptBest >= finder.minProgressForRecord &&
      finder.currentAttemptBest > finder.bestFrame) {
    writeLog("INFO ", fmt::format("New Record! {} -> {}", finder.bestFrame,
                                   finder.currentAttemptBest));
    finder.bestFrame = finder.currentAttemptBest;
    finder.shiftAttempts.clear();
    finder.addAttempts = 0;
  }

  auto &inputs = g.macro.inputs;
  bool changed = false;
  int effectiveFrame = std::max(frame, finder.minActionFrame);
  int frameLimit = std::max(
      std::max(finder.bestFrame, effectiveFrame),
      finder.currentAttemptBest <= finder.warmupFrames ? finder.warmupFrames
                                                       : effectiveFrame);

  changed = pathfinderShiftCandidate(inputs, finder.shiftAttempts,
                                     frameLimit, finder.minActionFrame,
                                     finder.backtrackStep, finder.maxShiftPerAction,
                                     finder.maxActionsBack, isPrimaryCandidate);

  if (!changed)
    changed = pathfinderShiftCandidate(inputs, finder.shiftAttempts,
                                       frameLimit, finder.minActionFrame,
                                       finder.backtrackStep, finder.maxShiftPerAction,
                                       finder.maxActionsBack, isSecondaryCandidate);

  if (!changed)
    changed = pathfinderShiftCandidate(inputs, finder.shiftAttempts, frameLimit,
                                       finder.minActionFrame,
                                       finder.backtrackStep,
                                       finder.maxShiftPerAction,
                                       finder.maxActionsBack,
                                       +[](input const &) { return true; });

  if (!changed) {
    if (finder.addAttempts < finder.maxAddedActions) {
      int targetFrame = std::max(
          finder.minActionFrame,
          finder.currentAttemptBest <= finder.warmupFrames
              ? finder.minActionFrame + finder.backtrackStep * finder.addAttempts
              : frame - finder.backtrackStep * (finder.addAttempts + 1));
      input press(targetFrame, 1, false, true);
      input release(targetFrame + 1, 1, false, false);
      bool inserted = false;
      if (!hasExactAction(inputs, press)) {
        insertSorted(inputs, press);
        inserted = true;
      }
      if (!hasExactAction(inputs, release)) {
        insertSorted(inputs, release);
        inserted = true;
      }
      if (!inserted) {
        finder.addAttempts++;
        targetFrame = std::max(finder.minActionFrame,
                               targetFrame + finder.backtrackStep);
        press.frame = targetFrame;
        release.frame = targetFrame + 1;
        if (!hasExactAction(inputs, press))
          insertSorted(inputs, press);
        if (!hasExactAction(inputs, release))
          insertSorted(inputs, release);
      }
      finder.addAttempts++;
      writeLog("INFO ",
               fmt::format("BACKTRACK: Added action at frame {}", targetFrame));
      changed = true;
    }
  }

  if (!changed) {
    stop(fmt::format("CRITICAL STUCK at frame {}!",
                     std::max(finder.bestFrame, frame)),
         true);
    return;
  }

  Loader::get()->queueInMainThread([] {
    auto &pf = PathFinder::get();
    if (!pf.running) {
      pf.handlingDeath = false;
      return;
    }

    if (PlayLayer *playLayer = PlayLayer::get()) {
      if (playLayer->m_isPaused)
        if (PauseLayer *pauseLayer = Global::getPauseLayer())
          pauseLayer->onResume(nullptr);

      playLayer->m_levelSettings->m_platformerMode
          ? playLayer->resetLevelFromStart()
          : playLayer->resetLevel();
    }

    pf.currentAttemptBest = 0;
    pf.restartSerial++;
    pf.handlingDeath = false;
  });
}
