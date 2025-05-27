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
#include <set>

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
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
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
// Helper function to compute SHA-256 hash of a string
std::string computeStringHash(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.c_str(), input.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &length) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);

    std::ostringstream result;
    for (unsigned int i = 0; i < length; ++i) {
        result << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return result.str();
}
/// Function to compute SHA-256 hash of a file (simple implementation)
std::string computeFileHash(const fs::path& filePath) 
{
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

    std::ofstream bypassFile(repoPath / ".bypass");
    bypassFile << "# Add files or patterns to ignore\n";
    bypassFile.close();

    std::ofstream logFile(repoPath / "commit_log.txt");
    logFile.close();

    // Create .users file with secure permissions
    std::ofstream usersFile(repoPath / ".users");
    usersFile.close();
    chmod((repoPath / ".users").c_str(), 0600);

    std::cout << "Repository '" << repositoryPath << "' initialized successfully.\n";
    std::cout << "Change your terminal to the project folder: cd " << repositoryPath << "\n";
}
// Run a hook script if it exists; return true if success, false if failed
bool runHook(const std::string& hookName) {
    fs::path hookPath = fs::path(repositoryPath) / hookName;
    if (fs::exists(hookPath) && fs::is_regular_file(hookPath)) {
        std::string cmd = "/bin/bash '" + hookPath.string() + "'";
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::cerr << hookName << " hook failed (exit code " << result << ").\n";
            return false;
        }
    }
    return true;
}
// Function to add files to staging area
void addToStaging(const std::vector<std::string>& filePaths) {
    loadRepositoryPath();
    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }
    fs::path stagingDir = fs::path(repositoryPath) / ".staging";
    if (!fs::exists(stagingDir)) {
        fs::create_directory(stagingDir);
    }
    for (const auto& filePath : filePaths) {
        fs::path src = fs::absolute(filePath);
        if (!fs::exists(src)) {
            std::cerr << "Error: File " << src << " does not exist.\n";
            continue;
        }
        fs::path dest = stagingDir / src.filename();
        fs::copy(src, dest, fs::copy_options::overwrite_existing);
        std::cout << "Staged: " << src << "\n";
    }
}

// Function to reset (unstage) files from staging area
void resetStaging(const std::vector<std::string>& filePaths) {
    loadRepositoryPath();
    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }
    fs::path stagingDir = fs::path(repositoryPath) / ".staging";
    for (const auto& filePath : filePaths) {
        fs::path stagedFile = stagingDir / fs::path(filePath).filename();
        if (fs::exists(stagedFile)) {
            fs::remove(stagedFile);
            std::cout << "Unstaged: " << filePath << "\n";
        } else {
            std::cout << "File not staged: " << filePath << "\n";
        }
    }
}

