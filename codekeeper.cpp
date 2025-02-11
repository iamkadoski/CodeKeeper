#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <regex>
#include <sstream>
#include <random>
#include <chrono>
#include <iomanip>
#include <unistd.h> // For getuid()

namespace fs = std::filesystem;

// Structure to hold metadata for commits
struct Commit
{
    std::string message;
    std::string timestamp;
    std::vector<std::string> filePaths;
    std::vector<std::string> versionPaths;
};

std::string repositoryPath;
std::string projectName;

// Function to generate a timestamp
std::string getTimestamp()
{
    std::time_t now = std::time(nullptr);
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

class TreeNode {
public:
    std::string name;
    bool isDirectory;
    std::vector<TreeNode*> children;

    TreeNode(const std::string& name, bool isDirectory)
        : name(name), isDirectory(isDirectory) {}

    void AddChild(TreeNode* child) {
        children.push_back(child);
    }
};

class FileTree {
public:
    TreeNode* root;

    FileTree(const std::string& rootName) {
        root = new TreeNode(rootName, true);
    }

    void AddFile(const std::string& path) {
        std::vector<std::string> segments = SplitPath(path);
        TreeNode* currentNode = root;

        for (const std::string& segment : segments) {
            TreeNode* childNode = FindChild(currentNode, segment);
            if (childNode == nullptr) {
                bool isDirectory = segment.find('.') == std::string::npos;
                childNode = new TreeNode(segment, isDirectory);
                currentNode->AddChild(childNode);
            }
            currentNode = childNode;
        }
    }

    void PrintTree(TreeNode* node, int depth = 0) const {
        std::string indent(depth * 2, ' ');
        std::cout << indent << (node->isDirectory ? "[DIR] " : "[FILE] ") << node->name << std::endl;
        for (TreeNode* child : node->children) {
            PrintTree(child, depth + 1);
        }
    }

private:
    std::vector<std::string> SplitPath(const std::string& path) const {
        std::vector<std::string> segments;
        size_t start = 0, end = 0;

        while ((end = path.find('/', start)) != std::string::npos) {
            segments.push_back(path.substr(start, end - start));
            start = end + 1;
        }
        segments.push_back(path.substr(start));
        return segments;
    }

    TreeNode* FindChild(TreeNode* node, const std::string& name) const {
        for (TreeNode* child : node->children) {
            if (child->name == name) {
                return child;
            }
        }
        return nullptr;
    }
};




// Function to split a string by delimiter
std::vector<std::string> splitString(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::string generateGUID()
{
    // Get the current timestamp
    auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    // Generate random components
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 15); // For hex characters

    std::stringstream guidStream;
    guidStream << std::hex << std::setw(8) << std::setfill('0') << (timestamp & 0xFFFFFFFF);
    guidStream << "-";
    for (int i = 0; i < 3; ++i)
    {
        guidStream << std::setw(4) << std::setfill('0') << dis(gen);
        guidStream << "-";
    }
    guidStream << std::setw(12) << std::setfill('0') << dis(gen);

    return guidStream.str();
}

// Function to check if the repository has been initialized
bool isRepositoryInitialized( std::string &projectName)
{
    std::string centralOriginPath = "/usr/bin/Codekeeper";    // You can change this path if needed
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



void initRepository(std::string &projectName) {
    // Get the current working directory
    fs::path currentPath = fs::current_path();

    // Extract the directory name
    const std::string directoryName = currentPath.filename().string();

    if (projectName.empty()) {
        projectName = directoryName;
    }

    // Ensure the application has proper permissions
    if (getuid() != 0) { // Check if running as root
        std::cerr << "Error: You must run 'codekeeper init' as root to set up the central repository.\n";
        return;
    }

    std::string centralOriginPath = "/usr/bin/Codekeeper"; // You can change this path if needed

    // Create central origin directory if it doesn't exist
    if (!fs::exists(centralOriginPath)) {
        fs::create_directories(centralOriginPath);
    }

    std::string repoName = "." + projectName;
    std::string keepDirectory = centralOriginPath + "/.keep"; // Create the .keep directory within the central origin path
    std::string repositoryPath = keepDirectory + "/" + repoName; // Place the project folder under .keep

    // Create the .keep directory if it doesn't exist
    if (!fs::exists(keepDirectory)) {
        fs::create_directory(keepDirectory);
    }

    // Create the project details file in the local repo
    std::ofstream repoFile(keepDirectory + "/projectdetails");
    repoFile << repositoryPath;
    repoFile.close();

    // Create the local configuration file in the project directory
    std::ofstream localConfigFile(currentPath.string() + "/.codekeeper_config");
    if (localConfigFile.is_open()) {
        localConfigFile << "central_repository_path=" << repositoryPath << "\n";
        localConfigFile.close();
    } else {
        std::cerr << "Error: Unable to write to local configuration file.\n";
        return;
    }

    // Create the ".versions" folder inside the project folder
    fs::create_directories(repositoryPath + "/.versions");

    // Create the .bypass file inside the hidden repository
    std::ofstream bypassFile(repositoryPath + "/.bypass");
    bypassFile << "# Add files or patterns to ignore\n";
    bypassFile.close();

    // Create the commit_log.txt file
    std::ofstream logFile(repositoryPath + "/commit_log.txt");
    logFile.close();

    std::cout << "Repository '" << repositoryPath << "' initialized successfully.\n";
    std::cout << "Local configuration file created in project directory.\n";
}



std::string loadRepositoryPath()
{
    std::string porojectfilepath = fs::current_path();
    //std::string centralConfigFile = "/usr/bin/Codekeeper/codekeeper_config";
    std::string centralConfigFile = porojectfilepath + "/.codekeeper_config";
    std::ifstream configFile(centralConfigFile);

    if (!configFile.is_open())
    {
        std::cerr << "Error: Central configuration file not found. Run 'codekeeper init'.\n";
        return "";
    }

    std::string line, repositoryPath;
    while (std::getline(configFile, line))
    {
        if (line.find("central_repository_path=") == 0)
        {
            repositoryPath = line.substr(line.find('=') + 1);
            break;
        }
    }

    if (repositoryPath.empty())
    {
        std::cerr << "Error: Central repository path not set in the configuration file.\n";
    }

    return repositoryPath;
}

std::string getCentralRepositoryPath()
{
    std::ifstream configFile(".codekeeper_config");
    std::string repositoryPath;
    if (configFile.is_open())
    {
        std::getline(configFile, repositoryPath);
    }
    return repositoryPath;
}

void setCentralRepositoryPath(const std::string &path)
{
    std::ofstream configFile(".codekeeper_config");
    configFile << path;
}

// Function to retrieve files by commit message
void retrieveFiles(const std::string &commitMessage)
{
    if (repositoryPath.empty() || !fs::exists(repositoryPath + "/commit_log.txt"))
    {
        std::cerr << "Error: Repository not initialized or log file missing.\n";
        return;
    }

    std::ifstream logFile(repositoryPath + "/commit_log.txt");
    if (!logFile)
    {
        std::cerr << "Error: Commit log file not found.\n";
        return;
    }

    std::string line;
    while (std::getline(logFile, line))
    {
        size_t pos = line.find('|');
        std::string message = line.substr(0, pos);

        if (message == commitMessage)
        {
            size_t pos2 = line.find('|', pos + 1);
            size_t pos3 = line.find_last_of('|');
            std::string fileData = line.substr(pos2 + 1, pos3 - pos2 - 1);
            std::vector<std::string> tokens = splitString(fileData, '|');
            size_t half = tokens.size() / 2;

            for (size_t i = 0; i < half; ++i)
            {
                fs::copy(tokens[half + i], fs::path(tokens[i]).filename(),
                         fs::copy_options::overwrite_existing);
            }

            std::cout << "Files retrieved successfully.\n";
            logFile.close();
            return;
        }
    }

    std::cerr << "Error: Commit message not found.\n";
    logFile.close();
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

bool filesAreEqual(const std::string &filePath1, const std::string &filePath2)
{
    std::ifstream file1(filePath1, std::ios::binary);
    std::ifstream file2(filePath2, std::ios::binary);
    return std::equal(std::istreambuf_iterator<char>(file1), std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(file2));
}

// function to View History
void viewHistory()
{
    repositoryPath = loadRepositoryPath();

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
    repositoryPath = loadRepositoryPath();

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

// function foir archiving

void archiveVersions()
{
    repositoryPath = loadRepositoryPath();

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

// Function for Rollback
void rollback(const std::string &target, const std::string &commitGUID = "")
{
    repositoryPath = loadRepositoryPath();

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

    std::string line, foundVersionPath;
    while (std::getline(logFile, line))
    {
        std::vector<std::string> tokens = split(line, '|');
        if (tokens.size() < 4)
            continue;

        if ((!commitGUID.empty() && tokens[0] == commitGUID) ||
            (commitGUID.empty() && std::find(tokens.begin() + 3, tokens.end(), target) != tokens.end()))
        {
            // Found matching commit or file
            size_t versionIndex = 3 + (tokens.size() - 3) / 2;
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

// Function to create a new branch
void createBranch(const std::string &branchName)
{
    if (repositoryPath.empty())
    {
        repositoryPath = loadRepositoryPath();
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

// Function to collect all files from a directory
void collectFilesFromDirectory(const std::string &dirPath, std::vector<std::string> &files, const std::vector<std::string> &ignoredFiles)
{
    for (const auto &entry : std::filesystem::recursive_directory_iterator(dirPath))
    {
        if (entry.is_regular_file())
        {
            std::string filePath = entry.path().string();
            if (std::find(ignoredFiles.begin(), ignoredFiles.end(), filePath) == ignoredFiles.end())
            {
                files.push_back(filePath);
            }
            else
            {
                std::cout << "Skipping ignored file: " << filePath << "\n";
            }
        }
    }
}

// Function to expand wildcards for files and directories
void expandWildcard(const std::string &pattern, std::vector<std::string> &files)
{
    std::regex re(pattern);
    for (const auto &entry : std::filesystem::directory_iterator("."))
    {
        if (std::filesystem::is_regular_file(entry) || std::filesystem::is_directory(entry))
        {
            std::string entryPath = entry.path().string();
            if (std::regex_match(entryPath, re))
            {
                files.push_back(entryPath);
            }
        }
    }
}



//commit includes file tree
void commitFiles(const std::vector<std::string>& filePaths, const std::string& commitMessage) {
    repositoryPath = loadRepositoryPath();

    if (repositoryPath.empty()) {
        std::cerr << "Error: Repository not initialized. Run 'codekeeper init'.\n";
        return;
    }

    FileTree fileTree("Root");
    std::vector<std::string> allFiles;

    for (const auto& filePath : filePaths) {
        if (!fs::exists(filePath)) {
            std::cerr << "Error: File or directory " << filePath << " does not exist.\n";
            continue;
        }

        if (fs::is_regular_file(filePath)) {
            allFiles.push_back(filePath);
        } else if (fs::is_directory(filePath)) {
            collectFilesFromDirectory(filePath, allFiles, {});
        } else {
            std::cerr << "Error: Unsupported file type for " << filePath << ".\n";
        }
    }

    std::string commitID = generateGUID();
    std::vector<std::string> versionPaths;

    for (const auto& filePath : allFiles) {
        fileTree.AddFile(filePath);
        std::string versionFile = repositoryPath + "/.versions/version_" + commitID + "_" +
                                  std::to_string(std::time(nullptr)) + "_" + fs::path(filePath).filename().string();
        fs::copy(filePath, versionFile, fs::copy_options::overwrite_existing);
        versionPaths.push_back(versionFile);
    }

    std::string timestamp = getTimestamp();
    std::ofstream logFile(repositoryPath + "/commit_log.txt", std::ios::app);

    if (!logFile) {
        std::cerr << "Error: Could not open commit log file.\n";
        return;
    }

    logFile << commitMessage << "|" << commitID << "|" << timestamp;
    for (const auto& filePath : allFiles) {
        logFile << "|" << filePath;
    }
    logFile << "|";
    for (const auto& versionPath : versionPaths) {
        logFile << versionPath << "|";
    }
    logFile << "\n";
    logFile.close();

    std::cout << "Files committed successfully with message: " << commitMessage << "\n";
    fileTree.PrintTree(fileTree.root);
}

// Function to display help message
void displayHelp()
{
    std::cout << "CodeKeeper Help:\n";
    std::cout << "Available Commands:\n";
    std::cout << "  init                 Initialize a new repository.\n";
    std::cout << "  commit <message> [files] Commit specified files or directories.\n";
    std::cout << "                       Use '*.*' or '.' to commit all files.\n";
    std::cout << "  rollback [file|guid] Revert a file or repository to a specific version.\n";
    std::cout << "  history              View commit history.\n";
    std::cout << "  conflicts [file]     Check for conflicts in a file.\n";
    std::cout << "  convert [directorypath]     Convert directory to a git repo.\n";
    std::cout << "  resolve [file] [res] Resolve a conflict with the specified resolution file.\n";
    std::cout << "  archive              Archive the .versions folder.\n";
    std::cout << "  auth                 Authenticate a user.\n";
    std::cout << "  merge [branch1 branch2] Merge changes from two branches.\n";
    std::cout << "\nAuthentication:\n";
    std::cout << "  Users must authenticate using a valid username and password.\n";
    std::cout << "  Only authenticated users can commit, rollback, or resolve conflicts.\n";
    std::cout << "\nFor more details, consult the documentation.\n";
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
        if (argv[3] == "")
        {
            std::cerr << "Error: Please provide commit-id or filename.\n";
            return 1;
        }
        std::string filename = argv[3];
        rollback(filename);
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
    else if (command == "Convert")
    {
        if (argc < 3)
        {
            std::cerr << "Error: Please provide branch name.\n";
            return 1;
        }
        std::string dirpath = argv[2];
        convertToGitRepo(dirpath);
    }
    else if (command == "history")
    {

        viewHistory();
    }
     else if (command == "archive")
    {

       archiveVersions();
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
