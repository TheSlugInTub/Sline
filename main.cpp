#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <csignal>
#include <set>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#    include <windows.h>
#    include <Lmcons.h>
#else
#    include <unistd.h>
#    include <pwd.h>
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <errno.h>
#endif

struct Color
{
    int r = 0;
    int g = 0;
    int b = 0;

    Color(int r, int g, int b) : r(r), g(g), b(b) {}

    Color() {}
};

void PrintColoredText(const std::string& text, const Color& fg, const Color& bg = {})
{
    // Build the VT sequence string
    std::stringstream ss;
    ss << "\x1b[38;2;" // Foreground color
       << static_cast<int>(fg.r) << ";" << static_cast<int>(fg.g) << ";" << static_cast<int>(fg.b)
       << "m"
       << "\x1b[48;2;" // Background color
       << static_cast<int>(bg.r) << ";" << static_cast<int>(bg.g) << ";" << static_cast<int>(bg.b)
       << "m" << text << "\x1b[0m"; // Reset formatting

    // Print the colored text
    std::cout << ss.str();
}

void PrintColoredTextWithoutBackground(const std::string& text, const Color& fg)
{
    // Build the VT sequence string - only foreground color
    std::stringstream ss;
    ss << "\x1b[38;2;" // Foreground color
       << static_cast<int>(fg.r) << ";" << static_cast<int>(fg.g) << ";" << static_cast<int>(fg.b)
       << "m" << text << "\x1b[0m"; // Reset formatting

    // Print the colored text
    std::cout << ss.str();
}

std::vector<std::filesystem::directory_entry>
ListFilesInDirectory(const std::filesystem::path&    directoryPath,
                     const std::vector<std::string>& extensions)
{
    std::vector<std::filesystem::directory_entry> files;
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath))
    {
        if (entry.is_directory() || extensions.empty() ||
            std::any_of(extensions.begin(), extensions.end(), [&entry](const std::string& ext)
                        { return entry.path().extension() == ext; }))
        {
            files.push_back(entry);
        }
    }

    return files;
}

void DisplayDirectoryContents(const std::filesystem::path&    directoryPath,
                              const std::vector<std::string>& extensions)
{
    auto files = ListFilesInDirectory(directoryPath, extensions);

    for (const auto& entry : files)
    {
        if (entry.is_directory())
        {
            PrintColoredText(entry.path().filename().string() + "\n", Color(0, 0, 0),
                             Color(50, 180, 20));
        }
    }

    for (const auto& entry : files)
    {
        if (!entry.is_directory())
        {
            PrintColoredTextWithoutBackground(entry.path().filename().string() + "\n", Color(255, 255, 255));
        }
    }
}

void RunStartupFile();

void ExecuteCommand(const std::string& input)
{
    if (input.substr(0, 3) == "cd ")
    {
        std::string path = input.substr(3);

        if (std::filesystem::exists(path))
            std::filesystem::current_path(std::filesystem::path(path));
        else
            std::cout << "Directory does not exist!\n";
    }
    else if (input == "exit")
    {
        exit(0);
    }
    else if (input == "ls")
    {
        std::string              currentDir = std::filesystem::current_path().string();
        std::filesystem::path    lsPath = currentDir;
        std::vector<std::string> bruh;
        DisplayDirectoryContents(lsPath, bruh);
    }
    else if (input == "clear")
    {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }
    else if (input == "runrc")
    {
        RunStartupFile();
    }
    else
    {
        system(input.c_str());
    }
}

std::string GetConfigDirectory()
{
#ifdef _WIN32
    char  username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    GetUserName(username, &username_len);
    std::string usernameStr(username);

    char driveLetter = 0;
    for (char letter = 'C'; letter <= 'Z'; ++letter) // Most systems use C-Z for fixed drives
    {
        std::string drivePath = std::string(1, letter) + ":\\";
        if (GetDriveTypeA(drivePath.c_str()) == DRIVE_FIXED)
        {
            driveLetter = letter;
            break;
        }
    }

    std::filesystem::path configDir = std::filesystem::path(
        std::string(1, driveLetter) + ":\\Users\\" + usernameStr + "\\AppData\\Roaming\\Sline\\");

    std::filesystem::create_directories(configDir);

    return configDir.string();
#else
    struct passwd* pw = getpwuid(getuid());
    if (!pw)
    {
        throw std::runtime_error("Failed to get user information");
    }

    std::string configDir = std::string(pw->pw_dir) + "/.config/Sline/";

    // You might want to create the directory if it doesn't exist
    // Note: You'll need to #include <sys/stat.h> and <errno.h> for this
    /*
    if (mkdir(configDir.c_str(), 0755) == -1 && errno != EEXIST) {
        std::cout << "Failed to create config directory: " << configDir << '\n';
    }
    */

    return configDir;
#endif
}

void RunStartupFile()
{
    std::string rcName = ".sline_rc";
    std::string rcPath = GetConfigDirectory() + rcName;

    std::ofstream rcfileOF(rcPath);

    std::ifstream rcfile(rcPath);
    std::string   line;

    if (rcfile.is_open())
    {
        while (std::getline(rcfile, line))
        {
            std::cout << line;
            ExecuteCommand(line);
            ;
        }

        rcfile.close();
    }
    else
    {
        std::cerr << "Unable to open RC file at: " << rcPath << std::endl;
    }
    rcfileOF.close();
}

void SignalCallback(int signum)
{
    // Instead of terminating, set flag to gracefully exit
    // if (signum == SIGINT)
    // {
    //     std::cout << "\nReceived interrupt signal. Shutting down gracefully...\n";
    // }
    return;
}

void Initialize()
{
    signal(SIGINT, SignalCallback);
#if _WIN32
    // Enable virtual terminal processing
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    SetConsoleOutputCP(CP_UTF8);
#else
#endif
}

int main()
{
    std::string input;

    Initialize();
    RunStartupFile();

    while (true)
    {
        std::string currentDir = std::filesystem::current_path().string();

        PrintColoredText(" ", Color {}, Color(100, 155, 222));
        PrintColoredText(currentDir + " ", Color(), Color(100, 155, 222));
        PrintColoredTextWithoutBackground(" ", Color(100, 155, 222));

        std::getline(std::cin, input);

        if (input.empty())
        {
            continue;
        }

        ExecuteCommand(input);
    }

    return 0;
}
