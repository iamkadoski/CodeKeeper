#include "httplib.h"
#include "json.hpp"

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
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ====== CodeKeeper Core (adapted from codekeeper.cpp) ======

std::string repositoryPath;
std::string currentUser;
bool isAuthenticated = false;

std::string getTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

std::vector<std::string> split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) tokens.push_back(token);
    return tokens;
}

bool filesAreEqual(const std::string &fp1, const std::string &fp2) {
    std::ifstream f1(fp1, std::ios::binary);
    std::ifstream f2(fp2, std::ios::binary);
    return std::equal(std::istreambuf_iterator<char>(f1), std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(f2));
}

std::string escape(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c == '|') output += "\\|";
        else output += c;
    }
    return output;
}

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
    for (unsigned int i = 0; i < length; ++i)
        result << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    return result.str();
}

std::string computeFileHash(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return "";
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) return "";
    const EVP_MD* md = EVP_sha256();
    if (1 != EVP_DigestInit_ex(mdctx, md, nullptr)) { EVP_MD_CTX_free(mdctx); return ""; }
    char buffer[8192];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        if (1 != EVP_DigestUpdate(mdctx, buffer, file.gcount())) { EVP_MD_CTX_free(mdctx); return ""; }
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    if (1 != EVP_DigestFinal_ex(mdctx, hash, &hashLen)) { EVP_MD_CTX_free(mdctx); return ""; }
    EVP_MD_CTX_free(mdctx);
    std::ostringstream result;
    for (unsigned int i = 0; i < hashLen; ++i)
        result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return result.str();
}

void loadRepositoryPath() {
    std::ifstream repoFile(".repo_path");
    if (repoFile.is_open()) {
        std::getline(repoFile, repositoryPath);
        repoFile.close();
    } else {
        repositoryPath.clear();
    }
}

bool isPathSafe(const std::string& filePath) {
    try {
        fs::path resolved = fs::weakly_canonical(fs::absolute(filePath));
        fs::path repo = fs::weakly_canonical(fs::absolute(repositoryPath));
        auto rel = fs::relative(resolved, repo);
        return !rel.empty() && rel.string().find("..") == std::string::npos;
    } catch (...) {
        return false;
    }
}

std::string hashPassword(const std::string& username, const std::string& password) {
    return computeStringHash(username + ":" + password);
}

bool registerUser(const std::string& username, const std::string& password) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return false;
    std::ofstream usersFile(fs::path(repositoryPath) / ".users", std::ios::app);
    if (!usersFile.is_open()) return false;
    usersFile << username << ":" << hashPassword(username, password) << "\n";
    usersFile.close();
    return true;
}

bool authenticateUser(const std::string& username, const std::string& password) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return false;
    std::ifstream usersFile(fs::path(repositoryPath) / ".users");
    if (!usersFile.is_open()) return false;
    std::string line, hash = hashPassword(username, password);
    while (std::getline(usersFile, line)) {
        size_t sep = line.find(":");
        if (sep != std::string::npos) {
            std::string user = line.substr(0, sep);
            std::string pass = line.substr(sep + 1);
            if (user == username && pass == hash) {
                currentUser = username;
                isAuthenticated = true;
                return true;
            }
        }
    }
    return false;
}

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

void saveSession() {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    std::ofstream sessionFile(fs::path(repositoryPath) / ".session");
    if (sessionFile.is_open()) {
        sessionFile << currentUser;
        sessionFile.close();
    }
}

void clearSession() {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    fs::remove(fs::path(repositoryPath) / ".session");
}

void logoutUser() {
    currentUser.clear();
    isAuthenticated = false;
    clearSession();
}

bool runHook(const std::string& hookName) {
    fs::path hookPath = fs::path(repositoryPath) / hookName;
    if (fs::exists(hookPath) && fs::is_regular_file(hookPath)) {
        pid_t pid = fork();
        if (pid == 0) {
            execlp("/bin/bash", "bash", hookPath.c_str(), nullptr);
            _exit(127);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return true;
            return false;
        }
    }
    return true;
}

