#include "macro_timeline_layer.hpp"

#include "game_ui.hpp"
#include "macro_editor.hpp"

#include <algorithm>

namespace {
// ── Layout constants ───────────────────────────────────────────────
constexpr float kEdgeMargin = 28.f;
constexpr float kBottomBarHeight = 80.f;
constexpr float kTrackHeight = 20.f;
constexpr float kTrackY = 36.f; // center-Y of the track inside the bottom bar
constexpr float kSidePanelWidth = 172.f;
constexpr float kSidePanelHeight = 220.f;
constexpr float kInfoBarHeight = 28.f;

// ── Colours (rgb) ──────────────────────────────────────────────────
constexpr ccColor3B kPanelBg = {18, 22, 30};        // #12161E
constexpr ccColor3B kPanelOutline = {255, 170, 35}; // #FFAA23
constexpr ccColor3B kTrackBg = {8, 12, 18};         // #080C12
constexpr ccColor3B kTrackFill = {58, 255, 96};     // #3AFF60
constexpr ccColor3B kTrackGlow = {35, 255, 120};    // #23FF78
constexpr ccColor3B kValueBoxBg = {14, 14, 18};     // #0E0E12
constexpr ccColor3B kGold = {255, 211, 90};         // #FFD35A
constexpr ccColor3B kInfoBarBg = {10, 14, 20};      // #0A0E14

// ── Helpers ────────────────────────────────────────────────────────
std::string getButtonName(int button) {
  if (btnNames.contains(button))
    return btnNames.at(button);
  return std::to_string(button);
}

float frameToRatio(int frame, int maxFrame) {
  if (maxFrame <= 0)
    return 0.f;
  return std::clamp(static_cast<float>(frame) / static_cast<float>(maxFrame),
                    0.f, 1.f);
}
} // namespace

// ════════════════════════════════════════════════════════════════════
//  open()
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::open() {
  if (!PlayLayer::get()) {
    FLAlertLayer::create("Timeline", "Open a level first.", "Ok")->show();
    return;
  }

  auto *layer = create();
  layer->m_noElasticity = true;
  layer->show();
}

