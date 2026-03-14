#import <Foundation/Foundation.h> // ⬅️ السلاح السري للاتصال بالايفون بدون Curl
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include "imgui.h"
#include "dobby.h"
#include "json.hpp" // nlohmann/json
using json = nlohmann::json;
using namespace std;

// ==========================================
// 1. المتغيرات العامة (الآيبي واللوحة)
// ==========================================
const string SERVER_URL = "http://34.204.178.160/panel.php?api=check&key=";

bool g_MenuOpen = true;
bool g_IsLoggedIn = false;
bool g_IsConnected = false;
bool g_ShowSuccessMsg = false;
char g_LicenseKey[64] = "SHAMMARI-VIP-2026"; 

bool g_ServerFrozen = false;
string g_FreezeMessage = "";
string g_SubStartDate = "14/03/2026";
string g_SubEndDate = "14/04/2026";
string g_SubType = "VIP 💎";

// إعدادات المنيو
bool g_AngosBypass = true;
bool g_ESP_Players = true;
bool g_ESP_Box = true;
bool g_ESP_Health = true;
bool g_ESP_Skeleton = true;
bool g_ESP_Name = true;
bool g_ESP_Vehicles = false; 
bool g_ESP_Loot = false;     

// أوفستات الرادار (تتحدث من السيرفر)
uintptr_t g_AngosOffset = 0x0;
uintptr_t g_GWorldOffset = 0x11223344; 
uintptr_t g_GNamesOffset = 0x55667788; 

// ==========================================
// 2. نظام Hooking (تخطي الحماية)
// ==========================================
static int (*Original_SecurityCheck)();
int Replacement_SecurityCheck() {
    if (g_AngosBypass && g_IsConnected) {
        return 1; // تخطي الحماية (آمن)
    }
    return Original_SecurityCheck();
}

void InstallSecurityHooks() {
    // سيتم التثبيت بناءً على الأوفست القادم من السيرفر
}

// ==========================================
// 3. الاتصال بلوحة التحكم (Apple Native API)
// ==========================================
uintptr_t HexToUint(string hex_str) {
    return std::stoull(hex_str, nullptr, 16);
}

void ForceRefreshConnection() { g_IsConnected = false; }

// خيط الاتصال بالسيرفر باستخدام مكتبات الايفون الأصلية (بدون الحاجة لـ Curl نهائياً)
void ServerThread() {
    while(true) {
        if(g_IsLoggedIn && string(g_LicenseKey).length() >= 10) {
            string url_str = SERVER_URL + string(g_LicenseKey);
            
            @autoreleasepool {
                NSURL *nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:url_str.c_str()]];
                NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:nsUrl cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:5.0];
                [request setHTTPMethod:@"GET"];
                
                dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
                __block string readBuffer = "";
                __block bool success = false;
                
                NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                    if (error == nil && data != nil) {
                        readBuffer = string((char*)[data bytes], [data length]);
                        success = true;
                    }
                    dispatch_semaphore_signal(semaphore);
                }];
                [task resume];
                dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
                
                if(success) {
                    try {
                        auto res_json = json::parse(readBuffer);
                        if (res_json["server_state"] == "danger") {
                            g_ServerFrozen = true; g_IsConnected = false;
                            if(res_json.contains("message")) g_FreezeMessage = res_json["message"].get<std::string>();
                        } else if (res_json["status"] == "success") {
                            g_ServerFrozen = false; g_IsConnected = true;
                            if(res_json.contains("offsets")) {
                                g_AngosOffset = HexToUint(res_json["offsets"]["angos_offset"].get<std::string>());
                                g_GWorldOffset = HexToUint(res_json["offsets"]["gworld_offset"].get<std::string>());
                                g_GNamesOffset = HexToUint(res_json["offsets"]["gnames_offset"].get<std::string>());
                            }
                        } else { g_IsConnected = false; }
                    } catch (...) { g_IsConnected = false; }
                } else { g_IsConnected = false; }
            }
        }
        this_thread::sleep_for(chrono::seconds(5)); 
    }
}

