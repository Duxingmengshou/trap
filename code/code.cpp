#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>

int main() {
    // 获取 %TEMP% 路径
    char tempPath[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tempPath)) {
        std::cerr << "无法获取临时目录路径！" << std::endl;
        return 1;
    }

    std::string filePath = std::string(tempPath) + "__TEMPCLIP.GlobalTYDecode";

    // 打开剪贴板
    if (!OpenClipboard(nullptr)) {
        std::cerr << "无法打开剪贴板！" << std::endl;
        return 1;
    }

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "无法写入文件！" << std::endl;
        CloseClipboard();
        return 1;
    }

    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        // 剪贴板是文本
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            LPCWSTR pszText = static_cast<LPCWSTR>(GlobalLock(hData));
            if (pszText) {
                // 转换成 UTF-8 保存
                int len = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    // 如果超过 256MB，直接写入标识
                    const size_t MAX_SIZE = 256ull * 1024 * 1024;
                    if ((size_t)len > MAX_SIZE) {
                        std::ofstream outFile(filePath, std::ios::binary);
                        std::string marker = "__TOO_LARGE__";
                        outFile.write(marker.c_str(), marker.size());
                        outFile.close();
                        std::cout << "文本内容过大 (>256MB)，写入标识: " << marker << std::endl;
                    }
                    else {
                        std::string utf8Str(len - 1, '\0'); // -1 去掉结尾的 \0
                        WideCharToMultiByte(CP_UTF8, 0, pszText, -1, utf8Str.data(), len, nullptr, nullptr);

                        std::ofstream outFile(filePath, std::ios::binary);
                        if (outFile.is_open()) {
                            outFile.write(utf8Str.c_str(), utf8Str.size());
                            outFile.close();
                            std::cout << "已将文本写入: " << filePath << std::endl;
                        }
                        else {
                            std::cerr << "无法写入文件！" << std::endl;
                        }
                    }
                }

            }
            else {
                std::cerr << "无法锁定剪贴板数据！" << std::endl;
            }
            GlobalUnlock(hData);
        }
    }
    else {
        // 剪贴板中不是文本，写入标识
        std::string marker = "__NON_TEXT__";
        outFile.write(marker.c_str(), marker.size());
        std::cout << "检测到非文本内容，写入标识: " << marker << std::endl;
    }

    outFile.close();
    CloseClipboard();
    return 0;
}
