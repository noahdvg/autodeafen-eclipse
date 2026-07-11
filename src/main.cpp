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

    bool g_deafenKeyWasSent = false;
    bool g_leftShiftWasDown = false;
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
            int64_t{1}, int64_t{255}
        ));
    }

    void setShortcutVK(int vk) {
        Mod::get()->setSavedValue("shortcut-vk", static_cast<int64_t>(vk));
    }

    bool isExtendedKey(int vk) {
#ifdef GEODE_IS_WINDOWS
        switch (vk) {
            case VK_RMENU: case VK_RCONTROL: case VK_INSERT: case VK_DELETE:
            case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
            case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
            case VK_NUMLOCK: case VK_CANCEL: case VK_SNAPSHOT: case VK_DIVIDE:
                return true;
            default:
                return false;
        }
#else
        return false;
#endif
    }

    std::string keyName(int vk) {
#ifdef GEODE_IS_WINDOWS
        if (vk >= 'A' && vk <= 'Z') return std::string(1, static_cast<char>(vk));
        if (vk >= '0' && vk <= '9') return std::string(1, static_cast<char>(vk));
        if (vk >= VK_F1 && vk <= VK_F24) return fmt::format("F{}", vk - VK_F1 + 1);

        switch (vk) {
            case VK_SPACE: return "Space";
            case VK_TAB: return "Tab";
            case VK_RETURN: return "Enter";
            case VK_ESCAPE: return "Escape";
            case VK_BACK: return "Backspace";
            case VK_LSHIFT: return "Left Shift";
            case VK_RSHIFT: return "Right Shift";
            case VK_LCONTROL: return "Left Ctrl";
            case VK_RCONTROL: return "Right Ctrl";
            case VK_LMENU: return "Left Alt";
            case VK_RMENU: return "Right Alt";
            case VK_OEM_3: return "`";
            default: break;
        }

        UINT scan = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC) << 16;
        if (isExtendedKey(vk)) scan |= 1u << 24;
        char name[64]{};
        if (GetKeyNameTextA(static_cast<LONG>(scan), name, sizeof(name)) > 0) return name;
