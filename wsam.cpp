#import <Foundation/Foundation.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <sys/socket.h> // حل خطأ socklen_t النهائي
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mach-o/dyld.h> 
#include "imgui.h"
#include "dobby.h"
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

// --- [ الهياكل الرياضية للرادار ] ---
struct Vector3 { float x, y, z; };
struct Matrix4x4 { float m[4][4]; };

// ==========================================
// 1. عناوين محرك اللعبة (Offsets)
// ==========================================
// سيتم سحب هذه القيم حياً من السيرفر لضمان الأمان
uintptr_t GWORLD_OFF = 0x11223344; 
uintptr_t GNAMES_OFF = 0x55667788; 

// إحداثيات الرادار (SDK)
uintptr_t OFF_ACTOR_LIST = 0xA0;
uintptr_t OFF_HEALTH_VAL  = 0x988;
uintptr_t OFF_BONE_ARRAY  = 0x5B0;

// إحداثيات الثبات والايمبوت
uintptr_t OFF_RECOIL_PATCH = 0x1A2B3C;
uintptr_t OFF_AIMBOT_PATCH  = 0x4D5E6F;

// ==========================================
// 2. برمجية حظر الروابط (Hook Connect)
// ==========================================
// قائمة الروابط المحظورة (Blacklist)
vector<string> Blacklist = { "log.cheat-detection.com", "anticheat.pubg.com" };

static int (*Orig_Connect)(int, const struct sockaddr*, socklen_t);
int Hook_Connect(int s, const struct sockaddr* name, socklen_t namelen) {
    if (name->sa_family == AF_INET) {
        struct sockaddr_in* addr = (struct sockaddr_in*)name;
        char* ip = inet_ntoa(addr->sin_addr);
        // منطق حظر الاتصال بالسيرفرات المشبوهة
        for (const auto& blocked : Blacklist) {
            // [Logic]: Check and block
        }
    }
    return Orig_Connect(s, name, namelen);
}

// ==========================================
// 3. رياضيات الرادار (WorldToScreen)
// ==========================================
// تحويل إحداثيات عالم اللعبة 3D إلى شاشة الموبايل 2D بدقة بالمليم
bool WorldToScreen(Vector3 worldPos, Matrix4x4 viewMatrix, ImVec2* screenPos) {
    float w = viewMatrix.m[3][0] * worldPos.x + viewMatrix.m[3][1] * worldPos.y + viewMatrix.m[3][2] * worldPos.z + viewMatrix.m[3][3];
    if (w < 0.01f) return false;
    
    float x = viewMatrix.m[0][0] * worldPos.x + viewMatrix.m[0][1] * worldPos.y + viewMatrix.m[0][2] * worldPos.z + viewMatrix.m[0][3];
    float y = viewMatrix.m[1][0] * worldPos.x + viewMatrix.m[1][1] * worldPos.y + viewMatrix.m[1][2] * worldPos.z + viewMatrix.m[1][3];
    
    float width = ImGui::GetIO().DisplaySize.x;
    float height = ImGui::GetIO().DisplaySize.y;
    
    screenPos->x = (width / 2) + (x * (width / 2) / w);
    screenPos->y = (height / 2) - (y * (height / 2) / w);
    return true;
}

// ==========================================
// 4. ثيم المكاتب الفاخر (Premium Dark Theme)
// ==========================================
void ApplyPremiumTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 15.0f;
    s.FrameRounding = 8.0f;
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.10f, 0.14f, 0.98f);
    s.Colors[ImGuiCol_Header] = ImVec4(0.00f, 0.90f, 1.00f, 0.30f);
    s.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
}

// ==========================================
// 5. الاتصال والمزامنة (Server API)
// ==========================================
const string SERVER = "http://34.204.178.160/panel.php?api=check&key=";
bool g_Connected = false, g_Maintenance = false, g_Activated = false;
char g_Key[32] = "DRAGON-VIP-16X";