void initRepository(const std::string& projectName, bool local = false) {
    std::regex safeName("^[a-zA-Z0-9_-]+$");
    if (!std::regex_match(projectName, safeName))
        throw std::runtime_error("Invalid project name: only letters, digits, _ and - are allowed.");
    fs::path repoPath;
    if (local) {
        repoPath = fs::weakly_canonical(fs::current_path());
        if (fs::exists(repoPath / ".repo_path")) {
            repositoryPath = repoPath.string();
            return;
        }
    } else {
        fs::path baseDir = "/var/lib/CodeKeeper";
        if (!fs::exists(baseDir)) fs::create_directories(baseDir);
        repoPath = fs::weakly_canonical(baseDir / fs::path(projectName).filename());
        if (!fs::exists(repoPath)) fs::create_directory(repoPath);
    }
    repositoryPath = repoPath.string();
    std::ofstream repoFile(".repo_path");
    repoFile << repositoryPath; repoFile.close();
    fs::create_directory(repoPath / "versions");
    std::ofstream bypassFile(repoPath / ".bypass");
    bypassFile << "# Add files or patterns to ignore\n"; bypassFile.close();
    std::ofstream logFile(repoPath / "commit_log.txt"); logFile.close();
    std::ofstream usersFile(repoPath / ".users"); usersFile.close();
    chmod((repoPath / ".users").c_str(), 0600);
}

// Recursively collect regular files from a path (skips hidden, repo, and binary files)
std::vector<std::string> collectFiles(const std::string& path) {
    std::vector<std::string> files;
    fs::path p = fs::absolute(path);
    if (!fs::exists(p)) return files;
    if (fs::is_regular_file(p)) {
        files.push_back(p.string());
    } else if (fs::is_directory(p)) {
        for (const auto& entry : fs::recursive_directory_iterator(p)) {
            if (fs::is_regular_file(entry)) {
                std::string fname = entry.path().filename().string();
                if (fname[0] == '.' || fname == "commit_log.txt" || fname == "codekeeper" || fname == "codekeeper.exe") continue;
                auto rel = fs::relative(entry.path(), p);
                bool inInternal = false;
                for (const auto& part : rel) {
                    std::string s = part.string();
                    if (s == ".staging" || s == "versions" || s == "branches" || s[0] == '.') { inInternal = true; break; }
                }
                if (!inInternal) files.push_back(fs::absolute(entry.path()).string());
            }
        }
    }
    return files;
}

void addToStaging(const std::vector<std::string>& filePaths) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    fs::path stagingDir = fs::path(repositoryPath) / ".staging";
    if (!fs::exists(stagingDir)) fs::create_directory(stagingDir);
    for (const auto& filePath : filePaths) {
        auto expanded = collectFiles(filePath);
        for (const auto& src : expanded) {
            if (!isPathSafe(src)) continue;
            fs::path dest = stagingDir / fs::path(src).filename();
            fs::copy(src, dest, fs::copy_options::overwrite_existing);
        }
    }
}

void resetStaging(const std::vector<std::string>& filePaths) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    fs::path stagingDir = fs::path(repositoryPath) / ".staging";
    for (const auto& filePath : filePaths) {
        fs::path stagedFile = stagingDir / fs::path(filePath).filename();
        if (fs::exists(stagedFile)) fs::remove(stagedFile);
    }
}

std::vector<std::string> getStagedFiles() {
    loadRepositoryPath();
    std::vector<std::string> stagedFiles;
    fs::path stagingDir = fs::path(repositoryPath) / ".staging";
    if (fs::exists(stagingDir)) {
        for (const auto& entry : fs::directory_iterator(stagingDir)) {
            if (fs::is_regular_file(entry)) stagedFiles.push_back(entry.path().filename().string());
        }
    }
    return stagedFiles;
}

