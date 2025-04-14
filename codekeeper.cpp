#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <openssl/sha.h>
#include <iomanip>
#include <cstring>
#include <regex>
#include <openssl/evp.h>
#include <sys/stat.h>


namespace fs = std::filesystem;

// Structure to hold metadata for commits
struct Commit {
    std::string message;
    std::string timestamp;
    std::vector<std::string> filePaths;
    std::vector<std::string> versionPaths;
};

std::string repositoryPath;


// Function to generate a timestamp
std::string getTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y_%m_%d_%H_%M_%S", std::localtime(&now));

    return buf;
}

// utility function
std::vector<std::string> split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to split a string by delimiter
std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool filesAreEqual(const std::string &filePath1, const std::string &filePath2)
{
    std::ifstream file1(filePath1, std::ios::binary);
    std::ifstream file2(filePath2, std::ios::binary);
    return std::equal(std::istreambuf_iterator<char>(file1), std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(file2));
}

// Escape special characters for log safety
std::string escape(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c == '|') output += "\\|";
        else output += c;
    }
    return output;
}

/// Function to compute SHA-256 hash of a file (simple implementation)
std::string computeFileHash(const fs::path& filePath) {
        std::ifstream file(filePath, std::ios::binary);
    if (!file) return "";

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) return "";

    const EVP_MD* md = EVP_sha256();
    if (1 != EVP_DigestInit_ex(mdctx, md, nullptr)) return "";

    char buffer[8192];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        if (1 != EVP_DigestUpdate(mdctx, buffer, file.gcount())) return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    if (1 != EVP_DigestFinal_ex(mdctx, hash, &hashLen)) return "";

    EVP_MD_CTX_free(mdctx);

    std::ostringstream result;
    for (unsigned int i = 0; i < hashLen; ++i) {
        result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return result.str();
}

//function to get current repo path
std::string getCentralRepositoryPath()
{
    std::ifstream configFile(".repo_path");
    std::string repositoryPath;
    if (configFile.is_open())
    {
        std::getline(configFile, repositoryPath);
    }
    return repositoryPath;
}

// Function to load the repository path
void loadRepositoryPath() {
    std::ifstream repoFile(".repo_path");
    if (repoFile.is_open()) {
        std::getline(repoFile, repositoryPath);
        repoFile.close();
    } else {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        repositoryPath.clear();
    }
}

// Initialize the repository
void initRepository(const std::string& projectName) 
{
    // std::string repoName = projectName.empty() ? fs::current_path().filename().string() : projectName;
    // fs::path baseDir = fs::current_path();
    // fs::path repoPath = fs::weakly_canonical(baseDir / fs::path(repoName).filename());

fs::path baseDir = "/var/lib/CodeKeeper";  // A safer, writable location
if (!fs::exists(baseDir)) {
    fs::create_directories(baseDir);
}

std::regex safeName("^[a-zA-Z0-9_-]+$");
if (!std::regex_match(projectName, safeName)) {
    throw std::runtime_error("Invalid project name: only letters, digits, _ and - are allowed.");
}

std::string repoName = projectName.empty() ? baseDir.filename().string() : projectName;
fs::path repoPath = fs::weakly_canonical(baseDir / fs::path(repoName).filename());


    if (!fs::exists(repoPath)) {
        fs::create_directory(repoPath);
    }

    repositoryPath = repoPath.string();

    std::ofstream repoFile(".repo_path");
    repoFile << repositoryPath;
    repoFile.close();

    fs::create_directory(repoPath / "versions");

    std::ofstream bypassFile(fs::current_path() / ".bypass");
    bypassFile << "# Add files or patterns to ignore\n";
    bypassFile.close();

    std::ofstream logFile(repoPath / "commit_log.txt");
    logFile.close();

    std::cout << "Repository '" << repositoryPath << "' initialized successfully.\n";
    std::cout << "Change your terminal to the project folder: cd " << repositoryPath << "\n";
}

