// Shared navigation and settings inspector. Included inside ui.cpp's private namespace.
struct SceneEditorItem {
    const char *id;
    int category, reset;
    UiText full;
    UiText shortName, description;
    bool Settings::*enabled;
};
const SceneEditorItem sceneItems[] = {
    {"ssao", 0, 0, UiText::None, UiText::PanelSceneLight, UiText::PanelSceneAODescription, &Settings::ssao},
    {"adaptation", 0, 5, UiText::Adaptation, UiText::PanelSceneAdaptation,
     UiText::PanelSceneAdaptationDescription, &Settings::adaptation},
    {"balance", 0, 10, UiText::LightingBalance, UiText::PanelSceneBalance,
     UiText::PanelSceneBalanceDescription, &Settings::lightingBalance},
    {"local", 0, 6, UiText::LocalLighting, UiText::PanelSceneLocal, UiText::PanelSceneLocalDescription,
     &Settings::localLighting},
    {"sky", 1, 2, UiText::None, UiText::PanelSceneAir, UiText::PanelSceneSkyDescription, &Settings::sky},
    {"rays", 1, 1, UiText::SunRays, UiText::PanelSceneLight, UiText::PanelSceneRaysDescription,
     &Settings::sunRays},
    {"fog", 1, 3, UiText::Fog, UiText::PanelSceneAir, UiText::PanelSceneFogDescription, &Settings::fog},
    {"dof", 2, 4, UiText::DepthOfField, UiText::PanelSceneDepth, UiText::PanelSceneDepthDescription,
     &Settings::dof},
    {"foliage", 2, 9, UiText::Vegetation, UiText::PanelSceneFoliage, UiText::PanelSceneFoliageDescription,
     nullptr},
    {"reflections", 3, 13, UiText::SurfaceReflections, UiText::PanelSceneReflections,
     UiText::PanelSceneReflectionsDescription, &Settings::surfaceReflections},
    {"particles", 3, 11, UiText::SpellEffects, UiText::PanelSceneParticles,
     UiText::PanelSceneParticlesDescription, &Settings::sfx}};
const UiText sceneGroups[] = {UiText::PanelSceneLight, UiText::PanelSceneAir, UiText::PanelSceneFocus,
                              UiText::PanelSceneSurfaces};
const char *SceneItemName(int i, bool full = false) {
    if (i == 0)
        return "SSAO";
    if (i == 4)
        return "SkyFX";
    const auto &effect = sceneItems[i];
    if (full || i == 5 || i == 6)
        return Tr(effect.full);
    return Tr(effect.shortName);
}

