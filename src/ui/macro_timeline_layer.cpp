#include "macro_timeline_layer.hpp"

#include "macro_editor.hpp"

#include <algorithm>

namespace {
std::string getButtonName(int button) {
  if (btnNames.contains(button))
    return btnNames.at(button);
  return std::to_string(button);
}

float frameToRatio(int frame, int maxFrame) {
  if (maxFrame <= 0)
    return 0.f;
  return std::clamp(static_cast<float>(frame) / static_cast<float>(maxFrame), 0.f,
                    1.f);
}
} // namespace

void MacroTimelineLayer::open() {
  if (!PlayLayer::get()) {
    FLAlertLayer::create("Timeline", "Open a level first.", "Ok")->show();
    return;
  }

  auto *layer = create();
  layer->m_noElasticity = true;
  layer->show();
}

bool MacroTimelineLayer::setup() {
  auto &g = Global::get();

#ifdef GEODE_IS_WINDOWS
  cursorWasHidden = cocos2d::CCEGLView::sharedOpenGLView()->getShouldHideCursor();
  cocos2d::CCEGLView::sharedOpenGLView()->showCursor(true);
#endif

  originalInputs = g.macro.inputs;
  inputs = originalInputs;
  if (!inputs.empty())
    maxFrame = std::max(1, static_cast<int>(inputs.back().frame));

  m_bgSprite->setOpacity(0);
  m_mainLayer->setPosition({CCDirector::sharedDirector()->getWinSize().width / 2.f,
                            95.f});

  auto *panel = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  panel->setContentSize({430.f, 132.f});
  panel->setColor({0, 0, 0});
  panel->setOpacity(120);
  panel->setPosition({0, 0});
  m_mainLayer->addChild(panel);

  auto *title = CCLabelBMFont::create("Macro Timeline", "goldFont.fnt");
  title->setScale(0.55f);
  title->setPosition({-145.f, 48.f});
  m_mainLayer->addChild(title);

  statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
  statusLabel->setAnchorPoint({0, 0.5f});
  statusLabel->setScale(0.5f);
  statusLabel->setOpacity(160);
  statusLabel->setPosition({-208.f, 28.f});
  m_mainLayer->addChild(statusLabel);

  frameLabel = CCLabelBMFont::create("Frame: 0", "bigFont.fnt");
  frameLabel->setAnchorPoint({0, 0.5f});
  frameLabel->setScale(0.34f);
  frameLabel->setPosition({-208.f, 5.f});
  m_mainLayer->addChild(frameLabel);

  selectedLabel = CCLabelBMFont::create("No input selected", "bigFont.fnt");
  selectedLabel->setAnchorPoint({0, 0.5f});
  selectedLabel->setScale(0.28f);
  selectedLabel->setPosition({-208.f, -18.f});
  m_mainLayer->addChild(selectedLabel);

  timelineSlider =
      Slider::create(this, menu_selector(MacroTimelineLayer::onSlider), 1.f);
  timelineSlider->setPosition({-205.f, -49.f});
  timelineSlider->setAnchorPoint({0.f, 0.f});
  timelineSlider->setScale(0.72f);
  timelineSlider->setValue(0.f);
  m_mainLayer->addChild(timelineSlider);

  markerNode = cocos2d::CCDrawNode::create();
  markerNode->setPosition({0.f, 0.f});
  m_mainLayer->addChild(markerNode, 5);

  editorMenu = CCMenu::create();
  editorMenu->setPosition({0.f, 0.f});
  m_mainLayer->addChild(editorMenu, 10);

  auto addButton = [&](char const *text, cocos2d::CCPoint pos,
                       cocos2d::SEL_MenuHandler callback, int tag = 0,
                       float scale = 0.48f) {
    auto *sprite = ButtonSprite::create(text);
    sprite->setScale(scale);
    auto *button = CCMenuItemSpriteExtra::create(sprite, this, callback);
    button->setPosition(pos);
    button->setTag(tag);
    editorMenu->addChild(button);
    return button;
  };

  addButton("Timeline Save", {165.f, -48.f},
            menu_selector(MacroTimelineLayer::onSave), 0, 0.44f);
  addButton("Add", {100.f, -48.f}, menu_selector(MacroTimelineLayer::onAddInput));
  addButton("Delete", {38.f, -48.f},
            menu_selector(MacroTimelineLayer::onDeleteInput), 0, 0.44f);
  addButton("<", {-20.f, -48.f}, menu_selector(MacroTimelineLayer::onPrevInput));
  addButton(">", {8.f, -48.f}, menu_selector(MacroTimelineLayer::onNextInput));

  auto *frameBg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  frameBg->setContentSize({70.f, 30.f});
  frameBg->setOpacity(85);
  frameBg->setPosition({87.f, 24.f});
  m_mainLayer->addChild(frameBg);

  frameInput = TextInput::create(60, "Frame", "chatFont.fnt");
  frameInput->setPosition({87.f, 24.f});
  frameInput->setScale(0.72f);
  frameInput->setString("0");
  frameInput->getInputNode()->setAllowedChars("0123456789");
  frameInput->getInputNode()->setMaxLabelLength(8);
  frameInput->getInputNode()->setDelegate(this);
  m_mainLayer->addChild(frameInput);

  addButton("-10", {36.f, 24.f}, menu_selector(MacroTimelineLayer::nudgeFrame), -10,
            0.38f);
  addButton("+10", {138.f, 24.f}, menu_selector(MacroTimelineLayer::nudgeFrame), 10,
            0.38f);
  addButton("Button", {38.f, -2.f}, menu_selector(MacroTimelineLayer::switchButton), 0,
            0.38f);
  addButton("Action", {100.f, -2.f}, menu_selector(MacroTimelineLayer::switchAction), 0,
            0.38f);
  addButton("Player", {162.f, -2.f}, menu_selector(MacroTimelineLayer::switchPlayer), 0,
            0.38f);

  auto *closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
  closeSpr->setScale(0.7f);
  auto *closeBtn =
      CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(MacroTimelineLayer::onClose));
  closeBtn->setPosition({205.f, 50.f});
  editorMenu->addChild(closeBtn);

  setCurrentFrame(inputs.empty() ? 0 : static_cast<int>(inputs.front().frame));
  refreshStatus();
  rebuildMarkers();
  return true;
}