// Function to commit multiple files
void commitFiles(const std::vector<std::string>& filePaths, const std::string& commitMessage) 
{
    loadRepositoryPath();
    fs::path repoPath = fs::weakly_canonical(repositoryPath);
    fs::path versionDir = repoPath / "versions";

    if (repositoryPath.empty() || !fs::exists(versionDir)) {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }

    std::ifstream bypassFile(fs::current_path() / ".bypass");
    std::vector<std::string> ignoredFiles;
    std::string line;
    while (std::getline(bypassFile, line)) {
        if (!line.empty() && line[0] != '#') {
            ignoredFiles.push_back(line);
        }
    }

    std::vector<std::string> versionPaths;
    std::vector<std::string> fileHashes;
    for (const auto& filePath : filePaths) {
        fs::path file = fs::absolute(filePath);

        if (!fs::exists(file)) {
            std::cerr << "Error: File " << file << " does not exist.\n";
            return;
        }

        if (std::find(ignoredFiles.begin(), ignoredFiles.end(), filePath) != ignoredFiles.end()) {
            std::cout << "Skipping ignored file: " << filePath << "\n";
            continue;
        }

        std::string versionFile = "version_" + std::to_string(std::time(nullptr)) + "_" + file.filename().string();
        fs::path versionFilePath = versionDir / versionFile;

        fs::copy(file, versionFilePath, fs::copy_options::overwrite_existing);
        versionPaths.push_back(versionFilePath.string());
        fileHashes.push_back(computeFileHash(file));
    }

    std::string timestamp = getTimestamp();
    std::ofstream logFile(repoPath / "commit_log.txt", std::ios::app);
    logFile << escape(commitMessage) << "|" << timestamp;

    for (size_t i = 0; i < filePaths.size(); ++i) {
        logFile << "|" << escape(fs::absolute(filePaths[i]).string())
                << "|" << fileHashes[i];
    }

    logFile << "|";

    for (const auto& versionPath : versionPaths) {
        logFile << versionPath << "|";
    }

    logFile << "\n";
    logFile.close();
    std::cout << "Files committed successfully with message: " << commitMessage << "\n";
}

// Function to retrieve files by commit message
void retrieveFiles(const std::string& commitMessage) {
    loadRepositoryPath();
    fs::path repoPath = fs::weakly_canonical(repositoryPath);
    fs::path logPath = repoPath / "commit_log.txt";

    if (repositoryPath.empty() || !fs::exists(logPath)) {
        std::cerr << "Error: Repository not initialized or log file missing.\n";
        return;
    }

    std::ifstream logFile(logPath);
    std::string line;

    while (std::getline(logFile, line)) {
        size_t pos = line.find('|');
        std::string message = line.substr(0, pos);

        if (message == escape(commitMessage)) {
            size_t pos2 = line.find('|', pos + 1);
            size_t pos3 = line.find_last_of('|');
            std::string fileData = line.substr(pos2 + 1, pos3 - pos2 - 1);
            std::vector<std::string> tokens = splitString(fileData, '|');
            std::vector<std::string> originalPaths, hashes;

            for (size_t i = 0; i + 1 < tokens.size(); i += 2) {
                originalPaths.push_back(tokens[i]);
                hashes.push_back(tokens[i + 1]);
            }

            size_t half = tokens.size() / 2;
            for (size_t i = 0; i < originalPaths.size(); ++i) {
                fs::path dest = fs::current_path() / fs::path(originalPaths[i]).filename();
                fs::copy(tokens[half + i * 2], dest, fs::copy_options::overwrite_existing);
            }

            std::cout << "Files retrieved successfully.\n";
            return;
        }
    }

    std::cerr << "Error: Commit message not found.\n";
}

