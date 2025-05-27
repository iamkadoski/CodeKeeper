
# CodeKeeper

**CodeKeeper** is a lightweight version control application tailored for small teams of developers. 
It provides an easy-to-use command-line interface for managing file versions, resolving conflicts, 
and tracking changes. CodeKeeper is designed to operate with a central repository for streamlined collaboration and efficiency.

---

## Features

- **Initialize Repository**
  - Set up a new central repository.
  - Automatically creates essential directories (`.keep`, `.versions`, etc.).
  - Checks if the repository is already initialized to prevent duplication.

- **File Versioning**
  - Commit files or entire directories using wildcards (`*.*`, `.`).
  - Maintain a history of commits with GUIDs, timestamps, and file paths.

- **Conflict Detection and Resolution**
  - Check for conflicts in files against the latest committed version.
  - Resolve conflicts by providing a resolution file.

- **Branching and Merging**
  - Create branches for isolated development.
  - Merge changes from two branches with automatic conflict detection.

- **History and Rollback**
  - View the history of commits, including GUIDs, timestamps, and file details.
  - Rollback a file or repository to a specific version or commit.

- **Archiving**
  - Archive the `.versions` directory for backup or storage.

- **Authentication**
  - Ensure that only authorized users can perform certain actions (e.g., commit, resolve conflicts).

---

## Help Menu Explanined

- **SHA-256 Commit IDs:** Every commit is uniquely identified and tamper-evident.
- **Branching:** Create and switch between branches for parallel development.
- **Staging Area:** Add/reset files before committing, similar to `git add`/`git reset`.
- **Status Command:** View staged, modified, and untracked files.
- **Authentication:** User registration, login, logout, and enforcement for protected actions.
- **Remote Support:** Set a remote repository, push and pull commits and versions.
- **Hooks:** Pre-commit and post-commit hooks for automation.
- **Merge & Conflict Resolution:** Merge files/branches, interactive/manual conflict resolution, and conflict listing.
- **User Management:** List users, show current user (`whoami`).
- **Robust Initialization:** Secure repo setup, including user file permissions.


## Installation

### Prerequisites

- **Operating System**: Linux (requires `sudo` for certain operations).
- **Dependencies**: C++ Standard Library and `filesystem` support.
- **Optional Tools**: `zip` (for archiving).

### Building CodeKeeper

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd CodeKeeper
   ```

2. Compile the application:
   ```bash
   g++ -std=c++17 -o ./build/codekeeper codekeeper.cpp -lssl -lcrypto
   ```

3. Run install.sh file for installation, the shell script will create the necessary folders and move the application to the required folder:
   ```bash
   sudo chmod +x install.sh

   ./install.sh
   ```

---

## Usage

### General Help
```bash
codekeeper --help
```

### Commands

1. **Initialize Repository**
   ```bash
   sudo codekeeper init <project_name>
   ```

2. **Commit Files**
   ```bash
   codekeeper commit "<commit_message>" <file1> <file2> ...
   ```
   Use `.` or `*.*` to commit all files in the current directory.

3. **View History**
   ```bash
   codekeeper history
   ```

4. **Rollback**
   ```bash
   codekeeper rollback <file|commit_guid>
   ```

5. **Check for Conflicts**
   ```bash
   codekeeper conflicts <file>
   ```

6. **Resolve Conflict**
   ```bash
   codekeeper resolve <file> <resolution_file>
   ```

7. **Create a Branch**
   ```bash
   codekeeper branch <branch_name>
   ```

8. **Merge Branches**
   ```bash
   codekeeper merge <branch1> <branch2>
   ```

9. **Archive Versions**
   ```bash
   codekeeper archive
   ```

## Quick Start

### 1. Build

Requires C++17, OpenSSL, and Linux (tested on Ubuntu):

```bash
sudo apt-get install libssl-dev
# Compile
 g++ -std=c++17 -o codekeeper codekeeper.cpp -lssl -lcrypto
```

### 2. Post-Install Setup (as root/admin)

```bash
sudo bash codekeeper-postinstall.sh
```
This creates `/var/lib/CodeKeeper` and a skeleton repo structure.

### 3. Initialize a Repository

```bash
./codekeeper init <projectName>
cd /var/lib/CodeKeeper/<projectName>
```

### 4. User Registration & Authentication

```bash
./codekeeper auth register <username> <password>
./codekeeper auth login <username> <password>
```

### 5. Basic Workflow

```bash
# Add files to staging
./codekeeper add file1.cpp file2.cpp

# Check status
./codekeeper status

# Commit staged files
./codekeeper commit "Initial commit" file1.cpp file2.cpp

# View history
./codekeeper history
```

---

## Command Reference

| Command | Description |
|---------|-------------|
| `init <projectName>` | Initialize a new repository |
| `add <files>` | Add files to staging area |
| `reset <files>` | Remove files from staging area |
| `status` | Show status of working directory and staging area |
| `commit <message> <files>` | Commit specified files (if staging is empty) |
| `rollback <target> [commitGUID]` | Revert a file or repo to a specific version |
| `history` | View commit history |
| `conflicts <file>` | Check for conflicts in a file |
| `resolve <file> <resolutionFile>` | Resolve a conflict |
| `archive` | Archive the `.versions` folder |
| `auth register <user> <pass>` | Register a new user |
| `auth login <user> <pass>` | Login as a user |
| `auth logout` | Logout current user |
| `merge <branch1> <branch2>` | Merge two branches |
| `switch <branch>` | Switch to a branch |
| `set-remote <path>` | Set remote repository path |
| `push` | Push commits/versions to remote |
| `pull` | Pull commits/versions from remote |
| `list-conflicts` | List all files with conflicts |
| `whoami` | Show current authenticated user |
| `list-users` | List all registered users |
| `merge-files <f1> <f2> <out> [--interactive]` | Merge two files (optionally interactively) |

---

## Hooks
- Place executable `.pre-commit` and `.post-commit` scripts in the repo root to run before/after each commit.

---

## Security & Permissions
- The `.users` file is created with mode 600 (owner read/write only).
- Only authenticated users can commit, rollback, or resolve conflicts.
- Session state is stored in `.session` in the repo root.

---

## Remote Repository
- Use `set-remote <path>` to configure a remote CodeKeeper repo.
- `push` and `pull` sync commits and versioned files.

---

## Troubleshooting
- If you see `Error: You must authenticate first`, run `auth login`.
- If registration fails, ensure the `.users` file exists and is writable.
- For OpenSSL errors, ensure `libssl-dev` is installed and linked.

---

## Notes
- This project is for educational and internal use. Not a drop-in replacement for Git.
- For advanced usage, see comments in `codekeeper.cpp` and the help command (`./codekeeper`).



---

## Configuration

- The central repository path is configured in `/usr/bin/codekeeper/codekeeper_config`.
- Ensure the `.keep` directory and other project files are correctly created within the central origin path.

---

## Contributing

Contributions to CodeKeeper are welcome! If you'd like to improve the application or report issues, 
please create a pull request or open an issue on GitHub.

---


