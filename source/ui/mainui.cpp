#include "boxedwine.h"
#include "boxedwineui.h"

#include <SDL.h>
#include <stdio.h>

#include "examples/imgui_impl_sdl.h"
#include "examples/imgui_impl_opengl3.h"

static std::vector<ListViewItem> appListViewItems;
static std::vector<AppButton> appButtons;
static ImFont* appBarFont;
static ImFont* defaultFont;
static bool extendedAppBarFont;
static int currentView;

// should correspond to index in appButtons
#define VIEW_APPS 0
#define VIEW_CONTAINERS 2

void uiDraw() {    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.x));
    ImGui::Begin("mainWindow", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    drawAppBar(appButtons, currentView, appBarFont, extendedAppBarFont);
    ImGui::Separator();
    if (currentView==VIEW_CONTAINERS) {
        drawContainersView(ImGui::GetWindowContentRegionMax());
    } else {
        drawListView("Apps", appListViewItems, ImGui::GetWindowContentRegionMax());
    }
    BaseDlg::runDialogs();
    ImGui::End();
}

void loadFonts(const std::string& basePath) {
    std::string fontPath = basePath + "LiberationMono-Regular.ttf";
    if (Fs::doesNativePathExist(fontPath)) {
        ImGuiIO& io = ImGui::GetIO();
        defaultFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 15.0f);
        fontPath = basePath + "LiberationSans-Bold.ttf";
        if (Fs::doesNativePathExist(fontPath)) {
        appBarFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f);    
            if (appBarFont) {
                static const ImWchar icons_ranges[] = { 0xe000, 0xe0fe, 0 }; // Will not be copied by AddFont* so keep in scope.
                ImFontConfig config;
                config.MergeMode = true;
                fontPath = basePath + "OpenFontIcons.ttf";
                if (Fs::doesNativePathExist(fontPath)) {
                    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f, &config, icons_ranges);
                    extendedAppBarFont = io.Fonts->Build();
                }
            }
        }
    }
}

void createButton() {
    appButtons.clear();
    appButtons.push_back(AppButton(getTranslation(MAIN_BUTTON_APPS), [](){
        currentView = VIEW_APPS;
    }));
    appButtons.push_back(AppButton(getTranslation(MAIN_BUTTON_INSTALL), [](){
        new InstallDlg();
    }));    
    appButtons.push_back(AppButton(getTranslation(MAIN_BUTTON_CONTAINERS), [](){
        BoxedwineData::updateCachedContainerSizes();
        currentView = VIEW_CONTAINERS;
    }));
    appButtons.push_back(AppButton(getTranslation(MAIN_BUTTON_SETTINGS), [](){
        new SettingsDlg();
    }));
    appButtons.push_back(AppButton(getTranslation(MAIN_BUTTON_HELP), [](){
    }));
}

void loadApps() {
    appListViewItems.clear();
    for (auto& container : BoxedwineData::getContainers()) {
        for (auto& app : container->getApps()) {
            GLuint texture = 0;
            int width = 0;
            int height = 0;
            std::string iconPath = app->getIcon();
            if (iconPath.length()) {
                LoadTextureFromFile(iconPath.c_str(), &texture, &width, &height);
            }
            appListViewItems.push_back(ListViewItem(app->getName(), (void*)(U64)texture, width, height, [](bool right) {
            }));
        }
    }
}

bool uiShow(const std::string& basePath) {        
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    /*
    // done in main
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return false;
    }
    */

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Boxedwine UI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING)
    bool err = false;
    glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)SDL_GL_GetProcAddress(name); });
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();    
    loadFonts(basePath);
    BoxedwineData::loadUI();
    loadApps(); // need to be after we create the context for images to work
    createButton();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    GlobalSettings::loadTheme();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done && !GlobalSettings::startUpArgs.readyToLaunch)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        uiDraw();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    return GlobalSettings::startUpArgs.readyToLaunch;
}