// Function for Rollback
void rollback(const std::string &target, const std::string &commitGUID = "")
{
    std::string repopath = getCentralRepositoryPath();
    if (repopath.empty())
    {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }

    std::ifstream logFile(repopath + "/commit_log.txt");
    if (!logFile.is_open())
    {
        std::cerr << "Error: Commit log file not found.\n";
        return;
    }

    std::string line, foundVersionPath;
    while (std::getline(logFile, line))
    {
        std::vector<std::string> tokens = split(line, '|');
        if (tokens.size() < 4) continue;

        std::string message = tokens[0];
        std::string timestamp = tokens[1];

        size_t totalTokens = tokens.size();
        size_t fileHashCount = (totalTokens - 2) / 2;  // after message and timestamp
        size_t versionStart = 2 + fileHashCount;

        // If commitGUID is provided, match it exactly
        if (!commitGUID.empty() && message != commitGUID) continue;

        for (size_t i = 2; i < versionStart; i += 2)
        {
            std::string filePath = tokens[i];
            std::string hash = tokens[i + 1];
            std::string filename = fs::path(filePath).filename().string();

            if (filename == fs::path(target).filename().string())
            {
                // Corresponding version path is at same index in versionPaths
                size_t versionIndex = versionStart + (i - 2) / 2;
                if (versionIndex < tokens.size())
                {
                    foundVersionPath = tokens[versionIndex];
                    break;
                }
            }
        }

        if (!foundVersionPath.empty()) break;
    }

    logFile.close();

    if (foundVersionPath.empty())
    {
        std::cerr << "Error: No matching commit or version found for " << target << ".\n";
        return;
    }

    try
    {
        fs::copy(foundVersionPath, target, fs::copy_options::overwrite_existing);
        std::cout << "✅ Rolled back " << target << " to version: " << foundVersionPath << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during rollback: " << e.what() << "\n";
    }
}

// Function to check if the repository has been initialized
bool isRepositoryInitialized(const std::string &projectName)
{
    std::string centralOriginPath = "/usr/bin/codekeeper";    // You can change this path if needed
    std::string keepDirectory = centralOriginPath + "/.keep"; // Path to the .keep directory
    std::string repoName = "." + projectName;
    std::string repositoryPath = keepDirectory + "/" + repoName; // Path to the project folder under .keep

    // Check if the .keep directory exists and the project folder is there
    if (fs::exists(keepDirectory) && fs::exists(repositoryPath))
    {
        std::string projectDetailsFile = repositoryPath + "/projectdetails";
        if (fs::exists(projectDetailsFile))
        {
            return true; // Repository is initialized
        }
    }

    return false; // Repository not initialized
}

// Function to create a new branch
void createBranch(const std::string &branchName)
{
    if (repositoryPath.empty())
    {
       loadRepositoryPath();
        if (repositoryPath.empty())
        {
            return;
        }
    }

    std::string branchesPath = repositoryPath + "/branches";
    if (!fs::exists(branchesPath))
    {
        fs::create_directory(branchesPath);
    }

    std::string branchPath = branchesPath + "/" + branchName;
    if (fs::exists(branchPath))
    {
        std::cerr << "Error: Branch '" << branchName << "' already exists.\n";
        return;
    }

    fs::create_directory(branchPath);
    std::cout << "Branch '" << branchName << "' created successfully.\n";
}

// function to View History
void viewHistory()
{
    loadRepositoryPath();

    if (repositoryPath.empty())
    {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }

    std::ifstream logFile(repositoryPath + "/commit_log.txt");
    if (!logFile.is_open())
    {
        std::cerr << "Error: Commit log file not found.\n";
        return;
    }

    std::string line;
    while (std::getline(logFile, line))
    {

        // Commit format: GUID|Message|Timestamp|File1|File2|...|VersionPath1|VersionPath2|...
        std::vector<std::string> tokens = split(line, '|');
        if (tokens.size() < 4)
            continue; // Skip malformed entries

        std::cout << "Commit GUID: " << tokens[0] << "\n";
        std::cout << "Message: " << tokens[1] << "\n";
        std::cout << "Timestamp: " << tokens[2] << "\n";
        std::cout << "Files:\n";
        for (size_t i = 3; i < tokens.size() - (tokens.size() - 3) / 2; ++i)
        {
            std::cout << "  - " << tokens[i] << "\n";
        }
        std::cout << "------------------------\n";
    }

    logFile.close();
}