// ════════════════════════════════════════════════════════════════════
//  setup()  —  REDESIGNED LAYOUT
// ════════════════════════════════════════════════════════════════════
bool MacroTimelineLayer::setup() {
  auto &g = Global::get();
  auto winSize = CCDirector::sharedDirector()->getWinSize();

#ifdef GEODE_IS_WINDOWS
  cursorWasHidden =
      cocos2d::CCEGLView::sharedOpenGLView()->getShouldHideCursor();
  cocos2d::CCEGLView::sharedOpenGLView()->showCursor(true);
#endif

  // ── macro state init (unchanged) ─────────────────────────────────
  originalInputs = g.macro.inputs;
  inputs = originalInputs;
  if (!inputs.empty())
    maxFrame = std::max(1, static_cast<int>(inputs.back().frame));

  previewWasPlaying = g.state == state::playing;
  if (g.state == state::recording)
    Macro::resetState(true);
  g.state = state::playing;
  g.currentAction = 0;
  g.currentFrameFix = 0;
  g.macro.xdBotMacro = g.macro.botInfo.name == "xdBot";
  Interface::updateLabels();
  Interface::updateButtons();

  // ── strip default popup bg ───────────────────────────────────────
  m_bgSprite->setOpacity(0);
  m_mainLayer->setPosition({0.f, 0.f});

  // ── dim overlay ──────────────────────────────────────────────────
  auto *screenDim = CCLayerColor::create({0, 0, 0, 80});
  screenDim->setContentSize(winSize);
  screenDim->setPosition({0.f, 0.f});
  m_mainLayer->addChild(screenDim, -5);

  // ────────────────────────────────────────────────────────────────
  //  BOTTOM TIMELINE BAR
  // ────────────────────────────────────────────────────────────────
  float barLeft = kEdgeMargin;
  float barRight = winSize.width - kEdgeMargin;
  float barWidth = barRight - barLeft;
  float barCenterX = winSize.width / 2.f;
  float barBottom = kEdgeMargin;
  float barCenterY = barBottom + kBottomBarHeight / 2.f;

  // dark rounded background
  auto *bottomPanel =
      CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  bottomPanel->setContentSize({barWidth, kBottomBarHeight});
  bottomPanel->setColor(kTrackBg);
  bottomPanel->setOpacity(185);
  bottomPanel->setPosition({barCenterX, barCenterY});
  m_mainLayer->addChild(bottomPanel, 1);

  // green glow behind track
  float trackCenterY = barBottom + kTrackY;
  auto *trackGlow = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  trackGlow->setContentSize({barWidth - 16.f, kTrackHeight + 12.f});
  trackGlow->setColor(kTrackGlow);
  trackGlow->setOpacity(55);
  trackGlow->setPosition({barCenterX, trackCenterY});
  m_mainLayer->addChild(trackGlow, 2);

  // green track strip
  float trackWidth = barWidth - 24.f;
  float trackStartX = barLeft + 12.f;
  auto *track = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  track->setContentSize({trackWidth, kTrackHeight});
  track->setColor(kTrackFill);
  track->setOpacity(210);
  track->setPosition({barCenterX, trackCenterY});
  m_mainLayer->addChild(track, 3);

  // ── Info labels inside bottom bar ────────────────────────────────
  frameLabel = CCLabelBMFont::create("Frame: 0", "bigFont.fnt");
  frameLabel->setAnchorPoint({0.f, 0.5f});
  frameLabel->setScale(0.34f);
  frameLabel->setPosition({barLeft + 8.f, barBottom + kBottomBarHeight - 14.f});
  m_mainLayer->addChild(frameLabel, 6);

  selectedLabel = CCLabelBMFont::create("No input selected", "bigFont.fnt");
  selectedLabel->setAnchorPoint({0.f, 0.5f});
  selectedLabel->setScale(0.26f);
  selectedLabel->setOpacity(200);
  selectedLabel->setPosition(
      {barLeft + 140.f, barBottom + kBottomBarHeight - 14.f});
  m_mainLayer->addChild(selectedLabel, 6);

  statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
  statusLabel->setAnchorPoint({1.f, 0.5f});
  statusLabel->setScale(0.5f);
  statusLabel->setOpacity(160);
  statusLabel->setPosition(
      {barRight - 8.f, barBottom + kBottomBarHeight - 14.f});
  m_mainLayer->addChild(statusLabel, 6);

  // ── Timeline slider (invisible bar, drives playhead) ─────────────
  timelineSlider =
      Slider::create(this, menu_selector(MacroTimelineLayer::onSlider), 1.f);
  timelineSlider->setPosition({trackStartX, trackCenterY});
  timelineSlider->setAnchorPoint({0.f, 0.f});
  timelineSlider->setScale(trackWidth / 200.f);
  timelineSlider->setValue(0.f);
  timelineSlider->m_sliderBar->setOpacity(0);
  timelineSlider->m_touchLogic->setOpacity(0);
  if (auto *thumb = timelineSlider->getThumb())
    thumb->setVisible(false);
  m_mainLayer->addChild(timelineSlider, 8);

  // ── Marker / playhead draw node ──────────────────────────────────
  markerNode = cocos2d::CCDrawNode::create();
  markerNode->setPosition({0.f, 0.f});
  m_mainLayer->addChild(markerNode, 7);

  // ────────────────────────────────────────────────────────────────
  //  RIGHT SIDE EDITOR PANEL
  // ────────────────────────────────────────────────────────────────
  float sidePanelX = winSize.width - kSidePanelWidth / 2.f - kEdgeMargin;
  float sidePanelY =
      barBottom + kBottomBarHeight + 14.f + kSidePanelHeight / 2.f;

  // outline (slightly bigger, orange)
  auto *sideOutline =
      CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  sideOutline->setContentSize({kSidePanelWidth + 5.f, kSidePanelHeight + 5.f});
  sideOutline->setColor(kPanelOutline);
  sideOutline->setOpacity(100);
  sideOutline->setPosition({sidePanelX, sidePanelY});
  m_mainLayer->addChild(sideOutline, 9);

  // dark panel background
  auto *sidePanel = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  sidePanel->setContentSize({kSidePanelWidth, kSidePanelHeight});
  sidePanel->setColor(kPanelBg);
  sidePanel->setOpacity(225);
  sidePanel->setPosition({sidePanelX, sidePanelY});
  m_mainLayer->addChild(sidePanel, 10);

  // ── Editor menu (all interactive buttons go here) ────────────────
  editorMenu = CCMenu::create();
  editorMenu->setPosition({0.f, 0.f});
  m_mainLayer->addChild(editorMenu, 12);

  // ── Helper lambdas ───────────────────────────────────────────────
  auto sideCenterX = sidePanelX;
  auto sideTop = sidePanelY + kSidePanelHeight / 2.f - 8.f;

  auto addSideLabel = [&](char const *text, float y, float scale = 0.32f,
                          cocos2d::ccColor3B color = ccc3(230, 230, 230)) {
    auto *label = CCLabelBMFont::create(text, "bigFont.fnt");
    label->setPosition({sideCenterX - 74.f, y});
    label->setAnchorPoint({0.f, 0.5f});
    label->setScale(scale);
    label->setColor(color);
    m_mainLayer->addChild(label, 13);
    return label;
  };

  auto addValueBox = [&](float y, CCLabelBMFont *&label) {
    auto *bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
    bg->setContentSize({78.f, 24.f});
    bg->setColor(kValueBoxBg);
    bg->setOpacity(140);
    bg->setPosition({sideCenterX + 20.f, y});
    m_mainLayer->addChild(bg, 13);

    label = CCLabelBMFont::create("", "bigFont.fnt");
    label->setScale(0.26f);
    label->setPosition({sideCenterX + 20.f, y});
    label->setColor({255, 255, 255});
    m_mainLayer->addChild(label, 14);
  };

  auto addButton = [&](char const *text, cocos2d::CCPoint pos,
                       cocos2d::SEL_MenuHandler callback, int tag = 0,
                       float scale = 0.30f) {
    auto *sprite = ButtonSprite::create(text);
    sprite->setScale(scale);
    auto *button = CCMenuItemSpriteExtra::create(sprite, this, callback);
    button->setPosition(pos);
    button->setTag(tag);
    editorMenu->addChild(button);
    return button;
  };

  // ── Panel contents ───────────────────────────────────────────────
  // "Selected" header
  addSideLabel("Selected", sideTop, 0.36f, kGold);

  // Frame row
  float rowFrame = sideTop - 30.f;
  addSideLabel("Frame:", rowFrame);

  auto *frameBg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
  frameBg->setContentSize({60.f, 24.f});
  frameBg->setColor(kValueBoxBg);
  frameBg->setOpacity(140);
  frameBg->setPosition({sideCenterX + 14.f, rowFrame});
  m_mainLayer->addChild(frameBg, 13);

  frameInput = TextInput::create(68, "Frame", "chatFont.fnt");
  frameInput->setPosition({sideCenterX + 14.f, rowFrame});
  frameInput->setScale(0.55f);
  frameInput->setString("0");
  frameInput->getInputNode()->setAllowedChars("0123456789");
  frameInput->getInputNode()->setMaxLabelLength(8);
  frameInput->getInputNode()->setDelegate(this);
  m_mainLayer->addChild(frameInput, 14);

  addButton("-10", {sideCenterX - 42.f, rowFrame},
            menu_selector(MacroTimelineLayer::nudgeFrame), -10, 0.24f);
  addButton("+10", {sideCenterX + 68.f, rowFrame},
            menu_selector(MacroTimelineLayer::nudgeFrame), 10, 0.24f);

  // Action row
  float rowAction = rowFrame - 36.f;
  addSideLabel("Action:", rowAction);
  addValueBox(rowAction, actionValueLabel);
  addButton("Swap", {sideCenterX + 68.f, rowAction},
            menu_selector(MacroTimelineLayer::switchAction), 0, 0.24f);

  // Button row
  float rowButton = rowAction - 36.f;
  addSideLabel("Button:", rowButton);
  addValueBox(rowButton, buttonValueLabel);
  addButton("Cycle", {sideCenterX + 68.f, rowButton},
            menu_selector(MacroTimelineLayer::switchButton), 0, 0.22f);

  // Player row
  float rowPlayer = rowButton - 36.f;
  addSideLabel("Player:", rowPlayer);
  addValueBox(rowPlayer, playerValueLabel);
  addButton("Switch", {sideCenterX + 68.f, rowPlayer},
            menu_selector(MacroTimelineLayer::switchPlayer), 0, 0.22f);

  // ── Bottom button rows ───────────────────────────────────────────
  float btnRow1 = rowPlayer - 34.f;
  float btnRow2 = btnRow1 - 28.f;

  addButton("Prev", {sideCenterX - 48.f, btnRow1},
            menu_selector(MacroTimelineLayer::onPrevInput), 0, 0.28f);
  addButton("Next", {sideCenterX + 2.f, btnRow1},
            menu_selector(MacroTimelineLayer::onNextInput), 0, 0.28f);
  addButton("Save", {sideCenterX + 56.f, btnRow1},
            menu_selector(MacroTimelineLayer::onSave), 0, 0.30f);

  addButton("Add", {sideCenterX - 30.f, btnRow2},
            menu_selector(MacroTimelineLayer::onAddInput), 0, 0.28f);
  addButton("Delete", {sideCenterX + 40.f, btnRow2},
            menu_selector(MacroTimelineLayer::onDeleteInput), 0, 0.28f);

  // ── Close button ─────────────────────────────────────────────────
  auto *closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
  closeSpr->setScale(0.65f);
  auto *closeBtn = CCMenuItemSpriteExtra::create(
      closeSpr, this, menu_selector(MacroTimelineLayer::onClose));
  closeBtn->setPosition({sidePanelX + kSidePanelWidth / 2.f - 2.f,
                         sidePanelY + kSidePanelHeight / 2.f - 2.f});
  editorMenu->addChild(closeBtn);

  // ── finish ───────────────────────────────────────────────────────
  setCurrentFrame(inputs.empty() ? 0 : static_cast<int>(inputs.front().frame));
  refreshStatus();
  rebuildMarkers();
  return true;
}

