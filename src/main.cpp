#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <eclipse/eclipse.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

namespace {
    bool g_deafenSent = false;
    bool g_undeafenSent = false;
    bool g_shiftWasDown = false;
    bool g_popupOpen = false;

    bool enabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    float deafenPercent() {
        return std::clamp(static_cast<float>(Mod::get()->getSettingValue<double>("deafen-percent")), 0.f, 100.f);
    }

    float undeafenPercent() {
        return std::clamp(static_cast<float>(Mod::get()->getSettingValue<double>("undeafen-percent")), 0.f, 100.f);
    }

    int shortcutVK() {
        return static_cast<int>(std::clamp(Mod::get()->getSettingValue<int64_t>("shortcut-vk"), int64_t{1}, int64_t{255}));
    }

    void resetAttemptState() {
        g_deafenSent = false;
        g_undeafenSent = false;
    }

    std::string keyName(int vk) {
#ifdef GEODE_IS_WINDOWS
        UINT scan = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC) << 16;
        if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN ||
            vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME ||
            vk == VK_INSERT || vk == VK_DELETE || vk == VK_DIVIDE || vk == VK_NUMLOCK) {
            scan |= 1u << 24;
        }
        char name[64]{};
        if (GetKeyNameTextA(static_cast<LONG>(scan), name, sizeof(name)) > 0)
            return name;
