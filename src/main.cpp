#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <eclipse/eclipse.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

using namespace geode::prelude;

namespace {
    constexpr int DEFAULT_KEY = 0x77; // F8

    bool g_isDeafened = false;
    bool g_leftShiftWasDown = false;
    bool g_rightShiftWasDown = false;
    bool g_popupOpen = false;

    bool enabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    float deafenPercent() {
        return std::clamp(
            static_cast<float>(Mod::get()->getSettingValue<double>("deafen-percent")),
            0.f,
            100.f
        );
    }

    int shortcutVK() {
        return static_cast<int>(std::clamp(
            Mod::get()->getSavedValue<int64_t>("shortcut-vk", DEFAULT_KEY),
            int64_t{1},
            int64_t{255}
        ));
    }

    void setShortcutVK(int vk) {
        Mod::get()->setSavedValue("shortcut-vk", static_cast<int64_t>(vk));
    }

    std::string keyName(int vk) {
#ifdef GEODE_IS_WINDOWS
        if (vk >= 'A' && vk <= 'Z') return std::string(1, static_cast<char>(vk));
        if (vk >= '0' && vk <= '9') return std::string(1, static_cast<char>(vk));
        if (vk >= VK_F1 && vk <= VK_F24) return fmt::format("F{}", vk - VK_F1 + 1);

        UINT scan = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC) << 16;
        if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN ||
            vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME ||
            vk == VK_INSERT || vk == VK_DELETE || vk == VK_DIVIDE || vk == VK_NUMLOCK ||
            vk == VK_RCONTROL || vk == VK_RMENU) {
            scan |= 1u << 24;
        }
        char name[64]{};
        if (GetKeyNameTextA(static_cast<LONG>(scan), name, sizeof(name)) > 0) {
            return name;
        }
#endif
        return fmt::format("Key {}", vk);
    }

    void pressShortcut() {
#ifdef GEODE_IS_WINDOWS
        auto vk = static_cast<WORD>(shortcutVK());
        auto scan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));

        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = vk;
        inputs[0].ki.wScan = scan;

        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        auto sent = SendInput(2, inputs, sizeof(INPUT));
        if (sent != 2) {
            log::warn("Auto Deafen could only send {} of 2 keyboard events (error {})", sent, GetLastError());
        }