void EffectColorPicker(const char *id,UiText label,bool &custom,float &red,float &green,float &blue,
                       UiText defaultColor=UiText::AuraNativeColor,UiText restore=UiText::AuraRestoreColor,
                       DWORD defaultRgb=0xffffffffu) {
    ImGui::PushID(id);
    ImGui::PushFont(nullptr,13);
    ImGui::BeginDisabled(uiParameterDisabled);
    const auto p=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
    const bool open=ImGui::InvisibleButton("open",{width,30},ImGuiButtonFlags_EnableNav);
    auto *draw=ImGui::GetWindowDrawList();
    TextAt(draw,{p.x+4,p.y+7},Tr(label),Text,13);
    char hex[10];std::snprintf(hex,sizeof(hex),"#%02X%02X%02X",int(red*255+.5f),int(green*255+.5f),int(blue*255+.5f));
    const char *value=custom?hex:Tr(defaultColor);
    const float valueWidth=ImGui::CalcTextSize(value).x,swatchX=p.x+width-valueWidth-37;
    const bool hasPalette=defaultRgb!=0xffffffffu;
    const ImVec4 palette{float((defaultRgb>>16)&255)/255,float((defaultRgb>>8)&255)/255,float(defaultRgb&255)/255,1};
    const auto swatch=Opacity(custom?ImGui::ColorConvertFloat4ToU32({red,green,blue,1}):
                              hasPalette?ImGui::ColorConvertFloat4ToU32(palette):Raised);
    draw->AddRectFilled({swatchX,p.y+5},{swatchX+22,p.y+25},swatch,3);
    draw->AddRect({swatchX,p.y+5},{swatchX+22,p.y+25},Opacity(Line),3);
    if(!custom&&!hasPalette)draw->AddLine({swatchX+4,p.y+21},{swatchX+18,p.y+9},Opacity(Muted));
    TextAt(draw,{swatchX+29,p.y+7},value,ImGui::IsItemHovered()?Lavender:Muted,13);
    if(open)ImGui::OpenPopup("picker");
    const auto display=ImGui::GetIO().DisplaySize;
    const float pickerWidth=std::clamp(std::min(display.x-42,display.y-142),160.f,260.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,10});
    ImGui::SetNextWindowSize({pickerWidth+20,0});
    if(ImGui::BeginPopup("picker",ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted(Tr(label));
        const bool wasCustom=custom;
        SquareSwitch("custom",custom);ImGui::SameLine();
        if(custom&&!wasCustom&&hasPalette){red=palette.x;green=palette.y;blue=palette.z;}
        ImGui::TextUnformatted(Tr(UiText::AuraCustomColor));
        float rgb[]={red,green,blue};
        if(!custom&&hasPalette){rgb[0]=palette.x;rgb[1]=palette.y;rgb[2]=palette.z;}
        ImGui::SetNextItemWidth(pickerWidth);
        constexpr auto flags=ImGuiColorEditFlags_PickerHueWheel|ImGuiColorEditFlags_InputRGB|
            ImGuiColorEditFlags_NoSidePreview|ImGuiColorEditFlags_NoOptions|ImGuiColorEditFlags_NoLabel|
            ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoSmallPreview|ImGuiColorEditFlags_NoInputs;
        bool changed=ImGui::ColorPicker3("##wheel",rgb,flags);
        ImGui::TextUnformatted(Tr(UiText::AuraRGB));
        if(ImGui::BeginTable("rgb",3,ImGuiTableFlags_SizingStretchSame)){
            constexpr const char *channels[]={"R","G","B"};
            constexpr const char *ids[]={"##red","##green","##blue"};
            for(int i=0;i<3;++i){
                ImGui::TableNextColumn();ImGui::TextUnformatted(channels[i]);ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                int value=int(rgb[i]*255+.5f);
                if(ImGui::InputInt(ids[i],&value,0,0)){
                    rgb[i]=std::clamp(value,0,255)/255.f;changed=true;
                }
            }
            ImGui::EndTable();
        }
        char code[7];std::snprintf(code,sizeof(code),"%02X%02X%02X",int(rgb[0]*255+.5f),int(rgb[1]*255+.5f),int(rgb[2]*255+.5f));
        ImGui::TextUnformatted(Tr(UiText::AuraHex));ImGui::SameLine();ImGui::TextUnformatted("#");ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::InputText("##hex",code,sizeof(code),ImGuiInputTextFlags_CharsHexadecimal|
                           ImGuiInputTextFlags_CharsUppercase|ImGuiInputTextFlags_AutoSelectAll)&&std::strlen(code)==6){
            unsigned packed=0;
            if(std::sscanf(code,"%06X",&packed)==1){
                rgb[0]=float((packed>>16)&255)/255;rgb[1]=float((packed>>8)&255)/255;rgb[2]=float(packed&255)/255;changed=true;
            }
        }
        if(changed) {
            red=std::round(rgb[0]*1000000)/1000000;
            green=std::round(rgb[1]*1000000)/1000000;
            blue=std::round(rgb[2]*1000000)/1000000;custom=true;
        }
        if(ImGui::Button((std::string(Tr(restore))+"###native").c_str(),{pickerWidth,24}))
            custom=false;
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::EndDisabled();ImGui::PopFont();ImGui::PopID();
}
void PanelToggle(const char *,const char *,bool &,const char *,bool);
void PanelCombo(UiText,const char *,int &,const char *const *,int);
void SceneParameters(Runtime &r) {
    auto &s = r.settings;
    const Settings defaults;
#define RR(label, help, field, lo, hi, fmt, step)                                                            \
    Row(UiText::label, Tr(UiText::help), s.field, lo, hi, defaults.field, fmt, step)
    switch (r.sceneSelection) {
    case 0:
        RR(Strength, AOStrengthHelp, aoStrength, 0, 2, "%.2f", .05f);
        RR(Radius, AORadiusHelp, aoRadius, .1f, 5, "%.2f", .1f);
        break;
    case 1:
        RR(AverageBrightness, AverageBrightnessHelp, adaptationKey, .04f, .5f, "%.2f", .01f);
        break;
    case 2:
        RR(Strength, LightingBalanceHelp, lightingBalanceStrength, 0, 1, "%.2f", .05f);
        break;
    case 3:
        RR(Strength, LocalStrengthHelp, localStrength, 0, 3, "%.2f", .05f);
        RR(Distance, LocalRangeHelp, localRange, .25f, 3, "%.2f", .05f);
        break;
    case 4:
        RR(Atmosphere, SkyStrengthHelp, skyStrength, 0, 1, "%.2f", .05f);
        RR(Clouds, CloudsHelp, cloudContrast, .5f, 2, "%.2f", .05f);
        break;
    case 5:
        RR(Strength, RayStrengthHelp, rayStrength, 0, 2, "%.2f", .05f);
        RR(Length, RayLengthHelp, rayLength, .1f, 1.5f, "%.2f", .05f);
        break;
    case 6:
        RR(Density, FogDensityHelp, fogDensity, 0, .03f, "%.4f", .0005f);
        RR(Start, FogStartHelp, fogStart, 0, 250, "%.0f", 5);
        break;
    case 7:
        RR(Focus, FocusHelp, focusDistance, 1, 500, "%.0f", 1);
        RR(Blur, BlurHelp, dofRadius, 0, 12, "%.1f", .5f);
        break;
    case 8:
        RR(NightBrightness, NightBrightnessHelp, nightFoliage, 0, 1, "%.2f", .05f);
        break;
    case 9:
        RR(Strength, SurfaceStrengthHelp, surfaceStrength, 0, 1, "%.2f", .05f);
        RR(SurfaceRoughness, SurfaceRoughnessHelp, surfaceRoughness, 0, 1, "%.2f", .05f);
        break;
    case 10:
        RR(Strength, SfxIntensityHelp, sfxIntensity, .5f, 2, "%.2f", .05f);
        RR(AuraGlow, AuraGlowHelp, sfxAura, 0, 2, "%.2f", .05f);
        RR(SfxGlow, SfxGlowHelp, sfxGlow, 0, 1, "%.2f", .05f);
        break;
    }
    switch (r.sceneSelection) {
    case 0:
        RR(Bias, AOBiasHelp, aoBias, .005f, .3f, "%.3f", .005f);
        RR(Distance, AODistanceHelp, aoDistance, 10, 500, "%.0f", 10);
        break;
    case 1:
        RR(MinEV, MinEVHelp, minEV, -4, 0, "%+.2f", .1f);
        RR(MaxEV, MaxEVHelp, maxEV, 0, 4, "%+.2f", .1f);
        RR(AdaptBright, AdaptBrightHelp, adaptBright, .1f, 5, "%.1f", .1f);
        RR(AdaptDark, AdaptDarkHelp, adaptDark, .1f, 5, "%.1f", .1f);
        break;
    case 4:
        RR(Horizon, HorizonHelp, skyHorizon, .1f, 1, "%.2f", .05f);
        RR(SunHalo, SunHaloHelp, sunHalo, 0, 2, "%.2f", .05f);
        RR(SkyDetail, SkyDetailHelp, skyDetail, 0, 1, "%.2f", .05f);
        RR(FineStars, FineStarsHelp, skyStars, 0, 1, "%.2f", .05f);
        break;
    case 5:
        RR(Decay, RayDecayHelp, rayDecay, .8f, .99f, "%.2f", .01f);
        break;
    case 6:
        RR(Height, FogHeightHelp, fogHeight, -500, 500, "%.0f", 5);
        RR(HeightFalloff, FogFalloffHelp, fogFalloff, 0, .2f, "%.3f", .005f);
        {
            float tint[] = {s.fogR, s.fogG, s.fogB};
            ImGui::PushFont(nullptr, 13);
            ImGui::BeginDisabled(uiParameterDisabled);
            if (ImGui::ColorEdit3(Label(UiText::FogColor).c_str(), tint,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions |
                                      ImGuiColorEditFlags_NoSidePreview)) {
                s.fogR = tint[0];
                s.fogG = tint[1];
                s.fogB = tint[2];
            }
            ImGui::EndDisabled();
            ImGui::PopFont();
        }
        break;
    case 7:
        RR(FocusRange, FocusRangeHelp, focusRange, 1, 200, "%.0f", 1);
        RR(NearBlur, NearBlurHelp, dofNear, 0, 1, "%.2f", .05f);
        break;
    case 9:
        RR(Distance, SurfaceDistanceHelp, surfaceDistance, 5, 100, "%.0f", 5);
        break;
    case 10: {
        RR(Saturation, SaturationHelp, sfxSaturation, .5f, 1.5f, "%.2f", .05f);
        RR(SfxSoftness, SfxSoftnessHelp, sfxSoftness, 0, 1, "%.2f", .05f);
        ImGui::BeginDisabled(uiParameterDisabled);
        PanelToggle("smooth-aura",Tr(UiText::SmoothAura),s.sfxSmoothAura,nullptr,true);
        ImGui::EndDisabled();
        EffectColorPicker("aura-color",UiText::ArmorColor,s.sfxAuraCustomColor,s.sfxAuraR,s.sfxAuraG,s.sfxAuraB);
        EffectColorPicker("weapon-color",UiText::WeaponColor,s.sfxWeaponCustomColor,s.sfxWeaponR,s.sfxWeaponG,s.sfxWeaponB);
        const char *trails[]={Tr(UiText::SkateTrailNative),Tr(UiText::SkateTrailMist),
                             Tr(UiText::SkateTrailRibbons),Tr(UiText::SkateTrailStardust),
                             Tr(UiText::SkateTrailLightning),Tr(UiText::SkateTrailEmbers)};
        int style=int(s.skateTrailStyle);
        PanelCombo(UiText::SkateTrail,"skate-trail",style,trails,int(std::size(trails)));
        s.skateTrailStyle=float(style);
        ImGui::BeginDisabled(style==0);
        EffectColorPicker("skate-color",UiText::SkateTrailColor,s.skateTrailCustomColor,
                          s.skateTrailR,s.skateTrailG,s.skateTrailB,UiText::SkateTrailDefaultColor,UiText::SkateTrailRestoreColor);
        ImGui::EndDisabled();
        const char *particles[]={Tr(UiText::SkateParticleGame),Tr(UiText::SkateParticleStars),
            Tr(UiText::SkateParticleBolts),Tr(UiText::SkateParticleTriangles),Tr(UiText::SkateParticleRings),
            Tr(UiText::SkateParticleCrystals)};
        int particleStyle=int(s.skateParticleStyle);
        PanelCombo(UiText::SkateParticles,"skate-particles",particleStyle,particles,int(std::size(particles)));
        s.skateParticleStyle=float(particleStyle);
        EffectColorPicker("skate-particle-color",UiText::SkateParticleColor,s.skateParticleCustomColor,
            s.skateParticleR,s.skateParticleG,s.skateParticleB,UiText::SkateTrailDefaultColor,UiText::SkateTrailRestoreColor,
            skate_particles::palette(skate_particles::Style(particleStyle)));
        break;
    }
    }
#undef RR
}

