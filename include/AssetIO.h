// include/assets/AssetIO.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <ShObjIdl.h> // IFileOpenDialog
#include <Urlmon.h>   // URLDownloadToFileW
#include <filesystem>
#include <string>
#include <optional>
#include <vector>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <system_error>

#include "PathManager.h" // must provide getMarkersPath(), getMapsPath()
#include "imgui.h"
#include "ImGuiToaster.h"

namespace AssetIO
{

    enum class AssetKind
    {
        Marker,
        Map
    };

    inline const std::filesystem::path& assetsDir(AssetKind kind)
    {
        static std::filesystem::path markers = PathManager::getMarkersPath();
        static std::filesystem::path maps = PathManager::getMapsPath();
        return (kind == AssetKind::Marker) ? markers : maps;
    }

    struct ComInit
    {
        HRESULT hr{E_FAIL};
        ComInit()
        {
            hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        }
        ~ComInit()
        {
            if (SUCCEEDED(hr))
                CoUninitialize();
        }
        bool ok() const
        {
            return SUCCEEDED(hr);
        }
    };

    // Lowercase + replace spaces with '_' + strip weird chars
    inline std::string slugify(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (unsigned char ch : s)
        {
            char c = static_cast<char>(std::tolower(ch));
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')
            {
                out.push_back(c);
            }
            else if (std::isspace(static_cast<unsigned char>(c)))
            {
                out.push_back('_');
            } // else drop
        }
        if (out.empty())
            out = "asset";
        return out;
    }

    // Ensure unique filename inside destDir (keeps extension)
    inline std::filesystem::path uniqueName(const std::filesystem::path& destDir,
                                            const std::string& baseFile)
    {
        std::filesystem::path name = destDir / baseFile;
        if (!std::filesystem::exists(name))
            return name;

        auto stem = std::filesystem::path(baseFile).stem().string();
        auto ext = std::filesystem::path(baseFile).extension().string();
        for (int i = 1; i < 100000; ++i)
        {
            auto trial = destDir / (stem + " (" + std::to_string(i) + ")" + ext);
            if (!std::filesystem::exists(trial))
                return trial;
        }
        // fallback improbable
        return destDir / (stem + "_dup" + ext);
    }

