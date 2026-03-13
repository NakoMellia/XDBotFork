#pragma once

#include "../pathfinder_test.hpp"

#include <algorithm>

class PathFinderTestSettingsLayer : public geode::Popup,
                                    public TextInputDelegate {
private:
  TextInput *searchAheadInput = nullptr;
  TextInput *holdFramesInput = nullptr;
  TextInput *simulationInput = nullptr;
  CCMenuItemToggler *autoApplyToggle = nullptr;

  STATIC_CREATE(PathFinderTestSettingsLayer, 230, 180)

  void textChanged(CCTextInputNode *) override {
    auto *mod = Mod::get();
    auto searchAhead =
        geode::utils::numFromString<int>(searchAheadInput->getString())
            .unwrapOr(20);
    auto holdFrames =
        geode::utils::numFromString<int>(holdFramesInput->getString())
            .unwrapOr(2);
    auto simulation =
        geode::utils::numFromString<int>(simulationInput->getString())
            .unwrapOr(180);

    mod->setSavedValue("pathfinder_test_search_ahead",
                       std::clamp(searchAhead, 1, 120));
    mod->setSavedValue("pathfinder_test_hold_frames",
                       std::clamp(holdFrames, 1, 30));
    mod->setSavedValue("pathfinder_test_simulation_frames",
                       std::clamp(simulation, 30, 600));
    PathFinderTest::loadSettings();
  }

  void onToggle(CCObject *) {
    Mod::get()->setSavedValue("pathfinder_test_auto_apply",
                              !autoApplyToggle->isToggled());
    PathFinderTest::loadSettings();
  }

  bool setup() {
    setTitle("Path Test");
    m_title->setScale(0.625f);
    m_title->setPositionY(162);

    auto addLabel = [&](char const *text, cocos2d::CCPoint pos) {
      auto *label = CCLabelBMFont::create(text, "bigFont.fnt");
      label->setPosition(pos);
      label->setScale(0.3f);
      m_mainLayer->addChild(label);
    };

    auto addInput = [&](TextInput *&input, char const *placeholder,
                        std::string const &value, cocos2d::CCPoint pos,
                        int maxLength) {
      input = TextInput::create(70, placeholder, "chatFont.fnt");
      input->setPosition(pos);
      input->setString(value.c_str());
      input->getInputNode()->setDelegate(this);
      input->getInputNode()->setAllowedChars("0123456789");
      input->getInputNode()->setMaxLabelLength(maxLength);
      m_mainLayer->addChild(input);
    };

    addLabel("Search Ahead", {m_size.width / 2, 132});
    addInput(searchAheadInput, "Ahead",
             std::to_string(Mod::get()->getSavedValue<int64_t>(
                 "pathfinder_test_search_ahead", 20)),
             {m_size.width / 2, 112}, 3);

    addLabel("Hold Frames", {m_size.width / 2, 88});
    addInput(holdFramesInput, "Hold",
             std::to_string(Mod::get()->getSavedValue<int64_t>(
                 "pathfinder_test_hold_frames", 2)),
             {m_size.width / 2, 68}, 2);

    addLabel("Simulation Frames", {m_size.width / 2, 44});
    addInput(simulationInput, "Sim",
             std::to_string(Mod::get()->getSavedValue<int64_t>(
                 "pathfinder_test_simulation_frames", 180)),
             {m_size.width / 2, 24}, 3);

    addLabel("Auto Apply", {68, 152});
    autoApplyToggle = CCMenuItemToggler::create(
        CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
        CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"), this,
        menu_selector(PathFinderTestSettingsLayer::onToggle));
    autoApplyToggle->setPosition({24, 152});
    autoApplyToggle->setScale(0.6f);
    autoApplyToggle->toggle(
        Mod::get()->getSavedValue<bool>("pathfinder_test_auto_apply", true));
    m_buttonMenu->addChild(autoApplyToggle);

    return true;
  }

public:
  void open(CCObject *) { create()->show(); }
};