bool PanelTextButton(const char *id, const char *text, ImVec2 size) {
    auto p = ImGui::GetCursorScreenPos();
    bool hit = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
    TextAt(ImGui::GetWindowDrawList(), {p.x, p.y + (size.y - 12) * .5f}, text,
           ImGui::IsItemHovered() ? Lavender : Muted, 12);
    return hit;
}
struct PanelItem {
    const char *id, *group, *label;
    bool *enabled = nullptr;
    int link = -1;
    const char *title = nullptr;
};
std::vector<PanelItem> PanelItems(Runtime &r) {
    auto &s = r.settings;
    auto g = [](UiText v) { return Tr(v); };
    switch (r.editorPage) {
    case 0:
        return {{"scene", g(UiText::PanelSections), Tr(UiText::LightAtmosphere), nullptr, 2},
                {"aa", g(UiText::PanelSections), Tr(UiText::Antialiasing), nullptr, 3},
                {"water", g(UiText::PanelSections), "WaterFX", nullptr, 1},
                {"optimization", g(UiText::PanelSections), Tr(UiText::Optimization), nullptr, 7},
                {"tone", g(UiText::PanelColor), g(UiText::PanelTone), &s.color},
                {"lights", g(UiText::PanelColor), g(UiText::PanelLights), &s.color},
                {"color", g(UiText::PanelColor), g(UiText::PanelColorBalance), &s.color},
                {"bloom", g(UiText::PanelEffects), "Bloom", &s.bloom}};
    case 1:
        return {{"waves", g(UiText::PanelSurface), g(UiText::PanelWaves)},
                {"material", g(UiText::PanelMaterial), g(UiText::PanelWaterColor)},
                {"refraction", g(UiText::PanelMaterial), g(UiText::PanelDistortion)},
                {"glints", g(UiText::PanelMaterial), g(UiText::PanelGlints)},
                {"shore", g(UiText::PanelDepth), g(UiText::PanelShore)},
                {"caustics", g(UiText::PanelDepth), g(UiText::PanelCaustics)},
                {"method", g(UiText::PanelReflections), g(UiText::PanelMethod)},
                {"trace", g(UiText::PanelReflections), g(UiText::PanelTrace)}};
    case 2: {
        std::vector<PanelItem> items;
        items.reserve(11);
        for (int i = 0; i < 11; ++i) {
            const auto &entry = sceneItems[i];
            items.push_back({entry.id, Tr(sceneGroups[entry.category]), SceneItemName(i),
                             entry.enabled ? &(s.*entry.enabled) : nullptr, -1, SceneItemName(i, true)});
        }
        return items;
    }
    case 3:
        return {{"fxaa", g(UiText::PanelMethods), "FXAA", &s.fxaa},
                {"smaa", g(UiText::PanelMethods), "SMAA", &s.smaa},
                {"msaa", g(UiText::PanelMethods), "MSAA"}};
    case 4:
        return {{"time", g(UiText::PanelWorld), Tr(UiText::TimeOfDay), &s.timeOverride},
                {"weather", g(UiText::PanelWorld), Tr(UiText::WeatherOverride), &s.weatherOverride},
                {"trees",g(UiText::Vegetation),g(UiText::Trees),&s.trees},
                {"grass",g(UiText::Vegetation),g(UiText::Grass),nullptr,9},
                {"winter-ground",g(UiText::Vegetation),g(UiText::WinterGround),&s.winterGround},
                {"puddles",g(UiText::WetSurfaces),g(UiText::Puddles),&s.puddles}};
    case 9:
        return {{"grass",g(UiText::Grass),g(UiText::GrassCoverage),&s.grass},
                {"grass-shape",g(UiText::Grass),g(UiText::GrassShape)},
                {"grass-wind",g(UiText::Grass),g(UiText::GrassWind)},
                {"grass-color",g(UiText::Grass),g(UiText::GrassColor)}};
    case 5:
        return {{"cursor", g(UiText::PanelInterface), g(UiText::PanelCursor), &s.customCursor},
                {"motion", g(UiText::PanelInterface), g(UiText::PanelMotion), &s.interfaceMotion},
                {"language", g(UiText::PanelInterface), g(UiText::PanelLanguage)}};
    case 8:
        return {{"appearance",g(UiText::MaterialsTab),g(UiText::MaterialAppearance),&s.materials},
                {"relief",g(UiText::MaterialsTab),g(UiText::MaterialRelief)},
                {"snow",g(UiText::MaterialsTab),g(UiText::MaterialSnow)},
                {"microcontrast",g(UiText::Image),g(UiText::MaterialMicrocontrast),&s.sharpening}};
    case 7:
        return {{"shops", g(UiText::PanelClient), g(UiText::PanelShops), &s.limitShopSigns},
                {"decorative", g(UiText::PanelClient), g(UiText::OptimizationDecorative), &s.hideDecorativeEffects, -1, g(UiText::OptimizationDecorativeTitle)},
                {"skills", g(UiText::PanelClient), g(UiText::OptimizationSkills), &s.hideSkillEffects, -1, g(UiText::OptimizationSkillsTitle)},
                {"pets", g(UiText::PanelClient), g(UiText::OptimizationPets), &s.hidePetEffects, -1, g(UiText::OptimizationPetsTitle)},
                {"visible-players", g(UiText::ActorVisibility), g(UiText::VisiblePlayers), &s.renderPlayers, -1, g(UiText::VisiblePlayersTitle)},
                {"visible-monsters", g(UiText::ActorVisibility), g(UiText::VisibleMonsters), &s.renderMonsters, -1, g(UiText::VisibleMonstersTitle)},
                {"visible-pets", g(UiText::ActorVisibility), g(UiText::VisiblePets), &s.renderPets, -1, g(UiText::VisiblePetsTitle)}};
    }
    return {};
}
const char *PanelTitle(int page) {
    switch (page) {
    case 9:return Tr(UiText::Grass);
    case 8:
        return Tr(UiText::MaterialsTab);
    case 0:
        return Tr(UiText::Image);
    case 1:
        return "WaterFX";
    case 3:
        return Tr(UiText::Antialiasing);
    case 4:
        return Tr(UiText::WorldTab);
    case 5:
        return Tr(UiText::PanelInterface);
    case 7:
        return Tr(UiText::Optimization);
    default:
        return Tr(UiText::LightAtmosphere);
    }
}
const char *PanelDescription(int page) {
    switch (page) {
    case 9:return Tr(UiText::GrassHelp);
    case 8:
        return Tr(UiText::MaterialsDescription);
    case 0:
        return Tr(UiText::PanelImageDescription);
    case 1:
        return Tr(UiText::PanelWaterDescription);
    case 3:
        return Tr(UiText::PanelAADescription);
    case 4:
        return Tr(UiText::PanelWorldDescription);
    case 5:
        return Tr(UiText::PanelInterfaceDescription);
    default:
        return Tr(UiText::PanelOptimizationDescription);
    }
}
void PanelNote(const char *text) {
    ImGui::PushFont(nullptr, 12);
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", text);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    ImGui::Dummy({0, 8});
}
void PanelCombo(UiText label, const char *id, int &selected, const char *const *names, int count) {
    ImGui::BeginDisabled(uiParameterDisabled);
    auto p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    TextAt(ImGui::GetWindowDrawList(), {p.x + 4, p.y + 4}, Tr(label), Text, 13);
    ImGui::SetCursorScreenPos({p.x + 4, p.y + 24});
    ImGui::SetNextItemWidth(w - 12);
    ImGui::PushFont(nullptr, 13);
    Combo((std::string("##") + id).c_str(), &selected, names, count);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({w, 54});
    ImGui::EndDisabled();
}
void PanelToggle(const char *id, const char *label, bool &value, const char *help = nullptr,
                 bool supported = true) {
    auto p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    TextAt(ImGui::GetWindowDrawList(), {p.x + 4, p.y + 7}, label, Text, 13);
    ImGui::SetCursorScreenPos({p.x + w - 34, p.y + 6});
    ImGui::BeginDisabled(!supported && !value);
    SquareSwitch(id, value, {28, 18});
    if (help)
        Tip(help, ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::EndDisabled();
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({w, 32});
}
void ResetPanelParameters(int page, int item, Settings &s, bool puddleItem=false) {
    if(puddleItem){s.resetSceneSection(17);return;}
    if(page==9){const Settings d;
        if(item==0){s.grassStyle=d.grassStyle;s.grassQuality=d.grassQuality;s.grassDistance=d.grassDistance;s.grassDensity=d.grassDensity;s.grassSlope=d.grassSlope;s.grassFade=d.grassFade;}
        if(item==1){s.grassHeight=d.grassHeight;s.grassWidth=d.grassWidth;s.grassVariation=d.grassVariation;}
        if(item==2){s.grassWind=d.grassWind;s.grassWindSpeed=d.grassWindSpeed;s.grassWindDirection=d.grassWindDirection;}
        if(item==3){s.grassGroundTint=d.grassGroundTint;s.grassBrightness=d.grassBrightness;s.grassSaturation=d.grassSaturation;s.grassDryness=d.grassDryness;s.grassShading=d.grassShading;}
        return;}
    if(page==8){s.resetSceneSection(item==3?14:15);return;}
    const Settings d;
#define DF(x) s.x = d.x
    if (page == 0)
        switch (item) {
        case 4:
            DF(exposure);
            DF(contrast);
            DF(gamma);
            break;
        case 5:
            DF(highlights);
            DF(shadows);
            break;
        case 6:
            DF(temperature);
            DF(tint);
            DF(saturation);
            break;
        case 7:
            s.resetBloom();
            break;
        }
    if (page == 1)
        switch (item) {
        case 0:
            DF(waterWaves);
            DF(waterLength);
            DF(waterSpeed);
            DF(waterRipples);
            DF(waterRippleScale);
            DF(sfxWake);
            break;
        case 1:
            DF(waterOpacity);
            DF(waterTexture);
            DF(waterTint);
            break;
        case 2:
            DF(waterRefraction);
            DF(waterDispersion);
            break;
        case 3:
            DF(waterGlints);
            DF(waterGloss);
            break;
        case 4:
            DF(waterDepth);
            DF(waterShore);
            break;
        case 5:
            DF(waterCaustics);
            DF(waterCausticScale);
            break;
        case 6:
            DF(waterReflectionMethod);
            DF(waterSSRQuality);
            DF(waterSSR);
            DF(waterSky);
            DF(waterReflectionResolution);
            break;
        case 7:
            DF(waterRaySteps);
            DF(waterRefinementSteps);
            DF(waterTraceDistance);
            DF(waterTraceBias);
            DF(waterTraceThickness);
            DF(waterReflectionConfidence);
            DF(waterSoftFallback);
            break;
        }
    if (page == 3 && item == 2)
        s.msaaSamples = 0;
    if (page == 4) {
        if (item == 0) {
            DF(timeMinutes);
            DF(freezeTime);
        } else {
            DF(weatherType);
            DF(weatherIntensity);
        }
    }

    if (page == 7 && item == 0)
        DF(shopSignDistance);
    if (page == 2)
        s.resetSceneSection(sceneItems[item].reset);
#undef DF
}
void PanelParameters(Runtime &r, int item) {
    auto &s = r.settings;
    const Settings d;
    const int page = r.editorPage;
    if (page == 2) {
        SceneParameters(r);
        return;
    }
#define SR(label, help, field, lo, hi, fmt, step)                                                            \
    Row(UiText::label, Tr(UiText::help), s.field, lo, hi, d.field, fmt, step)
    if(page==9){
        switch(item){
        case 0:{
            const char *styles[]={Tr(UiText::GrassStyleDetailed),Tr(UiText::GrassStyleSimple)};
            int style=int(std::round(s.grassStyle));
            PanelCombo(UiText::GrassStyle,"grass-style",style,styles,2);
            s.grassStyle=float(style);
            const char *quality[]={Tr(UiText::GrassLow),Tr(UiText::GrassMedium),Tr(UiText::High)};
            int q=int(std::round(s.grassQuality));PanelCombo(UiText::Quality,"grass-quality",q,quality,3);s.grassQuality=float(q);
            Tip(Tr(UiText::GrassQualityHelp));
            SR(Distance,GrassDistanceHelp,grassDistance,5,100,"%.0f",5);
            SR(Density,GrassDensityHelp,grassDensity,.25f,3,"%.2f",.1f);
            SR(GrassSlope,GrassSlopeHelp,grassSlope,0,60,"%.0f°",5);
            SR(GrassFade,GrassFadeHelp,grassFade,.1f,.5f,"%.2f",.05f);
            ImGui::BeginDisabled(uiParameterDisabled);
            PanelToggle("snow-grass",Tr(UiText::GrassSnow),s.grassSnow,Tr(UiText::GrassSnowHelp));
            ImGui::EndDisabled();
            break;}
        case 1:
            SR(GrassHeight,GrassHeightHelp,grassHeight,.1f,1.5f,"%.2f",.05f);
            SR(GrassWidth,GrassWidthHelp,grassWidth,.01f,.08f,"%.3f",.005f);
            SR(GrassVariation,GrassVariationHelp,grassVariation,0,1,"%.2f",.05f);break;
        case 2:
            SR(Strength,GrassWindHelp,grassWind,0,1.5f,"%.2f",.05f);
            SR(GrassWindSpeed,GrassWindSpeedHelp,grassWindSpeed,0,3,"%.2f",.1f);
            SR(GrassWindDirection,GrassWindDirectionHelp,grassWindDirection,0,360,"%.0f°",15);break;
        case 3:
            ImGui::BeginDisabled(uiParameterDisabled||s.grassStyle>.5f);
            PanelToggle("grass-ground-tint",Tr(UiText::GrassGroundTint),s.grassGroundTint,Tr(UiText::GrassGroundTintHelp));
            ImGui::EndDisabled();
            SR(GrassBrightness,GrassBrightnessHelp,grassBrightness,.5f,1.5f,"%.2f",.05f);
            SR(Saturation,SaturationHelp,grassSaturation,.5f,1.5f,"%.2f",.05f);
            SR(GrassDryness,GrassDrynessHelp,grassDryness,0,1,"%.2f",.05f);
            SR(GrassShading,GrassShadingHelp,grassShading,0,1,"%.2f",.05f);break;
        }
        if(!r.grassSupported())PanelNote(Tr(UiText::GrassUnavailable));
    }
    if(page==8) {
        if(item==0) {
            SR(Strength, MaterialStrengthHelp, materialStrength, 0, 1, "%.2f", .05f);
        } else if(item==1) {
            SR(MaterialRelief, MaterialReliefHelp, materialRelief, 0, 1, "%.2f", .05f);
            SR(MaterialGlints, MaterialGlintsHelp, materialGlints, 0, 1, "%.2f", .05f);
        } else if(item==2) {
            SR(MaterialSnowLook, MaterialSnowHelp, materialSnow, 0, 1, "%.2f", .05f);
        } else {
            SR(Strength, SharpeningHelp, sharpeningStrength, 0, 1, "%.2f", .05f);
            SR(TextureContrast, TextureContrastHelp, textureContrast, 0, 1, "%.2f", .05f);
        }
    }
    if (page == 0)
        switch (item) {
        case 4:
            SR(Exposure, ExposureHelp, exposure, -2, 2, "%+.2f", .01f);
            SR(Contrast, ContrastHelp, contrast, .5f, 1.5f, "%.2f", .01f);
            SR(Gamma, GammaHelp, gamma, .5f, 2, "%.2f", .01f);
            break;
        case 5:
            SR(Highlights, HighlightsHelp, highlights, -.5f, .5f, "%+.2f", .01f);
            SR(Shadows, ShadowsHelp, shadows, -.5f, .5f, "%+.2f", .01f);
            break;
        case 6:
            SR(Temperature, TemperatureHelp, temperature, -1, 1, "%+.2f", .01f);
            SR(Tint, TintHelp, tint, -1, 1, "%+.2f", .01f);
            SR(Saturation, SaturationHelp, saturation, 0, 2, "%.2f", .01f);
            break;
        case 7: {
            SR(Strength, BloomStrengthHelp, bloomStrength, 0, 2, "%.2f", .01f);
            SR(BrightnessThreshold, BrightnessThresholdHelp, threshold, 0, 1, "%.2f", .01f);
            SR(ThresholdSoftness, ThresholdSoftnessHelp, knee, .001f, .5f, "%.2f", .01f);
            SR(Radius, BloomRadiusHelp, radius, .5f, 4, "%.2f", .01f);
            const char *quality[] = {Tr(UiText::Economical), Tr(UiText::High)};
            int q = s.bloomScale == 4 ? 0 : 1;
            PanelCombo(UiText::Quality, "bloom-quality", q, quality, 2);
            s.bloomScale = q ? 2 : 4;
            break;
        }
        }
    if (page == 1)
        switch (item) {
        case 0:
            SR(WaveHeight, WaveHeightHelp, waterWaves, 0, .15f, "%.3f", .005f);
            SR(Wavelength, WavelengthHelp, waterLength, 1, 15, "%.2f", .25f);
            SR(Speed, SpeedHelp, waterSpeed, 0, 3, "%.2f", .05f);
            SR(Ripples, RipplesHelp, waterRipples, 0, 1, "%.2f", .02f);
            SR(RippleFrequency, RippleFrequencyHelp, waterRippleScale, .25f, 6, "%.2f", .05f);
            SR(WaterWake, WaterWakeHelp, sfxWake, 0, 1.5f, "%.2f", .05f);
            break;
        case 1:
            SR(Opacity, OpacityHelp, waterOpacity, .15f, 1, "%.2f", .02f);
            SR(OriginalTexture, OriginalTextureHelp, waterTexture, 0, 1, "%.2f", .02f);
            SR(TurquoiseTint, TurquoiseTintHelp, waterTint, 0, 1, "%.2f", .02f);
            break;
        case 2:
            SR(Refraction, RefractionHelp, waterRefraction, 0, 1, "%.2f", .02f);
            SR(Dispersion, DispersionHelp, waterDispersion, 0, 1, "%.2f", .05f);
            break;
        case 3:
            SR(SunGlints, SunGlintsHelp, waterGlints, 0, 2, "%.2f", .05f);
            SR(Smoothness, SmoothnessHelp, waterGloss, 0, 1, "%.2f", .02f);
            break;
        case 4:
            SR(Absorption, AbsorptionHelp, waterDepth, 0, 2, "%.2f", .05f);
            SR(ShoreTransition, ShoreTransitionHelp, waterShore, .05f, 3, "%.2f", .05f);
            break;
        case 5: {
            SR(Caustics, CausticsHelp, waterCaustics, 0, 2, "%.2f", .05f);
            const char *quality[] = {Tr(UiText::Economical), Tr(UiText::Detailed)};
            int q = s.waterCausticScale == 2 ? 1 : 0;
            PanelCombo(UiText::CausticQuality, "caustic-quality", q, quality, 2);
            s.waterCausticScale = q ? 2 : 4;
            break;
        }
        case 6: {
            int method = s.waterReflectionMethod < 0 ? (s.waterSSRQuality ? 2 : 1) : s.waterReflectionMethod,
                previous = method;
            const char *methods[] = {Tr(UiText::ReflectionSkyOnly), Tr(UiText::ReflectionPerformance),
                                     Tr(UiText::ReflectionQuality)};
            PanelCombo(UiText::ReflectionMethod, "reflection-method", method, methods, 3);
            if (method != previous) {
                s.waterReflectionMethod = method;
                s.waterSSRQuality = method == 2;
                s.waterRaySteps = 0;
            }
            SR(SkyReflection, SkyReflectionHelp, waterSky, 0, 1, "%.2f", .05f);
            bool disabled = uiParameterDisabled;
            uiParameterDisabled = disabled || method == 0;
            SR(WorldReflections, WorldReflectionsHelp, waterSSR, 0, 1, "%.2f", .05f);
            ImGui::BeginDisabled(method == 0);
            int resolution = s.waterReflectionResolution == 50   ? 0
                             : s.waterReflectionResolution == 75 ? 1
                                                                 : 2;
            const char *resolutions[] = {"50%", "75%", "100%"};
            PanelCombo(UiText::ReflectionResolution, "reflection-resolution", resolution, resolutions, 3);
            s.waterReflectionResolution = resolution == 0 ? 50 : resolution == 1 ? 75 : 100;
            ImGui::EndDisabled();
            uiParameterDisabled = disabled;
            break;
        }
        case 7: {
            bool disabled = uiParameterDisabled;
            uiParameterDisabled = disabled || s.waterReflectionMethod == 0;
            float automatic =
                (s.waterReflectionMethod < 0 ? s.waterSSRQuality != 0 : s.waterReflectionMethod == 2) ? 48.f
                                                                                                      : 24.f;
            float steps = s.waterRaySteps ? float(s.waterRaySteps) : automatic, previous = steps;
            Row(UiText::ReflectionSteps, Tr(UiText::ReflectionStepsHelp), steps, 8, 64, automatic, "%.0f", 1,
                false, "ray-steps");
            if (steps != previous)
                s.waterRaySteps = int(std::round(steps));
            float refine = float(s.waterRefinementSteps);
            Row(UiText::ReflectionRefinements, Tr(UiText::ReflectionRefinementsHelp), refine, 4, 16, 12,
                "%.0f", 1, false, "ray-refinements");
            s.waterRefinementSteps = int(std::round(refine));
            SR(Distance, ReflectionDistanceHelp, waterTraceDistance, 16, 512, "%.0f", 16);
            SR(ReflectionBias, ReflectionBiasHelp, waterTraceBias, .01f, .5f, "%.3f", .01f);
            SR(ReflectionThickness, ReflectionThicknessHelp, waterTraceThickness, .5f, 3, "%.2f", .05f);
            SR(ReflectionConfidence, ReflectionConfidenceHelp, waterReflectionConfidence, 0, .75f, "%.2f",
               .05f);
            ImGui::BeginDisabled(uiParameterDisabled);
            PanelToggle("soft-fallback", Tr(UiText::ReflectionSoftFallback), s.waterSoftFallback);
            ImGui::EndDisabled();
            uiParameterDisabled = disabled;
            break;

        }
        }
    if (page == 3) {
        if (item == 2) {
            const unsigned active = r.activeMSAA();
            char current[64];
            std::snprintf(current, sizeof(current), Tr(active ? UiText::MSAAActive : UiText::MSAAOff),
                          active);
            PanelNote(current);
            int modes[] = {-1, 2, 4, 8};
            const char *labels[] = {Tr(UiText::Off), Tr(UiText::MSAATwo), Tr(UiText::MSAAFour),
                                    Tr(UiText::MSAAEight)};
            int choice = -1;
            for (int i = 0; i < 4; ++i)
                if (active == unsigned(modes[i] < 0 ? 0 : modes[i]))
                    choice = i;
            ImGui::PushFont(nullptr, 13);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8);
            if (BeginCombo("##msaa-mode", current)) {
                if (choice < 0) {
                    ImGui::BeginDisabled();
                    ImGui::Selectable((std::string(current) + "###native-samples").c_str(), true);
                    ImGui::EndDisabled();
                }
                for (int i = 0; i < 4; ++i) {
                    ImGui::PushID(i);
                    ImGui::BeginDisabled(!r.msaaAvailable(modes[i]));
                    if (ImGui::Selectable((std::string(labels[i]) + "###samples").c_str(), choice == i))
                        s.msaaSamples = modes[i];
                    if (choice == i)
                        ImGui::SetItemDefaultFocus();
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::PopFont();
            ImGui::Dummy({0, 10});
        }
    }
    if (page == 7)
        switch (item) {
        case 0:
            PanelNote(Tr(UiText::ShopSignHelp));
            {
                bool disabled = uiParameterDisabled;
                uiParameterDisabled = !r.shopOptimizationSupported() || !s.limitShopSigns;
                SR(ShopSignDistance, ShopSignHelp, shopSignDistance, 5, 100, "%.0f", 5);
                uiParameterDisabled = disabled;
                if (!r.shopOptimizationSupported())
                    PanelNote(Tr(UiText::UnsupportedClient));
            }
            break;
        case 1:
            PanelNote(Tr(UiText::OptimizationDecorativeHelp));
            if (!r.nativeEffectOptimizationSupported())
                PanelNote(Tr(UiText::UnsupportedClient));
            break;
        case 2:
            PanelNote(Tr(UiText::OptimizationSkillsHelp));
            if (!r.nativeEffectOptimizationSupported())
                PanelNote(Tr(UiText::UnsupportedClient));
            break;
        case 3:
            PanelNote(Tr(UiText::OptimizationPetsHelp));
            if (!r.petEffectOptimizationSupported())
                PanelNote(Tr(UiText::UnsupportedClient));
            break;
        case 4:
        case 5:
        case 6: {
            PanelNote(Tr(item==4?UiText::VisiblePlayersHelp:item==5?UiText::VisibleMonstersHelp:UiText::VisiblePetsHelp));
            float &distance=item==4?s.playerDrawDistance:item==5?s.monsterDrawDistance:s.petDrawDistance;
            const float maximum=r.actorDistanceMaximum();
            float shown=actor_visibility::displayDistance(distance,maximum);
            const float before=shown;
            const bool disabled=uiParameterDisabled;
            uiParameterDisabled|=!r.actorVisibilitySupported();
            Row(UiText::ActorDrawDistance,Tr(UiText::ActorDrawDistanceHelp),shown,1,maximum,maximum,"%.0f",5,false,"actor-draw-distance",Tr(UiText::ActorDrawDistanceNative));
            if(shown!=before || (r.actorVisibilitySupported()&&distance>=maximum))
                distance=actor_visibility::savedDistance(shown,maximum);
            uiParameterDisabled=disabled;
            if(!r.actorVisibilitySupported())PanelNote(Tr(UiText::UnsupportedClient));
            break;
        }
        }
    if (page == 4 && !strcmp(PanelItems(r)[item].id,"puddles")) {
        SR(PuddleCoverage, PuddleCoverageHelp, puddleCoverage, 0, 1, "%.2f", .05f);
        SR(PuddleSize, PuddleSizeHelp, puddleSize, .5f, 12, "%.1f", .5f);
        SR(Strength, PuddleStrengthHelp, puddleStrength, 0, 1, "%.2f", .05f);
        SR(PuddleFillTime, PuddleTimeHelp, puddleFillTime, 5, 600, "%.0f", 5);
        SR(PuddleDryTime, PuddleTimeHelp, puddleDryTime, 5, 1200, "%.0f", 5);
        SR(PuddleGrass, PuddleMaterialsHelp, puddleGrass, 0, 1, "%.2f", .05f);
        SR(PuddleSoil, PuddleMaterialsHelp, puddleSoil, 0, 1, "%.2f", .05f);
        SR(PuddleCity, PuddleMaterialsHelp, puddleCity, 0, 1, "%.2f", .05f);
        SR(Ripples, PuddleRipplesHelp, puddleRipples, 0, 1, "%.2f", .05f);
        if(s.enabled && s.puddles && !r.sceneStatus().depth)PanelNote(Tr(UiText::SceneDepthUnavailable));
    }
    if (page == 4) {
        if (item == 0) {
            float minutes = r.environmentMinutes(), before = minutes;
            Row(UiText::TimeOfDay, Tr(UiText::TimeOfDayHelp), minutes, 0, 1439, 720, "%.0f", 1, true,
                "time-of-day");
            if (before != minutes)
                s.timeMinutes = minutes;
            ImGui::BeginDisabled(!s.timeOverride);
            PanelToggle("freeze-time", Tr(UiText::FreezeTime), s.freezeTime, Tr(UiText::FreezeTimeHelp));
            ImGui::EndDisabled();
        } else if(item==1) {
            const char *names[] = {Tr(UiText::WeatherClear), Tr(UiText::WeatherRain),
                                   Tr(UiText::WeatherSnow)};
            ImGui::BeginDisabled(!s.enabled || !s.weatherOverride);
            PanelCombo(UiText::WeatherOverride, "weather-type", s.weatherType, names, 3);
            ImGui::EndDisabled();
            bool disabled = uiParameterDisabled;
            uiParameterDisabled = disabled || s.weatherType == 0;
            SR(WeatherIntensity, WeatherIntensityHelp, weatherIntensity, 0, 100, "%.0f%%", 1);
            uiParameterDisabled = disabled;
        }
    }
    if (page == 4 && item == 2) {
        PanelNote(Tr(UiText::TreesExperimental));
        ImGui::BeginDisabled(uiParameterDisabled || !s.enabled);
        PanelToggle("tree-bark", Tr(UiText::TreeBark), s.treeBark);
        PanelToggle("tree-foliage", Tr(UiText::TreeFoliage), s.treeFoliage, Tr(UiText::TreeFoliageHelp));
        PanelToggle("tree-winter-foliage", Tr(UiText::TreeWinterFoliage), s.treeWinterFoliage,
                    Tr(UiText::TreeWinterFoliageHelp));
        ImGui::EndDisabled();
    }
    if (page == 5) {
        if (item == 2) {
            ImGui::PushFont(nullptr, 13);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0, .5f));
            for (int i = 0; i < 4; ++i) {
                ImGui::PushID(LanguageCodes[i]);
                if (ImGui::Selectable(LanguageNames[i], LanguageIndex(s.language) == size_t(i), 0, {0, 28}))
                    s.language = static_cast<Language>(i);
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
            ImGui::PopFont();
        }
    }
#undef SR
}

struct PanelMaster {
    const char *label;
    bool *value;
    const char *id;
    bool line;
    const char *help = nullptr;
};
PanelMaster EditorMaster(Runtime &r) {
    switch (r.editorPage) {
    case 0:
        return {Tr(UiText::ColorCorrection), &r.settings.color, "color-master", false};
    case 1:
        return {"WaterFX", &r.settings.water, "water-master", false};
    case 2:
        return {Tr(UiText::PanelSceneQuality), &r.settings.sceneQuality, "scene-quality", true,
                Tr(UiText::SceneQuality)};
    default:
        return {nullptr, nullptr, nullptr, false};
    }
}
bool CanResetPanel(int page, int item) {
    if(page==4&&(item==2||item==4))return false;
    if (page == 3)
        return item == 2;
    if (page == 5)
        return false;
    if (page == 7)
        return item == 0;
    return true;
}
UiText SceneNotice(Runtime &r, int item) {
    const auto &effect = sceneItems[item];
    const auto status = r.sceneStatus();
    if (!effect.enabled || !(r.settings.*effect.enabled) || !r.settings.enabled)
        return UiText::None;
    if ((item == 0 || item == 3 || item == 5 || item == 9) && !status.depth)
        return UiText::SceneDepthUnavailable;
    if (item == 4 && !status.sky)
        return UiText::SkyNotDetected;
    if (item == 5 && !status.sun)
        return UiText::RaysNeedSun;
    if (item == 3 && !status.light)
        return UiText::LightNotDetected;
    return UiText::None;
}
void PanelMasterControl(PanelMaster master, float width) {
    const auto p = ImGui::GetCursorScreenPos();
    auto *draw = ImGui::GetWindowDrawList();
    if (master.line)
        draw->AddLine(p, {p.x + width, p.y}, Line);
    draw->PushClipRect(p, {p.x + width - 32, p.y + 30}, true);
    TextAt(draw, {p.x, p.y + 8}, master.label, Muted, 12);
    draw->PopClipRect();
    ImGui::SetCursorScreenPos({p.x + width - 28, p.y + 7});
    SquareSwitch(master.id, *master.value, {22, 14});
    Tip(master.help ? master.help : master.label);
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, 28});
}
void PanelEditor(Runtime &r) {
    if (r.editorPage < 0 || r.editorPage >= 10)
        r.editorPage = 0;
    if (r.editorPage == 6) {
        r.editorPage = 1;
        r.editorSelection[1] = 6;
    }
    auto &s = r.settings;
    auto items = PanelItems(r);
    if (items.empty())
        return;
    int &selected = r.editorPage == 2 ? r.sceneSelection : r.editorSelection[r.editorPage];
    if (selected < 0 || selected >= int(items.size()) || items[selected].link >= 0)
        selected = r.editorPage == 0 ? 4 : 0;
    const auto origin = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    const float width = available.x, height = std::max(65.f, available.y - 1), rail = 174, gap = 15;
    const bool compact = width < 440;
    const auto master = EditorMaster(r);
    int nextPage = -1;
    float inspectorY = origin.y, inspectorHeight = height;
    if (compact) {
        ImGui::PushFont(nullptr, 13);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7, 6));
        ImGui::SetNextItemWidth(width);
        ImGui::SetNextWindowSizeConstraints({std::min(250.f, width), 0}, {std::max(250.f, width), 350});
        if (BeginCombo("##panel-navigation", items[selected].label)) {
            const char *group = nullptr;
            for (int i = 0; i < int(items.size()); ++i) {
                const auto &item = items[i];
                if (!group || std::strcmp(group, item.group)) {
                    group = item.group;
                    ImGui::PushFont(nullptr, 11);
                    ImGui::TextDisabled("%s", group);
                    ImGui::PopFont();
                }
                ImGui::PushID(item.id);
                if (ImGui::Selectable((std::string(item.label) + "###select").c_str(), selected == i)) {
                    if (item.link >= 0)
                        nextPage = item.link;
                    else
                        selected = i;
                }
                if (selected == i)
                    ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopFont();
        inspectorY = origin.y + 36;
        if (master.value) {
            ImGui::SetCursorScreenPos({origin.x, inspectorY});
            PanelMasterControl(master, width);
            inspectorY += 34;
        }
        inspectorHeight = std::max(50.f, height - (inspectorY - origin.y));
    } else {
        ImGui::BeginChild("panel-navigation", {rail, height}, ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::BeginChild("items", {rail, std::max(25.f, height - (master.value ? 34 : 0))},
                          ImGuiChildFlags_None);
        const char *group = nullptr;
        for (int i = 0; i < int(items.size()); ++i) {
            const auto &item = items[i];
            if (!group || std::strcmp(group, item.group)) {
                if (group)
                    ImGui::Dummy({0, 4});
                group = item.group;
                auto p = ImGui::GetCursorScreenPos();
                TextAt(ImGui::GetWindowDrawList(), {p.x + 9, p.y + 3}, group, Muted, 11);
                ImGui::Dummy({rail, 18});
            }
            ImGui::PushID(item.id);
            const auto p = ImGui::GetCursorScreenPos();
            auto *draw = ImGui::GetWindowDrawList();
            if (ImGui::InvisibleButton("select", {rail - 3, 23}, ImGuiButtonFlags_EnableNav)) {
                if (item.link >= 0)
                    nextPage = item.link;
                else
                    selected = i;
            }
            if (selected == i || ImGui::IsItemHovered())
                draw->AddRectFilled(p, {p.x + rail - 3, p.y + 23},
                                    selected == i ? IM_COL32(48, 41, 58, 255) : Raised, 2);
            if (selected == i)
                draw->AddRectFilled({p.x + 1, p.y + 5}, {p.x + 3, p.y + 18}, Accent, 1);
            draw->PushClipRect({p.x + 10, p.y}, {p.x + rail - 20, p.y + 23}, true);
            // Inactive effects retain the regular label color; only the status mark changes.
            TextAt(draw, {p.x + 10, p.y + 5}, item.label, selected == i ? Lavender : Text, 13,
                   selected == i ? uiEmphasis : nullptr);
            draw->PopClipRect();
            Tip(item.label);
            if (item.link >= 0) {
                draw->AddLine({p.x + rail - 15, p.y + 9}, {p.x + rail - 11, p.y + 12}, Muted);
                draw->AddLine({p.x + rail - 11, p.y + 12}, {p.x + rail - 15, p.y + 15}, Muted);
            } else if (item.enabled) {
                if (*item.enabled)
                    draw->AddCircleFilled({p.x + rail - 11, p.y + 11.5f}, 2.5f, Opacity(Accent), 12);
                else
                    draw->AddCircle({p.x + rail - 11, p.y + 11.5f}, 2.5f, Opacity(Muted), 12, 1);
            }
            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy({rail, 23});
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (master.value) {
            ImGui::SetCursorPos({8, height - 29});
            PanelMasterControl(master, rail - 16);
        }
        ImGui::EndChild();
        ImGui::GetWindowDrawList()->AddLine({origin.x + rail + 5, origin.y},
                                            {origin.x + rail + 5, origin.y + height}, Line);
    }
    if (nextPage >= 0) {
        r.editorPage = nextPage;
        ImGui::SetCursorScreenPos({origin.x, origin.y + height});
        ImGui::Dummy({0, 0});
        return;
    }
    const auto &item = items[selected];
    const float inspectorX = compact ? origin.x : origin.x + rail + gap;
    const float inspectorWidth = compact ? width : width - rail - gap;
    ImGui::SetCursorScreenPos({inspectorX, inspectorY});
    const auto id = std::string("panel-inspector-") + std::to_string(r.editorPage) + "-" + item.id;
    ImGui::BeginChild(id.c_str(), {inspectorWidth, inspectorHeight}, ImGuiChildFlags_None);
    ImGui::PushID(item.id);
    const auto p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    auto *draw = ImGui::GetWindowDrawList();
    const bool showEnable = item.enabled && !(r.editorPage == 0 && selected >= 4 && selected <= 6);
    const char *help = r.editorPage == 2 ? Tr(sceneItems[selected].description) : PanelDescription(r.editorPage);
    if(r.editorPage==8)help=Tr(selected==0?UiText::MaterialsCoverage:selected==2?UiText::MaterialSnowNote:
                             selected==3?UiText::MaterialMicroNote:UiText::MaterialReliefHelp);
    if(r.editorPage==3)help=Tr(selected==0?UiText::PanelFXAAHelp:selected==1?UiText::PanelSMAAHelp:UiText::MSAAHelp);
    if(r.editorPage==7)help=Tr(selected==0?UiText::ShopSignHelp:selected==1?UiText::OptimizationDecorativeHelp:
                             selected==2?UiText::OptimizationSkillsHelp:selected==3?UiText::OptimizationPetsHelp:
                             selected==4?UiText::VisiblePlayersHelp:selected==5?UiText::VisibleMonstersHelp:UiText::VisiblePetsHelp);
    if(r.editorPage==5&&selected<2)help=Tr(selected==0?UiText::V2CursorSampleNote:UiText::V2MotionNote);
    if(r.editorPage==4&&selected==2)help=Tr(UiText::TreesExperimental);
    const bool puddleItem=r.editorPage==4 && !strcmp(item.id,"puddles");
    if(puddleItem)help=Tr(UiText::PuddlesHelp);
    if(r.editorPage==4 && !strcmp(item.id,"winter-ground"))help=Tr(UiText::WinterGroundHelp);
    ImGui::InvisibleButton("description", {std::max(1.f,w-(showEnable?40:5)),25});
    Tip(help);
    draw->PushClipRect(p, {p.x + w - (showEnable ? 40 : 5), p.y + 25}, true);
    TextAt(draw, {p.x + 4, p.y + 2}, item.title ? item.title : item.label, Text, 16, uiEmphasis);
    draw->PopClipRect();
    bool supported = true;
    if(r.editorPage==9)supported=r.grassSupported();
    if (r.editorPage == 7) {
        if (selected == 0)
            supported = r.shopOptimizationSupported();
        if (selected > 0)
            supported = r.nativeEffectOptimizationSupported();
        if (selected == 3)
            supported = r.petEffectOptimizationSupported();
        if (selected >= 4)
            supported = r.actorVisibilitySupported();
    }
    if (showEnable) {
        ImGui::SetCursorScreenPos({p.x + w - 34, p.y + 3});
        const bool before = *item.enabled;
        const bool visibilityItem=r.editorPage==7&&selected>=4;
        ImGui::BeginDisabled((!supported && (visibilityItem?*item.enabled:!*item.enabled)) || (r.editorPage == 3 && !s.enabled));
        SquareSwitch("enabled", *item.enabled, {28, 18});
        Tip(r.editorPage==7?help:Tr(*item.enabled ? UiText::DisableEffect : UiText::EnableEffect),
            ImGuiHoveredFlags_AllowWhenDisabled);
        ImGui::EndDisabled();
        if (r.editorPage == 3 && *item.enabled != before && *item.enabled) {
            if (selected == 0)
                s.smaa = false;
            if (selected == 1)
                s.fxaa = false;
        }
    }
    const float bodyY = p.y + 34;
    draw->AddLine({p.x + 4, bodyY - 8}, {p.x + w - 5, bodyY - 8}, Line);
    ImGui::SetCursorScreenPos({p.x, bodyY});
    ImGui::Dummy({0,0});
    uiParameterDisabled = r.editorPage == 1 ? (!s.water || !s.enabled)
                          : item.enabled    ? !*item.enabled
                                            : false;
    if(puddleItem)uiParameterDisabled|=!s.enabled;
    if(r.editorPage==8)uiParameterDisabled|=!s.enabled || (selected<3&&!s.materials);
    if(r.editorPage==9)uiParameterDisabled|=!s.enabled||!s.grass||!supported;
    if (r.editorPage == 0 || r.editorPage == 3 || (r.editorPage == 4 && selected == 1) ||
        (r.editorPage == 2 && selected != 8))
        uiParameterDisabled |= !s.enabled;
    PanelParameters(r, selected);
    if (CanResetPanel(r.editorPage, selected)) {
        ImGui::Dummy({0, 7});
        const auto rp = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("reset-section", {w, 24}, ImGuiButtonFlags_EnableNav))
            ResetPanelParameters(r.editorPage, selected, s,puddleItem);
        ResetGlyph(draw, {rp.x + 1, rp.y + 3}, ImGui::IsItemHovered() ? Text : Muted);
        TextAt(draw, {rp.x + 25, rp.y + 6}, Tr(UiText::PanelReset), ImGui::IsItemHovered() ? Text : Muted,
               12);
    }
    if (r.editorPage == 2) {
        const auto notice = SceneNotice(r, selected);
        if (notice != UiText::None) {
            ImGui::Dummy({0, 12});
            PanelNote(Tr(notice));
        }
    }
    ImGui::PopID();
    ImGui::EndChild();
    ImGui::SetCursorScreenPos({origin.x, origin.y + height});
    ImGui::Dummy({0, 0});
}
void DrawPanelEditor(Runtime &r) {
    if (r.resetEditorSelection) {
        r.editorSelection.fill(0);
        r.editorSelection[0] = 4;
        r.sceneSelection = 0;
        r.resetEditorSelection = false;
    }
    if (r.editorPage == 6) {
        r.editorPage = 1;
        r.editorSelection[1] = 6;
    }
    const auto p = ImGui::GetCursorScreenPos();
    auto *draw = ImGui::GetWindowDrawList();
    ImGui::PushFont(nullptr, 12);
    if (r.editorPage != 0 && r.editorPage != 4 && r.editorPage != 8) {
        const auto parent=r.editorPage==9?UiText::WorldTab:UiText::Image;
        const float width = ImGui::CalcTextSize(Tr(parent)).x + 12;
        if (PanelTextButton("back", Tr(parent), {width, 25}))
            r.editorPage = r.editorPage==9?4:0;
        TextAt(draw, {p.x + width + 1, p.y + 6}, "/", Muted, 12);
        TextAt(draw, {p.x + width + 14, p.y + 6}, PanelTitle(r.editorPage), Text, 12);
    } else {
        TextAt(draw, {p.x, p.y + 6}, PanelTitle(r.editorPage), Text, 12);
        ImGui::Dummy({0, 25});
    }
    ImGui::PopFont();
    ImGui::SetCursorScreenPos({p.x, p.y + 31});
    ImGui::Dummy({0, 0});
    PanelEditor(r);
}