// Helper to get all staged files
std::vector<std::string> getStagedFiles() {
    loadRepositoryPath();
    std::vector<std::string> stagedFiles;
    fs::path stagingDir = fs::path(repositoryPath) / ".staging";
    if (fs::exists(stagingDir)) {
        for (const auto& entry : fs::directory_iterator(stagingDir)) {
            if (fs::is_regular_file(entry)) {
                stagedFiles.push_back(entry.path().string());
            }
        }
    }
    return stagedFiles;
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

    // Run pre-commit hook
    if (!runHook(".pre-commit")) {
        std::cerr << "Commit aborted by pre-commit hook.\n";
        return;
    }

    // If .staging exists and is not empty, use staged files only
    std::vector<std::string> filesToCommit = filePaths;
    fs::path stagingDir = repoPath / ".staging";
    if (fs::exists(stagingDir)) {
        std::vector<std::string> staged = getStagedFiles();
        if (!staged.empty()) {
            filesToCommit = staged;
        }
    }

    std::ifstream bypassFile(repoPath / ".bypass");
    std::vector<std::string> ignoredFiles;
    std::string line;
    while (std::getline(bypassFile, line)) {
        if (!line.empty() && line[0] != '#') {
            ignoredFiles.push_back(line);
        }
    }

    std::vector<std::string> versionPaths;
    std::vector<std::string> fileHashes;
    for (const auto& filePath : filesToCommit) {
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
    // Prepare commit content for hashing
    std::ostringstream commitContent;
    commitContent << commitMessage << "|" << timestamp;
    for (size_t i = 0; i < filesToCommit.size(); ++i) {
        commitContent << "|" << fs::absolute(filesToCommit[i]).string() << "|" << fileHashes[i];
    }
    commitContent << "|";
    for (const auto& versionPath : versionPaths) {
        commitContent << versionPath << "|";
    }
    std::string commitID = computeStringHash(commitContent.str());

    std::ofstream logFile(repoPath / "commit_log.txt", std::ios::app);
    logFile << commitID << "|" << escape(commitMessage) << "|" << timestamp;
    for (size_t i = 0; i < filesToCommit.size(); ++i) {
        logFile << "|" << escape(fs::absolute(filesToCommit[i]).string())
                << "|" << fileHashes[i];
    }
    logFile << "|";
    for (const auto& versionPath : versionPaths) {
        logFile << versionPath << "|";
    }
    logFile << "\n";
    logFile.close();
    std::cout << "Files committed successfully with message: " << commitMessage << "\n";

    // Run post-commit hook
    runHook(".post-commit");

    // Clear staging area after commit
    if (fs::exists(stagingDir)) {
        for (const auto& entry : fs::directory_iterator(stagingDir)) {
            fs::remove(entry);
        }
    }
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
        size_t pos1 = line.find('|');
        size_t pos2 = line.find('|', pos1 + 1);
        std::string message = line.substr(pos1 + 1, pos2 - pos1 - 1);

        if (message == escape(commitMessage)) {
            size_t pos3 = line.find('|', pos2 + 1);
            size_t pos4 = line.find_last_of('|');
            std::string fileData = line.substr(pos3 + 1, pos4 - pos3 - 1);
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
        if (tokens.size() < 5)
            continue;

        if ((!commitGUID.empty() && tokens[0] == commitGUID) ||
            (commitGUID.empty() && std::find(tokens.begin() + 4, tokens.end(), target) != tokens.end()))
        {
            // Found matching commit or file
            size_t versionIndex = 4 + (tokens.size() - 4) / 2;
            for (size_t i = versionIndex; i < tokens.size(); ++i)
            {
                if (fs::path(tokens[i]).filename() == fs::path(target).filename())
                {
                    foundVersionPath = tokens[i];
                    break;
                }
            }
        }
    }

    if (foundVersionPath.empty())
    {
        std::cerr << "Error: No matching commit or version found for " << target << ".\n";
        return;
    }

    fs::copy(foundVersionPath, target, fs::copy_options::overwrite_existing);
    std::cout << "Rolled back " << target << " to version: " << foundVersionPath << "\n";
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

// Function to switch branches
void switchBranch(const std::string& branchName)
{
    loadRepositoryPath();
    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }
    std::string branchesPath = repositoryPath + "/branches";
    std::string branchPath = branchesPath + "/" + branchName;
    if (!fs::exists(branchPath) || !fs::is_directory(branchPath)) {
        std::cerr << "Error: Branch '" << branchName << "' does not exist.\n";
        return;
    }
    // Save current branch
    std::ofstream currentBranchFile(repositoryPath + "/.current_branch");
    currentBranchFile << branchName;
    currentBranchFile.close();
    // Copy all files from branch directory to working directory
    for (const auto& entry : fs::directory_iterator(branchPath)) {
        if (fs::is_regular_file(entry)) {
            fs::path dest = fs::current_path() / entry.path().filename();
            fs::copy(entry.path(), dest, fs::copy_options::overwrite_existing);
        }
    }
    std::cout << "Switched to branch '" << branchName << "'.\n";
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
        if (tokens.size() < 5)
            continue; // Skip malformed entries

        std::cout << "Commit ID: " << tokens[0] << "\n";
        std::cout << "Message: " << tokens[1] << "\n";
        std::cout << "Timestamp: " << tokens[2] << "\n";
        std::cout << "Files:\n";
        for (size_t i = 3; i < tokens.size() - (tokens.size() - 4) / 2; ++i)
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
        if (std::find(tokens.begin() + 4, tokens.end(), filePath) != tokens.end())
        {
            size_t versionIndex = 4 + (tokens.size() - 4) / 2;
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
    loadRepositoryPath();

    if (repositoryPath.empty())
    {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }

    std::string versionsPath = repositoryPath + "/.versions";
    if (!fs::exists(versionsPath))
    {
        std::cerr << "Error: No .versions folder found.\n";
        return;
    }

    std::string archivePath = repositoryPath + "/.versions_archive_" + getTimestamp() + ".zip";

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

// Interactive/manual merge for two files
void interactiveMerge(const std::string& file1, const std::string& file2, const std::string& outputPath) {
    std::ifstream input1(file1);
    std::ifstream input2(file2);
    std::ofstream output(outputPath);
    if (!input1.is_open() || !input2.is_open() || !output.is_open()) {
        std::cerr << "Error: Unable to open one or more files for merging.\n";
        return;
    }
    std::string line1, line2;
    while (true) {
        bool has1 = static_cast<bool>(std::getline(input1, line1));
        bool has2 = static_cast<bool>(std::getline(input2, line2));
        if (!has1 && !has2) break;
        if (has1 && has2 && line1 != line2) {
            std::cout << "Conflict:\n1: " << line1 << "\n2: " << line2 << "\nChoose (1/2/e=edit): ";
            std::string choice;
            std::getline(std::cin, choice);
            if (choice == "1") output << line1 << "\n";
            else if (choice == "2") output << line2 << "\n";
            else {
                std::cout << "Edit> ";
                std::string editLine;
                std::getline(std::cin, editLine);
                output << editLine << "\n";
            }
        } else if (has1) {
            output << line1 << "\n";
        } else if (has2) {
            output << line2 << "\n";
        }
    }
    input1.close();
    input2.close();
    output.close();
    std::cout << "Interactive merge complete. Output written to: " << outputPath << "\n";
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

// User authentication state
std::string currentUser;
bool isAuthenticated = false;

// Hash a password (SHA-256)
std::string hashPassword(const std::string& password) {
    return computeStringHash(password);
}

// Register a new user
bool registerUser(const std::string& username, const std::string& password) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return false;
    std::ofstream usersFile(fs::path(repositoryPath) / ".users", std::ios::app);
    if (!usersFile.is_open()) return false;
    usersFile << username << ":" << hashPassword(password) << "\n";
    usersFile.close();
    return true;
}

// Load session from .session file
void loadSession() {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    std::ifstream sessionFile(fs::path(repositoryPath) / ".session");
    if (sessionFile.is_open()) {
        std::getline(sessionFile, currentUser);
        isAuthenticated = !currentUser.empty();
        sessionFile.close();
    } else {
        currentUser.clear();
        isAuthenticated = false;
    }
}

// Save session to .session file
void saveSession() {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    std::ofstream sessionFile(fs::path(repositoryPath) / ".session");
    if (sessionFile.is_open()) {
        sessionFile << currentUser;
        sessionFile.close();
    }
}

// Remove session file
void clearSession() {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    fs::remove(fs::path(repositoryPath) / ".session");
}

// Authenticate user
bool authenticateUser(const std::string& username, const std::string& password) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return false;
    std::ifstream usersFile(fs::path(repositoryPath) / ".users");
    if (!usersFile.is_open()) return false;
    std::string line, hash = hashPassword(password);
    while (std::getline(usersFile, line)) {
        size_t sep = line.find(":");
        if (sep != std::string::npos) {
            std::string user = line.substr(0, sep);
            std::string pass = line.substr(sep + 1);
            if (user == username && pass == hash) {
                currentUser = username;
                isAuthenticated = true;
                saveSession();
                return true;
            }
        }
    }
    return false;
}

// Logout
void logoutUser() {
    currentUser.clear();
    isAuthenticated = false;
    clearSession();
}

// Require authentication for protected actions
bool requireAuth() {
    loadSession();
    if (!isAuthenticated) {
        std::cerr << "Error: You must authenticate first (run 'codekeeper auth').\n";
        return false;
    }
    return true;
}



// Function to display help message
void displayHelp()
{
    std::cout << "CodeKeeper Help:\n";
    std::cout << "Available Commands:\n";
    std::cout << "  init                      Initialize a new repository.\n";
    std::cout << "  add [files]               Add files to staging area.\n";
    std::cout << "  reset [files]             Remove files from staging area.\n";
    std::cout << "  status                    Show status of working directory and staging area.\n";
    std::cout << "  commit [message] [files]  Commit specified files or directories (if staging is empty).\n";
    std::cout << "                            Use '*.*' or '.' to commit all files.\n";
    std::cout << "  rollback [target] [file|guid] Revert a file or repository to a specific version.\n";
    std::cout << "  history                   View commit history.\n";
    std::cout << "  conflicts [file]          Check for conflicts in a file.\n";
    std::cout << "  resolve [file] [res]      Resolve a conflict with the specified resolution file.\n";
    std::cout << "  archive                   Archive the .versions folder.\n";
    std::cout << "  auth                      Authenticate a user.\n";
    std::cout << "  merge [branch1 branch2]   Merge changes from two branches.\n";
    std::cout << "  switch [branch]           Switch to a different branch.\n";
    std::cout << "  set-remote <path>          Set the remote repository path.\n";
    std::cout << "  push                       Push commits and versions to remote.\n";
    std::cout << "  pull                       Pull commits and versions from remote.\n";
    std::cout << "  list-conflicts              List all files with conflicts.\n";
    std::cout << "  whoami                      Show current authenticated user.\n";
    std::cout << "  list-users                  List all registered users.\n";
    std::cout << "  merge-files <f1> <f2> <out> [--interactive]  Merge two files, optionally interactively.\n";
    std::cout << "\nAuthentication:\n";
    std::cout << "  Users must authenticate using a valid username and password.\n";
    std::cout << "  Only authenticated users can commit, rollback, or resolve conflicts.\n";
    std::cout << "\nFor more details, consult the documentation.\n";
}

// Function to show status of files (staged, modified, untracked)
void status() {
    loadRepositoryPath();
    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }
    fs::path stagingDir = fs::path(repositoryPath) / ".staging";
    std::vector<std::string> stagedFiles = getStagedFiles();
    std::set<std::string> stagedSet;
    for (const auto& f : stagedFiles) stagedSet.insert(fs::path(f).filename().string());

    // Get all files in working directory (excluding hidden and repo files)
    std::vector<std::string> workingFiles;
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (fs::is_regular_file(entry)) {
            std::string fname = entry.path().filename().string();
            if (fname[0] == '.' || fname == "commit_log.txt" || fname == "codekeeper.cpp" || fname == "codekeeper.exe") continue;
            workingFiles.push_back(fname);
        }
    }

    // Get last committed files
    std::vector<std::string> lastCommitted;
    fs::path logPath = fs::path(repositoryPath) / "commit_log.txt";
    if (fs::exists(logPath)) {
        std::ifstream logFile(logPath);
        std::string line;
        while (std::getline(logFile, line)) {
            // Only use last line
        }
        if (!line.empty()) {
            std::vector<std::string> tokens = split(line, '|');
            // Files start at index 3, every other token is a file path
            for (size_t i = 3; i < tokens.size() - (tokens.size() - 4) / 2; i += 2) {
                lastCommitted.push_back(fs::path(tokens[i]).filename().string());
            }
        }
    }
    std::set<std::string> committedSet(lastCommitted.begin(), lastCommitted.end());

    // Print status
    std::cout << "Staged files:\n";
    for (const auto& f : stagedSet) std::cout << "  " << f << "\n";
    std::cout << "\nModified files:\n";
    for (const auto& f : workingFiles) {
        if (committedSet.count(f) && !stagedSet.count(f)) {
            // Compare with last committed version
            // Find version path from log
            std::ifstream logFile(logPath);
            std::string line, versionPath;
            while (std::getline(logFile, line)) {}
            if (!line.empty()) {
                std::vector<std::string> tokens = split(line, '|');
                for (size_t i = 3; i < tokens.size() - (tokens.size() - 4) / 2; i += 2) {
                    if (fs::path(tokens[i]).filename().string() == f) {
                        size_t half = 3 + (tokens.size() - 4) / 2;
                        versionPath = tokens[half + (i - 3)];
                        break;
                    }
                }
            }
            if (!versionPath.empty() && !filesAreEqual(f, versionPath)) {
                std::cout << "  " << f << "\n";
            }
        }
    }
    std::cout << "\nUntracked files:\n";
    for (const auto& f : workingFiles) {
        if (!committedSet.count(f) && !stagedSet.count(f)) {
            std::cout << "  " << f << "\n";
        }
    }
}

