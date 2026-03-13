#include "macro_timeline_layer.hpp"

#include "macro_editor.hpp"

#include <algorithm>

namespace {
constexpr float kBottomBarHeight = 104.f;
constexpr float kBottomMargin = 24.f;
constexpr float kSidePanelWidth = 230.f;
constexpr float kSidePanelHeight = 250.f;
constexpr float kTrackHeight = 18.f;

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
  auto winSize = CCDirector::sharedDirector()->getWinSize();

#ifdef GEODE_IS_WINDOWS
  cursorWasHidden = cocos2d::CCEGLView::sharedOpenGLView()->getShouldHideCursor();
  cocos2d::CCEGLView::sharedOpenGLView()->showCursor(true);
#endif

  originalInputs = g.macro.inputs;
  inputs = originalInputs;
  if (!inputs.empty())
    maxFrame = std::max(1, static_cast<int>(inputs.back().frame));

  m_bgSprite->setOpacity(0);
  m_mainLayer->setPosition({0.f, 0.f});

  auto *screenDim = CCLayerColor::create({0, 0, 0, 55});
  screenDim->setContentSize(winSize);
  screenDim->setPosition({0.f, 0.f});
  m_mainLayer->addChild(screenDim, -5);

  auto *bottomPanel = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  bottomPanel->setContentSize({winSize.width - 56.f, kBottomBarHeight});
  bottomPanel->setColor({8, 12, 18});
  bottomPanel->setOpacity(155);
  bottomPanel->setPosition({winSize.width / 2.f, kBottomMargin + kBottomBarHeight / 2.f});
  m_mainLayer->addChild(bottomPanel);

  auto *bottomGlow =
      CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  bottomGlow->setContentSize({winSize.width - 62.f, kTrackHeight + 10.f});
  bottomGlow->setColor({35, 255, 120});
  bottomGlow->setOpacity(70);
  bottomGlow->setPosition(
      {winSize.width / 2.f, kBottomMargin + 28.f});
  m_mainLayer->addChild(bottomGlow, 2);

  auto *track = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  track->setContentSize({winSize.width - 72.f, kTrackHeight});
  track->setColor({58, 255, 96});
  track->setOpacity(230);
  track->setPosition({winSize.width / 2.f, kBottomMargin + 28.f});
  m_mainLayer->addChild(track, 3);

  auto *panelTitle = CCLabelBMFont::create("Timeline", "goldFont.fnt");
  panelTitle->setScale(0.55f);
  panelTitle->setPosition({76.f, kBottomMargin + 79.f});
  m_mainLayer->addChild(panelTitle, 6);

  statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
  statusLabel->setAnchorPoint({0.f, 0.5f});
  statusLabel->setScale(0.55f);
  statusLabel->setOpacity(180);
  statusLabel->setPosition({28.f, kBottomMargin + 58.f});
  m_mainLayer->addChild(statusLabel, 6);

  frameLabel = CCLabelBMFont::create("Frame: 0", "bigFont.fnt");
  frameLabel->setAnchorPoint({0.f, 0.5f});
  frameLabel->setScale(0.35f);
  frameLabel->setPosition({28.f, kBottomMargin + 82.f});
  m_mainLayer->addChild(frameLabel, 6);

  selectedLabel = CCLabelBMFont::create("No input selected", "bigFont.fnt");
  selectedLabel->setAnchorPoint({0.f, 0.5f});
  selectedLabel->setScale(0.28f);
  selectedLabel->setPosition({148.f, kBottomMargin + 82.f});
  m_mainLayer->addChild(selectedLabel, 6);

  timelineSlider =
      Slider::create(this, menu_selector(MacroTimelineLayer::onSlider), 1.f);
  timelineSlider->setPosition({30.f, kBottomMargin + 13.f});
  timelineSlider->setAnchorPoint({0.f, 0.f});
  timelineSlider->setScale((winSize.width - 220.f) / 200.f);
  timelineSlider->setValue(0.f);
  timelineSlider->setOpacity(0);
  timelineSlider->m_sliderBar->setOpacity(0);
  timelineSlider->m_touchLogic->setOpacity(0);
  m_mainLayer->addChild(timelineSlider, 8);

  markerNode = cocos2d::CCDrawNode::create();
  markerNode->setPosition({0.f, 0.f});
  m_mainLayer->addChild(markerNode, 7);

  auto *sidePanel = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  sidePanel->setContentSize({kSidePanelWidth, kSidePanelHeight});
  sidePanel->setColor({235, 235, 235});
  sidePanel->setOpacity(235);
  sidePanel->setPosition(
      {winSize.width - kSidePanelWidth / 2.f - 20.f, kBottomMargin + kBottomBarHeight + kSidePanelHeight / 2.f + 14.f});
  m_mainLayer->addChild(sidePanel, 10);