// function for Conflict resolution
bool checkConflicts(const std::string &filePath)
{
     loadRepositoryPath();

    if (repositoryPath.empty())
    {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return false;
    }

    std::ifstream logFile(repositoryPath + "/commit_log.txt");
    if (!logFile.is_open())
    {
        std::cerr << "Error: Commit log file not found.\n";
        return false;
    }

    std::string latestVersion;
    std::string line;
    while (std::getline(logFile, line))
    {
        std::vector<std::string> tokens = split(line, '|');
        if (std::find(tokens.begin() + 3, tokens.end(), filePath) != tokens.end())
        {
            size_t versionIndex = 3 + (tokens.size() - 3) / 2;
            for (size_t i = versionIndex; i < tokens.size(); ++i)
            {
                if (fs::path(tokens[i]).filename() == fs::path(filePath).filename())
                {
                    latestVersion = tokens[i];
                    break;
                }
            }
        }
    }

    if (!latestVersion.empty() && fs::exists(filePath))
    {
        // Compare latest version with the current file
        if (!filesAreEqual(filePath, latestVersion))
        {
            std::cerr << "Conflict detected in file: " << filePath << "\n";
            return true;
        }
    }

    return false;
}

void resolveConflict(const std::string &filePath, const std::string &resolutionPath)
{
    fs::copy(resolutionPath, filePath, fs::copy_options::overwrite_existing);
    std::cout << "Conflict resolved for " << filePath << " using " << resolutionPath << "\n";
}


void moveToGitRepo(const std::string &folderPath, const std::string &repoPath)
{
    // Ensure the source folder exists
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath))
    {
        std::cerr << "Error: Source folder does not exist or is not a directory.\n";
        return;
    }

    // Ensure the destination repo folder exists
    if (!fs::exists(repoPath))
    {
        std::cerr << "Error: Git repository does not exist at the specified path.\n";
        return;
    }

    // Create a path for the folder inside the Git repo
    std::string targetPath = repoPath + "/" + fs::path(folderPath).filename().string();

    // Move the folder to the Git repository
    try
    {
        fs::rename(folderPath, targetPath);
        std::cout << "Successfully moved folder to: " << targetPath << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: Failed to move folder. " << e.what() << std::endl;
        return;
    }

    // Change directory to the Git repository
    if (fs::exists(repoPath + "/.git"))
    {
        std::cout << "Git repository found. Proceeding with Git operations...\n";

        // Change the current working directory to the Git repository
        std::string gitCommand = "cd " + repoPath + " && git add . && git commit -m \"Added folder: " + fs::path(folderPath).filename().string() + "\"";
        int result = std::system(gitCommand.c_str());

        if (result == 0)
        {
            std::cout << "Successfully committed the folder to Git.\n";
        }
        else
        {
            std::cerr << "Error: Failed to commit the folder to Git.\n";
        }
    }
    else
    {
        std::cerr << "Error: No Git repository found in the specified path.\n";
    }
}

void archiveVersions()
{
    std::string tmeformat =getTimestamp();

    std::string repopath = getCentralRepositoryPath();
    std::cout << "repository path  " << repopath << "\n";
    if (repopath.empty())
    {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }

    std::string versionsPath = repopath + "/versions";
    if (!fs::exists(versionsPath))
    {
        std::cerr << "Error: No .versions folder found.\n";
        return;
    }

    std::string archivePath = repopath + "/.versions_archive_" + tmeformat + ".zip";

    // Use a system call to zip the .versions folder (requires zip utility installed)
    std::string command = "zip -r " + archivePath + " " + versionsPath;
    if (system(command.c_str()) == 0)
    {
        std::cout << "Archived .versions to " << archivePath << "\n";
    }
    else
    {
        std::cerr << "Error: Failed to archive .versions folder.\n";
    }
}