#endif
    }

    void deafenNow() {
        if (g_isDeafened) return;
        pressShortcut();
        g_isDeafened = true;
    }

    void undeafenNow() {
        if (!g_isDeafened) return;
        pressShortcut();
        g_isDeafened = false;
    }

    class AutoDeafenPopup : public Popup {
    protected:
        CCLabelBMFont* m_deafenLabel = nullptr;
        CCLabelBMFont* m_keyLabel = nullptr;
        CCLabelBMFont* m_enabledLabel = nullptr;
        bool m_listeningForKey = false;
        bool m_ignoreHeldKeys = false;

        bool init() {
            if (!Popup::init(370.f, 220.f)) return false;

            this->setTitle("Auto Deafen");

            auto help = CCLabelBMFont::create("Press either Shift key to open this menu", "goldFont.fnt");
            help->setScale(.42f);
            help->setPosition({185.f, 181.f});
            m_mainLayer->addChild(help);

            m_enabledLabel = CCLabelBMFont::create("", "bigFont.fnt");
            m_enabledLabel->setScale(.48f);
            m_enabledLabel->setPosition({145.f, 148.f});
            m_mainLayer->addChild(m_enabledLabel);

            auto toggleBtn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Toggle"), this, menu_selector(AutoDeafenPopup::onToggle)
            );
            toggleBtn->setPosition({292.f, 148.f});
            m_buttonMenu->addChild(toggleBtn);

            auto percentTitle = CCLabelBMFont::create("Deafen at", "bigFont.fnt");
            percentTitle->setScale(.45f);
            percentTitle->setAnchorPoint({0.f, .5f});
            percentTitle->setPosition({35.f, 103.f});
            m_mainLayer->addChild(percentTitle);

            auto minus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("-", 35, true, "bigFont.fnt", "GJ_button_04.png", 28.f, .8f),
                this, menu_selector(AutoDeafenPopup::onDeafenMinus)
            );
            minus->setPosition({190.f, 103.f});
            m_buttonMenu->addChild(minus);

            m_deafenLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_deafenLabel->setScale(.55f);
            m_deafenLabel->setPosition({245.f, 103.f});
            m_mainLayer->addChild(m_deafenLabel);

            auto plus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("+", 35, true, "bigFont.fnt", "GJ_button_04.png", 28.f, .8f),
                this, menu_selector(AutoDeafenPopup::onDeafenPlus)
            );
            plus->setPosition({300.f, 103.f});
            m_buttonMenu->addChild(plus);

            auto keyTitle = CCLabelBMFont::create("Discord keybind", "bigFont.fnt");
            keyTitle->setScale(.45f);
            keyTitle->setAnchorPoint({0.f, .5f});
            keyTitle->setPosition({35.f, 57.f});
            m_mainLayer->addChild(keyTitle);

            m_keyLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_keyLabel->setScale(.48f);
            m_keyLabel->setPosition({230.f, 57.f});
            m_mainLayer->addChild(m_keyLabel);

            auto bindBtn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Set Key", 70, true, "bigFont.fnt", "GJ_button_01.png", 28.f, .65f),
                this, menu_selector(AutoDeafenPopup::onBind)
            );
            bindBtn->setPosition({310.f, 57.f});
            m_buttonMenu->addChild(bindBtn);

            auto note = CCLabelBMFont::create("It undeafens when you die, complete, or leave the level.", "goldFont.fnt");
            note->setScale(.35f);
            note->setPosition({185.f, 24.f});
            m_mainLayer->addChild(note);

            refreshLabels();
            this->scheduleUpdate();
            g_popupOpen = true;
            return true;
        }

        void setPercent(float value) {
            Mod::get()->setSettingValue("deafen-percent", static_cast<double>(std::clamp(value, 0.f, 100.f)));
            refreshLabels();
        }

        void refreshLabels() {
            m_enabledLabel->setString(enabled() ? "Enabled" : "Disabled");
            m_deafenLabel->setString(fmt::format("{:.0f}%", deafenPercent()).c_str());
            m_keyLabel->setString(m_listeningForKey ? "Press a key..." : keyName(shortcutVK()).c_str());
        }

        void onToggle(CCObject*) {
            auto next = !enabled();
            Mod::get()->setSettingValue("enabled", next);
            if (!next) undeafenNow();
            refreshLabels();
        }

        void onDeafenMinus(CCObject*) { setPercent(deafenPercent() - 1.f); }
        void onDeafenPlus(CCObject*)  { setPercent(deafenPercent() + 1.f); }

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
                        setShortcutVK(vk);
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
            if (!value) undeafenNow();
        }).setDescription("Press Left Shift or Right Shift in-game to open setup.");
        eclipse::config::set("noah.autodeafen-eclipse/enabled", enabled());

        player.addButton("Auto Deafen Setup", [] { openPopup(); })
            .setDescription("Choose the percent and capture a readable keybind such as F8 or J.");
    }
}

$on_mod(Loaded) {
    registerEclipseControls();
}

class $modify(AutoDeafenPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_isDeafened = false;
        g_leftShiftWasDown = false;
        g_rightShiftWasDown = false;
        g_popupOpen = false;
        return true;
    }

    void resetLevel() {
        // A restart after death must restore voice before the next attempt.
        undeafenNow();
        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        undeafenNow();
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        undeafenNow();
        PlayLayer::levelComplete();
    }

    void onQuit() {
        undeafenNow();
        g_popupOpen = false;
        PlayLayer::onQuit();
    }

    void update(float dt) {
        PlayLayer::update(dt);

#ifdef GEODE_IS_WINDOWS
        bool leftDown = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
        bool rightDown = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;

        if (((leftDown && !g_leftShiftWasDown) || (rightDown && !g_rightShiftWasDown)) && !g_popupOpen) {
            openPopup();
        }

        g_leftShiftWasDown = leftDown;
        g_rightShiftWasDown = rightDown;
#endif

        if (!enabled() || m_isPracticeMode || g_popupOpen || g_isDeafened) return;

        if (getCurrentPercent() >= deafenPercent()) {
            deafenNow();
        }
    }
};