#endif
        return fmt::format("Key {}", vk);
    }

    void pressShortcut() {
#ifdef GEODE_IS_WINDOWS
        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = static_cast<WORD>(shortcutVK());
        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
#endif
    }

    class AutoDeafenPopup : public Popup {
    protected:
        CCLabelBMFont* m_deafenLabel = nullptr;
        CCLabelBMFont* m_undeafenLabel = nullptr;
        CCLabelBMFont* m_keyLabel = nullptr;
        CCLabelBMFont* m_enabledLabel = nullptr;
        bool m_listeningForKey = false;
        bool m_ignoreHeldKeys = false;

        bool init() {
            if (!Popup::init(360.f, 245.f)) return false;

            this->setTitle("Auto Deafen");

            auto help = CCLabelBMFont::create("Press Left Shift anytime to open this menu", "goldFont.fnt");
            help->setScale(.42f);
            help->setPosition({180.f, 205.f});
            m_mainLayer->addChild(help);

            m_enabledLabel = CCLabelBMFont::create("", "bigFont.fnt");
            m_enabledLabel->setScale(.48f);
            m_enabledLabel->setPosition({180.f, 174.f});
            m_mainLayer->addChild(m_enabledLabel);

            auto toggleBtn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Toggle"), this, menu_selector(AutoDeafenPopup::onToggle)
            );
            toggleBtn->setPosition({292.f, 174.f});
            m_buttonMenu->addChild(toggleBtn);

            addPercentRow("Deafen at", 132.f, true);
            addPercentRow("Undeafen at", 91.f, false);

            auto keyTitle = CCLabelBMFont::create("Deafen keybind", "bigFont.fnt");
            keyTitle->setScale(.45f);
            keyTitle->setAnchorPoint({0.f, .5f});
            keyTitle->setPosition({35.f, 48.f});
            m_mainLayer->addChild(keyTitle);

            m_keyLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_keyLabel->setScale(.48f);
            m_keyLabel->setPosition({230.f, 48.f});
            m_mainLayer->addChild(m_keyLabel);

            auto bindBtn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Set Key", 70, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .65f),
                this, menu_selector(AutoDeafenPopup::onBind)
            );
            bindBtn->setPosition({310.f, 48.f});
            m_buttonMenu->addChild(bindBtn);

            refreshLabels();
            this->scheduleUpdate();
            g_popupOpen = true;
            return true;
        }

        void addPercentRow(char const* title, float y, bool deafen) {
            auto label = CCLabelBMFont::create(title, "bigFont.fnt");
            label->setScale(.45f);
            label->setAnchorPoint({0.f, .5f});
            label->setPosition({35.f, y});
            m_mainLayer->addChild(label);

            auto minus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("-", 35, true, "bigFont.fnt", "GJ_button_04.png", 28.f, .8f),
                this, deafen ? menu_selector(AutoDeafenPopup::onDeafenMinus)
                              : menu_selector(AutoDeafenPopup::onUndeafenMinus)
            );
            minus->setPosition({190.f, y});
            m_buttonMenu->addChild(minus);

            auto value = CCLabelBMFont::create("", "goldFont.fnt");
            value->setScale(.55f);
            value->setPosition({245.f, y});
            m_mainLayer->addChild(value);
            if (deafen) m_deafenLabel = value;
            else m_undeafenLabel = value;

            auto plus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("+", 35, true, "bigFont.fnt", "GJ_button_04.png", 28.f, .8f),
                this, deafen ? menu_selector(AutoDeafenPopup::onDeafenPlus)
                              : menu_selector(AutoDeafenPopup::onUndeafenPlus)
            );
            plus->setPosition({300.f, y});
            m_buttonMenu->addChild(plus);
        }

        void setPercent(char const* setting, float value) {
            Mod::get()->setSettingValue(setting, static_cast<double>(std::clamp(value, 0.f, 100.f)));
            resetAttemptState();
            refreshLabels();
        }

        void refreshLabels() {
            m_enabledLabel->setString(enabled() ? "Enabled" : "Disabled");
            m_deafenLabel->setString(fmt::format("{:.0f}%", deafenPercent()).c_str());
            m_undeafenLabel->setString(fmt::format("{:.0f}%", undeafenPercent()).c_str());
            m_keyLabel->setString(m_listeningForKey ? "Press a key..." : keyName(shortcutVK()).c_str());
        }

        void onToggle(CCObject*) {
            Mod::get()->setSettingValue("enabled", !enabled());
            resetAttemptState();
            refreshLabels();
        }
        void onDeafenMinus(CCObject*) { setPercent("deafen-percent", deafenPercent() - 1.f); }
        void onDeafenPlus(CCObject*)  { setPercent("deafen-percent", deafenPercent() + 1.f); }
        void onUndeafenMinus(CCObject*) { setPercent("undeafen-percent", undeafenPercent() - 1.f); }
        void onUndeafenPlus(CCObject*)  { setPercent("undeafen-percent", undeafenPercent() + 1.f); }

        void onBind(CCObject*) {
            m_listeningForKey = true;
            m_ignoreHeldKeys = true;
            refreshLabels();
        }

        void update(float) override {
#ifdef GEODE_IS_WINDOWS
            if (!m_listeningForKey) return;

            bool anyHeld = false;
            for (int vk = 1; vk <= 255; ++vk) {
                if (GetAsyncKeyState(vk) & 0x8000) {
                    anyHeld = true;
                    if (!m_ignoreHeldKeys && vk != VK_LBUTTON && vk != VK_RBUTTON) {
                        Mod::get()->setSettingValue("shortcut-vk", static_cast<int64_t>(vk));
                        m_listeningForKey = false;
                        refreshLabels();
                        return;
                    }
                }
            }
            if (!anyHeld) m_ignoreHeldKeys = false;
#endif
        }

        void onClose(CCObject* sender) override {
            g_popupOpen = false;
            Popup::onClose(sender);
        }

    public:
        static AutoDeafenPopup* create() {
            auto ret = new AutoDeafenPopup();
            if (ret && ret->init()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }
    };

    void openPopup() {
        if (g_popupOpen) return;
        if (auto popup = AutoDeafenPopup::create()) popup->show();
    }

    void registerEclipseControls() {
        auto player = eclipse::MenuTab::find("Player");
        player.addToggle("noah.autodeafen-eclipse/enabled", "Auto Deafen", [](bool value) {
            Mod::get()->setSettingValue("enabled", value);
            resetAttemptState();
        }).setDescription("Press Left Shift in-game to open the Auto Deafen setup menu.");
        eclipse::config::set("noah.autodeafen-eclipse/enabled", enabled());

        player.addButton("Auto Deafen Setup", [] { openPopup(); })
            .setDescription("Customize deafen %, undeafen %, and the voice-app deafen keybind.");
    }
}

$on_mod(Loaded) {
    registerEclipseControls();
}

class $modify(AutoDeafenPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        resetAttemptState();
        g_shiftWasDown = false;
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        resetAttemptState();
    }

    void onQuit() {
        resetAttemptState();
        g_popupOpen = false;
        PlayLayer::onQuit();
    }

    void update(float dt) {
        PlayLayer::update(dt);

#ifdef GEODE_IS_WINDOWS
        bool shiftDown = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
        if (shiftDown && !g_shiftWasDown && !g_popupOpen) openPopup();
        g_shiftWasDown = shiftDown;
#endif

        if (!enabled() || m_isPracticeMode || g_popupOpen) return;

        float progress = getCurrentPercent();
        if (!g_deafenSent && progress >= deafenPercent()) {
            pressShortcut();
            g_deafenSent = true;
        }
        if (g_deafenSent && !g_undeafenSent && progress >= undeafenPercent()) {
            pressShortcut();
            g_undeafenSent = true;
        }
    }
};