// merging files
void mergeFiles(const std::string &file1, const std::string &file2, const std::string &outputPath)
{
    std::ifstream input1(file1);
    std::ifstream input2(file2);
    std::ofstream output(outputPath);

    if (!input1.is_open() || !input2.is_open() || !output.is_open())
    {
        std::cerr << "Error: Unable to open one or more files for merging.\n";
        return;
    }

    std::string line1, line2;
    while (std::getline(input1, line1) || std::getline(input2, line2))
    {
        if (!line1.empty() && !line2.empty() && line1 != line2)
        {
            // Conflict: Append both lines with markers
            output << "<<<<<<< " << file1 << "\n"
                   << line1 << "\n=======\n"
                   << line2 << "\n>>>>>>>\n";
        }
        else
        {
            // No conflict: Append whichever line is available
            output << (!line1.empty() ? line1 : line2) << "\n";
        }
        line1.clear();
        line2.clear();
    }

    input1.close();
    input2.close();
    output.close();
    std::cout << "Merge complete. Output written to: " << outputPath << "\n";
}

// merging branches
void mergeBranches(const std::string &branch1, const std::string &branch2)
{
    repositoryPath = getCentralRepositoryPath();

    if (repositoryPath.empty())
    {
        std::cerr << "Error: Central repository not configured.\n";
        return;
    }

    std::string branch1Path = repositoryPath + "/branches/" + branch1;
    std::string branch2Path = repositoryPath + "/branches/" + branch2;

    if (!fs::exists(branch1Path) || !fs::exists(branch2Path))
    {
        std::cerr << "Error: One or both branches do not exist.\n";
        return;
    }

    for (const auto &entry : fs::directory_iterator(branch1Path))
    {
        std::string file1 = entry.path();
        std::string file2 = branch2Path + "/" + fs::path(file1).filename().string();
        if (fs::exists(file2))
        {
            std::string outputFile = branch1Path + "/merged_" + fs::path(file1).filename().string();
            mergeFiles(file1, file2, outputFile);
        }
    }
    std::cout << "Branch merge complete. Resolve conflicts in the merged files if necessary.\n";
}


void convertToGitRepo(const std::string &folderPath)
{
    // Ensure the folder exists
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath))
    {
        std::cerr << "Error: Folder does not exist or is not a directory.\n";
        return;
    }

    // Check if the folder is already a Git repository
    if (fs::exists(folderPath + "/.git"))
    {
        std::cout << "This folder is already a Git repository.\n";
        return;
    }

    // Run `git init` to initialize the folder as a Git repository
    std::string gitCommand = "cd " + folderPath + " && git init";
    int result = std::system(gitCommand.c_str());

    if (result == 0)
    {
        std::cout << "Successfully converted folder to a Git repository.\n";
    }
    else
    {
        std::cerr << "Error: Failed to initialize Git repository.\n";
    }
}

