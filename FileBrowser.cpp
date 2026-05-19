#include "FileBrowser.h"
#include "imgui.h"
#include <algorithm>
#include <windows.h>
#include <shellapi.h>
#undef CreateDirectory   // 这一行是关键！
#undef CreateDirectoryA
#undef CreateDirectoryW
#pragma comment(lib, "shell32.lib")
std::string FileBrowser::WStringToUTF8(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, NULL, NULL);
    if (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
}
FileBrowser::FileBrowser()
    : m_historyIndex(-1)
    , m_showNewDirDialog(false)
    , m_showRenameDialog(false)
    , m_showDeleteConfirm(false)
    , m_showDriveList(false)
    , m_contextMenuItem(-1)
{
    m_clipboard.type = ClipboardOperation::None;
    m_columnWidths[0] = 350.0f;
    m_columnWidths[1] = 120.0f;
    m_columnWidths[2] = 150.0f;
    m_columnWidths[3] = 100.0f;
    m_drives = GetDrives();
    if (!m_drives.empty()) SetRootPath(m_drives[0]);
}

FileBrowser::~FileBrowser() {}

std::vector<std::string> FileBrowser::GetDrives()
{
    std::vector<std::string> drives;
    DWORD drivesMask = GetLogicalDrives();
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        if (drivesMask & 1) drives.push_back(std::string(1, drive) + ":\\");
        drivesMask >>= 1;
    }
    return drives;
}

void FileBrowser::SetRootPath(const std::string& path) { m_rootPath = path; NavigateTo(path); }

void FileBrowser::NavigateTo(const std::string& path)
{
    if (!fs::exists(path) || !fs::is_directory(path)) return;
    if (!m_currentPath.empty() && m_historyIndex < (int)m_navigationHistory.size() - 1)
        m_navigationHistory.erase(m_navigationHistory.begin() + m_historyIndex + 1, m_navigationHistory.end());
    if (!m_currentPath.empty()) { m_navigationHistory.push_back(m_currentPath); m_historyIndex++; }
    m_currentPath = path;
    LoadDirectoryContents();
}

void FileBrowser::NavigateUp()
{
    if (m_currentPath.size() > 3) NavigateTo(fs::path(m_currentPath).parent_path().string());
    else m_showDriveList = true;
}

void FileBrowser::NavigateBack()
{
    if (m_historyIndex > 0) { m_historyIndex--; m_currentPath = m_navigationHistory[m_historyIndex]; LoadDirectoryContents(); }
}

void FileBrowser::NavigateForward()
{
    if (m_historyIndex < (int)m_navigationHistory.size() - 1) { m_historyIndex++; m_currentPath = m_navigationHistory[m_historyIndex]; LoadDirectoryContents(); }
}

void FileBrowser::Refresh() { LoadDirectoryContents(); }

