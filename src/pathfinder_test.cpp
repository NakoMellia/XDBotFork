#include "pathfinder_test.hpp"

#include "hacks/show_trajectory.hpp"
#include "practice_fixes/practice_fixes.hpp"

#include <algorithm>

namespace {
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

void clearCollisionLogs(PlayerObject *player) {
  player->m_collisionLogTop->removeAllObjects();
  player->m_collisionLogBottom->removeAllObjects();
  player->m_collisionLogLeft->removeAllObjects();
  player->m_collisionLogRight->removeAllObjects();
}

void drawCandidate(PathFinderTest::Candidate const &candidate) {
  auto *node = PathFinderTest::drawNode();
  node->clear();
  node->setVisible(!candidate.points.empty());
  if (candidate.points.size() < 2)
    return;

  for (size_t index = 1; index < candidate.points.size(); ++index) {
    float alpha = 1.f - static_cast<float>(index - 1) /
                           static_cast<float>(candidate.points.size());
    node->drawSegment(candidate.points[index - 1], candidate.points[index], 0.8f,
                      ccc4f(0.15f, 1.f, 0.45f, std::max(0.15f, alpha)));
  }

  node->drawDot(candidate.points.front(), 3.5f, ccc4f(0.2f, 0.8f, 1.f, 0.8f));
  node->drawDot(candidate.points.back(), 4.5f,
                candidate.died ? ccc4f(1.f, 0.35f, 0.35f, 0.9f)
                               : ccc4f(0.25f, 1.f, 0.5f, 0.9f));
}

PathFinderTest::Candidate simulateCandidate(PlayLayer *pl, PlayerObject *realPlayer,
                                            PlayerObject *fakePlayer, int offset,
                                            int holdFrames, int simulationFrames) {
  auto &trajectory = ShowTrajectory::get();
  PathFinderTest::Candidate candidate;
  candidate.offset = offset;
  if (!pl || !realPlayer || !fakePlayer)
    return candidate;

  auto playerData = PlayerPracticeFixes::saveData(realPlayer);
  PlayerPracticeFixes::applyData(fakePlayer, playerData, false, true);

  trajectory.cancelTrajectory = false;
  trajectory.creatingTrajectory = true;
  Global::get().creatingTrajectory = true;

  if (pl->m_levelSettings->m_platformerMode) {
    realPlayer->m_isGoingLeft ? fakePlayer->pushButton(static_cast<PlayerButton>(2))
                              : fakePlayer->pushButton(static_cast<PlayerButton>(3));
  }

  for (int frame = 0; frame < simulationFrames; ++frame) {
    auto prevPos = fakePlayer->getPosition();
    candidate.points.push_back(prevPos);
    candidate.survivedFrames = frame;
    candidate.farthestX = std::max(candidate.farthestX, prevPos.x);

    clearCollisionLogs(fakePlayer);
    pl->checkCollisions(fakePlayer, trajectory.delta, false);

    if (trajectory.cancelTrajectory) {
      candidate.died = true;
      break;
    }

    if (frame == offset)
      fakePlayer->pushButton(static_cast<PlayerButton>(1));
    if (offset >= 0 && frame == offset + std::max(1, holdFrames))
      fakePlayer->releaseButton(static_cast<PlayerButton>(1));

    fakePlayer->update(trajectory.delta);
    fakePlayer->updateRotation(trajectory.delta);
    fakePlayer->updatePlayerScale();

    auto nextPos = fakePlayer->getPosition();
    candidate.points.push_back(nextPos);
    candidate.farthestX = std::max(candidate.farthestX, nextPos.x);
    candidate.survivedFrames = frame + 1;
  }

  trajectory.cancelTrajectory = false;
  trajectory.creatingTrajectory = false;
  Global::get().creatingTrajectory = false;
  return candidate;
}

bool isBetter(PathFinderTest::Candidate const &candidate,
              PathFinderTest::Candidate const &best) {
  if (candidate.survivedFrames != best.survivedFrames)
    return candidate.survivedFrames > best.survivedFrames;
  return candidate.farthestX > best.farthestX;
}
} // namespace

