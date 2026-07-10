#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <SpoutSender.h>
#include <Windows.h>
#include <gl/GL.h>
#include <chrono>
#include <mutex>
#include <unordered_set>

using namespace geode::prelude;

namespace sol {
enum class RenderPass { Normal, OriginalCapture, LayoutDisplay };
thread_local RenderPass g_pass = RenderPass::Normal;
thread_local bool g_rendering = false;

struct LevelCache {
    std::string compressed;
    int levelID = 0;
    void store(GJGameLevel* level) {
        compressed = level ? std::string(level->m_levelString) : std::string();
        levelID = level ? level->m_levelID.value() : 0;
    }
    void clear() { compressed.clear(); levelID = 0; }
};

class Capture final {
public:
    SpoutSender sender;
    GLuint fbo = 0;
    GLuint texture = 0;
    GLsizei width = 0;
    GLsizei height = 0;
    std::chrono::steady_clock::time_point last{};
    bool senderNamed = false;

    ~Capture() { release(); }

    void release() {
        sender.ReleaseSender();
        senderNamed = false;
        if (texture) glDeleteTextures(1, &texture);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        texture = fbo = 0;
        width = height = 0;
    }

    bool ensure(GLsizei w, GLsizei h) {
        if (w < 1 || h < 1) return false;
        if (fbo && width == w && height == h) return true;
        release();
        width = w; height = h;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        auto ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ok;
    }

    bool due() {
        const int fps = std::max(1, Mod::get()->getSettingValue<int64_t>("capture-fps"));
        auto now = std::chrono::steady_clock::now();
        if (last.time_since_epoch().count() && now - last < std::chrono::microseconds(1000000 / fps)) return false;
        last = now;
        return true;
    }

    void send(GLuint hostFbo) {
        if (!senderNamed) {
            auto name = Mod::get()->getSettingValue<std::string>("sender-name");
            sender.SetSenderName(name.c_str());
            senderNamed = true;
        }
        sender.SendTexture(texture, GL_TEXTURE_2D, width, height, true, hostFbo);
    }
};

LevelCache g_cache;
Capture g_capture;

// Independent semantic classification. UI/pause/menu nodes never enter this hook,
// so they remain visible in both the game and Spout output.
bool hideInLayout(GameObject* object) {
    if (!object || g_pass != RenderPass::LayoutDisplay) return false;
    const int id = object->m_objectID;
    static const std::unordered_set<int> triggerIDs = {
        901, 1006, 1007, 1049, 1268, 1346, 1347, 1520, 1585, 1595,
        1611, 1612, 1613, 1811, 1812, 1814, 1912, 1913, 1914, 2062,
        2063, 2067, 2902, 3006, 3007, 3008, 3016, 3022, 3027, 3032,
        3033, 3602, 3603, 3604, 3613, 3660, 3661
    };
    if (Mod::get()->getSettingValue<bool>("layout-hide-triggers") && triggerIDs.contains(id)) return true;
    // NoTouch is the stable global semantic marker used by decoration objects.
    if (Mod::get()->getSettingValue<bool>("layout-hide-decoration") && object->m_isNoTouch) return true;
    return false;
}
}

class $modify(SolGameObject, GameObject) {
    void visit() {
        if (sol::hideInLayout(this)) return;
        GameObject::visit();
    }
};

class $modify(SolPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        sol::g_cache.store(level); // exact original compressed level data, before any other local transformation
        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }

    void onQuit() {
        sol::g_capture.release();
        sol::g_cache.clear();
        PlayLayer::onQuit();
    }

    void visit() {
        if (sol::g_rendering || !Mod::get()->getSettingValue<bool>("enabled")) {
            PlayLayer::visit();
            return;
        }
        sol::g_rendering = true;

        GLint oldFbo = 0;
        GLint viewport[4]{};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo);
        glGetIntegerv(GL_VIEWPORT, viewport);

        if (sol::g_capture.due() && sol::g_capture.ensure(viewport[2], viewport[3])) {
            glBindFramebuffer(GL_FRAMEBUFFER, sol::g_capture.fbo);
            glViewport(0, 0, sol::g_capture.width, sol::g_capture.height);
            glClearColor(0.f, 0.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            sol::g_pass = sol::RenderPass::OriginalCapture;
            PlayLayer::visit();
            sol::g_capture.send(sol::g_capture.fbo);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFbo));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        sol::g_pass = sol::RenderPass::LayoutDisplay;
        PlayLayer::visit();
        sol::g_pass = sol::RenderPass::Normal;
        sol::g_rendering = false;
    }
};

$on_mod(Loaded) {
    log::info("SOL Spout Layout loaded (Geode 5.7.1 / GD 2.2081)");
}