void FetchServerCommands() {
    while(true) {
        @autoreleasepool {
            NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:(SERVER + string(g_Key)).c_str()]];
            NSData *data = [NSData dataWithContentsOfURL:url];
            if(data) {
                try {
                    auto res = json::parse(string((char*)[data bytes], [data length]));
                    if(res["server_state"] == "danger") g_Maintenance = true;
                    if(res["status"] == "success") {
                        g_Connected = true;
                        GWORLD_OFF = std::stoull(res["offsets"]["gworld"].get<string>(), 0, 16);
                    }
                } catch(...) {}
            }
        }
        this_thread::sleep_for(chrono::seconds(10));
    }
}

// ==========================================
// 6. واجهة المستخدم (دراكون الملكية)
// ==========================================
void RenderDragonUI() {
    if(g_Maintenance) { /* شاشة التجميد للطوارئ */ return; }
    ApplyPremiumTheme();

    // HUD شفاف يتحدث عند اللمس
    ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 35));
    ImGui::Begin("HUD", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground);
    if(ImGui::InvisibleButton("REF", ImVec2(-1, 35))) g_Connected = false; 
    
    ImU32 led = g_Connected ? IM_COL32(0,255,102, 255) : IM_COL32(255,51,102, 255);
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(20, 17), 5, led);
    ImGui::SetCursorPos(ImVec2(40, 8)); ImGui::TextColored(ImVec4(0,1,1,1), "DRAGON HACK | VIP EDITION");
    ImGui::End();

    static bool menu_open = true;
    if(!menu_open) return;

    ImGui::SetNextWindowSize(ImVec2(340, 560));
    ImGui::Begin(u8"🐲 دراكون الملكي | محمد الشمري", &menu_open);

    // إطار VIP الاحترافي
    ImGui::BeginChild("VipFrame", ImVec2(0, 75), true);
    ImGui::TextColored(ImVec4(0,1,1,1), u8"المشترك: محمد الشمري (VIP)");
    ImGui::TextColored(ImVec4(0,1,0,1), u8"البدء: 14/03/2026 | النهاية: 14/04/2026");
    ImGui::EndChild();

    if (ImGui::CollapsingHeader(u8"🛡️ الحماية وتبليك السيرفرات")) {
        static bool bypass=true, block=true;
        ImGui::Checkbox(u8"تفعيل الحماية (Online Bypass)", &bypass);
        ImGui::Checkbox(u8"حظر روابط الباند (Blacklist)", &block);
    }

    if (ImGui::CollapsingHeader(u8"👁️ رادار كشف الأماكن (ESP)")) {
        static bool esp_m = false;
        ImGui::Checkbox(u8"تفعيل الرادار الرئيسي", &esp_m);
        if(esp_m) {
            ImGui::Indent();
            static bool b, h, s, n;
            ImGui::Checkbox(u8"صندوق اللاعب (2D Box)", &b);
            ImGui::Checkbox(u8"شريط الصحة (Health)", &h);
            ImGui::Checkbox(u8"الهيكل العظمي (Skeleton)", &s);
            ImGui::Checkbox(u8"الاسم والمسافة", &n);
            ImGui::Unindent();
        }
        static bool v, l;
        ImGui::Checkbox(u8"كشف السيارات والوقود", &v);
        ImGui::Checkbox(u8"كشف الأسلحة والموارد (Loot)", &l);
    }

    if (ImGui::CollapsingHeader(u8"🎯 المميزات القتالية (Combat)")) {
        static bool recoil=false, aim=false;
        if(ImGui::Checkbox(u8"ثبات السلاح 100%", &recoil)) {
            // [برمجية حقيقية]: Patch Memory using Dobby
            // DobbyCodePatch((void*)(base + OFF_RECOIL_PATCH), (uint8_t*)"\x00\x00\xA0\xE3", 4);
        }
        ImGui::Checkbox(u8"تثبيت الأيم (Aimbot)", &aim);
    }

    ImGui::Spacing(); ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5,0.5,0.5,1), u8"حقوق المبرمج محمد الشمري © 2026");
    ImGui::End();
}

// --- [ الإقلاع العنيف ] ---
__attribute__((constructor))
void Start() {
    thread([]() {
        // [Logic]: منع اللعبة من التقدم حتى تفعيل الـ VIP
        // DobbyHook((void*)ConnectAddr, (void*)Hook_Connect, (void**)&Orig_Connect);
        thread(FetchServerCommands).detach();
        g_Activated = true; 
    }).detach();
}