// Function to display help message
void displayHelp()
{
    std::cout << "CodeKeeper Help:\n";
    std::cout << "Available Commands:\n";
    std::cout << "  init                      Initialize a new repository.\n";
    std::cout << "  commit [message] [files]  Commit specified files or directories.\n";
    std::cout << "                            Use '*.*' or '.' to commit all files.\n";
    std::cout << "  rollback [target] [file|guid] Revert a file or repository to a specific version.\n";
    std::cout << "  history                   View commit history.\n";
    std::cout << "  conflicts [file]          Check for conflicts in a file.\n";
    std::cout << "  resolve [file] [res]      Resolve a conflict with the specified resolution file.\n";
    std::cout << "  archive                   Archive the .versions folder.\n";
    std::cout << "  auth                      Authenticate a user.\n";
    std::cout << "  merge [branch1 branch2]   Merge changes from two branches.\n";
    std::cout << "\nAuthentication:\n";
    std::cout << "  Users must authenticate using a valid username and password.\n";
    std::cout << "  Only authenticated users can commit, rollback, or resolve conflicts.\n";
    std::cout << "\nFor more details, consult the documentation.\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        displayHelp();
        return 1;
    }

    std::string command = argv[1];

    if (command == "-h" || command == "--help")
    {
        displayHelp();
    }
    else if (command == "init")
    {
        std::string projectName = (argc == 3) ? argv[2] : "";

        // Check if repository has been initialized before proceeding
        if (isRepositoryInitialized(projectName))
        {
            std::cout << "Repository has already been initialized.\n";
        }
        else
        {
            initRepository(projectName);
        }
    }
    else if (command == "commit")
    {
        if (argc < 4)
        {
            std::cerr << "Error: Please provide commit message and file paths.\n";
            return 1;
        }
        std::string commitMessage = argv[2];
        std::vector<std::string> filePaths(argv + 3, argv + argc);
        commitFiles(filePaths, commitMessage);
    }
    else if (command == "rollack")
    {
        if (argc < 3)
        {
            std::cerr << "Error: Please provide filename or commit-id.\n";
            return 1;
        }
    
        std::string target = argv[2];
        std::string commitId = (argc >= 4) ? argv[3] : "";
        rollback(target, commitId);
    }
    else if (command == "retrieve")
    {
        if (argc < 3)
        {
            std::cerr << "Error: Please provide commit message.\n";
            return 1;
        }
        std::string commitMessage = argv[2];
        retrieveFiles(commitMessage);
    }
    else if (command == "branch")
    {
        if (argc < 3)
        {
            std::cerr << "Error: Please provide branch name.\n";
            return 1;
        }
        std::string branchName = argv[2];
        createBranch(branchName);
    }
    else if (command == "history")
    {

        viewHistory();
    }else if (command == "archive")
    {
        archiveVersions();
    }
    else if (command == "auth")
    {
        std::cout << "Authentication feature is not implemented yet.\n";
    }
    else if (command == "merge")
    {
        if (argc < 4)
        {
            std::cout << "the merge feature require 2 arguements i.e. branch1 and branch2 .\n";
            return 1;
        }
        std::string branch1 = argv[2];
        std::string branch2 = argv[3];
        mergeBranches(branch1, branch2);
      

    }
    else if (command == "conflicts")
    {
        if (argc < 3)
        {
            std::cerr << "Error: Please provide a file to check for conflicts.\n";
            return 1;
        }
        std::string fileName = argv[2];
        bool conflictDetected = checkConflicts(fileName);
        if (conflictDetected)
        {
            std::cout << "Conflict detected in file: " << fileName << "\n";
        }
        else
        {
            std::cout << "No conflicts detected in file: " << fileName << "\n";
        }
    }
    else if (command == "resolve")
    {
        if (argc < 4)
        {
            std::cerr << "Error: Please provide a file and resolution file.\n";
            return 1;
        }
        std::string fileName = argv[2];
        std::string resolutionFile = argv[3];
        resolveConflict(fileName, resolutionFile);
    }
    else if (command == "move")
    {
        if (argc < 4)
        {
            std::cerr << "Error: Please provide folder path and repo path.\n";
            return 1;
        }
        std::string folderPath = argv[2];
        std::string repoPath = argv[3];
        moveToGitRepo(folderPath, repoPath);
    }
    else if (command == "convert")
    {
        if (argc < 3)
        {
            std::cerr << "Error: Please provide folder path.\n";
            return 1;
        }
        std::string folderPath = argv[2];
        convertToGitRepo(folderPath);
    }
    else
    {
        std::cerr << "Error: Unknown command.\n";
        displayHelp();
    }

    return 0;
}