// ════════════════════════════════════════════════════════════════════
//  onClose()  —  unchanged logic
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::onClose(CCObject *) {
  if (!saved) {
    geode::createQuickPopup(
        "Exit Timeline", "<cr>Exit</c> without saving timeline changes?",
        "Cancel", "Exit", [this](auto, bool btn2) {
          if (!btn2)
            return;
#ifdef GEODE_IS_WINDOWS
          cocos2d::CCEGLView::sharedOpenGLView()->showCursor(
              cursorWasHidden ? false : true);
#endif
          if (!previewWasPlaying)
            Macro::resetState(true);
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
  if (!previewWasPlaying)
    Macro::resetState(true);
  this->setKeypadEnabled(false);
  this->setTouchEnabled(false);
  this->removeFromParentAndCleanup(true);
}

// ════════════════════════════════════════════════════════════════════
//  sortInputs()
// ════════════════════════════════════════════════════════════════════
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
  maxFrame =
      std::max(1, inputs.empty() ? 1 : static_cast<int>(inputs.back().frame));
}

// ════════════════════════════════════════════════════════════════════
//  markUnsaved()
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::markUnsaved() {
  saved = false;
  refreshStatus();
  rebuildMarkers();
}

// ════════════════════════════════════════════════════════════════════
//  refreshStatus()
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::refreshStatus() {
  statusLabel->setString(
      fmt::format("Inputs: {}{}", inputs.size(), saved ? "" : "  *unsaved")
          .c_str());
}

// ════════════════════════════════════════════════════════════════════
//  rebuildMarkers()  —  REDESIGNED for new layout coordinates
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::rebuildMarkers() {
  if (!markerNode)
    return;

  markerNode->clear();

  auto winSize = CCDirector::sharedDirector()->getWinSize();

  // Track geometry (must match setup())
  float barLeft = kEdgeMargin;
  float barRight = winSize.width - kEdgeMargin;
  float barWidth = barRight - barLeft;
  float trackWidth = barWidth - 24.f;
  float trackStartX = barLeft + 12.f;
  float trackCenterY = kEdgeMargin + kTrackY;

  // ── Frame ruler ticks above the track ────────────────────────────
  // Remove old tick labels first
  for (int tag = 9000; tag <= 9032; ++tag) {
    if (auto *node = m_mainLayer->getChildByTag(tag))
      node->removeFromParentAndCleanup(true);
  }

  int labelIndex = 0;
  int tickCount = std::max(1, maxFrame / 12);
  for (int frame = 0; frame <= maxFrame; frame += tickCount) {
    float x = trackStartX + trackWidth * frameToRatio(frame, maxFrame);

    // small tick line
    markerNode->drawSegment({x, trackCenterY + kTrackHeight / 2.f + 2.f},
                            {x, trackCenterY + kTrackHeight / 2.f + 14.f}, 0.5f,
                            ccc4f(1.f, 1.f, 1.f, 0.22f));

    // frame number label
    auto *label =
        CCLabelBMFont::create(std::to_string(frame).c_str(), "chatFont.fnt");
    label->setScale(0.3f);
    label->setOpacity(100);
    label->setPosition({x, trackCenterY + kTrackHeight / 2.f + 22.f});
    m_mainLayer->addChild(label, 6, 9000 + labelIndex++);
  }

  // ── Input markers ────────────────────────────────────────────────
  for (auto const &inp : inputs) {
    float x = trackStartX +
              trackWidth * frameToRatio(static_cast<int>(inp.frame), maxFrame);

    // Hold = green above track, Release = red below track
    if (inp.down) {
      // green marker ABOVE the track
      float extra = inp.player2 ? 6.f : 0.f;
      markerNode->drawSegment(
          {x, trackCenterY + kTrackHeight / 2.f + 2.f + extra},
          {x, trackCenterY + kTrackHeight / 2.f + 12.f + extra}, 0.8f,
          ccc4f(0.15f, 1.f, 0.3f, 0.9f));
    } else {
      // red marker BELOW the track
      float extra = inp.player2 ? 6.f : 0.f;
      markerNode->drawSegment(
          {x, trackCenterY - kTrackHeight / 2.f - 2.f - extra},
          {x, trackCenterY - kTrackHeight / 2.f - 10.f - extra}, 0.8f,
          ccc4f(1.f, 0.2f, 0.2f, 0.9f));
    }
  }

  // ── Highlight selected input ─────────────────────────────────────
  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(inputs.size())) {
    float x =
        trackStartX +
        trackWidth * frameToRatio(static_cast<int>(inputs[selectedIndex].frame),
                                  maxFrame);
    markerNode->drawSegment({x, trackCenterY - kTrackHeight / 2.f - 14.f},
                            {x, trackCenterY + kTrackHeight / 2.f + 14.f}, 1.3f,
                            ccc4f(1.f, 0.85f, 0.18f, 0.92f));
  }

  // ── Playhead (two-layer yellow dot) ──────────────────────────────
  float playheadX =
      trackStartX + trackWidth * frameToRatio(currentFrame, maxFrame);
  markerNode->drawDot({playheadX, trackCenterY}, 22.f,
                      ccc4f(1.f, 0.95f, 0.15f, 0.88f));
  markerNode->drawDot({playheadX, trackCenterY}, 15.f,
                      ccc4f(0.95f, 0.85f, 0.1f, 0.98f));
}

// ════════════════════════════════════════════════════════════════════
//  selectNearestInput()  —  unchanged
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::selectNearestInput() {
  if (inputs.empty()) {
    selectedIndex = -1;
    refreshSelectedInfo();
    return;
  }

  int bestIndex = 0;
  int bestDistance = std::abs(static_cast<int>(inputs[0].frame) - currentFrame);
  for (int index = 1; index < static_cast<int>(inputs.size()); ++index) {
    int distance =
        std::abs(static_cast<int>(inputs[index].frame) - currentFrame);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestIndex = index;
    }
  }

  selectedIndex = bestIndex;
  refreshSelectedInfo();
}