// Remote repository path management
std::string getRemotePath() {
    std::ifstream f(fs::path(repositoryPath) / ".remote");
    std::string remote;
    if (f.is_open()) std::getline(f, remote);
    return remote;
}
void setRemotePath(const std::string& remotePath) {
    std::ofstream f(fs::path(repositoryPath) / ".remote");
    f << remotePath;
}

// Push local commits and versions to remote
void pushToRemote() {
    loadRepositoryPath();
    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized.\n";
        return;
    }
    std::string remote = getRemotePath();
    if (remote.empty()) {
        std::cerr << "Error: No remote set. Use 'set-remote <path>'.\n";
        return;
    }
    // Copy commit_log.txt
    fs::copy(fs::path(repositoryPath) / "commit_log.txt", fs::path(remote) / "commit_log.txt", fs::copy_options::overwrite_existing);
    // Copy versions directory
    fs::path localVersions = fs::path(repositoryPath) / "versions";
    fs::path remoteVersions = fs::path(remote) / "versions";
    if (!fs::exists(remoteVersions)) fs::create_directories(remoteVersions);
    for (const auto& entry : fs::directory_iterator(localVersions)) {
        if (fs::is_regular_file(entry)) {
            fs::copy(entry, remoteVersions / entry.path().filename(), fs::copy_options::overwrite_existing);
        }
    }
    std::cout << "Push to remote complete.\n";
}

