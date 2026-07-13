# CodeKeeper

**CodeKeeper** is a lightweight, single-file version control system for small teams. It provides both a **command-line interface** and a **web dashboard** for managing file versions, resolving conflicts, branching, and tracking changes — all without Git's complexity.

> Built for developers who want simple version control without the overhead. C++17, one source file, zero framework dependencies.

---

## Features

### 🖥Web Interface
- **Modern dashboard** — dark-themed single-page app with tabs for Overview, Staging, Commit, History, and Branches
- **REST API** — all operations available via JSON endpoints (auth, add, commit, rollback, branches, conflicts)
- **Zero JS frameworks** — plain HTML/CSS/JS served by a lightweight C++ HTTP server
- **Real-time feedback** — toast notifications for all actions, live server log

###  Security
- **No command injection** — hooks use `fork()`+`execlp()` instead of `system()`
- **Salted passwords** — SHA-256 with per-user salt (username), not plain SHA-256
- **Path traversal protection** — all file operations validated against repository root
- **Tamper-evident commit chain** — each commit includes the parent commit hash
- **Dead code removed** — unused Git bridge functions with `system()` calls eliminated

### Directory & Recursive Operations
- `codekeeper add .` — recursively stage entire directory trees
- `codekeeper commit "msg" /path/to/dir` — commit directories directly
- **Smart exclusion** — automatically skips `.staging/`, `versions/`, `branches/`, hidden files, and binary
- **Staging-based commit** — omit file args to commit whatever is staged

### Local or Central Repository
- **`--local` flag** — create a repo right in your current working directory (no `/var/lib/CodeKeeper` needed)
- **Central mode** — repos stored in `/var/lib/CodeKeeper/<project>` for shared access
- **Remote sync** — push/pull commits and versions to other CodeKeeper repos

### Branching & Merging
- Create, switch, and merge branches
- Interactive and automatic file merging with conflict markers
- `list-conflicts` to find all files that differ from the last committed version

### Authentication & Users
- User registration with salted password hashing
- Session-based login (stored in `.session`)
- `whoami` and `list-users` for user management
- Protected operations (commit, rollback, resolve) require authentication

### 🔧 Hooks & Automation
- `.pre-commit` and `.post-commit` scripts run automatically
- Hooks execute via safe `fork()`/`exec()` — no shell injection

---

## Quick Start

### 1. Build

Requirements: C++17, OpenSSL, Linux

```bash
sudo apt-get install libssl-dev
cd CodeKeeper

# CLI only
g++ -std=c++17 -o build/codekeeper codekeeper.cpp -lssl -lcrypto

# CLI + Web server
g++ -std=c++17 -Iinclude -o build/codekeeper-web codekeeper-web.cpp -lssl -lcrypto -lpthread
```

### 2. Initialize (choose one)

**Option A: Local repo (in your project directory)**
```bash
cd /home/me/my-project
codekeeper init myapp --local
# Repo structure created right here
```

**Option B: Central repo (shared location)**
```bash
codekeeper init myapp
cd /var/lib/CodeKeeper/myapp
```

### 3. Set up authentication
```bash
codekeeper auth register alice my-secret-password
codekeeper auth login alice my-secret-password
```

### 4. Stage and commit files
```bash
# Stage individual files
codekeeper add file1.cpp file2.cpp

# Or stage an entire directory recursively
codekeeper add .

# Check what's staged
codekeeper status

# Commit (uses staged files when no file args given)
codekeeper commit "Initial commit"

# View history
codekeeper history
```

### 5. Launch the web dashboard
```bash
# From the project directory (where .repo_path lives)
codekeeper serve 8080
# Open http://localhost:8080

# Or directly:
codekeeper-web 8080 --dir /path/to/project --web /path/to/CodeKeeper/web
```

---

## Web Interface

The web dashboard provides a visual alternative to the CLI:

| Tab | Purpose |
|-----|---------|
| **Overview** | Dashboard with commit count, staged files, conflicts, current branch, recent commits |
| **Staging** | Stage/unstage files by path; add entire directories |
| **Commit** | Create commits with message; rollback files |
| **History** | Full commit log with IDs, messages, timestamps, and file counts |
| **Branches** | Create branches, switch between them, see current branch |

### API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/whoami` | Current authenticated user |
| `POST` | `/api/auth/login` | Login |
| `POST` | `/api/auth/register` | Register |
| `POST` | `/api/auth/logout` | Logout |
| `POST` | `/api/init` | Initialize repo (`name`, `local`) |
| `GET` | `/api/status` | Staged files and current branch |
| `POST` | `/api/add` | Stage files/directories |
| `POST` | `/api/reset` | Unstage files |
| `POST` | `/api/commit` | Create commit (`message`, `files`) |
| `GET` | `/api/history` | Full commit history |
| `POST` | `/api/rollback` | Rollback files |
| `POST` | `/api/branch` | Create branch (`name`) |
| `POST` | `/api/switch` | Switch branch (`branch`) |
| `GET` | `/api/branches` | List branches |
| `GET` | `/api/list-conflicts` | List conflicted files |

---

## Command Reference

### Repository Setup