void MacroTimelineLayer::onClose(CCObject *) {
  if (!saved) {
    geode::createQuickPopup(
        "Exit Timeline", "<cr>Exit</c> without saving timeline changes?",
        "Cancel", "Exit", [this](auto, bool btn2) {
          if (!btn2)
            return;
#ifdef GEODE_IS_WINDOWS
          cocos2d::CCEGLView::sharedOpenGLView()->showCursor(cursorWasHidden ? false
                                                                             : true);
#endif
          this->setKeypadEnabled(false);
          this->setTouchEnabled(false);
          this->removeFromParentAndCleanup(true);
        });
    return;
  }
#ifdef GEODE_IS_WINDOWS
  cocos2d::CCEGLView::sharedOpenGLView()->showCursor(cursorWasHidden ? false : true);
#endif
  this->setKeypadEnabled(false);
  this->setTouchEnabled(false);
  this->removeFromParentAndCleanup(true);
}

void MacroTimelineLayer::sortInputs() {
  std::sort(inputs.begin(), inputs.end(),
            [](input const &left, input const &right) {
              if (left.frame != right.frame)
                return left.frame < right.frame;
              if (left.player2 != right.player2)
                return left.player2 < right.player2;
              if (left.button != right.button)
                return left.button < right.button;
              return left.down > right.down;
            });
  maxFrame = std::max(1, inputs.empty() ? 1 : static_cast<int>(inputs.back().frame));
}

void MacroTimelineLayer::markUnsaved() {
  saved = false;
  refreshStatus();
  rebuildMarkers();
}

void MacroTimelineLayer::refreshStatus() {
  statusLabel->setString(
      fmt::format("Inputs: {}{}", inputs.size(), saved ? "" : "  *unsaved").c_str());
}

void MacroTimelineLayer::rebuildMarkers() {
  if (!markerNode)
    return;

  markerNode->clear();
  if (inputs.empty())
    return;

  float startX = -205.f;
  float width = 285.f;
  float baseY = -28.f;
  float height = 17.f;

  for (size_t index = 0; index < inputs.size(); ++index) {
    auto const &inp = inputs[index];
    float x = startX + width * frameToRatio(static_cast<int>(inp.frame), maxFrame);
    auto color = inp.down ? ccc4f(0.2f, 1.f, 0.4f, 0.95f)
                          : ccc4f(1.f, 0.4f, 0.35f, 0.95f);
    float extra = inp.player2 ? 6.f : 0.f;
    markerNode->drawSegment({x, baseY + extra}, {x, baseY + height + extra}, 0.7f,
                            color);
  }

  float playheadX = startX + width * frameToRatio(currentFrame, maxFrame);
  markerNode->drawSegment({playheadX, baseY - 8.f}, {playheadX, baseY + 32.f}, 1.f,
                          ccc4f(0.95f, 0.95f, 0.2f, 0.95f));
}

void MacroTimelineLayer::selectNearestInput() {
  if (inputs.empty()) {
    selectedIndex = -1;
    refreshSelectedInfo();
    return;
  }

  int bestIndex = 0;
  int bestDistance = std::abs(static_cast<int>(inputs[0].frame) - currentFrame);
  for (int index = 1; index < static_cast<int>(inputs.size()); ++index) {
    int distance = std::abs(static_cast<int>(inputs[index].frame) - currentFrame);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestIndex = index;
    }
  }

  selectedIndex = bestIndex;
  refreshSelectedInfo();
}