void PathFinderTest::loadSettings() {
  auto &test = get();
  auto *mod = Mod::get();
  test.searchAhead = static_cast<int>(std::clamp(
      mod->getSavedValue<int64_t>("pathfinder_test_search_ahead", 20),
      int64_t(1), int64_t(120)));
  test.holdFrames = static_cast<int>(std::clamp(
      mod->getSavedValue<int64_t>("pathfinder_test_hold_frames", 2), int64_t(1),
      int64_t(30)));
  test.simulationFrames = static_cast<int>(std::clamp(
      mod->getSavedValue<int64_t>("pathfinder_test_simulation_frames", 180),
      int64_t(30), int64_t(600)));
  test.autoApply =
      mod->getSavedValue<bool>("pathfinder_test_auto_apply", true);
}

cocos2d::CCDrawNode *PathFinderTest::drawNode() {
  static cocos2d::CCDrawNode *instance = nullptr;
  if (!instance) {
    instance = cocos2d::CCDrawNode::create();
    instance->retain();
    cocos2d::_ccBlendFunc blendFunc;
    blendFunc.src = GL_SRC_ALPHA;
    blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
    instance->setBlendFunc(blendFunc);
    instance->setVisible(false);
  }
  return instance;
}

bool PathFinderTest::isActive() { return get().active; }

void PathFinderTest::start() {
  auto &test = get();
  auto &g = Global::get();
  auto &trajectory = ShowTrajectory::get();

  auto *pl = PlayLayer::get();
  if (!pl || !pl->m_player1 || !trajectory.fakePlayer1) {
    FLAlertLayer::create("Path Finder Test", "Open a level first.", "Ok")->show();
    g.mod->setSavedValue("macro_pathfinder_test_enabled", false);
    return;
  }

  loadSettings();

  if (!drawNode()->getParent())
    pl->m_objectLayer->addChild(drawNode(), 501);

  PathFinderTest::Candidate baseline =
      simulateCandidate(pl, pl->m_player1, trajectory.fakePlayer1, -1,
                        test.holdFrames, test.simulationFrames);
  PathFinderTest::Candidate best = baseline;

  for (int offset = 0; offset <= test.searchAhead; ++offset) {
    auto candidate = simulateCandidate(pl, pl->m_player1, trajectory.fakePlayer1,
                                       offset, test.holdFrames,
                                       test.simulationFrames);
    if (isBetter(candidate, best))
      best = std::move(candidate);
  }

  test.active = true;
  test.lastCandidate = best;
  drawCandidate(best);

  int currentFrame = Global::getCurrentFrame();
  float gain = best.farthestX - baseline.farthestX;

  if (best.offset >= 0 && test.autoApply && isBetter(best, baseline)) {
    input press(currentFrame + best.offset, 1, false, true);
    input release(currentFrame + best.offset + std::max(1, test.holdFrames), 1,
                  false, false);
    if (!hasExactAction(g.macro.inputs, press))
      insertSorted(g.macro.inputs, press);
    if (!hasExactAction(g.macro.inputs, release))
      insertSorted(g.macro.inputs, release);
    g.macro.lastRecordedFrame =
        std::max(g.macro.lastRecordedFrame, currentFrame + best.offset);

    Notification::create(
        fmt::format("Path Test: jump +{}f, gain {:.0f}", best.offset, gain),
        NotificationIcon::Success)
        ->show();
    log::info("PathFinderTest applied jump at frame {} (+{}), survived {}, x gain {}",
              currentFrame + best.offset, best.offset, best.survivedFrames, gain);
    return;
  }

  if (best.offset >= 0 && isBetter(best, baseline)) {
    Notification::create(
        fmt::format("Path Test: best jump +{}f", best.offset),
        NotificationIcon::Info)
        ->show();
  } else {
    Notification::create("Path Test: no better jump found",
                         NotificationIcon::Warning)
        ->show();
  }
}

void PathFinderTest::stop(std::string const &message) {
  auto &test = get();
  test.active = false;
  test.lastCandidate = {};
  Mod::get()->setSavedValue("macro_pathfinder_test_enabled", false);
  if (drawNode()) {
    drawNode()->clear();
    drawNode()->setVisible(false);
    if (drawNode()->getParent())
      drawNode()->removeFromParent();
  }

  if (!message.empty())
    Notification::create(message, NotificationIcon::Info)->show();
}

class $modify(PathFinderTestPlayLayer, PlayLayer) {
  void setupHasCompleted() {
    PlayLayer::setupHasCompleted();
    if (PathFinderTest::drawNode()->getParent())
      PathFinderTest::drawNode()->removeFromParent();
    m_objectLayer->addChild(PathFinderTest::drawNode(), 501);
  }

  void onQuit() {
    PathFinderTest::stop();
    PlayLayer::onQuit();
  }
};