void commitFiles(const std::vector<std::string>& filePaths, const std::string& commitMessage) {
    loadRepositoryPath();
    fs::path repoPath = fs::weakly_canonical(repositoryPath);
    fs::path versionDir = repoPath / "versions";
    if (repositoryPath.empty() || !fs::exists(versionDir)) return;
    if (!runHook(".pre-commit")) return;
    std::vector<std::string> filesToCommit = filePaths;
    fs::path stagingDir = repoPath / ".staging";
    if (fs::exists(stagingDir)) {
        std::vector<std::string> staged = getStagedFiles();
        if (!staged.empty() && filesToCommit.empty()) {
            filesToCommit.clear();
            for (auto& s : staged) filesToCommit.push_back(fs::absolute(repoPath / ".staging" / s).string());
        }
    }
    std::ifstream bypassFile(repoPath / ".bypass");
    std::vector<std::string> ignoredFiles;
    std::string line;
    while (std::getline(bypassFile, line)) {
        if (!line.empty() && line[0] != '#') ignoredFiles.push_back(line);
    }
    std::vector<std::string> versionPaths;
    std::vector<std::string> fileHashes;
    // Expand directories to individual files
    std::vector<std::string> expandedFiles;
    for (const auto& fp : filesToCommit) {
        auto collected = collectFiles(fp);
        expandedFiles.insert(expandedFiles.end(), collected.begin(), collected.end());
    }
    for (const auto& filePath : expandedFiles) {
        if (!isPathSafe(filePath)) continue;
        fs::path file = fs::absolute(filePath);
        if (!fs::exists(file)) continue;
        if (std::find(ignoredFiles.begin(), ignoredFiles.end(), filePath) != ignoredFiles.end()) continue;
        std::string versionFile = "version_" + std::to_string(std::time(nullptr)) + "_" + file.filename().string();
        fs::path versionFilePath = versionDir / versionFile;
        fs::copy(file, versionFilePath, fs::copy_options::overwrite_existing);
        versionPaths.push_back(versionFilePath.string());
        fileHashes.push_back(computeFileHash(file));
    }
    std::string timestamp = getTimestamp();

    // Use expanded file list for commit, not original paths
    filesToCommit = expandedFiles;
    if (filesToCommit.empty()) return;

    std::string parentHash;
    {
        std::ifstream lf(repoPath / "commit_log.txt");
        std::string lastLine;
        while (std::getline(lf, lastLine)) {}
        if (!lastLine.empty()) parentHash = split(lastLine, '|')[0];
    }
    std::ostringstream commitContent;
    commitContent << commitMessage << "|" << timestamp;
    if (!parentHash.empty()) commitContent << "|parent=" << parentHash;
    for (size_t i = 0; i < filesToCommit.size(); ++i)
        commitContent << "|" << fs::absolute(filesToCommit[i]).string() << "|" << fileHashes[i];
    commitContent << "|";
    for (const auto& vp : versionPaths) commitContent << vp << "|";
    std::string commitID = computeStringHash(commitContent.str());
    std::ofstream logFile(repoPath / "commit_log.txt", std::ios::app);
    logFile << commitID << "|" << escape(commitMessage) << "|" << timestamp;
    for (size_t i = 0; i < filesToCommit.size(); ++i)
        logFile << "|" << escape(fs::absolute(filesToCommit[i]).string()) << "|" << fileHashes[i];
    logFile << "|";
    for (const auto& vp : versionPaths) logFile << vp << "|";
    logFile << "\n";
    logFile.close();
    runHook(".post-commit");
    if (fs::exists(stagingDir)) {
        for (const auto& entry : fs::directory_iterator(stagingDir)) fs::remove(entry);
    }
}

void rollback(const std::string &target, const std::string &commitGUID = "") {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    if (!isPathSafe(target)) return;
    std::ifstream logFile(repositoryPath + "/commit_log.txt");
    if (!logFile.is_open()) return;
    std::string line, foundVersionPath;
    while (std::getline(logFile, line)) {
        std::vector<std::string> tokens = split(line, '|');
        if (tokens.size() < 5) continue;
        if ((!commitGUID.empty() && tokens[0] == commitGUID) ||
            (commitGUID.empty() && std::find(tokens.begin() + 4, tokens.end(), target) != tokens.end())) {
            size_t versionIndex = 4 + (tokens.size() - 4) / 2;
            for (size_t i = versionIndex; i < tokens.size(); ++i) {
                if (fs::path(tokens[i]).filename() == fs::path(target).filename()) {
                    foundVersionPath = tokens[i]; break;
                }
            }
        }
    }
    if (foundVersionPath.empty()) return;
    fs::copy(foundVersionPath, target, fs::copy_options::overwrite_existing);
}

void createBranch(const std::string &branchName) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    std::string branchesPath = repositoryPath + "/branches";
    if (!fs::exists(branchesPath)) fs::create_directory(branchesPath);
    std::string branchPath = branchesPath + "/" + branchName;
    if (fs::exists(branchPath)) return;
    fs::create_directory(branchPath);
}

void switchBranch(const std::string& branchName) {
    loadRepositoryPath();
    if (repositoryPath.empty()) return;
    std::string branchesPath = repositoryPath + "/branches";
    std::string branchPath = branchesPath + "/" + branchName;
    if (!fs::exists(branchPath) || !fs::is_directory(branchPath)) return;
    std::ofstream currentBranchFile(repositoryPath + "/.current_branch");
    currentBranchFile << branchName; currentBranchFile.close();
    for (const auto& entry : fs::directory_iterator(branchPath)) {
        if (fs::is_regular_file(entry))
            fs::copy(entry.path(), fs::current_path() / entry.path().filename(), fs::copy_options::overwrite_existing);
    }
}

