#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

struct FileItem {
    std::string name;
    std::string path;
    std::string extension;
    uintmax_t size;
    fs::file_time_type modifiedTime;
    bool isDirectory;
    bool isHidden;
    int iconIndex;
    FileItem() : size(0), isDirectory(false), isHidden(false), iconIndex(0) {}
};

class FileBrowser {
public:
    FileBrowser();
    ~FileBrowser();
    std::string WStringToUTF8(const std::wstring& wstr);
    void SetRootPath(const std::string& path);
    void NavigateTo(const std::string& path);
    void NavigateUp();
    void NavigateBack();
    void NavigateForward();
    void Refresh();
    void Render();
    void CreateDirectory();
    void RenameSelected();
    void DeleteSelected();
    void CopySelected();
    void CutSelected();
    void Paste();
    const std::string& GetCurrentPath() const { return m_currentPath; }
private:
    void LoadDirectoryContents();
    void RenderToolbar();
    void RenderPathBar();
    void RenderFileList();
    void RenderStatusBar();
    void RenderContextMenu();
    void RenderSearchBar();
    void RenderDriveList();
    std::string FormatFileSize(uintmax_t size);
    std::string FormatTime(const fs::file_time_type& time);
    int GetIconIndex(const FileItem& item);
    std::vector<std::string> GetDrives();
private:
    std::string m_currentPath;
    std::string m_rootPath;
    std::vector<FileItem> m_items;
    std::vector<std::string> m_navigationHistory;
    int m_historyIndex;
    std::vector<int> m_selectedItems;
    std::string m_searchFilter;
    struct ClipboardOperation {
        enum Type { None, Copy, Cut } type;
        std::vector<std::string> paths;
        ClipboardOperation() : type(None) {}
    } m_clipboard;
    bool m_showNewDirDialog;
    bool m_showRenameDialog;
    bool m_showDeleteConfirm;
    bool m_showDriveList;
    std::string m_newDirName;
    std::string m_renameOldName;
    std::string m_renameNewName;
    int m_contextMenuItem;
    float m_columnWidths[5];
    std::vector<std::string> m_drives;
};