  auto *sideOutline = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  sideOutline->setContentSize({kSidePanelWidth + 6.f, kSidePanelHeight + 6.f});
  sideOutline->setColor({255, 170, 35});
  sideOutline->setOpacity(115);
  sideOutline->setPosition(sidePanel->getPosition());
  m_mainLayer->addChild(sideOutline, 9);

  editorMenu = CCMenu::create();
  editorMenu->setPosition({0.f, 0.f});
  m_mainLayer->addChild(editorMenu, 12);

  auto addButton = [&](char const *text, cocos2d::CCPoint pos,
                       cocos2d::SEL_MenuHandler callback, int tag = 0,
                       float scale = 0.46f) {
    auto *sprite = ButtonSprite::create(text);
    sprite->setScale(scale);
    auto *button = CCMenuItemSpriteExtra::create(sprite, this, callback);
    button->setPosition(pos);
    button->setTag(tag);
    editorMenu->addChild(button);
    return button;
  };

  auto sideCenterX = sidePanel->getPositionX();
  auto sideTop = sidePanel->getPositionY() + 88.f;

  auto addSideLabel = [&](char const *text, float y, float scale = 0.38f,
                          cocos2d::ccColor3B color = ccc3(25, 25, 25)) {
    auto *label = CCLabelBMFont::create(text, "bigFont.fnt");
    label->setPosition({sideCenterX - 76.f, y});
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(scale);
    label->setColor(color);
    m_mainLayer->addChild(label, 13);
    return label;
  };

  addSideLabel("Selected Input", sideTop + 20.f, 0.42f);

  addSideLabel("Frame:", sideTop - 16.f);
  auto *frameBg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  frameBg->setContentSize({82.f, 32.f});
  frameBg->setColor({32, 32, 32});
  frameBg->setOpacity(100);
  frameBg->setPosition({sideCenterX + 7.f, sideTop - 16.f});
  m_mainLayer->addChild(frameBg, 13);

  frameInput = TextInput::create(72, "Frame", "chatFont.fnt");
  frameInput->setPosition({sideCenterX + 7.f, sideTop - 16.f});
  frameInput->setScale(0.72f);
  frameInput->setString("0");
  frameInput->getInputNode()->setAllowedChars("0123456789");
  frameInput->getInputNode()->setMaxLabelLength(8);
  frameInput->getInputNode()->setDelegate(this);
  m_mainLayer->addChild(frameInput, 14);

  addSideLabel("Action:", sideTop - 72.f);
  addSideLabel("Button:", sideTop - 120.f);
  addSideLabel("Player:", sideTop - 168.f);

  addButton("Prev", {sideCenterX - 66.f, sideTop - 210.f},
            menu_selector(MacroTimelineLayer::onPrevInput), 0, 0.4f);
  addButton("Next", {sideCenterX + 2.f, sideTop - 210.f},
            menu_selector(MacroTimelineLayer::onNextInput), 0, 0.4f);
  addButton("Save", {sideCenterX + 70.f, sideTop - 210.f},
            menu_selector(MacroTimelineLayer::onSave), 0, 0.42f);

  addButton("-10", {sideCenterX - 58.f, sideTop - 16.f},
            menu_selector(MacroTimelineLayer::nudgeFrame), -10, 0.38f);
  addButton("+10", {sideCenterX + 67.f, sideTop - 16.f},
            menu_selector(MacroTimelineLayer::nudgeFrame), 10, 0.38f);

  addButton("Toggle", {sideCenterX + 40.f, sideTop - 72.f},
            menu_selector(MacroTimelineLayer::switchAction), 0, 0.38f);
  addButton("Cycle", {sideCenterX + 40.f, sideTop - 120.f},
            menu_selector(MacroTimelineLayer::switchButton), 0, 0.38f);
  addButton("Switch", {sideCenterX + 40.f, sideTop - 168.f},
            menu_selector(MacroTimelineLayer::switchPlayer), 0, 0.38f);
  addButton("Add", {sideCenterX - 45.f, sideTop - 248.f},
            menu_selector(MacroTimelineLayer::onAddInput), 0, 0.4f);
  addButton("Delete", {sideCenterX + 45.f, sideTop - 248.f},
            menu_selector(MacroTimelineLayer::onDeleteInput), 0, 0.4f);

  auto *closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
  closeSpr->setScale(0.7f);
  auto *closeBtn = CCMenuItemSpriteExtra::create(
      closeSpr, this, menu_selector(MacroTimelineLayer::onClose));
  closeBtn->setPosition({sidePanel->getPositionX() + 92.f, sidePanel->getPositionY() + 106.f});
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
  cocos2d::CCEGLView::sharedOpenGLView()->showCursor(cursorWasHidden ? false
                                                                     : true);
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

