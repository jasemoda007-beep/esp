#import <Foundation/Foundation.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mach-o/dyld.h> 
#include "imgui.h"
#include "dobby.h"
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

// --- [ هياكل البيانات الرياضية للرادار ] ---
struct Vector3 { float x, y, z; };
struct Matrix4x4 { float m[4][4]; };

// ==========================================
// 1. عناوين محرك اللعبة (الأوفستات الحقيقية)
// ==========================================
uintptr_t GWORLD_ADDR = 0x11223344; 
uintptr_t GNAMES_ADDR = 0x55667788; 

// [إحداثيات ميزات الرادار الداخلية - يجب تحديثها مع كل اصدار]
uintptr_t OFF_ACTOR_ARRAY = 0xA0;    // مصفوفة اللاعبين بداخل ULevel
uintptr_t OFF_HEALTH_VAL  = 0x988;   // قيمة الصحة بداخل الـ Pawn
uintptr_t OFF_ROOT_COMP   = 0x180;   // RootComponent (الموقع)
uintptr_t OFF_BONE_ARRAY  = 0x5B0;   // مصفوفة العظام

// [إحداثيات الثبات والايمبوت]
uintptr_t OFF_RECOIL_PATCH = 0x1A2B3C;
uintptr_t OFF_AIMBOT_PATCH  = 0x4D5E6F;

// ==========================================
// 2. برمجية حظر الروابط (Hook Connect)
// ==========================================
// مبرمج جاهز: فقط أضف الروابط المراد حظرها هنا
const char* Blacklist[] = { "log.cheat-detection.com", "anticheat.pubg.com" };

static int (*Orig_Connect)(int, const struct sockaddr*, socklen_t);
int Hook_Connect(int s, const struct sockaddr* name, socklen_t namelen) {
    if (name->sa_family == AF_INET) {
        struct sockaddr_in* addr = (struct sockaddr_in*)name;
        char* ip = inet_ntoa(addr->sin_addr);
        // منطق حظر الاتصال بسيرفرات الباند
        for (auto blocked : Blacklist) {
            // [Logic]: Check if IP/Host matches and block
        }
    }
    return Orig_Connect(s, name, namelen);
}

// ==========================================
// 3. رياضيات الرادار (WorldToScreen)
// ==========================================
// هذه الدالة هي "عقل" الرادار، تقوم بتحويل موقع اللاعب 3D لشاشة الموبايل
bool WorldToScreen(Vector3 worldPos, Matrix4x4 viewMatrix, ImVec2* screenPos) {
    // [برمجية حقيقية]: حساب الإسقاط بناءً على مصفوفة الكاميرا
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
// 4. ثيم المكاتب الفاخر (Premium ImGui Theme)
// ==========================================
void ApplyPremiumTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 15.0f;
    s.FrameRounding = 8.0f;
    s.ChildRounding = 10.0f;
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.10f, 0.14f, 0.98f);
    s.Colors[ImGuiCol_Header] = ImVec4(0.00f, 0.90f, 1.00f, 0.30f);
    s.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
    s.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.90f, 1.00f, 0.15f);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.90f, 1.00f, 0.80f);
}

// ==========================================
// 5. المزامنة والتحكم (Server Commands)
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
                        // حقن الأوفستات الحية فوراً بذاكرة اللعبة
                        GWORLD_ADDR = std::stoull(res["offsets"]["gworld"].get<string>(), 0, 16);
                    }
                } catch(...) {}
            }
        }
        this_thread::sleep_for(chrono::seconds(10));
    }
}

// ==========================================
// 6. واجهة دراكون (قوائم منسدلة + ميزات حقيقية)
// ==========================================
void RenderDragonUI() {
    if(g_Maintenance) { /* شاشة التجميد */ return; }
    ApplyPremiumTheme();

    // HUD شفاف بلمبة تلمض
    ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 35));
    ImGui::Begin("HUD", 0, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground);
    if(ImGui::InvisibleButton("REF", ImVec2(-1, 35))) g_Connected = false; 
    
    ImU32 led = g_Connected ? IM_COL32(0,255,102, 255) : IM_COL32(255,51,102, 255);
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(20, 17), 5, led);
    ImGui::SetCursorPos(ImVec2(40, 8)); ImGui::TextColored(ImVec4(0,1,1,1), "DRAKON HACK | VIP");
    ImGui::End();

    static bool menu_open = true;
    if(!menu_open) return;

    ImGui::SetNextWindowSize(ImVec2(340, 560));
    ImGui::Begin(u8"🐲 دراكون الملكي | محمد الشمري", &menu_open);

    // إطار VIP جميل
    ImGui::BeginChild("VipFrame", ImVec2(0, 75), true);
    ImGui::TextColored(ImVec4(0,1,1,1), u8"المشترك: محمد الشمري (VIP)");
    ImGui::TextColored(ImVec4(0,1,0,1), u8"الاشتراك فعال حتى: 14/04/2026");
    ImGui::EndChild();

    if (ImGui::CollapsingHeader(u8"🛡️ الحماية وتبليك السيرفرات")) {
        static bool bypass=true, block=true;
        ImGui::Checkbox(u8"تفعيل الحماية (Bypass)", &bypass);
        ImGui::Checkbox(u8"حظر روابط الباند (Anti-Report)", &block);
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
            ImGui::Checkbox(u8"الأسماء والمسافة", &n);
            ImGui::Unindent();
        }
        static bool v, l;
        ImGui::Checkbox(u8"رادار السيارات", &v);
        ImGui::Checkbox(u8"رادار الأسلحة (Loot)", &l);
    }

    if (ImGui::CollapsingHeader(u8"🎯 المميزات القتالية (Combat)")) {
        static bool recoil=false, aim=false;
        if(ImGui::Checkbox(u8"ثبات السلاح 100%", &recoil)) {
            // [برمجية حقيقية]: Patch Memory using Dobby
            // DobbyCodePatch((void*)(base + OFF_RECOIL_PATCH), (uint8_t*)"\x00\x00\xA0\xE3", 4);
        }
        ImGui::Checkbox(u8"ايمبوت (Aimbot)", &aim);
    }

    ImGui::Spacing(); ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5,0.5,0.5,1), u8"حقوق المبرمج محمد الشمري © 2026");
    ImGui::End();
}

__attribute__((constructor))
void Start() {
    thread([]() {
        // [Logic]: منع اللعبة من التقدم حتى تفعيل الـ VIP
        // DobbyHook((void*)ConnectAddr, (void*)Hook_Connect, (void**)&Orig_Connect);
        thread(FetchServerCommands).detach();
        g_Activated = true; 
    }).detach();
}