void MacroTimelineLayer::refreshSelectedInfo() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size())) {
    selectedLabel->setString("No input selected");
    frameInput->setString(std::to_string(currentFrame).c_str());
    return;
  }

  auto const &inp = inputs[selectedIndex];
  selectedLabel->setString(
      fmt::format("#{}  {}  {}  P{}", selectedIndex + 1, getButtonName(inp.button),
                  inp.down ? "Hold" : "Release", inp.player2 ? 2 : 1)
          .c_str());
  frameInput->setString(std::to_string(inp.frame).c_str());
}

void MacroTimelineLayer::setCurrentFrame(int frame, bool updateSlider) {
  currentFrame = std::clamp(frame, 0, maxFrame);
  frameLabel->setString(fmt::format("Frame: {}", currentFrame).c_str());
  if (timelineSlider && updateSlider)
    timelineSlider->setValue(frameToRatio(currentFrame, maxFrame));
  selectNearestInput();
  rebuildMarkers();
}

void MacroTimelineLayer::onSlider(CCObject *) {
  float value = timelineSlider->getValue();
  setCurrentFrame(static_cast<int>(std::round(value * maxFrame)), false);
}

void MacroTimelineLayer::onPrevInput(CCObject *) {
  if (inputs.empty())
    return;
  if (selectedIndex <= 0)
    selectedIndex = static_cast<int>(inputs.size()) - 1;
  else
    selectedIndex--;
  setCurrentFrame(static_cast<int>(inputs[selectedIndex].frame));
}

void MacroTimelineLayer::onNextInput(CCObject *) {
  if (inputs.empty())
    return;
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size()) - 1)
    selectedIndex = 0;
  else
    selectedIndex++;
  setCurrentFrame(static_cast<int>(inputs[selectedIndex].frame));
}

void MacroTimelineLayer::onDeleteInput(CCObject *) {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size()))
    return;
  inputs.erase(inputs.begin() + selectedIndex);
  if (selectedIndex >= static_cast<int>(inputs.size()))
    selectedIndex = static_cast<int>(inputs.size()) - 1;
  sortInputs();
  markUnsaved();
  setCurrentFrame(currentFrame);
}

void MacroTimelineLayer::onAddInput(CCObject *) {
  input inp(currentFrame, 1, false, true);
  inputs.push_back(inp);
  sortInputs();
  selectNearestInput();
  markUnsaved();
  setCurrentFrame(currentFrame);
}

void MacroTimelineLayer::onSave(CCObject *) {
  auto &g = Global::get();
  sortInputs();
  g.macro.inputs = inputs;
  g.macro.lastRecordedFrame =
      inputs.empty() ? 0 : static_cast<int>(inputs.back().frame);
  originalInputs = inputs;
  saved = true;
  refreshStatus();
  Notification::create("Timeline saved", NotificationIcon::Success)->show();
}

void MacroTimelineLayer::switchButton(CCObject *) {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size()))
    return;
  auto &inp = inputs[selectedIndex];
  inp.button = inp.button == 3 ? 1 : inp.button + 1;
  if (inp.player2)
    inp.button = std::clamp(inp.button, 1, 3);
  markUnsaved();
  refreshSelectedInfo();
  rebuildMarkers();
}

void MacroTimelineLayer::switchAction(CCObject *) {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size()))
    return;
  inputs[selectedIndex].down = !inputs[selectedIndex].down;
  markUnsaved();
  refreshSelectedInfo();
  rebuildMarkers();
}

void MacroTimelineLayer::switchPlayer(CCObject *) {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size()))
    return;
  inputs[selectedIndex].player2 = !inputs[selectedIndex].player2;
  markUnsaved();
  refreshSelectedInfo();
  rebuildMarkers();
}

void MacroTimelineLayer::nudgeFrame(CCObject *obj) {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size()))
    return;
  auto *node = static_cast<CCNode *>(obj);
  int delta = node->getTag();
  inputs[selectedIndex].frame =
      std::max(0, static_cast<int>(inputs[selectedIndex].frame) + delta);
  sortInputs();
  markUnsaved();
  setCurrentFrame(static_cast<int>(inputs[selectedIndex].frame));
}

void MacroTimelineLayer::textChanged(CCTextInputNode *inputNode) {
  if (!inputNode)
    return;
  if (inputNode != frameInput->getInputNode())
    return;
  auto text = inputNode->getString();
  if (text.empty())
    return;
  int frame = geode::utils::numFromString<int>(text).unwrapOr(currentFrame);
  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(inputs.size())) {
    inputs[selectedIndex].frame = std::max(0, frame);
    sortInputs();
    markUnsaved();
  }
  setCurrentFrame(frame);
}