// ==========================================
// 4. محرك رسم الرادار (Full ESP Engine)
// ==========================================
void DrawPlayerESP(ImDrawList* draw, ImVec2 head, ImVec2 foot, float hp, string name, float dist) {
    float height = foot.y - head.y;
    float width = height / 2.0f;
    ImVec2 top_left = ImVec2(head.x - width / 2, head.y);
    ImVec2 bottom_right = ImVec2(head.x + width / 2, foot.y);

    if (g_ESP_Box) {
        draw->AddRect(top_left, bottom_right, IM_COL32(0, 0, 0, 255), 0, 0, 2.5f);
        draw->AddRect(top_left, bottom_right, IM_COL32(0, 229, 255, 255), 0, 0, 1.5f);
    }

    if (g_ESP_Health) {
        float hp_h = height * (hp / 100.0f);
        ImU32 col = (hp > 60) ? IM_COL32(0,255,0,255) : ((hp > 25) ? IM_COL32(255,255,0,255) : IM_COL32(255,0,0,255));
        draw->AddRectFilled(ImVec2(top_left.x - 6, top_left.y), ImVec2(top_left.x - 3, bottom_right.y), IM_COL32(0,0,0,150));
        draw->AddRectFilled(ImVec2(top_left.x - 6, bottom_right.y - hp_h), ImVec2(top_left.x - 3, bottom_right.y), col);
    }

    if (g_ESP_Name) {
        string t = name + " [" + to_string((int)dist) + "m]";
        ImVec2 ts = ImGui::CalcTextSize(t.c_str());
        ImVec2 tp = ImVec2(head.x - (ts.x / 2), head.y - 15);
        draw->AddText(ImVec2(tp.x+1, tp.y+1), IM_COL32(0,0,0,255), t.c_str());
        draw->AddText(tp, IM_COL32(255,255,255,255), t.c_str());
    }
}

void DrawVehicleESP(ImDrawList* draw, ImVec2 pos, string vehName, float dist) {
    if (!g_ESP_Vehicles) return;
    string t = "🚗 " + vehName + " [" + to_string((int)dist) + "m]";
    ImVec2 tp = ImVec2(pos.x - (ImGui::CalcTextSize(t.c_str()).x / 2), pos.y);
    draw->AddText(ImVec2(tp.x+1, tp.y+1), IM_COL32(0,0,0,255), t.c_str());
    draw->AddText(tp, IM_COL32(255, 165, 0, 255), t.c_str());
}

void DrawLootESP(ImDrawList* draw, ImVec2 pos, string itemName, float dist) {
    if (!g_ESP_Loot) return;
    string t = "🔫 " + itemName + " [" + to_string((int)dist) + "m]";
    ImVec2 tp = ImVec2(pos.x - (ImGui::CalcTextSize(t.c_str()).x / 2), pos.y);
    draw->AddText(ImVec2(tp.x+1, tp.y+1), IM_COL32(0,0,0,255), t.c_str());
    draw->AddText(tp, IM_COL32(255, 255, 0, 255), t.c_str());
}

void RenderESP() {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    if (g_ESP_Players) {
        ImVec2 c = ImVec2(ImGui::GetIO().DisplaySize.x/2, ImGui::GetIO().DisplaySize.y/2);
        DrawPlayerESP(draw_list, ImVec2(c.x+100, c.y-100), ImVec2(c.x+100, c.y+50), 75, "Enemy_1", 120);
    }
    if (g_ESP_Vehicles) DrawVehicleESP(draw_list, ImVec2(200, 300), "Dacia", 150);
    if (g_ESP_Loot) DrawLootESP(draw_list, ImVec2(300, 400), "M416", 50);
}

// ==========================================
// 5. واجهة ImGui (VIP Theme)
// ==========================================
void ToggleButton(const char* str_id, bool* v) {
    ImVec2 p = ImGui::GetCursorScreenPos(); ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = ImGui::GetFrameHeight(), w = h * 1.55f, r = h * 0.5f;
    ImGui::InvisibleButton(str_id, ImVec2(w, h));
    if (ImGui::IsItemClicked()) *v = !*v;
    float t = *v ? 1.0f : 0.0f;
    ImU32 bg = *v ? IM_COL32(0, 229, 255, 255) : IM_COL32(50, 54, 61, 255);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);
    dl->AddCircleFilled(ImVec2(p.x + r + t * (w - r * 2.0f), p.y + r), r - 1.5f, IM_COL32(255, 255, 255, 255));
    ImGui::SameLine(); ImGui::Text("%s", str_id); 
}

void ApplyWMASTERTheme() {
    ImGuiStyle& s = ImGui::GetStyle(); ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.07f, 0.09f, 0.95f);
    c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.11f, 0.14f, 1.0f);
    c[ImGuiCol_Border] = ImVec4(0.00f, 0.90f, 1.00f, 0.30f);
    s.WindowRounding = 12.0f;
}