json getHistory() {
    loadRepositoryPath();
    json result = json::array();
    if (repositoryPath.empty()) return result;
    std::ifstream logFile(repositoryPath + "/commit_log.txt");
    if (!logFile.is_open()) return result;
    std::string line;
    while (std::getline(logFile, line)) {
        std::vector<std::string> tokens = split(line, '|');
        if (tokens.size() < 5) continue;
        json commit;
        commit["id"] = tokens[0];
        commit["message"] = tokens[1];
        commit["timestamp"] = tokens[2];
        json files = json::array();
        size_t half = (tokens.size() - 4) / 2;
        for (size_t i = 3; i < 3 + half; ++i) files.push_back(tokens[i]);
        commit["files"] = files;
        result.push_back(commit);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<std::string> getBranches() {
    loadRepositoryPath();
    std::vector<std::string> branches;
    if (repositoryPath.empty()) return branches;
    std::string branchesPath = repositoryPath + "/branches";
    if (fs::exists(branchesPath)) {
        for (const auto& entry : fs::directory_iterator(branchesPath))
            if (fs::is_directory(entry)) branches.push_back(entry.path().filename().string());
    }
    std::sort(branches.begin(), branches.end());
    return branches;
}

std::string getCurrentBranch() {
    loadRepositoryPath();
    if (repositoryPath.empty()) return "main";
    std::ifstream f(repositoryPath + "/.current_branch");
    std::string branch;
    if (f.is_open()) std::getline(f, branch);
    return branch.empty() ? "main" : branch;
}

std::vector<std::string> listConflicts() {
    loadRepositoryPath();
    std::vector<std::string> conflicts;
    if (repositoryPath.empty()) return conflicts;
    fs::path logPath = fs::path(repositoryPath) / "commit_log.txt";
    if (!fs::exists(logPath)) return conflicts;
    std::vector<std::string> lastCommitted;
    std::ifstream logFile(logPath);
    std::string line;
    while (std::getline(logFile, line)) {}
    if (!line.empty()) {
        std::vector<std::string> tokens = split(line, '|');
        for (size_t i = 3; i < tokens.size() - (tokens.size() - 4) / 2; i += 2)
            lastCommitted.push_back(fs::path(tokens[i]).filename().string());
    }
    for (const auto& f : lastCommitted) {
        std::ifstream lf2(logPath);
        std::string l2, versionPath;
        while (std::getline(lf2, l2)) {}
        if (!l2.empty()) {
            std::vector<std::string> tokens = split(l2, '|');
            for (size_t i = 3; i < tokens.size() - (tokens.size() - 4) / 2; i += 2) {
                if (fs::path(tokens[i]).filename().string() == f) {
                    size_t half = 3 + (tokens.size() - 4) / 2;
                    versionPath = tokens[half + (i - 3)];
                    break;
                }
            }
        }
        if (!versionPath.empty() && fs::exists(f) && !filesAreEqual(f, versionPath))
            conflicts.push_back(f);
    }
    return conflicts;
}

// Web server
std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Directory where the HTML file lives
std::string webDir;

int main(int argc, char* argv[]) {
    // Determine web directory: check relative to executable (project root), CWD, or installed path
    fs::path exePath = fs::absolute(argv[0]);
    // Priority 1: sibling of the parent directory (e.g. build/../web/)
    webDir = (exePath.parent_path().parent_path() / "web").string();
    if (!fs::exists(webDir + "/index.html")) {
        // Priority 2: current working directory
        webDir = fs::current_path().string() + "/web";
    }
    if (!fs::exists(webDir + "/index.html")) {
        // Priority 3: same directory as executable (e.g. build/web/)
        webDir = (exePath.parent_path() / "web").string();
    }
    if (!fs::exists(webDir + "/index.html")) {
        // Priority 4: installed system path
        webDir = "/usr/share/codekeeper/web";
    }
    if (!fs::exists(webDir + "/index.html")) {
        std::cerr << "Warning: Could not find web/index.html" << std::endl;
    }

    int port = 9898;
    std::string workDir;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            workDir = argv[++i];
        } else if (arg == "--web" && i + 1 < argc) {
            webDir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: codekeeper-web [port] [options]" << std::endl;
            std::cout << "  port         HTTP port (default: 8080)" << std::endl;
            std::cout << "  --dir <path> Working directory where .repo_path lives" << std::endl;
            std::cout << "  --web <path> Path to web/ directory with index.html" << std::endl;
            return 0;
        } else {
            try { port = std::stoi(arg); } catch (...) {}
        }
    }

    // Change to working directory if specified
    if (!workDir.empty()) {
        try { fs::current_path(workDir); } catch (...) {
            std::cerr << "Error: Could not change to directory: " << workDir << std::endl;
            return 1;
        }
    }

    std::cout << "CodeKeeper Web Server" << std::endl;
    std::cout << "Web root: " << webDir << std::endl;
    std::cout << "Listening on http://localhost:" << port << std::endl;

    httplib::Server svr;

    // CORS
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    // Serve static files
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        std::string content = readFile(webDir + "/index.html");
        if (content.empty()) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
            return;
        }
        res.set_content(content, "text/html");
    });

    // API: Whoami
    svr.Get("/api/whoami", [](const httplib::Request& req, httplib::Response& res) {
        loadSession();
        json j;
        if (isAuthenticated) {
            j["user"] = currentUser;
        } else {
            j["user"] = nullptr;
        }
        res.set_content(j.dump(), "application/json");
    });

    // API: Auth login
    svr.Post("/api/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string user = j.value("username", "");
            std::string pass = j.value("password", "");
            if (authenticateUser(user, pass)) {
                saveSession();
                json r = {{"ok", true}};
                res.set_content(r.dump(), "application/json");
            } else {
                json r = {{"ok", false}, {"error", "Invalid credentials"}};
                res.status = 401;
                res.set_content(r.dump(), "application/json");
            }
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: Auth register
    svr.Post("/api/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string user = j.value("username", "");
            std::string pass = j.value("password", "");
            if (user.empty() || pass.empty()) {
                json r = {{"ok", false}, {"error", "Username and password required"}};
                res.status = 400;
                res.set_content(r.dump(), "application/json");
                return;
            }
            if (registerUser(user, pass)) {
                json r = {{"ok", true}};
                res.set_content(r.dump(), "application/json");
            } else {
                json r = {{"ok", false}, {"error", "Could not register (repo not initialized?)"}};
                res.status = 500;
                res.set_content(r.dump(), "application/json");
            }
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: Auth logout
    svr.Post("/api/auth/logout", [](const httplib::Request& req, httplib::Response& res) {
        logoutUser();
        json r = {{"ok", true}};
        res.set_content(r.dump(), "application/json");
    });

    // API: Init repo
    svr.Post("/api/init", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string name = j.value("name", "");
            if (name.empty()) {
                json r = {{"ok", false}, {"error", "Project name required"}};
                res.status = 400;
                res.set_content(r.dump(), "application/json");
                return;
            }
            initRepository(name, j.value("local", false));
            json r = {{"ok", true}};
            res.set_content(r.dump(), "application/json");
        } catch (std::exception& e) {
            json r = {{"ok", false}, {"error", e.what()}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: Status
    svr.Get("/api/status", [](const httplib::Request& req, httplib::Response& res) {
        loadRepositoryPath();
        if (repositoryPath.empty()) {
            json r = {{"error", "Repository not initialized. Run 'init' first."}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
            return;
        }
        json r;
        r["staged"] = getStagedFiles();
        // Get current branch
        r["branch"] = getCurrentBranch();
        // Get staged file details
        json stagedInfo = json::array();
        for (const auto& f : getStagedFiles()) {
            stagedInfo.push_back(f);
        }
        res.set_content(r.dump(), "application/json");
    });

    // API: Add
    svr.Post("/api/add", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::vector<std::string> files = j.value("files", std::vector<std::string>());
            if (files.empty()) {
                json r = {{"ok", false}, {"error", "No files specified"}};
                res.status = 400;
                res.set_content(r.dump(), "application/json");
                return;
            }
            addToStaging(files);
            json r = {{"ok", true}};
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: Reset
    svr.Post("/api/reset", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::vector<std::string> files = j.value("files", std::vector<std::string>());
            resetStaging(files);
            json r = {{"ok", true}};
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: Commit
    svr.Post("/api/commit", [](const httplib::Request& req, httplib::Response& res) {
        loadSession();
        if (!isAuthenticated) {
            json r = {{"ok", false}, {"error", "Authentication required"}};
            res.status = 401;
            res.set_content(r.dump(), "application/json");
            return;
        }
        try {
            auto j = json::parse(req.body);
            std::string message = j.value("message", "");
            std::vector<std::string> files = j.value("files", std::vector<std::string>());
            if (message.empty()) {
                json r = {{"ok", false}, {"error", "Commit message required"}};
                res.status = 400;
                res.set_content(r.dump(), "application/json");
                return;
            }
            commitFiles(files, message);
            json r = {{"ok", true}};
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: History
    svr.Get("/api/history", [](const httplib::Request& req, httplib::Response& res) {
        loadRepositoryPath();
        if (repositoryPath.empty()) {
            json r = {{"error", "Repository not initialized"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
            return;
        }
        json j;
        j["commits"] = getHistory();
        res.set_content(j.dump(), "application/json");
    });

    // API: Rollback
    svr.Post("/api/rollback", [](const httplib::Request& req, httplib::Response& res) {
        loadSession();
        if (!isAuthenticated) {
            json r = {{"ok", false}, {"error", "Authentication required"}};
            res.status = 401;
            res.set_content(r.dump(), "application/json");
            return;
        }
        try {
            auto j = json::parse(req.body);
            std::string target = j.value("target", "");
            std::string guid = j.value("guid", "");
            if (target.empty()) {
                json r = {{"ok", false}, {"error", "Target required"}};
                res.status = 400;
                res.set_content(r.dump(), "application/json");
                return;
            }
            rollback(target, guid);
            json r = {{"ok", true}};
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: Branches
    svr.Get("/api/branches", [](const httplib::Request& req, httplib::Response& res) {
        loadRepositoryPath();
        json j;
        if (!repositoryPath.empty()) {
            j["branches"] = getBranches();
            j["current"] = getCurrentBranch();
        } else {
            j["branches"] = json::array();
            j["current"] = nullptr;
        }
        res.set_content(j.dump(), "application/json");
    });

    // API: Create branch
    svr.Post("/api/branch", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string name = j.value("name", "");
            if (name.empty()) {
                json r = {{"ok", false}, {"error", "Branch name required"}};
                res.status = 400;
                res.set_content(r.dump(), "application/json"); return;
            }
            createBranch(name);
            json r = {{"ok", true}};
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: Switch branch
    svr.Post("/api/switch", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string name = j.value("branch", "");
            if (name.empty()) {
                json r = {{"ok", false}, {"error", "Branch name required"}};
                res.status = 400;
                res.set_content(r.dump(), "application/json"); return;
            }
            switchBranch(name);
            json r = {{"ok", true}};
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            json r = {{"ok", false}, {"error", "Bad request"}};
            res.status = 400;
            res.set_content(r.dump(), "application/json");
        }
    });

    // API: List conflicts
    svr.Get("/api/list-conflicts", [](const httplib::Request& req, httplib::Response& res) {
        loadRepositoryPath();
        json j;
        if (!repositoryPath.empty()) {
            j["conflicts"] = listConflicts();
        } else {
            j["conflicts"] = json::array();
        }
        res.set_content(j.dump(), "application/json");
    });

    // API: Whoami (alias)
    svr.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 200;
    });

    // Serve static files from web directory
    svr.Get("/(.*)", [](const httplib::Request& req, httplib::Response& res) {
        std::string path = webDir + "/" + req.matches[1].str();
        // Security: prevent directory traversal
        fs::path resolved = fs::weakly_canonical(path);
        if (resolved.string().find(webDir) != 0) {
            res.status = 403;
            res.set_content("Forbidden", "text/plain");
            return;
        }
        if (!fs::exists(resolved) || !fs::is_regular_file(resolved)) {
            // Fallback to index.html for SPA routing
            std::string content = readFile(webDir + "/index.html");
            if (content.empty()) {
                res.status = 404;
                res.set_content("Not found", "text/plain");
                return;
            }
            res.set_content(content, "text/html");
            return;
        }
        // Determine content type
        std::string ext = resolved.extension().string();
        std::string mime = "text/plain";
        if (ext == ".html") mime = "text/html";
        else if (ext == ".css") mime = "text/css";
        else if (ext == ".js") mime = "application/javascript";
        else if (ext == ".json") mime = "application/json";
        else if (ext == ".png") mime = "image/png";
        else if (ext == ".svg") mime = "image/svg+xml";
        else if (ext == ".ico") mime = "image/x-icon";

        std::string content = readFile(resolved.string());
        res.set_content(content, mime);
    });

    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "Error: Could not start server on port " << port << std::endl;
        std::cerr << "Try a different port: codekeeper-web <port>" << std::endl;
        return 1;
    }
    return 0;
}