void FileBrowser::LoadDirectoryContents()
{
    m_items.clear();
    m_selectedItems.clear();
    try {
        if (m_currentPath.size() > 3) {
            FileItem parent;
            parent.name = "..";
            parent.path = fs::path(m_currentPath).parent_path().string();
            parent.isDirectory = true;
            parent.size = 0;
            m_items.push_back(parent);
        }
        for (const auto& entry : fs::directory_iterator(m_currentPath)) {
            FileItem item;
            std::wstring wname = entry.path().filename().wstring();
            item.name = WStringToUTF8(wname);
            item.path = entry.path().string();
            item.isDirectory = entry.is_directory();
            item.extension = item.isDirectory ? "" : entry.path().extension().string();
            item.size = (!item.isDirectory) ? (fs::file_size(entry.path())) : 0;
            item.modifiedTime = fs::last_write_time(entry.path());
            DWORD attributes = GetFileAttributesA(item.path.c_str());
            item.isHidden = (attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
            if (!item.isHidden) m_items.push_back(item);
        }
        std::sort(m_items.begin() + (m_currentPath.size() > 3 ? 1 : 0), m_items.end(),
            [](const FileItem& a, const FileItem& b) {
                if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
                return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
            });
    }
    catch (...) {}
}

void FileBrowser::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("File Explorer", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Folder")) CreateDirectory();
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) exit(0);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Copy")) CopySelected();
                if (ImGui::MenuItem("Cut")) CutSelected();
                if (ImGui::MenuItem("Paste")) Paste();
                ImGui::Separator();
                if (ImGui::MenuItem("Delete")) DeleteSelected();
                if (ImGui::MenuItem("Rename")) RenameSelected();
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        RenderToolbar();
        RenderPathBar();
        RenderSearchBar();
        ImGui::Separator();
        ImGui::BeginChild("MainContent");
        ImGui::BeginChild("Drives", ImVec2(150, 0), true);
        RenderDriveList();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("FileArea");
        RenderFileList();
        ImGui::EndChild();
        ImGui::EndChild();
        RenderStatusBar();
        RenderContextMenu();
        if (m_showNewDirDialog) {
            ImGui::OpenPopup("New Folder");
            if (ImGui::BeginPopupModal("New Folder", &m_showNewDirDialog)) {
                static char dirName[256] = "";
                ImGui::InputText("Name", dirName, sizeof(dirName));
                if (ImGui::Button("Create")) {
                    if (strlen(dirName) > 0) {
                        fs::create_directory(fs::path(m_currentPath) / dirName);
                        Refresh();
                        m_showNewDirDialog = false;
                        memset(dirName, 0, sizeof(dirName));
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    m_showNewDirDialog = false;
                    memset(dirName, 0, sizeof(dirName));
                }
                ImGui::EndPopup();
            }
        }
        if (m_showDriveList) {
            ImGui::OpenPopup("Select Drive");
            if (ImGui::BeginPopupModal("Select Drive", &m_showDriveList)) {
                for (const auto& drive : m_drives) {
                    if (ImGui::Button(drive.c_str(), ImVec2(100, 40))) {
                        NavigateTo(drive);
                        m_showDriveList = false;
                    }
                }
                ImGui::EndPopup();
            }
        }
        if (m_showDeleteConfirm && m_contextMenuItem >= 0 && m_contextMenuItem < (int)m_items.size()) {
            ImGui::OpenPopup("Confirm Delete");
            if (ImGui::BeginPopupModal("Confirm Delete", &m_showDeleteConfirm)) {
                ImGui::Text("Delete '%s'?", m_items[m_contextMenuItem].name.c_str());
                if (ImGui::Button("Delete")) {
                    fs::remove_all(fs::path(m_currentPath) / m_items[m_contextMenuItem].name);
                    Refresh();
                    m_showDeleteConfirm = false;
                    m_contextMenuItem = -1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) m_showDeleteConfirm = false;
                ImGui::EndPopup();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void FileBrowser::RenderDriveList()
{
    ImGui::Text("Drives");
    ImGui::Separator();
    for (const auto& drive : m_drives) {
        if (ImGui::Selectable(drive.c_str(), m_currentPath.substr(0, 3) == drive)) NavigateTo(drive);
    }
}

void FileBrowser::RenderToolbar()
{
    ImGui::BeginChild("Toolbar", ImVec2(0, 45), false);
    if (ImGui::Button("< Back")) NavigateBack();
    ImGui::SameLine();
    if (ImGui::Button("Fwd >")) NavigateForward();
    ImGui::SameLine();
    if (ImGui::Button("Up")) NavigateUp();
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) Refresh();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    if (ImGui::Button("New Folder")) CreateDirectory();
    ImGui::SameLine();
    if (ImGui::Button("Delete")) DeleteSelected();
    ImGui::SameLine();
    if (ImGui::Button("Rename")) RenameSelected();
    ImGui::EndChild();
}

void FileBrowser::RenderPathBar()
{
    ImGui::BeginChild("PathBar", ImVec2(0, 35), false);
    ImGui::Text("Path: ");
    ImGui::SameLine();
    static char pathBuffer[512];
    strcpy_s(pathBuffer, m_currentPath.c_str());
    ImGui::PushItemWidth(500);
    if (ImGui::InputText("##Path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (fs::exists(pathBuffer)) NavigateTo(pathBuffer);
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
}

void FileBrowser::RenderSearchBar()
{
    ImGui::BeginChild("SearchBar", ImVec2(0, 35), false);
    ImGui::Text("Search: ");
    ImGui::SameLine();
    static char searchBuffer[256] = "";
    if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer))) m_searchFilter = searchBuffer;
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_searchFilter.clear();
        memset(searchBuffer, 0, sizeof(searchBuffer));
    }
    ImGui::EndChild();
}

void FileBrowser::RenderFileList()
{
    ImGui::BeginChild("FileList", ImVec2(0, 0), false);
    ImGui::Columns(4);
    ImGui::SetColumnWidth(0, 300);
    ImGui::SetColumnWidth(1, 100);
    ImGui::SetColumnWidth(2, 150);
    ImGui::Text("Name"); ImGui::NextColumn();
    ImGui::Text("Size"); ImGui::NextColumn();
    ImGui::Text("Modified"); ImGui::NextColumn();
    ImGui::Text("Type"); ImGui::NextColumn();
    ImGui::Separator();
    for (int i = 0; i < (int)m_items.size(); i++) {
        const auto& item = m_items[i];
        if (!m_searchFilter.empty()) {
            std::string lowerName = item.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find(m_searchFilter) == std::string::npos) continue;
        }
        bool isSelected = std::find(m_selectedItems.begin(), m_selectedItems.end(), i) != m_selectedItems.end();
        if (isSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        ImGui::Text("%s%s", item.isDirectory ? "[D] " : "[F] ", item.name.c_str()); ImGui::NextColumn();
        if (!item.isDirectory && item.size > 0) ImGui::Text("%s", FormatFileSize(item.size).c_str());
        else if (item.isDirectory && item.name != "..") ImGui::Text("<DIR>");
        else ImGui::Text("");
        ImGui::NextColumn();
        ImGui::Text("%s", FormatTime(item.modifiedTime).c_str()); ImGui::NextColumn();
        ImGui::Text("%s", item.isDirectory ? "Folder" : (item.extension.empty() ? "File" : item.extension.c_str())); ImGui::NextColumn();
        if (isSelected) ImGui::PopStyleColor();
        std::string id = "##item" + std::to_string(i);
        if (ImGui::Selectable(id.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
            m_selectedItems.clear();
            m_selectedItems.push_back(i);
            m_contextMenuItem = i;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (item.isDirectory) {
                if (item.name == "..") NavigateUp();
                else NavigateTo(item.path);
            }
            else {
                ShellExecuteA(nullptr, "open", item.path.c_str(), nullptr, nullptr, SW_SHOW);
            }
        }
        ImGui::Separator();
    }
    ImGui::Columns(1);
    ImGui::EndChild();
}

void FileBrowser::RenderStatusBar()
{
    ImGui::BeginChild("StatusBar", ImVec2(0, ImGui::GetFrameHeightWithSpacing()), false);
    int count = (int)m_items.size();
    ImGui::Text("Items: %d", count);
    ImGui::EndChild();
}

void FileBrowser::RenderContextMenu()
{
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("New Folder")) CreateDirectory();
        if (m_contextMenuItem >= 0) {
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) DeleteSelected();
            if (ImGui::MenuItem("Rename")) RenameSelected();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy")) CopySelected();
            if (ImGui::MenuItem("Cut")) CutSelected();
            if (ImGui::MenuItem("Paste")) Paste();
        }
        ImGui::EndPopup();
    }
}