    // Windows modern file picker (images)
    inline std::optional<std::filesystem::path> pickImageFileWin32()
    {
        ComInit com;
        if (!com.ok())
            return std::nullopt;

        IFileOpenDialog* pDlg = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&pDlg));
        if (FAILED(hr) || !pDlg)
            return std::nullopt;

        // Filter for common image types; stb_image supports png/jpg/bmp/tga/gif/psd/pic/hdr
        COMDLG_FILTERSPEC filters[] = {
            {L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp;*.tga)", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp;*.tga"},
            {L"All files (*.*)", L"*.*"}};
        pDlg->SetFileTypes(2, filters);
        pDlg->SetFileTypeIndex(1);
        pDlg->SetOptions(FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        std::optional<std::filesystem::path> result;

        hr = pDlg->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDlg->GetResult(&pItem)) && pItem)
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath)
                {
                    result = std::filesystem::path(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pDlg->Release();
        return result;
    }

    // Copy a source file into app assets folder for given kind
    inline bool importFromPath(AssetKind kind, const std::filesystem::path& srcPath,
                               std::filesystem::path* outDst = nullptr,
                               std::string* outError = nullptr)
    {
        std::error_code ec;
        if (!std::filesystem::exists(srcPath))
        {
            if (outError)
                *outError = "Source does not exist";
            return false;
        }
        auto destRoot = assetsDir(kind);
        std::filesystem::create_directories(destRoot, ec);
        ec.clear();

        // Build a nice destination name
        auto filename = srcPath.filename().string();
        auto ext = std::filesystem::path(filename).extension().string();
        auto stem = std::filesystem::path(filename).stem().string();
        std::string base = slugify(stem) + ext;
        auto dst = uniqueName(destRoot, base);

        std::filesystem::copy_file(srcPath, dst,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            if (outError)
                *outError = "Copy failed: " + ec.message();
            return false;
        }
        if (outDst)
            *outDst = dst;
        return true;
    }

    // File picker → import into kind
    inline bool importFromPicker(AssetKind kind,
                                 std::filesystem::path* outDst = nullptr,
                                 std::string* outError = nullptr)
    {
        auto p = pickImageFileWin32();
        if (!p)
        {
            if (outError)
                *outError = "User cancelled";
            return false;
        }
        return importFromPath(kind, *p, outDst, outError);
    }

    // NEW: import many picked files
    inline bool importManyFromPicker(AssetKind kind,
                                     std::vector<std::filesystem::path>* outDsts = nullptr,
                                     std::string* outError = nullptr)
    {
        auto files = pickImageFilesWin32();
        if (files.empty()) {
            if (outError) *outError = "User cancelled";
            return false;
        }
    
        size_t ok = 0, fail = 0;
        std::vector<std::filesystem::path> dsts_local;
        for (auto& f : files) {
            std::filesystem::path dst;
            std::string err;
            if (importFromPath(kind, f, &dst, &err)) {
                ++ok;
                if (outDsts) outDsts->push_back(dst);
                else dsts_local.push_back(dst);
            } else {
                ++fail;
            }
        }
    
        if (ok == 0) {
            if (outError) *outError = "All imports failed";
            return false;
        }
        if (fail > 0 && outError) {
            *outError = "Imported " + std::to_string(ok) + " file(s), " + std::to_string(fail) + " failed";
        }
        return true;
    }
    // Download a URL directly into assets dir (uses URLMon; supports http/https)
    inline bool importFromUrl(AssetKind kind, const std::wstring& urlW,
                              std::filesystem::path* outDst = nullptr,
                              std::string* outError = nullptr)
    {
        // Guess filename from URL path
        std::wstring fnameW = L"asset";
        auto slash = urlW.find_last_of(L"/\\");
        if (slash != std::wstring::npos && slash + 1 < urlW.size())
        {
            fnameW = urlW.substr(slash + 1);
            if (fnameW.empty())
                fnameW = L"asset";
        }
        // sanitize/slugify
        std::string fnameUtf8(fnameW.begin(), fnameW.end());
        auto ext = std::filesystem::path(fnameUtf8).extension().string();
        auto stem = std::filesystem::path(fnameUtf8).stem().string();
        std::string base = slugify(stem) + ext;

        auto destRoot = assetsDir(kind);
        std::error_code ec;
        std::filesystem::create_directories(destRoot, ec);
        ec.clear();

        auto dst = uniqueName(destRoot, base);
        std::wstring dstW = dst.wstring();

        HRESULT hr = URLDownloadToFileW(nullptr, urlW.c_str(), dstW.c_str(), 0, nullptr);
        if (FAILED(hr))
        {
            if (outError)
                *outError = "Download failed (HRESULT " + std::to_string(hr) + ")";
            return false;
        }
        if (outDst)
            *outDst = dst;
        return true;
    }

    // List assets for UI (absolute paths)
    inline std::vector<std::filesystem::path> listAssets(AssetKind kind)
    {
        std::vector<std::filesystem::path> v;
        auto root = assetsDir(kind);
        std::error_code ec;
        if (!std::filesystem::exists(root))
            return v;
        for (auto& e : std::filesystem::directory_iterator(root, ec))
        {
            if (ec)
                break;
            if (e.is_regular_file())
                v.push_back(e.path());
        }
        std::sort(v.begin(), v.end());
        return v;
    }


    inline bool deleteAsset(AssetKind kind, const std::filesystem::path& file,
                            std::string* outError = nullptr)
    {
        std::error_code ec;
    
        // Resolve and validate root
        auto root = std::filesystem::weakly_canonical(assetsDir(kind), ec);
        if (ec || root.empty() || !std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
            if (outError) *outError = "Invalid assets root";
            return false;
        }
    
        // Resolve the target (weakly_canonical lets non-existing leaf still resolve)
        auto target = std::filesystem::weakly_canonical(file, ec);
        if (ec || target.empty()) {
            if (outError) *outError = "Path canonicalization failed";
            return false;
        }
    
        // Safety: target must be under root (and on same root/drive)
        // lexically_relative returns a path that does not start with ".." if target is inside root.
        auto rel = target.lexically_relative(root);
        if (rel.empty() || rel.native().starts_with(L"..") || rel.is_absolute()) {
            if (outError) *outError = "Refusing to delete outside assets dir";
            return false;
        }
    
        // Validate file
        if (!std::filesystem::exists(target)) {
            if (outError) *outError = "File not found";
            return false;
        }
        if (!std::filesystem::is_regular_file(target)) {
            if (outError) *outError = "Not a file";
            return false;
        }
    
        // Delete
        std::filesystem::remove(target, ec);
        if (ec) {
            if (outError) *outError = "Delete failed: " + ec.message();
            return false;
        }
        return true;
    }

   // NEW: pick multiple files
    inline std::vector<std::filesystem::path> pickImageFilesWin32()
    {
        std::vector<std::filesystem::path> out;
        ComInit com;
        if (!com.ok()) return out;
    
        IFileOpenDialog* pDlg = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDlg))) || !pDlg)
            return out;
    
        COMDLG_FILTERSPEC filters[] = {
            {L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp;*.tga)", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp;*.tga"},
            {L"All files (*.*)", L"*.*"}
        };
        pDlg->SetFileTypes(2, filters);
        pDlg->SetFileTypeIndex(1);
    
        DWORD opts = FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT; // <- key change
        pDlg->SetOptions(opts);
    
        if (SUCCEEDED(pDlg->Show(nullptr)))
        {
            IShellItemArray* pArray = nullptr;
            if (SUCCEEDED(pDlg->GetResults(&pArray)) && pArray)
            {
                DWORD count = 0;
                pArray->GetCount(&count);
                for (DWORD i = 0; i < count; ++i)
                {
                    IShellItem* pItem = nullptr;
                    if (SUCCEEDED(pArray->GetItemAt(i, &pItem)) && pItem)
                    {
                        PWSTR psz = nullptr;
                        if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &psz)) && psz)
                        {
                            out.emplace_back(psz);
                            CoTaskMemFree(psz);
                        }
                        pItem->Release();
                    }
                }
                pArray->Release();
            }
        }
        pDlg->Release();
        return out;
    }

   


    inline void openDeleteAssetPopUp(std::weak_ptr<ImGuiToaster> toaster_)
    {
        ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginPopupModal("DeleteAssets", nullptr))
        {
            static int tab = 0;
            ImGui::RadioButton("Markers", &tab, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Maps", &tab, 1);
            auto kind = (tab == 0) ? AssetKind::Marker : AssetKind::Map;

            auto assets = listAssets(kind);
            ImGui::Separator();
            ImGui::BeginChild("assets_scroll", ImVec2(500, 300), true);
            bool deletedThisFrame = false;
            for (auto& p : assets)
            {
                auto fname = p.filename().string();
                ImGui::TextUnformatted(fname.c_str());
                ImGui::SameLine();
                ImGui::PushID(fname.c_str());
                if (ImGui::SmallButton(("Delete##" + fname).c_str()))
                {
                    std::string err;
                    if (!AssetIO::deleteAsset(kind, p, &err))
                    {
                        std::cerr << "Delete failed: " << err << "\n";
                        if (auto t = toaster_.lock(); t)
                            t->Push(ImGuiToaster::Level::Error, "Delete failed: " + err);
                    }
                    else
                    {
                        deletedThisFrame = true;
                        if (auto t = toaster_.lock(); t)
                            t->Push(ImGuiToaster::Level::Good, "Image Deleted Successfully!!");
                    }
                }
                ImGui::PopID();
                if (deletedThisFrame) break;
            }
            ImGui::EndChild();
            ImGui::Separator();
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    //#TODO - INCOMPLETE EXTRA CRED
    inline void openUrlAssetPopUp(std::weak_ptr<ImGuiToaster> toaster_)
    {

        if (ImGui::BeginPopupModal("UrlAssets", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static int tab = 0;
            ImGui::RadioButton("Markers", &tab, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Maps", &tab, 1);
            auto kind = (tab == 0) ? AssetKind::Marker : AssetKind::Map;
            ImGui::Separator();
            auto url_color = (kind == AssetKind::Marker) ? ImVec4(0.60f, 0.80f, 1.00f, 1.0f) /*BLUE*/ :
                ImVec4(0.45f, 0.95f, 0.55f, 1.0f) /*GREEN*/;
            std::wstring url = L"https://example.com/some.png";
            std::filesystem::path dst = assetsDir(kind) ;
            std::string err;
            ImGui::PushStyleColor(ImGuiCol_Text, url_color);
            //ImGui::InputText("",url_color, url);
            ImGui::PopStyleColor();
            if (!AssetIO::importFromUrl(kind, url, &dst, &err))
            {
                std::cerr << "Import URL failed: " << err << "\n";
                if (auto t = toaster_.lock(); t)
                    t->Push(ImGuiToaster::Level::Error, "Import URL failed: " + err);
            }
            else
            {
                if (auto t = toaster_.lock(); t)
                    t->Push(ImGuiToaster::Level::Good, "Image Imported from URL");
            }

            ImGui::Separator();
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
} // namespace AssetIO