  auto winSize = CCDirector::sharedDirector()->getWinSize();
  float trackStartX = 36.f;
  float trackWidth = winSize.width - 78.f;
  float trackY = kBottomMargin + 28.f;

  int labelIndex = 0;
  for (int frame = 0; frame <= maxFrame; frame += std::max(1, maxFrame / 12)) {
    float x = trackStartX + trackWidth * frameToRatio(frame, maxFrame);
    markerNode->drawSegment({x, trackY + 13.f}, {x, trackY + 25.f}, 0.55f,
                            ccc4f(1.f, 1.f, 1.f, 0.25f));

    auto *label = CCLabelBMFont::create(std::to_string(frame).c_str(), "chatFont.fnt");
    label->setScale(0.33f);
    label->setOpacity(110);
    label->setPosition({x, trackY + 34.f});
    m_mainLayer->addChild(label, 6, 9000 + labelIndex++);
  }

  for (auto const &inp : inputs) {
    float x = trackStartX + trackWidth * frameToRatio(static_cast<int>(inp.frame), maxFrame);
    auto color = inp.down ? ccc4f(0.2f, 1.f, 0.3f, 0.95f)
                          : ccc4f(1.f, 0.25f, 0.25f, 0.95f);
    float extra = inp.player2 ? 15.f : 0.f;
    float length = inp.down ? 28.f : 18.f;
    markerNode->drawSegment({x, trackY - 34.f - extra}, {x, trackY - length - extra},
                            0.85f, color);
  }

  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(inputs.size())) {
    float x = trackStartX +
              trackWidth * frameToRatio(static_cast<int>(inputs[selectedIndex].frame), maxFrame);
    markerNode->drawSegment({x, trackY - 42.f}, {x, trackY + 32.f}, 1.35f,
                            ccc4f(1.f, 0.85f, 0.18f, 0.95f));
  }

  float playheadX = trackStartX + trackWidth * frameToRatio(currentFrame, maxFrame);
  markerNode->drawSolidCircle({playheadX, trackY}, 26.f, 0.f, 28,
                              ccc4f(1.f, 0.95f, 0.15f, 0.92f));
  markerNode->drawSolidCircle({playheadX, trackY}, 18.f, 0.f, 24,
                              ccc4f(0.92f, 0.82f, 0.1f, 0.98f));
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
  auto winSize = CCDirector::sharedDirector()->getWinSize();
  float panelCenterX = winSize.width - kSidePanelWidth / 2.f - 20.f;
  float panelTop = kBottomMargin + kBottomBarHeight + kSidePanelHeight / 2.f + 14.f + 88.f;

  auto updateOrCreate = [&](int tag, std::string const &text, float y) {
    CCLabelBMFont *label = m_mainLayer->getChildByTag(tag)
                               ? typeinfo_cast<CCLabelBMFont *>(m_mainLayer->getChildByTag(tag))
                               : nullptr;
    if (!label) {
      label = CCLabelBMFont::create("", "bigFont.fnt");
      label->setAnchorPoint({0.f, 0.5f});
      label->setScale(0.32f);
      label->setColor({35, 35, 35});
      label->setPosition({panelCenterX + 8.f, y});
      m_mainLayer->addChild(label, 13, tag);
    }
    label->setString(text.c_str());
  };

  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size())) {
    selectedLabel->setString("No input selected");
    frameInput->setString(std::to_string(currentFrame).c_str());
    updateOrCreate(8801, "()", panelTop - 16.f);
    updateOrCreate(8802, "()", panelTop - 72.f);
    updateOrCreate(8803, "()", panelTop - 120.f);
    updateOrCreate(8804, "()", panelTop - 168.f);
    return;
  }

  auto const &inp = inputs[selectedIndex];
  selectedLabel->setString(fmt::format("#{} at {}", selectedIndex + 1, inp.frame).c_str());
  frameInput->setString(std::to_string(inp.frame).c_str());
  updateOrCreate(8801, fmt::format("{}", inp.frame), panelTop - 16.f);
  updateOrCreate(8802, inp.down ? "Hold" : "Release", panelTop - 72.f);
  updateOrCreate(8803, getButtonName(inp.button), panelTop - 120.f);
  updateOrCreate(8804, inp.player2 ? "Two" : "One", panelTop - 168.f);
}

void MacroTimelineLayer::setCurrentFrame(int frame, bool updateSlider) {
  currentFrame = std::clamp(frame, 0, maxFrame);
  frameLabel->setString(fmt::format("Frame: {}", currentFrame).c_str());
  if (timelineSlider && updateSlider)
    timelineSlider->setValue(frameToRatio(currentFrame, maxFrame));
  selectNearestInput();

  for (int tag = 9000; tag <= 9032; ++tag) {
    if (auto *node = m_mainLayer->getChildByTag(tag))
      node->removeFromParentAndCleanup(true);
  }

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