#endif
        return fmt::format("Key {}", vk);
    }

    bool sendShortcut() {
#ifdef GEODE_IS_WINDOWS
        const WORD vk = static_cast<WORD>(shortcutVK());
        const WORD scan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
        const DWORD extended = isExtendedKey(vk) ? KEYEVENTF_EXTENDEDKEY : 0;

        INPUT events[2]{};
        events[0].type = INPUT_KEYBOARD;
        events[0].ki.wVk = vk;
        events[0].ki.wScan = scan;
        events[0].ki.dwFlags = extended;

        events[1] = events[0];
        events[1].ki.dwFlags = extended | KEYEVENTF_KEYUP;

        const UINT sent = SendInput(2, events, sizeof(INPUT));
        if (sent != 2) {
            log::error("Auto Deafen SendInput failed: sent {}/2, Win32 error {}", sent, GetLastError());
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    void deafenForAttempt() {
        if (g_deafenKeyWasSent) return;
        if (sendShortcut()) g_deafenKeyWasSent = true;
    }

    void undeafenForAttempt() {
        if (!g_deafenKeyWasSent) return;
        sendShortcut();
        g_deafenKeyWasSent = false;
    }

    class AutoDeafenPopup final : public Popup {
    protected:
        CCLabelBMFont* m_enabledLabel = nullptr;
        CCLabelBMFont* m_percentLabel = nullptr;
        CCLabelBMFont* m_keyLabel = nullptr;
        CCLabelBMFont* m_statusLabel = nullptr;
        bool m_recording = false;
        bool m_waitForAllKeysUp = false;

        bool init() {
            if (!Popup::init(400.f, 250.f)) return false;
            setTitle("Auto Deafen");

            auto subtitle = CCLabelBMFont::create("Left Shift opens this menu during a level", "goldFont.fnt");
            subtitle->setScale(.4f);
            subtitle->setPosition({200.f, 207.f});
            m_mainLayer->addChild(subtitle);

            m_enabledLabel = CCLabelBMFont::create("", "bigFont.fnt");
            m_enabledLabel->setScale(.48f);
            m_enabledLabel->setAnchorPoint({0.f, .5f});
            m_enabledLabel->setPosition({35.f, 168.f});
            m_mainLayer->addChild(m_enabledLabel);

            auto toggle = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Toggle", 85, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .7f),
                this, menu_selector(AutoDeafenPopup::onToggle)
            );
            toggle->setPosition({325.f, 168.f});
            m_buttonMenu->addChild(toggle);

            auto percentTitle = CCLabelBMFont::create("Deafen at", "bigFont.fnt");
            percentTitle->setScale(.45f);
            percentTitle->setAnchorPoint({0.f, .5f});
            percentTitle->setPosition({35.f, 122.f});
            m_mainLayer->addChild(percentTitle);

            auto minus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("-", 38, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .8f),
                this, menu_selector(AutoDeafenPopup::onMinus)
            );
            minus->setPosition({215.f, 122.f});
            m_buttonMenu->addChild(minus);

            m_percentLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_percentLabel->setScale(.58f);
            m_percentLabel->setPosition({270.f, 122.f});
            m_mainLayer->addChild(m_percentLabel);

            auto plus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("+", 38, true, "bigFont.fnt", "GJ_button_04.png", 30.f, .8f),
                this, menu_selector(AutoDeafenPopup::onPlus)
            );
            plus->setPosition({325.f, 122.f});
            m_buttonMenu->addChild(plus);

            auto keyTitle = CCLabelBMFont::create("Toggle-deafen key", "bigFont.fnt");
            keyTitle->setScale(.43f);
            keyTitle->setAnchorPoint({0.f, .5f});
            keyTitle->setPosition({35.f, 77.f});
            m_mainLayer->addChild(keyTitle);

            m_keyLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_keyLabel->setScale(.52f);
            m_keyLabel->setPosition({245.f, 77.f});
            m_mainLayer->addChild(m_keyLabel);

            auto record = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Record", 80, true, "bigFont.fnt", "GJ_button_01.png", 30.f, .65f),
                this, menu_selector(AutoDeafenPopup::onRecord)
            );
            record->setPosition({335.f, 77.f});
            m_buttonMenu->addChild(record);

            auto test = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Test key", 90, true, "bigFont.fnt", "GJ_button_02.png", 30.f, .65f),
                this, menu_selector(AutoDeafenPopup::onTest)
            );
            test->setPosition({90.f, 36.f});
            m_buttonMenu->addChild(test);

            m_statusLabel = CCLabelBMFont::create("", "goldFont.fnt");
            m_statusLabel->setScale(.33f);
            m_statusLabel->setPosition({265.f, 36.f});
            m_mainLayer->addChild(m_statusLabel);

            refresh();
            scheduleUpdate();
            g_popupOpen = true;
            return true;
        }

        void refresh() {
            m_enabledLabel->setString(enabled() ? "Enabled" : "Disabled");
            m_percentLabel->setString(fmt::format("{:.0f}%", deafenPercent()).c_str());
            m_keyLabel->setString(m_recording ? "Press one key..." : keyName(shortcutVK()).c_str());
            if (!m_recording && std::string(m_statusLabel->getString()).empty()) {
                m_statusLabel->setString("Dies / completes: presses it again");
            }
        }

        void setPercent(float value) {
            Mod::get()->setSettingValue("deafen-percent", static_cast<double>(std::clamp(value, 0.f, 100.f)));
            refresh();
        }

        void onToggle(CCObject*) {
            const bool next = !enabled();
            Mod::get()->setSettingValue("enabled", next);
            if (!next) undeafenForAttempt();
            m_statusLabel->setString(next ? "Auto Deafen enabled" : "Auto Deafen disabled");
            refresh();
        }

        void onMinus(CCObject*) { setPercent(deafenPercent() - 1.f); }
        void onPlus(CCObject*) { setPercent(deafenPercent() + 1.f); }

        void onRecord(CCObject*) {
            m_recording = true;
            m_waitForAllKeysUp = true;
            m_statusLabel->setString("Release keys, then press one key");
            refresh();
        }

        void onTest(CCObject*) {
            if (sendShortcut()) m_statusLabel->setString("Key pressed once");
            else m_statusLabel->setString("Key press failed");
        }

        void update(float) override {
#ifdef GEODE_IS_WINDOWS
            if (!m_recording) return;

            bool anyDown = false;
            for (int vk = 1; vk <= 255; ++vk) {
                if ((GetAsyncKeyState(vk) & 0x8000) == 0) continue;
                anyDown = true;

                if (!m_waitForAllKeysUp && vk != VK_LBUTTON && vk != VK_RBUTTON &&
                    vk != VK_MBUTTON && vk != VK_XBUTTON1 && vk != VK_XBUTTON2) {
                    setShortcutVK(vk); // first key only
                    m_recording = false;
                    m_statusLabel->setString(fmt::format("Recorded {}", keyName(vk)).c_str());
                    refresh();
                    return;
                }
            }
            if (!anyDown) m_waitForAllKeysUp = false;
#endif
        }

        void onClose(CCObject* sender) override {
            g_popupOpen = false;
            m_recording = false;
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
            if (!value) undeafenForAttempt();
        }).setDescription("Left Shift in a level opens the Auto Deafen setup menu.");
        eclipse::config::set("noah.autodeafen-eclipse/enabled", enabled());

        player.addButton("Auto Deafen Setup", [] { openPopup(); })
            .setDescription("Set percentage and record a readable key such as F8 or J.");
    }
}

$on_mod(Loaded) {
    registerEclipseControls();
}

class $modify(AutoDeafenPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        g_deafenKeyWasSent = false;
        g_leftShiftWasDown = false;
        g_popupOpen = false;
        return true;
    }

    void resetLevel() {
        undeafenForAttempt();
        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        undeafenForAttempt();
        PlayLayer::destroyPlayer(player, object);
    }

    void levelComplete() {
        undeafenForAttempt();
        PlayLayer::levelComplete();
    }

    void onQuit() {
        undeafenForAttempt();
        g_popupOpen = false;
        PlayLayer::onQuit();
    }

    void update(float dt) {
        PlayLayer::update(dt);

#ifdef GEODE_IS_WINDOWS
        const bool shiftDown = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
        if (shiftDown && !g_leftShiftWasDown && !g_popupOpen) openPopup();
        g_leftShiftWasDown = shiftDown;
#endif

        if (!enabled() || m_isPracticeMode || g_popupOpen || g_deafenKeyWasSent) return;
        if (getCurrentPercent() >= deafenPercent()) deafenForAttempt();
    }
};
