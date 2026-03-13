#pragma once

#include "includes.hpp"

class PathFinder {
public:
  static auto &get() {
    static PathFinder instance;
    return instance;
  }

  static void start();
  static void stop(std::string const &message = "", bool error = false);
  static void onFrame(int frame);
  static void onDeath(int frame);
  static void onLevelComplete(int frame);
  static void loadSettings();
  static bool isRunning();

private:
  bool running = false;
  bool handlingDeath = false;
  bool previousStopPlaying = false;
  int bestFrame = 0;
  int currentAttemptBest = 0;
  int backtrackStep = 4;
  int maxShiftPerAction = 6;
  int maxActionsBack = 8;
  int addAttempts = 0;
  int maxAddedActions = 24;
  std::unordered_map<std::string, int> shiftAttempts;

#ifdef GEODE_IS_WINDOWS
  bool consoleEnabled = true;
  bool consoleOpen = false;
#endif
};
