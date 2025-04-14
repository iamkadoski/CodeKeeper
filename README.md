
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

---

## Configuration

- The central repository path is configured in `/usr/bin/codekeeper/codekeeper_config`.
- Ensure the `.keep` directory and other project files are correctly created within the central origin path.

---

## Contributing

Contributions to CodeKeeper are welcome! If you'd like to improve the application or report issues, 
please create a pull request or open an issue on GitHub.

---