std::string FileBrowser::FormatFileSize(uintmax_t size)
{
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int idx = 0;
    double s = (double)size;
    while (s >= 1024.0 && idx < 4) { s /= 1024.0; idx++; }
    char buf[64];
    sprintf_s(buf, "%.1f %s", s, units[idx]);
    return std::string(buf);
}

std::string FileBrowser::FormatTime(const fs::file_time_type& time)
{
    return "";
}

int FileBrowser::GetIconIndex(const FileItem& item) { return 0; }

void FileBrowser::CreateDirectory() { m_showNewDirDialog = true; }

void FileBrowser::RenameSelected()
{
    if (!m_selectedItems.empty()) {
        m_contextMenuItem = m_selectedItems[0];
    }
}

void FileBrowser::DeleteSelected()
{
    if (!m_selectedItems.empty()) {
        m_contextMenuItem = m_selectedItems[0];
        m_showDeleteConfirm = true;
    }
}

void FileBrowser::CopySelected()
{
    m_clipboard.type = ClipboardOperation::Copy;
    m_clipboard.paths.clear();
    for (int idx : m_selectedItems) m_clipboard.paths.push_back(m_items[idx].path);
}

void FileBrowser::CutSelected()
{
    m_clipboard.type = ClipboardOperation::Cut;
    m_clipboard.paths.clear();
    for (int idx : m_selectedItems) m_clipboard.paths.push_back(m_items[idx].path);
}

void FileBrowser::Paste()
{
    if (m_clipboard.type == ClipboardOperation::None) return;
    for (const auto& src : m_clipboard.paths) {
        fs::path dst = fs::path(m_currentPath) / fs::path(src).filename();
        try {
            if (m_clipboard.type == ClipboardOperation::Copy) {
                if (fs::is_directory(src)) fs::copy(src, dst, fs::copy_options::recursive);
                else fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
            }
            else {
                fs::rename(src, dst);
            }
        }
        catch (...) {}
    }
    if (m_clipboard.type == ClipboardOperation::Cut) {
        m_clipboard.type = ClipboardOperation::None;
        m_clipboard.paths.clear();
    }
    Refresh();
}