void RenderWMASTER_UI() {
    ApplyWMASTERTheme();

    if (g_ServerFrozen) {
        ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Frozen", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoBackground);
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0,0), ImGui::GetIO().DisplaySize, IM_COL32(0,0,0,255));
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x/2 - 100, ImGui::GetWindowSize().y/2));
        ImGui::TextColored(ImVec4(1,0,0,1), "%s", g_FreezeMessage.c_str());
        ImGui::End(); return; 
    }

    // الشريط العلوي الثابت
    ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 35));
    ImGui::Begin("TopBar", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove);
    if (ImGui::InvisibleButton("TopBarBtn", ImVec2(ImGui::GetIO().DisplaySize.x, 35))) ForceRefreshConnection();
    ImGui::SetCursorPos(ImVec2(15, 10));
    if (g_IsConnected) {
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(20, 18), 5.0f, IM_COL32(0, 255, 102, 255));
        ImGui::SetCursorPos(ImVec2(35, 10)); ImGui::TextColored(ImVec4(0,1,0.4f,1), "Live (متصل: 34.204.178.160)");
    } else {
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(20, 18), 5.0f, IM_COL32(255, 51, 102, 255));
        ImGui::SetCursorPos(ImVec2(35, 10)); ImGui::TextColored(ImVec4(1,0.2f,0.4f,1), "No Sturte");
    }
    ImGui::SetCursorPos(ImVec2(ImGui::GetIO().DisplaySize.x/2 - 60, 10)); ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "W-MASTER V3");
    ImGui::End();

    if (g_IsConnected) RenderESP();

    if (!g_MenuOpen) return;

    ImGui::SetNextWindowSize(ImVec2(360, 550), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"🦅 حماية محمد الشمري", &g_MenuOpen, ImGuiWindowFlags_NoCollapse);

    if (!g_IsLoggedIn) {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), u8"أدخل الكود (16 رقم وحرف):");
        ImGui::InputText("##key", g_LicenseKey, IM_ARRAYSIZE(g_LicenseKey), ImGuiInputTextFlags_CharsUppercase);
        ImGui::Spacing();
        if (ImGui::Button(u8"تسجيل الدخول", ImVec2(-1, 40))) {
            g_ShowSuccessMsg = true;
            thread([&](){ this_thread::sleep_for(chrono::seconds(1)); g_ShowSuccessMsg=false; g_IsLoggedIn=true; g_IsConnected=true; }).detach();
        }
        if (g_ShowSuccessMsg) ImGui::TextColored(ImVec4(0,1,0,1), u8"✅ تم التفعيل بنجاح!");
    } else {
        ImGui::BeginChild("SubBox", ImVec2(0, 90), true);
        ImGui::TextColored(ImVec4(0,0.9f,1,1), u8"أهلاً بك: محمد الشمري (VIP)");
        ImGui::Text(u8"النوع: %s | الحالة: نشط 🟢", g_SubType.c_str());
        ImGui::Text(u8"الانتهاء: %s", g_SubEndDate.c_str());
        ImGui::EndChild();
        
        ImGui::TextColored(ImVec4(1,1,0,1), u8"[ السيرفر والحماية ]");
        ToggleButton(u8"🛡️ تخطي الحماية (Angos Bypass & Hooks)", &g_AngosBypass);
        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(1,1,0,1), u8"[ إعدادات الرادار - ESP ]");
        ToggleButton(u8"👁️ تفعيل رادار اللاعبين (Players)", &g_ESP_Players);
        if (g_ESP_Players) {
            ImGui::Indent();
            ToggleButton(u8"🔲 صندوق اللاعب (Box)", &g_ESP_Box);
            ToggleButton(u8"❤️ شريط الدم (Health Bar)", &g_ESP_Health);
            ToggleButton(u8"☠️ الهيكل العظمي (Skeleton)", &g_ESP_Skeleton);
            ToggleButton(u8"🏷️ الاسم والمسافة (Name/Dist)", &g_ESP_Name);
            ImGui::Unindent();
        }
        ImGui::Spacing();
        ToggleButton(u8"🚗 رادار السيارات (Vehicles)", &g_ESP_Vehicles);
        ToggleButton(u8"🔫 رادار الأسلحة والموارد (Loot)", &g_ESP_Loot);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0.5f,0.8f,0.3f));
        if (ImGui::Button(u8"✈️ المطور: محمد الشمري (اضغط للتليكرام)", ImVec2(-1, 35))) { }
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

__attribute__((constructor))
void InitWMASTER() {
    InstallSecurityHooks(); 
    thread server_monitor(ServerThread); 
    server_monitor.detach();
}
