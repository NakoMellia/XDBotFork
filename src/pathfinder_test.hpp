#pragma once

#include "includes.hpp"

class PathFinderTest {
public:
  struct Candidate {
    int offset = -1;
    int survivedFrames = 0;
    float farthestX = 0.f;
    bool died = false;
    std::vector<cocos2d::CCPoint> points;
  };

  static auto &get() {
    static PathFinderTest instance;
    return instance;
  }

  static void loadSettings();
  static void start();
  static void stop(std::string const &message = "");
  static bool isActive();
  static cocos2d::CCDrawNode *drawNode();

private:
  bool active = false;
  int searchAhead = 20;
  int holdFrames = 2;
  int simulationFrames = 180;
  bool autoApply = true;
  Candidate lastCandidate;
};