| Command | Description |
|---------|-------------|
| `init <name>` | Initialize a central repo at `/var/lib/CodeKeeper/<name>` |
| `init <name> --local` | Initialize a repo in the current directory |

### Staging

| Command | Description |
|---------|-------------|
| `add <files...>` | Stage files or directories (recursive) |
| `reset <files...>` | Unstage files |
| `status` | Show staged, modified, and untracked files |

### Committing

| Command | Description |
|---------|-------------|
| `commit <message> [files...]` | Commit files (or staged files if no files given) |
| `history` | View full commit history |
| `rollback <target> [commitGUID]` | Rollback a file to a previous version |

### Branches

| Command | Description |
|---------|-------------|
| `branch <name>` | Create a new branch |
| `switch <branch>` | Switch to an existing branch |
| `merge <branch1> <branch2>` | Merge two branches |

### Conflicts

| Command | Description |
|---------|-------------|
| `conflicts <file>` | Check if a file has conflicts |
| `resolve <file> <resolution>` | Resolve a conflict with a resolution file |
| `list-conflicts` | List all files with conflicts |
| `merge-files <f1> <f2> <out> [--interactive]` | Merge two files (optionally interactively) |

### Authentication & Users

| Command | Description |
|---------|-------------|
| `auth register <user> <pass>` | Register a new user |
| `auth login <user> <pass>` | Login |
| `auth logout` | Logout |
| `whoami` | Show current user |
| `list-users` | List all registered users |

### Remote & Archive

| Command | Description |
|---------|-------------|
| `set-remote <path>` | Configure a remote CodeKeeper repo path |
| `push` | Push commits and versions to remote |
| `pull` | Pull commits and versions from remote |
| `archive` | Archive the `versions/` directory |

### Web Server

| Command | Description |
|---------|-------------|
| `serve [port]` | Start the web dashboard (launches `codekeeper-web`) |

---

## Security Design

CodeKeeper has been hardened against common vulnerabilities:

| Issue | Mitigation |
|-------|-----------|
| **Command injection** | Hook execution uses `fork()`+`execlp()` — no shell involved. Dead `system()` calls removed. |
| **Password cracking** | Passwords hashed with SHA-256 **and salt** (`username:password`). No plaintext storage. |
| **Path traversal** | `isPathSafe()` validates all file paths resolve within the repository root using canonical paths. |
| **Session hijacking** | Session stored in `.session` file with `chmod 0600` on `.users`. |
| **Tampered history** | Each commit hash includes the **parent commit ID**, creating an auditable chain. |
| **Memory leaks** | OpenSSL EVP context is freed on all code paths (including error returns). |

### What's *not* implemented (by design)
- No encryption at rest — this is a local VCS for small teams, not a security product
- No network transport security — push/pull is filesystem-level; use SSH tunnels for remote access
- No SQL injection — no database (flat files only)

---

## Project Structure

```
CodeKeeper/
├── codekeeper.cpp          # CLI application (single file)
├── codekeeper-web.cpp      # Web server binary
├── codekeeper-postinstall.sh
├── install.sh
├── web/
│   └── index.html          # Single-page web app (embedded CSS + JS)
├── include/
│   ├── httplib.h           # Header-only HTTP library (cpp-httplib)
│   └── json.hpp            # Header-only JSON library (nlohmann/json)
└── build/
    ├── codekeeper          # Compiled CLI binary
    └── codekeeper-web      # Compiled web server binary
```

---

## Hooks

Place executable scripts in the repository root:

- **`.pre-commit`** — runs before each commit. Exit non-zero to abort.
- **`.post-commit`** — runs after each successful commit.

```bash
#!/bin/bash
# Example .pre-commit hook: ensure no TODO is left
if grep -r "TODO" --include="*.cpp" .; then
  echo "Remove TODOs before committing!"
  exit 1
fi
exit 0
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `Repository not initialized` | Run `codekeeper init <name>` or `codekeeper init <name> --local` first |
| `You must authenticate first` | Run `codekeeper auth login <user> <pass>` |
| `File is outside the repository` | Use `--local` init in your project directory, or move files into the repo |
| `Permission denied` | Ensure `/var/lib/CodeKeeper` is writable, or use `--local` |
| `web/index.html not found` | Run `codekeeper serve` from the CodeKeeper project root, or use `--web <path>` |
| Hook fails | Check hook script is executable (`chmod +x .pre-commit`) |

---

## Building from Source

```bash
# Dependencies
sudo apt-get install libssl-dev g++

# Build CLI
g++ -std=c++17 -o build/codekeeper codekeeper.cpp -lssl -lcrypto

# Build web server
g++ -std=c++17 -Iinclude -o build/codekeeper-web codekeeper-web.cpp -lssl -lcrypto -lpthread

# Run
./build/codekeeper --help
./build/codekeeper serve
```

---

## Notes

- CodeKeeper is designed for **educational and internal use** — not a drop-in Git replacement
- The entire core is a **single C++ file** — easy to audit, modify, and deploy
- The web interface adds **zero JavaScript framework dependencies** — just vanilla HTML/CSS/JS
- Compatible with Linux only (uses `fork()`, `waitpid()`, `sys/stat.h`)