// ════════════════════════════════════════════════════════════════════
//  refreshSelectedInfo()  —  unchanged
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::refreshSelectedInfo() {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inputs.size())) {
    selectedLabel->setString("No input selected");
    frameInput->setString(std::to_string(currentFrame).c_str());
    if (frameValueLabel)
      frameValueLabel->setString("()");
    if (actionValueLabel)
      actionValueLabel->setString("()");
    if (buttonValueLabel)
      buttonValueLabel->setString("()");
    if (playerValueLabel)
      playerValueLabel->setString("()");
    return;
  }

  auto const &inp = inputs[selectedIndex];
  selectedLabel->setString(
      fmt::format("#{} at {}", selectedIndex + 1, inp.frame).c_str());
  frameInput->setString(std::to_string(inp.frame).c_str());
  if (frameValueLabel)
    frameValueLabel->setString(std::to_string(inp.frame).c_str());
  if (actionValueLabel)
    actionValueLabel->setString((inp.down ? "Hold" : "Release"));
  if (buttonValueLabel)
    buttonValueLabel->setString(getButtonName(inp.button).c_str());
  if (playerValueLabel)
    playerValueLabel->setString(inp.player2 ? "Two" : "One");
}

// ════════════════════════════════════════════════════════════════════
//  setCurrentFrame()  —  updated for new layout
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::setCurrentFrame(int frame, bool updateSlider) {
  currentFrame = std::clamp(frame, 0, maxFrame);
  frameLabel->setString(fmt::format("Frame: {}", currentFrame).c_str());
  if (timelineSlider && updateSlider)
    timelineSlider->setValue(frameToRatio(currentFrame, maxFrame));
  selectNearestInput();

  // Remove old frame-ruler labels & redraw everything
  for (int tag = 9000; tag <= 9032; ++tag) {
    if (auto *node = m_mainLayer->getChildByTag(tag))
      node->removeFromParentAndCleanup(true);
  }

  rebuildMarkers();
}

// ════════════════════════════════════════════════════════════════════
//  onSlider()  —  unchanged
// ════════════════════════════════════════════════════════════════════
void MacroTimelineLayer::onSlider(CCObject *) {
  float value = timelineSlider->getValue();
  setCurrentFrame(static_cast<int>(std::round(value * maxFrame)), false);
}

// ════════════════════════════════════════════════════════════════════
//  navigation / editing  —  all unchanged
// ════════════════════════════════════════════════════════════════════
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