// Pull commits and versions from remote
void pullFromRemote() {
    loadRepositoryPath();
    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized.\n";
        return;
    }
    std::string remote = getRemotePath();
    if (remote.empty()) {
        std::cerr << "Error: No remote set. Use 'set-remote <path>'.\n";
        return;
    }
    // Copy commit_log.txt
    fs::copy(fs::path(remote) / "commit_log.txt", fs::path(repositoryPath) / "commit_log.txt", fs::copy_options::overwrite_existing);
    // Copy versions directory
    fs::path localVersions = fs::path(repositoryPath) / "versions";
    fs::path remoteVersions = fs::path(remote) / "versions";
    if (!fs::exists(localVersions)) fs::create_directories(localVersions);
    for (const auto& entry : fs::directory_iterator(remoteVersions)) {
        if (fs::is_regular_file(entry)) {
            fs::copy(entry, localVersions / entry.path().filename(), fs::copy_options::overwrite_existing);
        }
    }
    std::cout << "Pull from remote complete.\n";
}

// List all files with conflicts (differs from last committed version)
void listConflicts() {
    loadRepositoryPath();
    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized.\n";
        return;
    }
    fs::path logPath = fs::path(repositoryPath) / "commit_log.txt";
    if (!fs::exists(logPath)) {
        std::cerr << "Error: No commit log found.\n";
        return;
    }
    std::vector<std::string> lastCommitted;
    std::ifstream logFile(logPath);
    std::string line;
    while (std::getline(logFile, line)) {}
    if (!line.empty()) {
        std::vector<std::string> tokens = split(line, '|');
        for (size_t i = 3; i < tokens.size() - (tokens.size() - 4) / 2; i += 2) {
            lastCommitted.push_back(fs::path(tokens[i]).filename().string());
        }
    }
    std::set<std::string> conflicts;
    for (const auto& f : lastCommitted) {
        std::ifstream logFile2(logPath);
        std::string line2, versionPath;
        while (std::getline(logFile2, line2)) {}
        if (!line2.empty()) {
            std::vector<std::string> tokens = split(line2, '|');
            for (size_t i = 3; i < tokens.size() - (tokens.size() - 4) / 2; i += 2) {
                if (fs::path(tokens[i]).filename().string() == f) {
                    size_t half = 3 + (tokens.size() - 4) / 2;
                    versionPath = tokens[half + (i - 3)];
                    break;
                }
            }
        }
        if (!versionPath.empty() && fs::exists(f) && !filesAreEqual(f, versionPath)) {
            conflicts.insert(f);
        }
    }
    if (conflicts.empty()) {
        std::cout << "No conflicts detected.\n";
    } else {
        std::cout << "Conflicting files:\n";
        for (const auto& f : conflicts) std::cout << "  " << f << "\n";
    }
}

// Entry point for CodeKeeper CLI
int main(int argc, char* argv[]) {
    if (argc < 2) {
        displayHelp();
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "init") {
        if (argc < 3) {
            std::cerr << "Usage: codekeeper init <projectName>\n";
            return 1;
        }
        initRepository(argv[2]);
    } else if (cmd == "auth") {
        if (argc < 3) {
            std::cerr << "Usage: codekeeper auth <register|login|logout> [username] [password]\n";
            return 1;
        }
        std::string subcmd = argv[2];
        if (subcmd == "register") {
            if (argc < 5) {
                std::cerr << "Usage: codekeeper auth register <username> <password>\n";
                return 1;
            }
            if (registerUser(argv[3], argv[4])) {
                std::cout << "User registered: " << argv[3] << "\n";
            } else {
                std::cerr << "Error: Could not register user.\n";
            }
        } else if (subcmd == "login") {
            if (argc < 5) {
                std::cerr << "Usage: codekeeper auth login <username> <password>\n";
                return 1;
            }
            if (authenticateUser(argv[3], argv[4])) {
                std::cout << "Authenticated as " << argv[3] << "\n";
            } else {
                std::cerr << "Error: Authentication failed.\n";
            }
        } else if (subcmd == "logout") {
            logoutUser();
            std::cout << "Logged out.\n";
        } else {
            std::cerr << "Unknown auth subcommand.\n";
        }
    } else if (cmd == "commit") {
        if (!requireAuth()) return 1;
        if (argc < 4) {
            std::cerr << "Usage: codekeeper commit <message> <file1> [file2 ...]" << std::endl;
            return 1;
        }
        std::string message = argv[2];
        std::vector<std::string> files;
        for (int i = 3; i < argc; ++i) files.push_back(argv[i]);
        commitFiles(files, message);
    } else if (cmd == "add") {
        if (argc < 3) {
            std::cerr << "Usage: codekeeper add <file1> [file2 ...]" << std::endl;
            return 1;
        }
        std::vector<std::string> files;
        for (int i = 2; i < argc; ++i) files.push_back(argv[i]);
        addToStaging(files);
    } else if (cmd == "reset") {
        if (argc < 3) {
            std::cerr << "Usage: codekeeper reset <file1> [file2 ...]" << std::endl;
            return 1;
        }
        std::vector<std::string> files;
        for (int i = 2; i < argc; ++i) files.push_back(argv[i]);
        resetStaging(files);
    } else if (cmd == "status") {
        status();
    } else if (cmd == "history") {
        viewHistory();
    } else if (cmd == "rollback") {
        if (!requireAuth()) return 1;
        if (argc < 3) {
            std::cerr << "Usage: codekeeper rollback <file|guid> [commitGUID]" << std::endl;
            return 1;
        }
        std::string target = argv[2];
        std::string guid = (argc > 3) ? argv[3] : "";
        rollback(target, guid);
    } else if (cmd == "conflicts") {
        if (argc < 3) {
            std::cerr << "Usage: codekeeper conflicts <file>" << std::endl;
            return 1;
        }
        checkConflicts(argv[2]);
    } else if (cmd == "resolve") {
        if (!requireAuth()) return 1;
        if (argc < 4) {
            std::cerr << "Usage: codekeeper resolve <file> <resolutionFile>" << std::endl;
            return 1;
        }
        resolveConflict(argv[2], argv[3]);
    } else if (cmd == "archive") {
        archiveVersions();
    } else if (cmd == "merge") {
        if (argc < 4) {
            std::cerr << "Usage: codekeeper merge <branch1> <branch2>" << std::endl;
            return 1;
        }
        mergeBranches(argv[2], argv[3]);
    } else if (cmd == "switch") {
        if (argc < 3) {
            std::cerr << "Usage: codekeeper switch <branch>" << std::endl;
            return 1;
        }
        switchBranch(argv[2]);
    } else if (cmd == "set-remote") {
        if (argc < 3) {
            std::cerr << "Usage: codekeeper set-remote <path>" << std::endl;
            return 1;
        }
        setRemotePath(argv[2]);
        std::cout << "Remote set to: " << argv[2] << std::endl;
    } else if (cmd == "push") {
        pushToRemote();
    } else if (cmd == "pull") {
        pullFromRemote();
    } else if (cmd == "list-conflicts") {
        listConflicts();
    } else if (cmd == "whoami") {
        loadSession();
        if (isAuthenticated) std::cout << currentUser << std::endl;
        else std::cout << "Not authenticated." << std::endl;
    } else if (cmd == "list-users") {
        loadRepositoryPath();
        if (repositoryPath.empty()) {
            std::cerr << "Error: Repository not initialized.\n";
            return 1;
        }
        std::ifstream usersFile(fs::path(repositoryPath) / ".users");
        std::string line;
        while (std::getline(usersFile, line)) {
            size_t sep = line.find(":");
            if (sep != std::string::npos) std::cout << line.substr(0, sep) << std::endl;
        }
    } else if (cmd == "merge-files") {
        if (argc < 5) {
            std::cerr << "Usage: codekeeper merge-files <file1> <file2> <output> [--interactive]" << std::endl;
            return 1;
        }
        if (argc > 5 && std::string(argv[5]) == "--interactive") {
            interactiveMerge(argv[2], argv[3], argv[4]);
        } else {
            mergeFiles(argv[2], argv[3], argv[4]);
        }
    } else {
        displayHelp();
    }
    return 0;
}
